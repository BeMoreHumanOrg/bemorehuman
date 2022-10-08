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
// Currenl live array index values: 4-48.
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
        while (br[current2ndRow].eltid <= x)
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
// n is length of array
// x is an [n,2] array of char implemented as a single-dimension array. Use X macro to find index.
//
// OUT
// returns the spearman value
//
double spearman(int n, const uint8_t *x)
{
    int col, i;
    uint8_t val;
    double val1=0.0, val2=0.0, val3=0.0, val4=0.0, val5=0.0, val6=0.0, val7=0.0, val8=0.0, val9=0.0, val10=0.0,
           val11=0.0, val12=0.0, val13=0.0, val14=0.0, val15=0.0, val16=0.0, val17=0.0, val18=0.0, val19=0.0, val20=0.0,
           val21=0.0, val22=0.0, val23=0.0, val24=0.0, val25=0.0, val26=0.0, val27=0.0, val28=0.0, val29=0.0, val30=0.0,
           val31=0.0, val32=0.0;
    double rank[MAX_RATN_FOR_VALGEN * 2];

    // Plan: Walk the input to find numx instances of a rating. After walking, we can populate rank.

    for (col = 0; col < 2; col++)
    {
        uint8_t num1, num2, num3, num4, num5, num6, num7, num8, num9, num10,
            num11, num12, num13, num14, num15, num16, num17, num18, num19, num20,
            num21, num22, num23, num24, num25, num26, num27, num28, num29, num30,
            num31, num32;
        uint8_t start1, start2, start3, start4, start5, start6, start7, start8, start9, start10,
            start11, start12, start13, start14, start15, start16, start17, start18, start19, start20,
            start21, start22, start23, start24, start25, start26, start27, start28, start29, start30,
            start31, start32;

        num1 = num2 = num3 = num4 = num5 = num6 = num7 = num8 = num9 = num10 =
        num11 = num12 = num13 = num14 = num15 = num16 = num17 = num18 = num19 = num20 =
        num21 = num22 = num23 = num24 = num25 = num26 = num27 = num28 = num29 = num30 =
        num31 = num32 = 0;

        // walk each column of input data
        for (i = 0; i < n; i++)
        {
            val = BDCX(i, col);
            if (1 == val) num1++;
            else if (2 == val) num2++;
            else if (3 == val) num3++;
            else if (4 == val) num4++;
            else if (5 == val) num5++;
            else if (6 == val) num6++;
            else if (7 == val) num7++;
            else if (8 == val) num8++;
            else if (9 == val) num9++;
            else if (10 == val) num10++;
            else if (11 == val) num11++;
            else if (12 == val) num12++;
            else if (13 == val) num13++;
            else if (14 == val) num14++;
            else if (15 == val) num15++;
            else if (16 == val) num16++;
            else if (17 == val) num17++;
            else if (18 == val) num18++;
            else if (19 == val) num19++;
            else if (20 == val) num20++;
            else if (21 == val) num21++;
            else if (22 == val) num22++;
            else if (23 == val) num23++;
            else if (24 == val) num24++;
            else if (25 == val) num25++;
            else if (26 == val) num26++;
            else if (27 == val) num27++;
            else if (28 == val) num28++;
            else if (29 == val) num29++;
            else if (30 == val) num30++;
            else if (31 == val) num31++;
            else num32++;
        }

        // Figure out where each value would sit in a sorted list and compute averages if we need to.
        // NOTE: The valn is probably a non-integer.
        start1 = 0;
        if (0 != num1)
        {
            val1 = start1 + 1;
            for (i = 2; i <= num1; i++)
                val1 = val1 + start1 + i;
            val1 = val1 / num1;
        }

        start2 = num1;
        if (0 != num2)
        {
            val2 = start2 + 1;
            for (i = 2; i <= num2; i++)
                val2 = val2 + start2 + i;
            val2 = val2 / num2;
        }

        start3 = start2 + num2;
        if (0 != num3)
        {
            val3 = start3 + 1;
            for (i = 2; i <= num3; i++)
                val3 = val3 + start3 + i;
            val3 = val3 / num3;
        }

        start4 = start3 + num3;
        if (0 != num4)
        {
            val4 = start4 + 1;
            for (i = 2; i <= num4; i++)
                val4 = val4 + start4 + i;
            val4 = val4 / num4;
        }

        start5 = start4 + num4;
        if (0 != num5)
        {
            val5 = start5 + 1;
            for (i = 2; i <= num5; i++)
                val5 = val5 + start5 + i;
            val5 = val5 / num5;
        }

        start6 = start5 + num5;
        if (0 != num6)
        {
            val6 = start6 + 1;
            for (i = 2; i <= num6; i++)
                val6 = val6 + start6 + i;
            val6 = val6 / num6;
        }

        start7 = start6 + num6;
        if (0 != num7)
        {
            val7 = start7 + 1;
            for (i = 2; i <= num7; i++)
                val7 = val7 + start7 + i;
            val7 = val7 / num7;
        }

        start8 = start7 + num7;
        if (0 != num8)
        {
            val8 = start8 + 1;
            for (i = 2; i <= num8; i++)
                val8 = val8 + start8 + i;
            val8 = val8 / num8;
        }

        start9 = start8 + num8;
        if (0 != num9)
        {
            val9 = start9 + 1;
            for (i = 2; i <= num9; i++)
                val9 = val9 + start9 + i;
            val9 = val9 / num9;
        }

        start10 = start9 + num9;
        if (0 != num10)
        {
            val10 = start10 + 1;
            for (i = 2; i <= num10; i++)
                val10 = val10 + start10 + i;
            val10 = val10 / num10;
        }

        start11 = start10 + num10;
        if (0 != num11)
        {
            val11 = start11 + 1;
            for (i = 2; i <= num11; i++)
                val11 = val11 + start11 + i;
            val11 = val11 / num11;
        }

        start12 = start11 + num11;
        if (0 != num12)
        {
            val12 = start12 + 1;
            for (i = 2; i <= num12; i++)
                val12 = val12 + start12 + i;
            val12 = val12 / num12;
        }

        start13 = start12 + num12;
        if (0 != num13)
        {
            val13 = start13 + 1;
            for (i = 2; i <= num13; i++)
                val13 = val13 + start13 + i;
            val13 = val13 / num13;
        }

        start14 = start13 + num13;
        if (0 != num14)
        {
            val14 = start14 + 1;
            for (i = 2; i <= num14; i++)
                val14 = val14 + start14 + i;
            val14 = val14 / num14;
        }

        start15 = start14 + num14;
        if (0 != num15)
        {
            val15 = start15 + 1;
            for (i = 2; i <= num15; i++)
                val15 = val15 + start15 + i;
            val15 = val15 / num15;
        }

        start16 = start15 + num15;
        if (0 != num16)
        {
            val16 = start16 + 1;
            for (i = 2; i <= num16; i++)
                val16 = val16 + start16 + i;
            val16 = val16 / num16;
        }

        start17 = start16 + num16;
        if (0 != num17)
        {
            val17 = start17 + 1;
            for (i = 2; i <= num17; i++)
                val17 = val17 + start17 + i;
            val17 = val17 / num17;
        }

        start18 = start17 + num17;
        if (0 != num18)
        {
            val18 = start18 + 1;
            for (i = 2; i <= num18; i++)
                val18 = val18 + start18 + i;
            val18 = val18 / num18;
        }

        start19 = start18 + num18;
        if (0 != num19)
        {
            val19 = start19 + 1;
            for (i = 2; i <= num19; i++)
                val19 = val19 + start19 + i;
            val19 = val19 / num19;
        }

        start20 = start19 + num19;
        if (0 != num20)
        {
            val20= start20 + 1;
            for (i = 2; i <= num20; i++)
                val20 = val20 + start20 + i;
            val20 = val20 / num20;
        }

        start21 = start20 + num20;
        if (0 != num21)
        {
            val21 = start21 + 1;
            for (i = 2; i <= num21; i++)
                val21 = val21 + start21 + i;
            val21 = val21 / num21;
        }

        start22 = start21 + num21;
        if (0 != num22)
        {
            val22 = start22 + 1;
            for (i = 2; i <= num22; i++)
                val22 = val22 + start22 + i;
            val22 = val22 / num22;
        }

        start23 = start22 + num22;
        if (0 != num23)
        {
            val23 = start23 + 1;
            for (i = 2; i <= num23; i++)
                val23 = val23 + start23 + i;
            val23 = val23 / num23;
        }

        start24 = start23 + num23;
        if (0 != num24)
        {
            val24 = start24 + 1;
            for (i = 2; i <= num24; i++)
                val24 = val24 + start24 + i;
            val24 = val24 / num24;
        }

        start25 = start24 + num24;
        if (0 != num25)
        {
            val25 = start25 + 1;
            for (i = 2; i <= num25; i++)
                val25 = val25 + start25 + i;
            val25 = val25 / num25;
        }

        start26 = start25 + num25;
        if (0 != num26)
        {
            val5 = start26 + 1;
            for (i = 2; i <= num26; i++)
                val26 = val26 + start26 + i;
            val26 = val26 / num26;
        }

        start27 = start26 + num26;
        if (0 != num27)
        {
            val27 = start27 + 1;
            for (i = 2; i <= num27; i++)
                val27 = val27 + start27 + i;
            val27 = val27 / num27;
        }

        start28 = start27 + num27;
        if (0 != num28)
        {
            val28 = start28 + 1;
            for (i = 2; i <= num28; i++)
                val28 = val28 + start28 + i;
            val28 = val28 / num28;
        }

        start29 = start28 + num28;
        if (0 != num29)
        {
            val29 = start29 + 1;
            for (i = 2; i <= num29; i++)
                val29 = val29 + start29 + i;
            val29 = val29 / num29;
        }

        start30 = start29 + num29;
        if (0 != num30)
        {
            val30 = start30 + 1;
            for (i = 2; i <= num30; i++)
                val30 = val30 + start30 + i;
            val30 = val30 / num30;
        }

        start31 = start30 + num30;
        if (0 != num31)
        {
            val31 = start31 + 1;
            for (i = 2; i <= num31; i++)
                val31 = val31 + start31 + i;
            val31 = val31 / num31;
        }

        start32 = start31 + num31;
        if (0 != num32)
        {
            val32 = start32 + 1;
            for (i = 2; i <= num32; i++)
                val32 = val32 + start32 + i;
            val32 = val32 / num32;
        }

        // Walk input x again, assigning RANK values.
        for (i = 0; i < n; i++)
        {
            val = BDCX(i, col);
            if (1 == val) RANK(i, col) = val1;
            else if (2 == val) RANK(i, col) = val2;
            else if (3 == val) RANK(i, col) = val3;
            else if (4 == val) RANK(i, col) = val4;
            else if (5 == val) RANK(i, col) = val5;
            else if (6 == val) RANK(i, col) = val6;
            else if (7 == val) RANK(i, col) = val7;
            else if (8 == val) RANK(i, col) = val8;
            else if (9 == val) RANK(i, col) = val9;
            else if (10 == val) RANK(i, col) = val10;
            else if (11 == val) RANK(i, col) = val11;
            else if (12 == val) RANK(i, col) = val12;
            else if (13 == val) RANK(i, col) = val13;
            else if (14 == val) RANK(i, col) = val14;
            else if (15 == val) RANK(i, col) = val15;
            else if (16 == val) RANK(i, col) = val16;
            else if (17 == val) RANK(i, col) = val17;
            else if (18 == val) RANK(i, col) = val18;
            else if (19 == val) RANK(i, col) = val19;
            else if (20 == val) RANK(i, col) = val20;
            else if (21 == val) RANK(i, col) = val21;
            else if (22 == val) RANK(i, col) = val22;
            else if (23 == val) RANK(i, col) = val23;
            else if (24 == val) RANK(i, col) = val24;
            else if (25 == val) RANK(i, col) = val25;
            else if (26 == val) RANK(i, col) = val26;
            else if (27 == val) RANK(i, col) = val27;
            else if (28 == val) RANK(i, col) = val28;
            else if (29 == val) RANK(i, col) = val29;
            else if (30 == val) RANK(i, col) = val30;
            else if (31 == val) RANK(i, col) = val31;
            else
                RANK(i, col) = val32;
        }
    } // end column rankings for loop

    double xrho, xnum = 0, xdem1 = 0, xdem2 = 0, xnconst;

    xnconst = (double) n * (((double) n + 1.0) / 2.0) * (((double) n + 1.0) / 2.0);

    // Work on numreator.
    for (i = 0; i < n; i++)
    {
        xnum += (RANK(i, 0) * RANK(i, 1));
    }
    xnum -= xnconst;

    // Work on denominator.
    for (i = 0; i < n; i++)
    {
        xdem1 += (RANK(i, 0) * RANK(i, 0));
    }
    xdem1 -= xnconst;
    xdem1 = sqrt(xdem1);

    for (i = 0; i < n; i++)
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
        for (i = 0; i < n; i++)
        {
            double diff;
            diff = RANK(i, 0) - RANK(i, 1);
            sum += (diff * diff);
        }

        // Insert sum into spear calc.
        xrho = 1.0 - ((6.0 * sum) / (double) ((n * n * n) - n));
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
        sprintf(out_buffer, "\"%d\",\"%d\",\"%d\",\"%f\",\"%f\",\"%f\",\"%c\"\n", (int) el1, (int) el2,
            num_rat, b, a,
            spear, conf_value);
        fwrite(out_buffer, strlen(out_buffer), 1, fp2);
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
