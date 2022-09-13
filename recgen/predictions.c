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

// Making these static because their scope is intentionally limited to this file.
// The workingset is the working area where we build up the predictions.
static __thread prediction_t *g_workingset;

void create_workingset(size_t num_recs)
{
    g_workingset = malloc(num_recs * sizeof(prediction_t));
    // NOTE: We don't free this ever because it sticks around forever.
}

static void init_workingset(size_t num_recs)
{
    // Populate the workingset with empty predictions.
    size_t i;
    for (i = 0; i < num_recs; i++)
    {
        g_workingset[i].rating_count = 0;
        g_workingset[i].elementid =  (exp_elt_t) i + 1;
        g_workingset[i].rating_accum = 0.0;
        g_workingset[i].rating = -1;
    }
} // end ini_workingset()


//
// Compare any two predictions, for use as a callback function by qsort()
//
// Returns -1 if the first rating (recommendation value) is more than the second
//          0 if the ratings are equal
//          1 if the first rating is less than the second rating
// NOTE: this behavior is flipped (normally sort is ascending) so we get highest
// rating values at beginning of sorted structure
//
static int predcmp(const void *p1, const void *p2)
{
    prediction_t x = *(const prediction_t *) p1,
                 y = *(const prediction_t *) p2;

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
            // Favor preds with higher ratingCounts b/c they're a bit stronger.
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


// Tally gets called once for each live user's ratings and calculates possible recommendation values for the other elements
static void tally(valence_t *bb,
                  bb_ind_t *bind_seg,
                  valence_t *bb_ds,
                  bb_ind_t *bind_seg_ds,
                  uint32_t user_rated,
                  short user_rating,
                  int8_t *tiny_slopes,
                  int8_t *tiny_offsets)
{
    double rating, denominator;
    int numerator = 0, int_rating;
    valence_t *bb_ptr;
    exp_elt_t prediction_to_make;
    short short_slope, short_offset = 0;
    uint8_t slope_index=0, offset_index=0;

    // There are two similar but different sections of code below. They are separate for increased clarity. Merging would save a little
    // redundancy but add complexity in human understanding. Because this piece is the core of the recommender, let's err
    // on the side of human understanding.

    // First we need to find the (x,uR) valences where x goes from 1 to (uR - 1).
    // Iterate over, e.g., (1..232,233) given 233 as userRated passed in to Tally()

    bb_ind_t  y_start = bind_seg_ds[user_rated];
    exp_elt_t  user_rated_next;
    
    user_rated_next = user_rated;
    
    while (-1 == bind_seg_ds[++user_rated_next])
    {
        if (user_rated_next == (BE.num_elts - 1))
            break;
    }
    
    bb_ind_t y_start_next = bind_seg_ds[user_rated_next];
    
    // Do we have valences with a user_rated in the y position?
    if (y_start > -1)
    {
        // If y_start_next is -1, there is no next y.
        if (-1 == y_start_next)
        {
            // We only get here if there is no next y.
            y_start_next = (bb_ind_t) (g_num_confident_valences - 1);
            syslog(LOG_INFO, "y_start is %ld and y_start_next is %ld", y_start, y_start_next);
        }

        for (bb_ptr = &bb_ds[y_start]; bb_ptr < &bb_ds[y_start_next]; bb_ptr++)
        {
            // Get the prediction_to_make value from the val_key.
            GETELT(bb_ptr->eltid, prediction_to_make);
            
            GETHIBITS(bb_ptr->soindex, slope_index);
            GETLOBITS(bb_ptr->soindex, offset_index);
            
            short_slope = tiny_slopes[slope_index];
            short_offset = tiny_offsets[offset_index];

            // We are solving for x so we want x = (y - b) / m
            // Numerator calculation assumes it's operating on ints.
            numerator = (FLOAT_TO_SHORT_MULT * user_rating) - short_offset;
     
            // Guard against div by 0.
            if (0 == short_slope)
                short_slope = 1; // add an epsilon to slope.
     
            denominator = short_slope / (double) FLOAT_TO_SHORT_MULT;
            rating = numerator / denominator;

            if (rating < RATINGS_BOUND_LOWER)
            {
                g_workingset[prediction_to_make-1].rating_accum += RATINGS_BOUND_LOWER;
            }
            else
            {
                if (rating > RATINGS_BOUND_UPPER)
                {
                    g_workingset[prediction_to_make-1].rating_accum += RATINGS_BOUND_UPPER;
                }
                else
                {
                    g_workingset[prediction_to_make-1].rating_accum += (float) rating;
                }
            }
            g_workingset[prediction_to_make-1].rating_count++;
        } // end for loop
    } // end if y_start not 0
    
    // Here we need to do the (uR, y) valences where y goes from uR+1 to NUM_ELTS.
    // Get the starting point of the fixed x value in the Beast.
    bb_ind_t  x_start = bind_seg[user_rated];
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
        {
            // We only get here if there is no next x
            x_start_next = (bb_ind_t) (g_num_confident_valences - 1);
            syslog(LOG_INFO, "DEBUG: x_start is %ld and x_start_next is %ld", x_start, x_start_next);
        }
        
        for (bb_ptr = &bb[x_start]; bb_ptr < &bb[x_start_next]; bb_ptr++)
        {
            GETELT(bb_ptr->eltid, prediction_to_make);
            
            GETHIBITS(bb_ptr->soindex, slope_index);
            GETLOBITS(bb_ptr->soindex, offset_index);
            
            short_slope = tiny_slopes[slope_index];
            short_offset = tiny_offsets[offset_index];
            
            // We are solving for y so we want y = mx + b
            int_rating = user_rating * short_slope + short_offset;
            
            if (int_rating < RATINGS_BOUND_LOWER)
            {
                g_workingset[prediction_to_make - 1].rating_accum += RATINGS_BOUND_LOWER;
            }
            else
            {
                if (int_rating > RATINGS_BOUND_UPPER)
                {
                    g_workingset[prediction_to_make - 1].rating_accum += RATINGS_BOUND_UPPER;
                }
                else
                {
                    g_workingset[prediction_to_make - 1].rating_accum += int_rating;
                }
            }
            g_workingset[prediction_to_make - 1].rating_count++;
        } // end for loop across x,y pairs for fixed x = userRated
    } // end if x_start not 0
} // end Tally()


// Composite the prediction values.
static void composite(int num_recs)
{
    int i;
    for (i = 0; i < num_recs; i++)
    {
        if (g_workingset[i].rating_count < MIN_VALENCES_FOR_PREDICTIONS)
        {
            g_workingset[i].rating = -10;
            continue;
        }
        g_workingset[i].rating = g_workingset[i].rating_accum / g_workingset[i].rating_count;
    }
} // end composite()


// Put the passed-in eltid's entry at the head of the list.
static void find_single(int num_recs, int eltid)
{
    int i;
    for (i = 0; i < num_recs; i++)
    {
        if ((exp_elt_t ) eltid == g_workingset[i].elementid)
        {
            // found it! now put it first
            if (g_workingset[i].rating_count < MIN_VALENCES_FOR_PREDICTIONS)
            {
                g_workingset[0].rating = -1;
            }
            else
            {
                g_workingset[0].rating = g_workingset[i].rating_accum / g_workingset[i].rating_count;
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
    long long start, finish;
    start = current_time_micros();

    // Get a handle to the combined beast & bind leashes.
    valence_t *bb = bb_leash();
    
    // Get a handle to the bind index.
    bb_ind_t  *bind_seg = bind_seg_leash();
    
    // Get a handle to the combined beast & bind leashes, DS version.
    valence_t *bb_ds = bb_ds_leash();
    
    // Get a handle to the bind index, DS version.
    bb_ind_t  *bind_seg_ds = bind_seg_ds_leash();
    
    // Get a handle to the Popularity index.
    popularity_t *pop = pop_leash();
        
    int8_t *tiny_slopes = tiny_slopes_leash();
    int8_t *tiny_offsets = tiny_offsets_leash();

    // Check for valid ratings passed in.
    if ((NULL == ur) || (0 == rat_length))
    {
        syslog(LOG_ERR, "ERROR: cannot make Predictions for empty UserRating list");
        return (false);
    }
    init_workingset(BE.num_elts);

    int userid = ur[0].userid;
    int i = 0;

    while (i < rat_length)
    {
        tally(bb, bind_seg, bb_ds, bind_seg_ds, ur[i].elementid, ur[i].rating, tiny_slopes, tiny_offsets);
        i++;
    }

    // Now set rating = accum / count.
    composite(BE.num_elts);

    // Are we recommending top numRecs items?
    if (0 == eltid)
    {
        qsort(g_workingset, BE.num_elts, sizeof(prediction_t), predcmp);
        // Copy workingset to recs. Note that numRecs is small. Bit below
        // should work b/c we've just sorted WorkingSet by pred value
        int ws_walker = 0;
        exp_elt_t curr_elt;

        /*
        // b debugging
        // Print the ratings intelligently. How many of each value are there?
        syslog(LOG_INFO, "&&& Ratings for this user &&&");
        short r1count = 0, r2count = 0, r3count = 0, r4count = 0, r5count = 0;
         
        for (i = 0; i < ratlength; i++)
        {
            switch (ur[i].rating)
            {
                case 1:
                    r1count++;
                    break;
                case 2:
                    r2count++;
                    break;
                case 3:
                    r3count++;
                    break;
                case 4:
                    r4count++;
                    break;
                case 5:
                    r5count++;
                    break;
                default:
                    syslog(LOG_ERR, "INVALID rating of %d from user %d", ur[i].rating, ur[i].userId);
            }
        }
        syslog(LOG_INFO, "r1count: %d, r2count: %d, r3count: %d, r4count: %d, r5count: %d", r1count, r2count, r3count, r4count, r5count);
        
        // Print the rec values.
        for (i = 0; i < numRecs * 10; i++)
        {
            syslog(LOG_INFO, "before finding correct popularity recvalue[%d] is %f, ratingCount is %d, pop is %d",
                   i,
                   (double) g_WorkingSet[i].rating,
                   g_WorkingSet[i].ratingCount,
                   Pop[g_WorkingSet[i].elementId]);
        }
        // e debugging
        */

        i = 0;
        
        while (i < num_recs)
        {
            curr_elt = g_workingset[ws_walker].elementid;

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

    // we walk the preds list
    float rec_value = 0.0;
    double tmp_float;
    long tmp_long;

    /*
    // b debugging
    for (i = 0; i < numRecs; i++)
    {
        syslog(LOG_INFO, "before rounding recvalue[%d] is %f, ratingCount is %d, pop is %d",
               i,
               (double) recs[i].rating,
               recs[i].ratingCount,
               Pop[recs[i].elementId]);
    }
    // e debugging
    */

    // Clean up the elt recommendations for this user.
    for (i = 0; i < num_recs; i++)
    {
        rec_value = recs[i].rating;
        if ((int) (rec_value) == -10)
        {
            syslog(LOG_WARNING, "WARNING: found a -1 for a prediction for element %d, user %d.",
                   recs[i].elementid, userid);
        }

        // *10, round, /10 to get just one single digit after decimal place
        tmp_float = (double) rec_value;
        tmp_long = bmh_round(tmp_float);
        rec_value = tmp_long / (float) 10.0;
        recs[i].rating = rec_value;
    } // end for loop

    finish = current_time_micros();
    syslog(LOG_INFO, "Time to perform Prediction: %d microseconds.", (int) (finish - start));
    return (true);
}// end Predictions()

