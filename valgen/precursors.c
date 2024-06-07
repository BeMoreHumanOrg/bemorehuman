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

#include "valgen.h"

// Forward declarations
static void linefit(uint8_t, const uint8_t *, const uint8_t *, double *, double *, int *);

// BeastConf is a lookup table to determine the 95% confidence of relevance of a particular valence. Values in
//    the table are gleaned from a 1950's textbook
// NOTE: to use the BC, the array index is the same as the valence numpairs.

// The plus one is b/c I want the index # to be same as numpairs (NOTE: I added a value for array index 4 b/c we have a
//   number of numpairs at that level).
// Current live array index values: 4-48.
static double g_BeastConf[MAX_RATN_FOR_VALGEN + 1] =
                    {0.0, 0.0, 0.0, 0.0, 1.000, 1.000, 0.886, 0.786, 0.738, 0.700, 0.648, 0.618, 0.587,
                    0.560, 0.538, 0.521, 0.503, 0.485, 0.472, 0.460, 0.447, 0.435, 0.425, 0.415, 0.406,
                    0.398, 0.390, 0.382, 0.375, 0.368, 0.362, 0.356, 0.350, 0.345, 0.340, 0.335, 0.330,
                    0.325, 0.321, 0.317, 0.313, 0.309, 0.305, 0.301, 0.298, 0.294, 0.291, 0.288, 0.285};

//
// Create the big-ass chunk of memory to store valences
//
// The Pairs and Valences structures need to have the same number of nodes so the structures
// should be treated similarly. Note that there is no Valence structure. We just write them out to file.
//

pair_t *g_pairs[NTHREADS];

static int g_brds_walker[NTHREADS] = {0};

void buildValsInit(uint32_t thread)
{
    // Zero-out this thread's g_pairs structure based on which thread is active right now.
    memset(g_pairs[thread], 0, sizeof(pair_t) * (BE.num_elts + 1));
} // end buildValsInit()


// We're building up a subset of the pairs which happen to be all the particular x-value elements.
// For example: (3,7), (3,18), (3,22)...
//
// Plan:
// 1. walk the brds (sorted by elt then userid) for the passed-in x
// 2. find all the x, y for each user (in the BR, which is sorted by userid then elt)
// 3. process as was done previously
void build_pairs(uint32_t thread, uint32_t x)
{
    uint32_t el1, el2;
    uint32_t br_limit = BE.num_ratings - 1;
    
    // Catch-up if needed.
    while (brds[g_brds_walker[thread]].eltid < x)
           g_brds_walker[thread]++;

    // 1. Walk the BRDS (sorted by elt then userid) for the passed-in x.
    while (x == brds[g_brds_walker[thread]].eltid)
    {
        unsigned short ra1;
        uint32_t user1, user2;
        user1 = brds[g_brds_walker[thread]].userId;
        ra1 = brds[g_brds_walker[thread]].rating;

        // 2. Find all the x, y for each user (in the BR, which is sorted by userid then elt).
        // Find the x,y for all user1 in the BR.
        uint32_t current2ndRow;
        current2ndRow = br_index[user1];

        // Ok, now we're at the first entry in BR that matches userid from brds.
        // Grab all the ratings for this user, to build up the (x,y) combinations.
        // First we need to bump the counter to make sure elt y id is > elt x id for the (x,y) pair.
        while (br[current2ndRow].eltid <= x && current2ndRow < br_limit)
        {
            current2ndRow++;
        }
        user2 = br[current2ndRow].userId;

        while (user1 == user2)
        {
            // Search for the x,y stuff.
            unsigned short ra2;
            ra2 = br[current2ndRow].rating;

            el1 = (uint32_t) x;
            el2 = (uint32_t) br[current2ndRow].eltid;

            // Make sure we don't process the (x,x).
            if (el1 != el2)
            {
                // Find the correct place in the g_pairs structure. it's el2 :)
                int8_t num_rat = g_pairs[thread][el2].num_rat;

                // We are only storing a maximum of (MAX_RATN_FOR_VALGEN) per valence.
                if (num_rat < MAX_RATN_FOR_VALGEN)
                {
                    g_pairs[thread][el2].rat1[num_rat] = (uint8_t) ra1;
                    g_pairs[thread][el2].rat2[num_rat] = (uint8_t) ra2;

                    // Bump the ratings counter.
                    g_pairs[thread][el2].num_rat++;
                } // end if num_rat < MAX_RATN_FOR_VALGEN
            } // end check we're not doing x,x

            current2ndRow++;
            if (current2ndRow < BE.num_ratings)
            {
                user2 = br[current2ndRow].userId;
            }
            else
            {
                user2 = 0;
            }
        } // end while user1 == user2

        g_brds_walker[thread]++;
    } // end while we're looking at the same x element id
}// end build_pairs()


//
// This routine finds a best-fit line that runs throught the (rat1, rat2) datapoints.
//
// IN
// n is length of array
// x is x array
// y is y array
//
// OUT
// a is y-offset
// b is slope
// fail_code is um, failure code
//
static void linefit(uint8_t n, const uint8_t *x, const uint8_t *y, double *a, double *b, int *fail_code)
{
    // Calculate the averages of arrays x and y.
    double xa = 0, ya = 0;
    int i;
    for (i = 0; i < n; i++)
    {
        xa += x[i];
        ya += y[i];
    }
    xa /= n;
    ya /= n;

    // Calculate auxiliary sums.
    double xx = 0, xy = 0;
    for (i = 0; i < n; i++)
    {
        double tmpx, tmpy;
        tmpx = x[i] - xa;
        tmpy = y[i] - ya;
        xx += tmpx * tmpx;
        xy += tmpx * tmpy;
    }

    // Calculate regression line parameters.
    // Make sure slope is not infinite.
    if (fabs(xx) != 0)
    {
        *b = xy / xx;
        *a = ya - *b * xa;
    }
    else
    {
        *fail_code = INFINITE_SLOPE;
        return;
    }
} // end linefit()


//
// This routine finds the spearman correlation coefficient for a (rat1, rat2) pair.
//
// IN
// array_len is length of array
// input_array is an [array_len,2] array of char implemented as a single-dimension array.
//
// OUT
// returns the spearman value
//
double spearman(int array_len, const uint8_t *input_array)
{
    int col, i;
    uint8_t rating;
    double rank[MAX_RATN_FOR_VALGEN * 2];

    // Walk the input to find num[x] instances of a rating. After walking, we can populate rank.
    for (col = 0; col < 2; col++)
    {
        uint8_t num[MAX_BUCKETS] = {0}; // Initialize all counts to zero
        uint8_t start[MAX_BUCKETS] = {0}; // Initialize all counts to zero
        double avg[MAX_BUCKETS] = {0.0}; // Initialize all counts to zero

        // Walk each column of input data
        for (i = 0; i < array_len; i++)
        {
            rating = input_array[(i) * 2 + (col)];
            if (rating >= 1 && rating <= MAX_BUCKETS)
                num[rating - 1]++;
        }

        // Figure out where each rating would sit in a sorted list and compute averages if we need to.
        for (i = 0; i < MAX_BUCKETS; i++)
        {
            start[i] = (i == 0) ? 0 : start[i - 1] + num[i - 1];
            if (num[i] != 0)
            {
                double sum = start[i] + 1;
                for (int j = 2; j <= num[i]; j++)
                    sum += start[i] + j;
                avg[i] = sum / num[i];
            }
        }

        // Walk input input_array again, assigning RANK values.
        for (i = 0; i < array_len; i++)
        {
            rating = input_array[(i) * 2 + (col)];
            if (rating >= 1 && rating <= MAX_BUCKETS)
                RANK(i, col) = avg[rating - 1];
        }
    } // end column rankings for loop

    double xrho, xnum = 0, xdem1 = 0, xdem2 = 0, xnconst;

    xnconst = (double) array_len * (((double) array_len + 1.0) / 2.0) * (((double) array_len + 1.0) / 2.0);

    // Work on numerator.
    for (i = 0; i < array_len; i++)
    {
        xnum += (RANK(i, 0) * RANK(i, 1));
    }
    xnum -= xnconst;

    // Work on denominator.
    for (i = 0; i < array_len; i++)
    {
        xdem1 += (RANK(i, 0) * RANK(i, 0));
    }
    xdem1 -= xnconst;
    xdem1 = sqrt(xdem1);

    for (i = 0; i < array_len; i++)
    {
        xdem2 += (RANK(i, 1) * RANK(i, 1));
    }
    xdem2 -= xnconst;
    xdem2 = sqrt(xdem2);

    xrho = xnum / (xdem1 * xdem2);

    // This code does not like constant y as it may result in taking a square root of a neg. Or something.
    if (isnan(xrho))
    {
        // if we're in a NaN situation, compute spearman's rho like I did previously.
        double sum = 0;
        for (i = 0; i < array_len; i++)
        {
            double diff;
            diff = RANK(i, 0) - RANK(i, 1);
            sum += (diff * diff);
        }

        // Insert sum into spear calc.
        xrho = 1.0 - ((6.0 * sum) / (double) ((array_len * array_len * array_len) - array_len));
    }

    return xrho;
} // end spearman()


// Create the valences only for the x-value ones.
// NOTE: x must be upper exclusive bound.
void buildValences(uint32_t thread, uint32_t x)
{
    // These hold one rating per byte.
    uint8_t rat1[MAX_RATN_FOR_VALGEN];
    uint8_t rat2[MAX_RATN_FOR_VALGEN];

    uint32_t elts_walker;
    int i;
    double a, b;

    FILE *fp2;

    // Stick the thread number on the end of the filename. E.g., 8 threads means 8 output files. No collisions. :)
    char fname[512];
    sprintf(fname, "%s%s%u", BE.working_dir, "/valences.out", thread);
    
    // MUST be "a" and not "w" because these files get written to multiple times.
    fp2 = fopen(fname,"a");

    if (NULL == fp2)
    {
        syslog(LOG_ERR, "ERROR: cannot open output file %s", fname);
        exit(1);
    }

    // Get the first one.
    uint32_t el1;
    el1 = x;

    // The idea at this point is that we shouldn't be doing _any_ hunting or rb_finding in here!
    // Find the first x,y in g_pairs by using walker b/c it may not be the 0th position.
    // elts_walker is y.
    for (elts_walker = 1; elts_walker < BE.num_elts; elts_walker++)
    {
        // Have we found the next y-value that has at least 1 rating?
        if (g_pairs[thread][elts_walker].num_rat > RATINGS_THRESH) break;
    }

    // Keep going as long as the y-value is less than the max possible.
    while (elts_walker < BE.num_elts)
    {
        uint32_t el2;
        int8_t num_rat;
        el2 = elts_walker;
        num_rat = g_pairs[thread][elts_walker].num_rat;

        // syslog(LOG_INFO, "GH0 el1 %lu, el2 %lu, num_rat %d, thresh %d, index %lu", el1, el2, num_rat, RATINGS_THRESH, index);

        if (num_rat > MAX_RATN_FOR_VALGEN)
        {
            syslog(LOG_ERR, "In a bad place because num_rat (%d) is bigger than the max (%d).", num_rat, MAX_RATN_FOR_VALGEN);
            syslog(LOG_ERR, "Skipping (%d,%d) and continuing on...", el1, el2);

            // Set up for next pass thru.
            elts_walker++;
            for (; elts_walker < BE.num_elts; elts_walker++)
            {
                // Have we found the next y-value that has at least 1 rating?
                if (g_pairs[thread][elts_walker].num_rat > RATINGS_THRESH) break;
            }
            continue;
        } // end if

        for (i = num_rat - 1; i >= 0; i--)
        {
            // Use a single byte for the rating.
            rat1[i] = g_pairs[thread][elts_walker].rat1[i];
            rat2[i] = g_pairs[thread][elts_walker].rat2[i];
        } // end for loop

        int fail_code = 0;

        linefit((uint8_t) num_rat, rat1, rat2, &a, &b, &fail_code);

        if (INFINITE_SLOPE == fail_code)
        {
            // Problem is that one of the rat sets is all the same value.
            // Ok, to deal, bump one of the ratings by one in the axis that's a problem.
            bool rat1ok = false;
            bool rat2ok = false;

            for (i = 0; i < (num_rat - 1); i++)
            {
                if (rat1[i] != rat1[i + 1])
                {
                    // rat1 is not a problem. note it.
                    rat1ok = true;
                }
            }
            for (i = 0; i < (num_rat - 1); i++)
            {
                if (rat2[i] != rat2[i + 1])
                {
                    // rat2 is not a problem. note it.
                    rat2ok = true;
                }
            }
            if (!rat1ok)
            {
                // rat1 is a problem. bump a rat.
                rat1[0] = (uint8_t ) (rat1[0] + 1);
            }
            if (!rat2ok)
            {
                // rat2 is a problem. bump a rat.
                rat2[0] = (uint8_t ) (rat2[0] + 1);
            }

            // Try again.
            fail_code = 0;
            linefit((uint8_t ) num_rat, rat1, rat2, &a, &b, &fail_code);

            if (fail_code != 0)
            {
                syslog(LOG_ERR, "Error from recovery from linefit.");
                syslog(LOG_ERR, "ERROR: linefit: with data for pair %d,%d. num_rat is %d Killing...",
                       (int) el1, (int) el2, num_rat);
                exit(1);
            }
        } // end if we had an infinite slope

        double spear, abs_spear;
        uint8_t bx[num_rat * 2];

        for (i = 0; i < num_rat; i++)
            BX(i, 0) = rat1[i];
        for (i = 0; i < num_rat; i++)
            BX(i, 1) = rat2[i];

        spear = spearman(num_rat, bx);
        abs_spear = fabs(spear);

        // begin CSV-appending
        char conf_value = 'f';

        // Simple test first.
        if (abs_spear > g_BeastConf[(int) num_rat])
        {
            conf_value = 't';
        }
        else
        {
            // Smarten-up the confidence check by using machine-specific EPSILON.
            if (fabs(abs_spear - g_BeastConf[(int) num_rat]) < (double) FLT_EPSILON)
            {
                conf_value = 't';
            }
        }

        char out_buffer[256];

        // Only write to file if we're confident that we want to use the valence.
        if ('t' == conf_value)
        {
            sprintf(out_buffer, "%d,%d,%d,%f,%f,%f\n", (int) el1, (int) el2,
                num_rat, b, a,
                spear);
            fwrite(out_buffer, strlen(out_buffer), 1, fp2);
        }
        // end CSV-appending

        // Set up for next pass thru.
        elts_walker++;
        for (; elts_walker < BE.num_elts; elts_walker++)
        {
            // Have we found the next y-value that has at least 1 rating?
            if (g_pairs[thread][elts_walker].num_rat > RATINGS_THRESH) break;
        }
    } // end while there are more pairs to process, using index

    if (fclose(fp2) != 0)
    {
        syslog(LOG_ERR, "Error closing output file.");
        exit(1);
    }
} // end buildValences()
