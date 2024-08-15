// SPDX-FileCopyrightText: 2022 Brian Calhoun <brian@bemorehuman.org>
//
// SPDX-License-Identifier: MIT
//
// MIT License
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
// of the Software, and to permit persons to whom the Software is furnished to do
// so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
// This file is part of bemorehuman. See https://bemorehuman.org

#include "recgen.h"

//
// This file contains helper functions that relate to predictions.
//

#ifdef USE_FCGI
static __thread prediction_t *g_workingset;  // The workingset is the working area where we build up the predictions.
#else
static prediction_t *g_workingset;  // The workingset is the working area where we build up the predictions.
#endif


static bool pcr_created = false;  // Have we populated the pre-calculated ratings yet?

static int pcrx[256][32];    // Pre-calculated rating of x = (y - b)/m
static int pcry[256][32];    // Pre-calculated rating of y = mx + b


void create_workingset(size_t num_recs)
{
    g_workingset = malloc(num_recs * sizeof(prediction_t));
    // NOTE: We don't free this ever because it sticks around forever.
}

static void init_workingset(size_t num_recs)
{
    // Populate the workingset with empty predictions.
    for (size_t i = 0; i < num_recs; i++)
    {
        g_workingset[i].rating_count = 0;
        g_workingset[i].elementid =  (exp_elt_t) i + 1;
        g_workingset[i].rating_accum = 0;
        g_workingset[i].rating = -1;
    }
} // end ini_workingset()


//
// Compare any two predictions, for use as a callback function by qsort()
//
// Returns -1 if the first rating (recommendation value) is more than the second
//          0 if the ratings are equal
//          1 if the first rating is less than the second rating
// NOTE: this behavior is flipped (normally sort is ascending) so we get the highest
// rating values at beginning of sorted structure
//
static int predcmp(const void *p1, const void *p2)
{
    const prediction_t x = *(const prediction_t *) p1;
    const prediction_t y = *(const prediction_t *) p2;

    if (x.rating > y.rating)
    {
        return (-1);
    }
    else
    {
        if (x.rating < y.rating)
        {
            return (1);
        }
        else
        {
            // Favor predictions with higher ratingCounts b/c they're a bit stronger.
            if (x.rating_count > y.rating_count)
            {
                return (-1);
            }
            else
            {
                return (1);
            }
        }
    }
} // end predcmp()


// Pre-calculate all possible ratings so we don't have to do this when generating each individual rec. 256 possible
// slope-offset pairs and 32 possible ratings values for a total of 8192 possible combinations.
static void create_pcrs(const int8_t *tiny_slopes,
                        const double *tiny_slopes_inv,
                        const int8_t *tiny_offsets)
{
    int counter = 0;
    syslog(LOG_INFO, "....Inside create_pcrs....");
    for (uint8_t i = 0; counter < 256; i++, counter++)   // slope - offset combination
    {
        for (int j = 1; j < 33; j++)   // user-rating
        {
            // pcrx holds the rating of x = (y - b) / m
            // Note: We want to multiply, not divide at this point b/c divide is slower (this code runs a lot)
            // Note: It's important to get the scaling right for all variables. This code is correct!
            double rating = FLOAT_TO_SHORT_MULT * (tiny_slopes_inv[GET_HIGH_4_BITS(i)] * (FLOAT_TO_SHORT_MULT * j
                                                       - tiny_offsets[GET_LOW_4_BITS(i)]));
            rating = (rating < RATINGS_BOUND_LOWER) ? RATINGS_BOUND_LOWER
                    : (rating > RATINGS_BOUND_UPPER) ? RATINGS_BOUND_UPPER : rating;
            pcrx[i][j-1] = (int) bmh_round(rating);

            // pcry holds the rating of y = mx + b.
            rating = j * tiny_slopes[GET_HIGH_4_BITS(i)] + tiny_offsets[GET_LOW_4_BITS(i)];
            rating = (rating < RATINGS_BOUND_LOWER) ? RATINGS_BOUND_LOWER
                    : (rating > RATINGS_BOUND_UPPER) ? RATINGS_BOUND_UPPER : rating;
            pcry[i][j-1] = (int) bmh_round(rating);
        } // end for loop across all possible user-rating values
    } // end for loop across all possible soindex values
    pcr_created = true;
} // end create_pcrs()

// Tally gets called once for each live user and calculates possible recommendation values for the other elements
static void tally(valence_t *bb,
                  const bb_ind_t *bind_seg,
                  valence_t *bb_ds,
                  const bb_ind_t *bind_seg_ds,
                  int rat_length,
                  rating_t ur[])
{
    for (int i=0; i < rat_length; i++)
    {
        int rating;
        const valence_t *bb_ptr;
        exp_elt_t prediction_to_make;
        const uint32_t user_rated = ur[i].elementid;
        const uint8_t user_rating = ur[i].rating;

        // There are two similar but different sections of code below. They are separate for increased clarity. Merging would save a little
        // redundancy but add complexity in human understanding. Because this piece is the core of the recommender, let's err
        // on the side of human understanding.

        // First we need to find the (x,uR) valences where x goes from 1 to (uR - 1).
        // Iterate over, e.g., (1..232,233) given 233 as userRated passed in to Tally()

        const bb_ind_t  y_start = bind_seg_ds[user_rated];

        exp_elt_t user_rated_next = user_rated;

        while (-1 == bind_seg_ds[++user_rated_next])
            if (user_rated_next == (BE.num_elts - 1)) break;

        bb_ind_t y_start_next = bind_seg_ds[user_rated_next];

        // Do we have valences with a user_rated in the y position?
        if (y_start > -1)
        {
            // If y_start_next is -1, there is no next y.
            if (-1 == y_start_next)
                y_start_next = (bb_ind_t) (g_num_confident_valences - 1);

            const valence_t *const end_ptr = &bb_ds[y_start_next];
            for (bb_ptr = &bb_ds[y_start]; bb_ptr < end_ptr; ++bb_ptr)
            {
                // Get the prediction_to_make value from the val_key.
                prediction_to_make = GET_ELT(bb_ptr->eltid);

                // We are solving for x, so we want x = (y - b) / m
                rating = pcrx[bb_ptr->soindex][user_rating - 1];
                g_workingset[prediction_to_make - 1].rating_accum += rating;
                g_workingset[prediction_to_make - 1].rating_count++;
            } // end for loop
        } // end if y_start not 0

        // Here we need to do the (uR, y) valences where y goes from uR+1 to NUM_ELTS.
        // Get the starting point of the fixed x value in the Beast.
        const bb_ind_t  x_start = bind_seg[user_rated];
        user_rated_next = user_rated;
        while (-1 == bind_seg[++user_rated_next])
        {
            if (user_rated_next == (BE.num_elts - 1))
                break;
        }

        bb_ind_t x_start_next = bind_seg[user_rated_next];

        // Do we have valences with a user_rated in the x position?
        if (x_start > -1)
        {
            // If x_start_next is -1, there is no next x.
            if (-1 == x_start_next)
                x_start_next = (bb_ind_t) (g_num_confident_valences - 1);

            const valence_t *const end_ptr = &bb[x_start_next];
            for (bb_ptr = &bb[x_start]; bb_ptr < end_ptr; bb_ptr++)
            {
                // Get the prediction_to_make value from the val_key.
                prediction_to_make = GET_ELT(bb_ptr->eltid);

                // We are solving for y, so we want y = mx + b. Scaling is correct.
                rating = pcry[bb_ptr->soindex][user_rating - 1];
                g_workingset[prediction_to_make - 1].rating_accum += rating;
                g_workingset[prediction_to_make - 1].rating_count++;
            } // end for loop across x,y pairs for fixed x = userRated
        } // end if x_start not 0
    } // end for loop across user's ratings
} // end Tally()


// Composite the prediction values.
static void composite(uint64_t num_recs)
{
    for (int i = 0; i < (int) num_recs; i++)
    {
        if (g_workingset[i].rating_count < MIN_VALENCES_FOR_PREDICTIONS)
        {
            g_workingset[i].rating = -10;
            continue;
        }
        g_workingset[i].rating = (int16_t) bmh_round(g_workingset[i].rating_accum / (double) g_workingset[i].rating_count);
    }
} // end composite()


// Put the passed-in eltid's entry at the head of the list.
static void find_single(uint64_t num_recs, int eltid)
{
    for (int i = 0; i < (int) num_recs; i++)
    {
        if ((exp_elt_t ) eltid == g_workingset[i].elementid)
        {
            // found it! now put it first
            if (g_workingset[i].rating_count < MIN_VALENCES_FOR_PREDICTIONS)
            {
                g_workingset[0].rating = -10;
            }
            else
            {
                g_workingset[0].rating = (int16_t) bmh_round(g_workingset[i].rating_accum / (double) g_workingset[i].rating_count);
            }

            g_workingset[0].elementid = (exp_elt_t) eltid;
            g_workingset[0].rating_count = g_workingset[i].rating_count;
            g_workingset[0].rating_accum = g_workingset[i].rating_accum;

            return;
        }
    }
}  // end find_single()


// Param eltid is for the situations when we want to know about a rec for a specific product id.
// Use param target_pop for the situation when we want to get recs from a target popularity bucket (or more popular).
bool predictions(rating_t ur[], int rat_length, prediction_t recs[], int num_recs, int eltid, popularity_t target_pop)
{
    const long long start = current_time_micros();

    // Get a handle to the combined beast & bind leashes.
    valence_t *bb = bb_leash();
    
    // Get a handle to the bind index.
    const bb_ind_t  *bind_seg = bind_seg_leash();
    
    // Get a handle to the combined beast & bind leashes, DS version.
    valence_t *bb_ds = bb_ds_leash();
    
    // Get a handle to the bind index, DS version.
    const bb_ind_t  *bind_seg_ds = bind_seg_ds_leash();
    
    // Get a handle to the Popularity index.
    const popularity_t *pop = pop_leash();

    const int8_t *tiny_slopes = tiny_slopes_leash();
    const double *tiny_slopes_inv = tiny_slopes_inv_leash();
    const int8_t *tiny_offsets = tiny_offsets_leash();

    // If we haven't created the pre-computed ratings yet, do that. Only need to do this once per valence load, so
    // the creation should really be closer to that. It's here for now just see if it's worth it overall.
    if (!pcr_created) create_pcrs(tiny_slopes,
                                  tiny_slopes_inv,
                                  tiny_offsets);

    // Check for valid ratings passed in.
    if ((NULL == ur) || (0 == rat_length))
    {
        syslog(LOG_ERR, "ERROR: cannot make Predictions for empty UserRating list");
        return (false);
    }
    init_workingset(BE.num_elts);

    const int userid = ur[0].userid;
    int i;

    tally(bb, bind_seg, bb_ds, bind_seg_ds, rat_length, ur);

    // Now set rating = accum / count.
    composite(BE.num_elts);

    // Are we recommending top numRecs items?
    if (0 == eltid)
    {
#if defined(__linux__) || defined(__APPLE__)
        qsort(g_workingset, BE.num_elts, sizeof(prediction_t), predcmp);
#endif

#if defined(__NetBSD__)
        // On NetBSD, qsort is very slow for this particular situation. Mergesort is much faster.
        mergesort(g_workingset, BE.num_elts, sizeof(prediction_t), predcmp);
#endif
        
        // Copy workingset to recs. Note that numRecs is small. The bit below
        // should work b/c we've just sorted WorkingSet by pred value
        int ws_walker = 0;

        i = 0;

        // clean up target_pop
        if (target_pop < LOWEST_POP_NUMBER || target_pop > HIGHEST_POP_NUMBER)
            target_pop = LOWEST_POP_NUMBER;

        while (i < num_recs)
        {
            const exp_elt_t curr_elt = g_workingset[ws_walker].elementid;

            // check to see if the rec to make is in target popularity bucket.
            if (pop[curr_elt] <= target_pop)
            {
                recs[i].elementid = curr_elt;
                recs[i].rating = g_workingset[ws_walker].rating;
                recs[i].rating_accum = g_workingset[ws_walker].rating_accum;
                recs[i].rating_count = g_workingset[ws_walker].rating_count;
                i++;
            }
            ws_walker++;
        } // end while walking numRecs to check for targetPop
    } // end if we're recommending numRecs items
    else
    {
        find_single(BE.num_elts, eltid);
        recs[0].elementid = g_workingset[0].elementid;
        recs[0].rating = g_workingset[0].rating;
        recs[0].rating_accum = g_workingset[0].rating_accum;
        recs[0].rating_count = g_workingset[0].rating_count;
    } // end else we're trying to find a single rec

    // Clean up the elt recommendations for this user.
    for (i = 0; i < num_recs; i++)
    {
        if ((int) (recs[i].rating) == -10)
        {
            syslog(LOG_WARNING, "WARNING: found a -1 for a prediction for element %d, user %d.",
                   recs[i].elementid, userid);
        }

        recs[i].rating = (int16_t) bmh_round(recs[i].rating / 10.0);
    } // end for loop

    const long long finish = current_time_micros();
    syslog(LOG_INFO, "Time to perform Prediction: %d microseconds.", (int) (finish - start));

    return (true);
}// end Predictions()

