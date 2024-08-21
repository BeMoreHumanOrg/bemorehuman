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
#ifdef linux
#include <malloc.h>
#endif
#include "recgen.h"

//
// This file contains stuff for Beast (and friends). Beast is a big chunk of memory which holds the primitives
// that make up Valences.
//

//
// Globals
//

size_t g_num_confident_valences; // The number of valences we're confident about.

// These guys are static because we want them in the bss segment. Pass around pointers as needed.
static popularity_t *g_pop = NULL;        // this is the popularity of the things to be recommended -- used for obscurity filtering
static valence_t *g_bb = NULL;            // this is the combined Beast & Beast index (bind), or bb
static bb_ind_t *g_bind_seg = NULL;       // fixed x locations which are the offsets of the X's in the bb
static valence_t *g_bb_ds = NULL;         // this is the combined Beast & bind, or bb for the DS (Differently Sorted)
static bb_ind_t *g_bind_seg_ds = NULL;    // fixed y locations which are the offsets of the Y's in the bb_ds
static valence_xy_t *g_bb_ds_temp = NULL; // this is the combined Beast & bind, or bb for the DS (Differently Sorted), temp version for DS creation
static rating_t *g_big_rat = NULL;        // this is the valgen-outputted user ratings
static uint32_t *g_big_rat_index = NULL;  // this is a person index into g_big_rat

// forward declaration
static int pull_from_files(bool);

static fewbit_t g_slopes[256];
static fewbit_t g_offsets[256];
static int8_t g_tiny_slopes[NUM_SO_BUCKETS];
static double g_tiny_slopes_inv[NUM_SO_BUCKETS];
static int8_t g_tiny_offsets[NUM_SO_BUCKETS];

static uint64_t g_valence_count = 0;

// Begin DS sorting and population implementation.

//
// Compare any two valences for use by quicksort_iterative().
// This is to sort the copied beast stuff to create the DS (Differently Sorted) analogue. Sort by elt2, elt1.
//
static int valence_cmp(const void* p1, const void* p2)
{
    valence_xy_t a, b;

    ASSIGN(a, (*(const valence_xy_t *) p1));
    ASSIGN(b, (*(const valence_xy_t *) p2));
    
    exp_elt_t exp_a_elt1, exp_a_elt2;
    exp_elt_t exp_b_elt1, exp_b_elt2;
    EXPAND((a.x), exp_a_elt1);
    EXPAND((a.eltid), exp_a_elt2);
    EXPAND((b.x), exp_b_elt1);
    EXPAND((b.eltid), exp_b_elt2);

    // Is a < b?
    if ((exp_a_elt2 < exp_b_elt2) || ((exp_a_elt2 == exp_b_elt2) && (exp_a_elt1 < exp_b_elt1)))
    {
        return (-1);
    }
    else
    {
        // Is a > b?
        if ((exp_a_elt2 > exp_b_elt2) || ((exp_a_elt2 == exp_b_elt2) && (exp_a_elt1 > exp_b_elt1)))
        {
            return (1);
        }
        else
        {
            // They're the same (we shouldn't get here b/c that would mean, e.g.  a valence like (5,5).
            return (0);
        }
    }
} // end valence_cmp()

// Sort the DS_temp.
static void quicksort_iterative(valence_xy_t array[], uint64_t len)
{
    uint64_t left = 0, stack[MAX_STACK], pos = 0;
    uint64_t seed = (unsigned) rand(); // NOLINT because true randomness not necessary
    valence_xy_t pivot;

    for ( ; ; )
    {
        for (; left+1 < len; len++)
        {                // sort left to len-1
            if (pos == MAX_STACK) len = stack[pos = 0];  // stack overflow, reset
            ASSIGN(pivot, array[left+seed%(len-left)]);  // pick random pivot
            seed = seed*69069+1;                         // next pseudorandom number
            stack[pos++] = len;                          // sort right part later
            for (uint64_t right = left-1; ; )
            {
                valence_xy_t temp;
                // inner loop: partitioning
                while (valence_cmp(&(array[++right]), &pivot) < 0) {}  // look for greater element
                while (valence_cmp(&pivot, &(array[--len])) < 0) {}    // look for smaller element
                if (right >= len) break;                 // partition point found?

                ASSIGN(temp, array[right]);
                ASSIGN(array[right], array[len]);        // the only swap
                ASSIGN(array[len], temp);
            } // end for loop partitioned, continue left part
        } // end for
        if (pos == 0) break;                             // stack empty?
        left = len;                                      // left to right is sorted
        len = stack[--pos];                              // get next range to sort
    }  // end for
}  // end quicksort_iterative()


// Populate the DS by reading sth from the filesystem like the db export, similar to pullfromfiles().
// Make sure to set the bb_seg_ds as the bb_ds gets filled up.
// This function is only for DS creation, not DS loading from filesystem binary cache!
static void populate_ds()
{
    // We are creating the DS at this point so pass in true.
    pull_from_files(true);

    // Walk the g_bind_seg_ds and initialize the whole thing.
    for (uint64_t i = 0; i <= BE.num_elts; i++)
    {
        g_bind_seg_ds[i].count = 0;
        g_bind_seg_ds[i].offset = UINT64_MAX;
    }

    // Sort the DS_temp.
    quicksort_iterative(g_bb_ds_temp,  g_num_confident_valences);

    syslog(LOG_INFO, "Finished with quicksort_iterative()");

    // Create the ds which needs to be copied from the ds_temp.
    g_bb_ds = (valence_t *) calloc(g_num_confident_valences, sizeof(valence_t));
    if (g_bb_ds == 0)
    {
        syslog(LOG_ERR, "FATAL ERROR: Out of memory when creating bb_ds prior to exporting it.");
        exit (-1);
    }

    // Create the bind_seg_ds.
    g_bind_seg_ds = (bb_ind_t *) calloc((unsigned long) BE.num_elts, sizeof(bb_ind_t));
    if (g_bind_seg_ds == 0)
    {
        syslog(LOG_ERR, "FATAL ERROR: Out of memory when creating bind_seg_ds prior to exporting it.");
        exit (-1);
    }

    // Walk the bb_ds_temp to create the g_bb_ds and set the g_bind_seg_ds properly.
    exp_elt_t exp_id_2, prev_exp_id_2 = 0;
    for (uint64_t i = 0; i < g_num_confident_valences; i++)
    {
        // Create the g_bb_ds.
        g_bb_ds[i].soindex = g_bb_ds_temp[i].soindex;
        g_bb_ds[i].eltid[0] = g_bb_ds_temp[i].x[0];
        g_bb_ds[i].eltid[1] = g_bb_ds_temp[i].x[1];
        g_bb_ds[i].eltid[2] = g_bb_ds_temp[i].x[2];

        // Now create the g_bind_seg_ds.
        EXPAND(g_bb_ds_temp[i].eltid, exp_id_2);

        // Create an index of y-value starting positions in BeastDS.
        if (exp_id_2 != prev_exp_id_2)
        {
            // Need to do the below bit (num_elts - 1) times.
            g_bind_seg_ds[exp_id_2].offset = i;
            prev_exp_id_2 = exp_id_2;
        }

        // add to count
        g_bind_seg_ds[exp_id_2].count++;

    } // end for loop over valences
    
    free (g_bb_ds_temp);
    
} // end DSPopulation()
// end DS sorting and population implementation


// begin slope & offset counting helpers

//
// A function to compare any two guys, for use as a callback function by qsort()
//
// Returns -1 if the first rating (recommendation value) is more than the second
//          0 if the ratings are equal
//          1 if the first rating is less than the second rating
// NOTE: this behavior is flipped (normally sort is ascending) so we get highest
// rating values at beginning of sorted structure
//
static int guycmp(const void *p1, const void *p2)
{
    const guy_t x = *(const guy_t *) p1;
    const guy_t y = *(const guy_t *) p2;

    if (x.count > y.count)
    {
        return (-1);
    }
    else
    {
        if (x.count < y.count)
        {
            return (1);
        }
        else
        {
            return (0);
        }
    }
}

static int guycmp2(const void *p1, const void *p2)
{
    const guy_t x = *(const guy_t *) p1;
    const guy_t y = *(const guy_t *) p2;

    if (x.guy > y.guy)
    {
        return (-1);
    }
    else
    {
        if (x.guy < y.guy)
        {
            return (1);
        }
        else
        {
            return (0);
        }
    }
} // end slope & offset counting helpers


// Load the Beast export binary files to memory.
static void pull_from_beast_export()
{
    // Begin fread-ing the 4 memory structures.

    // Read the bb
    char file_to_open[1024];
    strlcpy(file_to_open, BE.valence_cache_dir, sizeof(file_to_open));
    strlcat(file_to_open, "/", sizeof(file_to_open));
    strlcat(file_to_open, VALENCES_BB, sizeof(file_to_open));
    FILE *val_out = fopen(file_to_open,"r");
    assert(NULL != val_out);

    size_t num_valences_read = fread(g_bb, sizeof(valence_t), g_num_confident_valences, val_out);
    syslog(LOG_INFO, "Number of valences read from bin file: %lu and we expected %lu to be read.",
           num_valences_read, (unsigned long) g_num_confident_valences);
    assert(num_valences_read == g_num_confident_valences);
    g_valence_count = num_valences_read;
    fclose(val_out);
    
    // Read the Beast segment starts
    strlcpy(file_to_open, BE.valence_cache_dir, sizeof(file_to_open));
    strlcat(file_to_open, "/", sizeof(file_to_open));
    strlcat(file_to_open, VALENCES_BB_SEG, sizeof(file_to_open));
    val_out = fopen(file_to_open,"r");
    assert(NULL != val_out);

    num_valences_read = fread(g_bind_seg, sizeof(bb_ind_t), BE.num_elts, val_out);
    syslog(LOG_INFO, "Number of valence segment starts read from bin file: %lu and we expected %" PRIu64 " to be read.",
           num_valences_read, BE.num_elts);
    fclose(val_out);
    assert(num_valences_read == BE.num_elts);

    // Read the bb_ds
    strlcpy(file_to_open, BE.valence_cache_dir, sizeof(file_to_open));
    strlcat(file_to_open, "/", sizeof(file_to_open));
    strlcat(file_to_open, VALENCES_BB_DS, sizeof(file_to_open));
    val_out = fopen(file_to_open,"r");
    assert(NULL != val_out);

    num_valences_read = fread(g_bb_ds, sizeof(valence_t), g_num_confident_valences, val_out);
    syslog(LOG_INFO, "Number of valences read from DS bin file: %lu and we expected %lu to be read.",
           num_valences_read, (unsigned long) g_num_confident_valences);
    fclose(val_out);
    assert(num_valences_read == g_num_confident_valences);
    
    // Read the Beast DS segment starts
    strlcpy(file_to_open, BE.valence_cache_dir, sizeof(file_to_open));
    strlcat(file_to_open, "/", sizeof(file_to_open));
    strlcat(file_to_open, VALENCES_BB_SEG_DS, sizeof(file_to_open));
    val_out = fopen(file_to_open,"r");
    assert(NULL != val_out);

    num_valences_read = fread(g_bind_seg_ds, sizeof(bb_ind_t), BE.num_elts, val_out);
    syslog(LOG_INFO, "Number of valence segment starts read from DS bin file: %lu and we expected %" PRIu64 " to be read.",
           num_valences_read, BE.num_elts);
    fclose(val_out);
    assert(num_valences_read == BE.num_elts);

    // debugging
    /*
    for (int i=0; i < 50; i++)
    {
        // print out some bind_seg values
        syslog(LOG_INFO, "%d: bind_seg %ld, bind_seg_ds %ld",
               i,
               g_bind_seg[i],
               g_bind_seg_ds[i]);
    }
    
    for (int i=0; i < 50; i++)
    {
        Exp_elt_t ee1 = 0, ee2 = 0;
        uint8_t so1, so2;
        
        // print out some bb values
        EXPAND(g_bb[i].eltid, ee1);
        EXPAND(g_bb_ds[i].eltid, ee2);
        so1 = g_bb[i].soindex;
        so2 = g_bb_ds[i].soindex;
        
        syslog(LOG_INFO, "%d: bb elt %d, bb_ds elt %d, bb soindex %d, bb_ds soindex %d",
               i,
               ee1,
               ee2,
               so1,
               so2
               );
    }
    */
} // end pullFromBeastExport()


// Load the valence file (human-readable data that can go into a db easily) to memory and returns 0 on success or 1 on error.
// Parameter createDS: are we creating the DS structure? We need to know b/c we populate a different structure if so.
static int pull_from_files(bool createDS)
{
    FILE *fp;
    size_t i, j;
    char filename[strlen(BE.working_dir) + 16];
    g_valence_count = 0;

    // Walk the bind_seg and initialize it.
    for (i = 0; i <= BE.num_elts; i++)
    {
        g_bind_seg[i].count = 0;
        g_bind_seg[i].offset = UINT64_MAX;
    }
    syslog(LOG_INFO, "BE.num_elts is %" PRIu64, BE.num_elts);

    syslog(LOG_INFO, "BE.valence_files_dir is ---%s---", BE.working_dir);
    strlcpy(filename, BE.working_dir, sizeof(filename));
    strlcat(filename, "/valences.out", sizeof(filename));
    syslog(LOG_INFO, "filename is ---%s---", filename);

    fp = fopen(filename, "r");
    if (!fp)
    {
        printf("Can't open file %s. Exiting.\n", filename);
        exit(-1);
    }

    char *line = NULL;
    size_t line_capacity = 0;
    ssize_t line_length;
    const char delimiter[4] = ",";
    char *token;
    exp_elt_t id_1 = 0, id_2 = 0, prev_id_1 = 0;
    double slope = 0, offset = 0 ;
    signed char tiny_slope, tiny_offset;
    bool found = false;

    // In this function, add the slope/offset compression cache creation.
    // We have to know the cache values before we interpret the valences file.

    // Stage 1 of 2: Make a first pass through the valences file first to build compression arrays.
    // Create the slope/offset tracing arrays

    ///////////////////////////////////////////////////
    // begin creating slopes & offsets compression cache

    // How many unique slopes & offsets are there?

    // 256 is max size b/c we compress slopes/offsets down to signed 8-bit. These arrays
    // are of unique slopes/offsets.
    uint32_t slope_count = 0;
    uint32_t offset_count = 0;

    guy_t slopes[256];
    guy_t offsets[256];
    guy_t slopes_freq[256];
    guy_t offsets_freq[256];

    size_t ncv = 0;   // This is for the sanity check against num_confident_valences.

    for (i = 0; i < 256; i++)
    {
        slopes[i].count = 1;
        offsets[i].count = 1;
    }

    // Walk the valences file
    while ((line_length = getline(&line, &line_capacity, fp)) != -1)
    {
        // Is the file coming from linux/unix or pre Mac OSX?
        if (line_length && (line[line_length - 1] == '\n' || line[line_length - 1] == '\r'))
            line[--line_length] = '\0';
        // Is the file coming from windows?
        if (line_length && (line[line_length - 1] == '\r'))
            line[--line_length] = '\0';

        int token_number = 0;

        // Get the first token.
        token = strtok(line, delimiter);

        // Walk through other tokens.
        while( token != NULL )
        {
            switch (token_number)
            {
                case 0:   // elt1
                    id_1 = (exp_elt_t ) strtol(token, NULL, 10);
                    break;
                case 1:   // elt2
                    id_2 = (exp_elt_t ) strtol(token, NULL, 10);
                    break;
                case 2:   // numpairs
                    break;
                case 3:   // slope
                    slope = strtof(token, NULL);
                    break;
                case 4:   // offset
                    offset = strtof(token, NULL);
                    break;
                case 5:   // coeff, which we are ignoring at this point
                    break;
                default:
                    syslog(LOG_ERR, "ERROR: hit the default case situation when parsing tokens in a line. Why?");
            }
            token = strtok(NULL, delimiter);
            token_number++;
        } // end while() parsing tokens

        ncv++;

        // valence is now 4 bytes and only 4 bits each for slope and offset
        slope = slope * FLOAT_TO_SHORT_MULT;
        tiny_slope = (signed char) bmh_round(slope);
        offset = offset * FLOAT_TO_SHORT_MULT;
        tiny_offset = (signed char) bmh_round(offset);

        // Now figure out if slope/offset are unique or we've seen them before
        // Do the slopes.
        j = 0;
        found = false;

        while (!found && j < slope_count)
        {
            // Walk the slope_arr to see if this slope is in there.
            if (tiny_slope == slopes[j].guy)
            {
                found = true;
                slopes[j].count++;
            }
            j++;
        }
        if (!found)
        {
            // Couldn't find the slope, so insert it.
            slopes[slope_count].guy = tiny_slope;
            slope_count++;
        }

        // Do the offsets.
        j = 0;
        found = false;

        while (!found && j < offset_count)
        {
            // Walk the offset_arr to see if this offset is in there.
            if (tiny_offset == offsets[j].guy)
            {
                found = true;
                offsets[j].count++;
            }
            j++;
        }
        if (!found)
        {
            // couldn't find the offset, so insert it
            offsets[offset_count].guy = tiny_offset;
            offset_count++;
        }
    } // end walking the valences file to create slope/offset tracing arrays

    assert(ncv == g_num_confident_valences);

    // Set the slopes_freq and offsets_freq
    for (i=0; i < slope_count; i++)
    {
        slopes_freq[i].count = slopes[i].count;
        slopes_freq[i].guy = slopes[i].guy;
    }
    for (i=0; i < offset_count; i++)
    {
        offsets_freq[i].count = offsets[i].count;
        offsets_freq[i].guy = offsets[i].guy;
    }

    // Sort based on frequency descending so we know which slopes & offsets are most popular (will inform how we set up lookup)
    qsort(slopes_freq, slope_count, sizeof(guy_t), guycmp);
    qsort(offsets_freq, offset_count, sizeof(guy_t), guycmp);

    // Sort based on slope/offset value so we can clump neighbors together (will inform how we set up lookup).
    qsort(slopes, slope_count, sizeof(guy_t), guycmp2);
    qsort(offsets, offset_count, sizeof(guy_t), guycmp2);

    // Begin Analysis
    double next_boundary;
    double next_boundary_inc;
    double cume = 0;
    int8_t bucket_value;
    double bucket_value_double = 0;
    int bucket_elt_total = 0;
    double bucket_percent_total = 0;
    double percents[256];
    bool special_slope = false, special_offset = false;
    double tot_buck_pct = 0;

    typedef struct bucket
    {
        int8_t value, start, stop;
    } bucket_t;
    bucket_t buckets_slope[NUM_SO_BUCKETS], buckets_offset[NUM_SO_BUCKETS];
    int8_t curr_bucket = 0;

    syslog(LOG_INFO, "After processing slopes & offsets, slope count is %d, offset_count is %d", slope_count, offset_count);

    // Begin slope-specific.
    syslog(LOG_INFO, "SLOPES");

    next_boundary_inc = 5;  // a couple of sane defaults if we can't do better below
    next_boundary = next_boundary_inc;

    // Find the slope 1 location (it's likely to exist) and remove it.
    for (i = 0; i < slope_count; i++)
    {
        // Slope 1 gets its own bucket, so skip the calculation for him b/c it skews everything.
        if (slopes[i].guy == 10)
        {
            // Calculate the next_boundary.
            tot_buck_pct = 100.0 - (100.0 * ((double) slopes[i].count / (double) g_num_confident_valences));
            next_boundary_inc =  tot_buck_pct / NUM_SO_BUCKETS;
            next_boundary = next_boundary_inc;
            syslog(LOG_INFO, "next_boundary is %f, percent for slope 1 is %f, tot_buck_pct is %f, slopes[i].count: %d, g_num_conf_val: %zu",
                   next_boundary, 100.0 - tot_buck_pct, tot_buck_pct, slopes[i].count, g_num_confident_valences);

            // Remove this array elt completely (actually, just slide the ones below up one and proceed because current
            // elt is good)
            //   - and set final elt to 0's
            for (j = i; j < (slope_count - 1); j++)
            {
                slopes[j].count = slopes[j+1].count;
                slopes[j].guy = slopes[j+1].guy;
            }
            slopes[slope_count - 1].count = 0;
            slopes[slope_count - 1].guy = 0;
            special_slope = true;

            break;
        } // end if slope == 1
    } // end for loop looking for 0 guy

    // We know the boundaries are cume constant intervals and the 1,0 bucket.
    for (i = 0; i < slope_count; i++)
    {
        double percent;
        percent = (double) 100 * slopes[i].count / (double) g_num_confident_valences;

        // Will the current percent put us in a new bucket?
        if (cume + percent > next_boundary || i == (slope_count - 1))
        {
            // compute/print bucket value now that we're at the end
            for (j = 0; j < (size_t) bucket_elt_total; j++)
            {
                bucket_value_double += percents[j] * slopes[i - bucket_elt_total + j].guy;
            }
            bucket_value_double /= bucket_percent_total;
            bucket_value = (int8_t) bmh_round(bucket_value_double);
            syslog(LOG_INFO, "### Bucket value: %d, bvdouble: %f", bucket_value, bucket_value_double);

            // Populate a structure so we can create the cache file later.
            // Need to save bucket_value, range for this bucket.
            // Range is (slopes[i - bucket_elt_total].guy..slopes[i - 1].guy).
            buckets_slope[curr_bucket].value = bucket_value;
            buckets_slope[curr_bucket].start  = slopes[i - 1].guy;
            buckets_slope[curr_bucket].stop = slopes[i - bucket_elt_total].guy;

            syslog(LOG_INFO, "### Just about to cross a bucket boundary.");

            // Add a guard here such that will ensure max of (NUM_SO_BUCKETS - 1) buckets during this calculation.
            if (next_boundary < (tot_buck_pct - next_boundary_inc))
                next_boundary += next_boundary_inc;
            else
                next_boundary = 200;   // unrealistically high so we don't reach it
            bucket_elt_total = 0;
            bucket_percent_total = 0;
            bucket_value_double = 0;
            curr_bucket++;

            // Need to account for rounding error for the final bucket
            if (curr_bucket == (NUM_SO_BUCKETS - 2))
                next_boundary = 200;
        } // end if current percent will put is into a new bucket

        // Save the individual percentages then at end take (individ percent/total_percent) to get the % contribution of this elt.
        percents[bucket_elt_total] = percent;
        bucket_elt_total++;
        bucket_percent_total += percent;

        cume += percent;

        syslog(LOG_INFO, "slope[%3zu] is %4d \tcount is %-14u percent is %-20f cume is %-20f", i, slopes[i].guy, slopes[i].count,
               percent,
               cume);

    } // end slope-specific stuff

    // Write to file.
    //
    // sample:
    // slopes  10      39      28      17      10      8       7       6       5       4       3       2       1       0       -4      -14
    // offsets 0       47      26      17      13      11      10      9       8       7       6       5       3       -3      -10     -24

    // Goal: during valence load from file, need to go from slope --> 4-bit index quickly.
    // Goal: during recgen, need to go from 4-bit index --> slope as fast as freaking possible.

    // Now dump the cache to the filesystem
    FILE *fp2;

    char fname[255];
    strlcpy(fname, BE.valence_cache_dir, sizeof(fname));
    strlcat(fname, "/", sizeof(fname));
    strlcat(fname, SO_COMP_OUTFILE, sizeof(fname));

    // Check if dir exists and if it doesn't, create it.
    if (!check_make_dir(BE.valence_cache_dir))
    {
        syslog(LOG_CRIT, "Can't create directory %s so exiting. More details might be in stderr.", BE.valence_cache_dir);
        exit(-1);
    }

    fp2 = fopen(fname,"w");

    if (NULL == fp2)
    {
        syslog(LOG_ERR, "ERROR: cannot open output file %s", fname);
        exit(1);
    }
    char out_buffer[256];

    // Write the slopes.
    strlcpy(out_buffer, "slopes\t", sizeof(out_buffer));
    if (special_slope)
        strlcat(out_buffer, "10\t", sizeof(out_buffer));

    char bstr[6];
    for (i=0; (int8_t) i < curr_bucket; i++)
    {
        itoa(buckets_slope[i].value, bstr);
        strlcat(out_buffer, bstr, sizeof(out_buffer));
        if ((int8_t) i < curr_bucket - 1)
            strlcat(out_buffer, "\t", sizeof(out_buffer));
    } // end for loop across buckets

    strlcat(out_buffer, "\n", sizeof(out_buffer));
    fwrite(out_buffer, strlen(out_buffer), 1, fp2);

    // Begin offset-specific.
    curr_bucket = 0;
    bucket_elt_total = 0;
    syslog(LOG_INFO, "OFFSETS");
    cume = 0;
    next_boundary = next_boundary_inc; // a sane default if we can't calculate a better value below

    // Find the offset 0 location (it's likely to exist) and remove it.
    for (i = 0; i < offset_count; i++)
    {
        // Offset 0 gets its own bucket, so skip the calculation for him b/c it skews everything.
        if (offsets[i].guy == 0)
        {
            // Calculate the next_boundary.
            tot_buck_pct = 100.0 - (100.0 * ((double) offsets[i].count / (double) g_num_confident_valences));
            next_boundary_inc =  tot_buck_pct / NUM_SO_BUCKETS;
            next_boundary = next_boundary_inc;
            syslog(LOG_INFO, "next_boundary is %f, percent for offset 0 is %f, tot_buck_pct is %f, offsets[i].count: %d, g_num_conf_val: %zu",
                   next_boundary, 100.0 - tot_buck_pct, tot_buck_pct, offsets[i].count, g_num_confident_valences);

            // Remove this array elt completely (actually, just slide the ones below up one and proceed because current
            // elt is good.)
            //   - and set final elt to 0's
            for (j = i; j < (offset_count - 1); j++)
            {
                offsets[j].count = offsets[j+1].count;
                offsets[j].guy = offsets[j+1].guy;
            }
            offsets[offset_count - 1].count = 0;
            offsets[offset_count - 1].guy = 0;
            special_offset = true;

            break;
        } // end if offset == 0
    } // end for loop looking for 0 guy

    // We know the boundaries are cume constant intervals and the 1,0 bucket.
    for (i = 0; i < offset_count; i++)
    {
        double percent;
        percent = (double) 100 * offsets[i].count / (double) g_num_confident_valences;

        // Will the current percent put us in a new bucket?
        if (cume + percent > next_boundary || i == (offset_count - 1))
        {
            // compute/print bucket value now that we're at the end
            for (j = 0; (int) j < bucket_elt_total; j++)
                bucket_value_double += percents[j] * offsets[i - bucket_elt_total + j].guy;
            bucket_value_double /= bucket_percent_total;
            bucket_value = (int8_t) bmh_round(bucket_value_double);
            syslog(LOG_INFO, "### Bucket value: %d, bvdouble: %f", bucket_value, bucket_value_double);

            // Populate a structure so we can create the cache file later.
            // Need to save bucket_value, range for this bucket.
            // Range is (offsets[i - bucket_elt_total].guy..offsets[i - 1].guy).
            buckets_offset[curr_bucket].value = bucket_value;
            buckets_offset[curr_bucket].start = offsets[i - 1].guy;
            buckets_offset[curr_bucket].stop = offsets[i - bucket_elt_total].guy;

            // Add a guard here such that will ensure max of (NUM_SO_BUCKETS - 1) buckets during this calculation.
            if (next_boundary < (tot_buck_pct - next_boundary_inc))
                next_boundary += next_boundary_inc;
            else
                next_boundary = 200;   // unrealistically high so we don't reach it
            bucket_elt_total = 0;
            bucket_percent_total = 0;
            bucket_value_double = 0;
            curr_bucket++;

            // Need to account for rounding error for the final bucket
            if (curr_bucket == (NUM_SO_BUCKETS - 2))
                next_boundary = 200;
        } // end if current percent will put is into a new bucket

        // Save the individual percentages then at end take (individ percent/total_percent) to get the % contribution of this elt.
        percents[bucket_elt_total] = percent;
        bucket_elt_total++;
        bucket_percent_total += percent;

        cume += percent;

        syslog(LOG_INFO, "offsets[%3zu] is %4d \tcount is %-14u percent is %-20f cume is %-20f", i, offsets[i].guy, offsets[i].count,
               percent,
               cume);
    } // end find offset bucket boundaries


    // Write the offsets to output file.
    strlcpy(out_buffer, "offsets\t", sizeof(out_buffer));
    if (special_offset)
        strlcat(out_buffer, "0\t", sizeof(out_buffer));

    for (i=0; (int8_t) i < curr_bucket; i++)
    {
        itoa(buckets_offset[i].value, bstr);
        strlcat(out_buffer, bstr, sizeof(out_buffer));
        if ((int8_t) i < curr_bucket - 1)
            strlcat(out_buffer, "\t", sizeof(out_buffer));
    } // end for loop across buckets

    strlcat(out_buffer, "\n", sizeof(out_buffer));
    fwrite(out_buffer, strlen(out_buffer), 1, fp2);

    if (fclose(fp2) != 0)
    {
        syslog(LOG_ERR, "Error closing slopes/offsets output file %s.", fname);
        exit(1);
    }

    // end creating slopes & offsets compression cache
    ///////////////////////////////////////////////////

    // Populate memory structures

    // Assign the g_tiny_slopes
    int8_t f_start = special_slope == true ? 1 : 0;

    if (f_start) g_tiny_slopes[0] = 10;   // right at the front of the queue b/c he's a large percentage of the s/o
    for (i = 0; (int) i < (NUM_SO_BUCKETS - f_start); i++)
    {
        g_tiny_slopes[i + f_start] = buckets_slope[i].value;
        g_tiny_slopes_inv[i + f_start] = (double) 1.0 / g_tiny_slopes[i + f_start];
    }

    // Use popularity order to determine order of slopes/offsets in memory
    for (i = 0; i < slope_count; i++ )
    {
        g_slopes[i].value = slopes_freq[i].guy;

        // Find where the fewbits guy is.
        if (special_slope && i == 0)
        {
            g_slopes[i].fewbit = 0;       // fewbit is the link to g_tiny_slopes
            continue;
        } // end if special slope is true

        // Assign the fewbit value.
        for (j = 0; (int) j < (NUM_SO_BUCKETS - f_start); j++)
        {
            if (g_slopes[i].value >= buckets_slope[j].start && g_slopes[i].value <= buckets_slope[j].stop)
                g_slopes[i].fewbit = j + f_start;
        }
    } // end for loop across number of unique slopes

    // Assign the g_tiny_offsets
    f_start = special_offset == true ? 1 : 0;

    if (f_start) g_tiny_offsets[0] = 0;   // right at the front of the queue b/c he's a large percentage of the s/o
    for (i = 0; (int) i < (NUM_SO_BUCKETS - f_start); i++)
        g_tiny_offsets[i + f_start] = buckets_offset[i].value;

    // Use popularity order to determine order of slopes/offsets in memory
    for (i = 0; i < offset_count; i++ )
    {
        g_offsets[i].value = offsets_freq[i].guy;

        // Find where the fewbits guy is.
        if (special_slope && i == 0)
        {
            g_offsets[i].fewbit = 0;       // fewbit is the link to g_tiny_offsets
            continue;
        } // end if special slope is true

        // Assign the fewbit value.
        for (j = 0; (int) j < (NUM_SO_BUCKETS - f_start); j++)
        {
            if (g_offsets[i].value >= buckets_offset[j].start && g_offsets[i].value <= buckets_offset[j].stop)
                g_offsets[i].fewbit = j + f_start;
        }
    } // end for loop across number of unique offsets

    // end populating memory structures

    // Mem structures are in place now.

    // Stage 2 of 2: Make another pass through the valences file to populate the beast.
    if (fseek(fp, 0L, SEEK_SET) != 0)
    {
        // Handle repositioning error.
        syslog(LOG_ERR, "There was a problem setting the valences file pointer to the beginning of the file. Exiting.");
        exit(-1);
    }

    while ((line_length = getline(&line, &line_capacity, fp)) != -1)
    {
        // Is the file coming from linux/unix or pre Mac OSX?
        if (line_length && (line[line_length - 1] == '\n' || line[line_length - 1] == '\r'))
            line[--line_length] = '\0';
        // Is the file coming from windows?
        if (line_length && (line[line_length - 1] == '\r'))
            line[--line_length] = '\0';

        int token_number = 0;

        // Get the first token.
        token = strtok(line, delimiter);

        // Walk through other tokens.
        while( token != NULL )
        {
            switch (token_number)
            {
                case 0:   // elt1
                    id_1 = (exp_elt_t ) strtol(token, NULL, 10);
                    break;
                case 1:   // elt2
                    id_2 = (exp_elt_t ) strtol(token, NULL, 10);
                    break;
                case 2:   // numpairs
                    break;
                case 3:   // slope
                    slope = strtof(token, NULL);
                    break;
                case 4:   // offset
                    offset = strtof(token, NULL);
                    break;
                case 5:   // coeff, which we are ignoring at this point
                    break;
                default:
                    syslog(LOG_ERR, "ERROR: hit the default case situation when parsing tokens in a line. Why?");
            }
            token = strtok(NULL, delimiter);
            token_number++;
        } // end while() parsing tokens

        // Set the y element value in the bb.
        if (createDS)
            SETELT(g_bb_ds_temp[g_valence_count].eltid, id_2);
        else
            SETELT(g_bb[g_valence_count].eltid, id_2);

        // Valence is now 4 bytes. Of that, only 4 bits for index for each of slope and offset
        slope = slope * FLOAT_TO_SHORT_MULT;
        tiny_slope = (signed char) bmh_round(slope);
        offset = offset * FLOAT_TO_SHORT_MULT;
        tiny_offset = (signed char) bmh_round(offset);

        // Search the g_slopes to find the 4-bit index.
        for (j = 0; j < slope_count; j++)
        {
            if (tiny_slope == g_slopes[j].value)
            {
                if (createDS)
                    SETHIBITS(g_bb_ds_temp[g_valence_count].soindex, g_slopes[j].fewbit);
                else
                    SETHIBITS(g_bb[g_valence_count].soindex, g_slopes[j].fewbit);
                found = true;
                break;
            }
        }

        // If we can't find it, print error.
        if (! found)
        {
            syslog(LOG_ERR, "Couldn't find tiny_offset: %d", tiny_slope);

            // Set to something popular.
            if (createDS)
                SETHIBITS(g_bb_ds_temp[g_valence_count].soindex, g_slopes[0].fewbit);
            else
                SETHIBITS(g_bb[g_valence_count].soindex, g_slopes[0].fewbit);
        }

        // Search the g_offsets to find the 4-bit index.
        found = false;
        for (j = 0; j < offset_count; j++)
        {
            if (tiny_offset == g_offsets[j].value)
            {
                if (createDS)
                    SETLOBITS(g_bb_ds_temp[g_valence_count].soindex, g_offsets[j].fewbit);
                else
                    SETLOBITS(g_bb[g_valence_count].soindex, g_offsets[j].fewbit);
                found = true;
                break;
            }
        }

        // If we can't find it, log error.
        if (! found)
        {
            syslog(LOG_ERR, "Couldn't find index for tiny_offset: %d", tiny_offset);

            // Set to something popular.
            if (createDS)
                SETLOBITS(g_bb_ds_temp[g_valence_count].soindex, g_offsets[0].fewbit);
            else
                SETLOBITS(g_bb[g_valence_count].soindex, g_offsets[0].fewbit);
        }
        found = false;

        // If we're filling up the g_bb_ds_temp, then set the x value.
        if (createDS)
            COMPACT(g_bb_ds_temp[g_valence_count].x, id_1);

        // Create an index of x-value starting positions in Beast.
        if (id_1 != prev_id_1)
        {
            g_bind_seg[id_1].offset = g_valence_count;
            prev_id_1 = id_1;
        }

        // add to count in bind_seg
        g_bind_seg[id_1].count++;

        g_valence_count++;

        // Spit something out every 10 M to generally track progress.
        if (0 == (g_valence_count % 10000000))
            syslog(LOG_INFO, "g_valence_count is: %" PRIu64, g_valence_count);

    } // end while we still have lines to process in this file
    syslog(LOG_INFO, "g_valence_count is %" PRIu64, g_valence_count);
    free(line);
    fclose(fp);
    return 0;
} // end pullFromFiles()


// This dumps the Beast and associated indexes (2 structures total) to the filesystem for quick loading later. Like a few seconds quick.
void export_beast()
{
    // Now write the loaded bb to a file for quick-fast in a hurry loading later.
    char filename[strlen(BE.valence_cache_dir) + strlen(VALENCES_BB_SEG) + 2];
    strlcpy(filename, BE.valence_cache_dir, sizeof(filename));
    strlcat(filename, "/", sizeof(filename));
    strlcat(filename, VALENCES_BB, sizeof(filename));

    // Check if dir exists and if it doesn't, create it.
    if (!check_make_dir(BE.valence_cache_dir))
    {
        syslog(LOG_CRIT, "Can't create directory %s so exiting. More details might be in stderr.", BE.valence_cache_dir);
        exit(-1);
    }

    FILE *val_out = fopen(filename,"w");

    assert(NULL != val_out);
    size_t num_valences_written = fwrite(g_bb, sizeof(valence_t), g_valence_count, val_out);
    syslog(LOG_INFO, "Number of valences written to bin file: %lu and we expected %" PRIu64 " to be written.",
           num_valences_written, g_valence_count);
    fclose(val_out);
    
    // Now write the loaded bb segment starts to a file for quick-fast in a hurry loading later.
    strlcpy(filename, BE.valence_cache_dir, sizeof(filename));
    strlcat(filename, "/", sizeof(filename));
    strlcat(filename, VALENCES_BB_SEG, sizeof(filename));
    val_out = fopen(filename,"w");
    assert(NULL != val_out);
    num_valences_written = fwrite(g_bind_seg, sizeof(bb_ind_t), BE.num_elts, val_out);
    syslog(LOG_INFO, "Number of valence segment starts written to bin file: %lu and we expected %" PRIu64 " to be written.",
           num_valences_written, BE.num_elts);
    fclose(val_out);
} // end exportBeast()


// This dumps the DS and associated indexes (2 structures total) to the filesystem for quick loading later. Like a few seconds quick.
void export_ds()
{
    // Now write the loaded bb ds to a file for quick-fast in a hurry loading later.
    char filename[strlen(BE.valence_cache_dir) + strlen(VALENCES_BB_SEG_DS) + 2];
    strlcpy(filename, BE.valence_cache_dir, sizeof(filename));
    strlcat(filename, "/", sizeof(filename));
    strlcat(filename, VALENCES_BB_DS, sizeof(filename));

    // Check if dir exists and if it doesn't, create it.
    if (!check_make_dir(BE.valence_cache_dir))
    {
        syslog(LOG_CRIT, "Can't create directory %s so exiting. More details might be in stderr.", BE.valence_cache_dir);
        exit(-1);
    }

    FILE *val_out = fopen(filename,"w");

    assert(NULL != val_out);
    size_t num_valences_written = fwrite(g_bb_ds, sizeof(valence_t), g_valence_count, val_out);
    syslog(LOG_INFO, "Number of valences written to DS bin file: %lu and we expected %" PRIu64 " to be written.",
           num_valences_written, g_valence_count);
    fclose(val_out);
    
    // Now write the loaded bb segment starts to a file for quick-fast in a hurry loading later.
    strlcpy(filename, BE.valence_cache_dir, sizeof(filename));
    strlcat(filename, "/", sizeof(filename));
    strlcat(filename, VALENCES_BB_SEG_DS, sizeof(filename));
    val_out = fopen(filename,"w");
    assert(NULL != val_out);
    num_valences_written = fwrite(g_bind_seg_ds, sizeof(bb_ind_t), BE.num_elts, val_out);
    syslog(LOG_INFO, "Number of valence segment starts written to DS bin file: %lu and we expected %" PRIu64 " to be written.",
           num_valences_written, BE.num_elts);
    fclose(val_out);
} // end exportDS()


// This function loads element popularity from the pop.out flat file.
bool pop_load()
{
    if (NULL != g_pop)
    {
        free(g_pop);
    }

    // +1 to allow addressing of elt to match.
    g_pop = (popularity_t *) malloc(sizeof(popularity_t) * (BE.num_elts + 1));
    if (NULL == g_pop)
    {
        syslog(LOG_ERR, "FATAL ERROR: Out of memory when creating popularity structure.");
        return (-1);
    }

    char filename[strlen(BE.working_dir) + strlen(POP_OUTFILE) + 2];

    strlcpy(filename, BE.working_dir, sizeof(filename));
    strlcat(filename, "/", sizeof(filename));
    strlcat(filename, POP_OUTFILE, sizeof(filename));

    FILE *fp = fopen(filename, "r");
    if (!fp)
    {
        syslog(LOG_ERR, "Can't open file %s. Exiting.", filename);
        exit(-1);
    }

    char *line = NULL;
    size_t line_capacity = 0;
    ssize_t line_length;
    int i = 0;                   // remember the pop values start at index 1, not 0
    while ((line_length = getline(&line, &line_capacity, fp)) != -1)
    {
        // Is the file coming from linux/unix or pre Mac OSX?
        if (line_length && (line[line_length - 1] == '\n' || line[line_length - 1] == '\r'))
            line[--line_length] = '\0';
        // Is the file coming from windows?
        if (line_length && (line[line_length - 1] == '\r'))
            line[--line_length] = '\0';

        // Convert line to a popularity_t.
        const popularity_t popularity = strtol(line, NULL, 10);

        // Put this Popularity in g_pop.
        g_pop[i] = popularity;
        i++;
    } // end while more lines in pop.out

    // Check number of rows read against num_elts. Rows read should be one more, for the 0 row.
    if ((unsigned long) i != (BE.num_elts + 1))
    {
        syslog(LOG_ERR,
               "ERROR: BE.num_elts (%" PRIu64 ") did not equal number of rows minus 1 (%d) from pop.out. Exiting.",
               BE.num_elts, i);
        exit(-1);
    }

    free(line);
    fclose(fp);

    syslog(LOG_INFO, "Successfully loaded up the g_pop.");

    return (true);
} // end popularity loading


// This function loads compressed slopes/offsets from a flat file.
static void load_so_compressed()
{
    char filename[strlen(BE.valence_cache_dir) + strlen(SO_COMP_OUTFILE) + 2];
    strlcpy(filename, BE.valence_cache_dir, sizeof(filename));
    strlcat(filename, "/", sizeof(filename));
    strlcat(filename, SO_COMP_OUTFILE, sizeof(filename));

    FILE *fp = fopen(filename, "r");
    if (!fp)
    {
        syslog(LOG_ERR, "Can't open file %s. Exiting.", filename);
        exit(-1);
    }

    char *line = NULL;
    size_t line_capacity = 0;
    ssize_t line_length;
    const char delimiter[4] = "\t";
    bool do_slopes = false;

    while ((line_length = getline(&line, &line_capacity, fp)) != -1)
    {
        // Is the file coming from linux/unix or pre Mac OSX?
        if (line_length && (line[line_length - 1] == '\n' || line[line_length - 1] == '\r'))
            line[--line_length] = '\0';
        // Is the file coming from windows?
        if (line_length && (line[line_length - 1] == '\r'))
            line[--line_length] = '\0';

        // Use strtok to tokenize line.
        int token_number = 0;

        // Get the first token.
        const char *token = strtok(line, delimiter);

        // Walk through other tokens.
        while(token != NULL)
        {
            if (0 == token_number)
            {
                if (!strcmp(token, "slopes")) do_slopes = true;
                else do_slopes = false;
            }
            else
            {
                if (token_number <= NUM_SO_BUCKETS)
                {
                    if (do_slopes)
                    {
                        g_tiny_slopes[token_number - 1] = (int8_t) strtol(token, NULL, 10);
                        if (0 == g_tiny_slopes[token_number - 1])
                            g_tiny_slopes_inv[token_number - 1] = 0; // guard against div by 0 and leave this as 0.
                        else
                            g_tiny_slopes_inv[token_number - 1] = (double) 1.0 / g_tiny_slopes[token_number - 1];
                    }
                    else
                        g_tiny_offsets[token_number - 1] = (int8_t) strtol(token, NULL, 10);
                }
                else
                {
                    syslog(LOG_ERR, "Too many tokens in %s", SO_COMP_OUTFILE);
                    exit(-1);
                }
            }
            token = strtok(NULL, delimiter);
            token_number++;
        } // end while parsing tokens
    } // end while more lines in slope/offset compressed outfile

    free(line);
    fclose(fp);

    syslog(LOG_INFO, "Successfully loaded up the g_tiny_slopes and g_tiny_offsets.");

    /*
    // b debugging
    for (int i = 0; i < NUM_SO_BUCKETS; i++)
    {
        syslog(LOG_INFO, "g_tiny_slopes[%d]:%d", i, g_tiny_slopes[i]);
    }
    for (int i = 0; i < NUM_SO_BUCKETS; i++)
    {
        syslog(LOG_INFO, "g_tiny_offsets[%d]:%d", i, g_tiny_offsets[i]);
    }
    // e debugging
    */

} // end compressed slope/offset loading


// Load valences from valgen output or beast output depending on passed-in read method.
bool  load_beast(int read_method, bool ds_load)
{
    // Sanity check
    if (g_num_confident_valences == 0)
    {
        syslog(LOG_ERR, "FATAL ERROR: There are no confident valences so not much we can do. Exiting.");
        return (false);
    }

    // If g_bb and friends exist, clear 'em out first. This may be the situation if we got here because we're
    // rebuilding valence cache in parallel during recgen runtime, and now we need to reload the cache.
    // Elsewhere, we're already blocking the access to g_bb, so we don't need to worry about that here.
    if (NULL != g_bb)
    {
        free(g_bb);
        if (NULL != g_bind_seg) free(g_bind_seg);

#ifdef linux
        // Seriously free the mem. Looking at you glibc.
        malloc_trim(0);
#endif
    }

    // Create the bb.
    g_bb = (valence_t *) calloc(g_num_confident_valences, sizeof(valence_t));
    if (g_bb == 0)
    {
        syslog(LOG_ERR, "FATAL ERROR: Out of memory when creating bb.");
        return (false);
    }

    // Create the bind_seg. +1 b/c indexing starts at 1
    g_bind_seg = (bb_ind_t *) calloc((unsigned long) BE.num_elts + 1, sizeof(bb_ind_t));
    if (g_bind_seg == 0)
    {
        syslog(LOG_ERR, "FATAL ERROR: Out of memory when creating bind_seg.");
        return (false);
    }

    if (ds_load)
    {
        if (NULL != g_bb_ds)
        {
            free(g_bb_ds);
            if (NULL != g_bind_seg_ds) free(g_bind_seg_ds);
#ifdef linux
            // Seriously free the mem. Looking at you glibc.
            malloc_trim(0);
#endif
        }
        // Create the bb_ds.
        g_bb_ds = (valence_t *) calloc(g_num_confident_valences, sizeof(valence_t));
        if (g_bb_ds == 0)
        {
            syslog(LOG_ERR, "FATAL ERROR: Out of memory when creating bb_ds.");
            return (false);
        }
        // Create the bind_seg_ds. +1 b/c indexing starts at 1
        g_bind_seg_ds = (bb_ind_t *) calloc((unsigned long) BE.num_elts + 1, sizeof(bb_ind_t));
        if (g_bind_seg_ds == 0)
        {
            syslog(LOG_ERR, "FATAL ERROR: Out of memory when creating bind_seg_ds.");
            return (false);
        }
    }

    syslog(LOG_INFO, "Number of bytes allocated for bb / bb_ds: %ld each",
           sizeof(valence_t) * g_num_confident_valences);

    switch (read_method)
    {
        case LOAD_VALENCES_FROM_VALGEN:
            pull_from_files(false);  // we are not creating the DS at this point
            break;

        case LOAD_VALENCES_FROM_BEAST_EXPORT:
            load_so_compressed();
            pull_from_beast_export();
            break;

        default:
            syslog(LOG_ERR, "FATAL ERROR: Unknown method for loading valences.");
            return (false);
    }
    return (true);
} // end beastLoad()


// Load ratings from valgen output.
bool big_rat_load()
{
    if (NULL != g_big_rat)
    {
        free(g_big_rat);
        if (NULL != g_big_rat_index) free (g_big_rat_index);

#ifdef linux
        // Really free the memory. Why, glibc?
        malloc_trim(0);
#endif
    }

    // Create the big_rat.
    g_big_rat = (rating_t *) calloc(BE.num_ratings, sizeof(rating_t));
    if (g_big_rat == 0)
    {
        syslog(LOG_ERR, "FATAL ERROR: Out of memory when creating g_big_rat.");
        return (false);
    }

    // Create the big_rat_index
    g_big_rat_index = (uint32_t *) calloc(BE.num_people + 1, sizeof(uint32_t));
    if (g_big_rat_index == 0)
    {
        syslog(LOG_ERR, "FATAL ERROR: Out of memory when creating g_big_rat_index.");
        return (false);
    }

    // Begin fread-ing the 2 memory structures.

    // Read the big_rat
    char file_to_open[strlen(BE.working_dir) + strlen(RATINGS_BR_INDEX) + 2];

    strlcpy(file_to_open, BE.working_dir, sizeof(file_to_open));
    strlcat(file_to_open, "/", sizeof(file_to_open));
    strlcat(file_to_open, RATINGS_BR, sizeof(file_to_open));
    FILE *rat_out = fopen(file_to_open,"r");
    assert(NULL != rat_out);

    size_t num_ratings_read = fread(g_big_rat, sizeof(rating_t), BE.num_ratings, rat_out);
    syslog(LOG_INFO, "Number of ratings read from bin file: %lu and we expected %lu to be read.",
           num_ratings_read, (unsigned long) BE.num_ratings);
    assert(num_ratings_read == BE.num_ratings);
    fclose(rat_out);

    // Read the big_rat_index
    strlcpy(file_to_open, BE.working_dir, sizeof(file_to_open));
    strlcat(file_to_open, "/", sizeof(file_to_open));
    strlcat(file_to_open, RATINGS_BR_INDEX, sizeof(file_to_open));
    rat_out = fopen(file_to_open,"r");
    assert(NULL != rat_out);

    num_ratings_read = fread(g_big_rat_index, sizeof(uint32_t), BE.num_people + 1, rat_out);
    syslog(LOG_INFO, "Number of index locations read from bin file: %lu and we expected %" PRIu64 " to be read.",
           num_ratings_read, BE.num_people + 1);
    fclose(rat_out);
    assert(num_ratings_read == BE.num_people + 1);

    return true;
} // end big_rat_load()


// Create the Differently Sorted beast.
bool create_ds()
{
    // Free the beast (if it exists) b/c we can't keep him in RAM at the same time as bb_ds.
    if (g_bb)
    {
        free(g_bb);

#ifdef linux
        // Seriously free the mem. Looking at you glibc.
        malloc_trim(0);
#endif
    }

    // Create the bb_ds with special Valence_xy_t which is only used for DS creation b/c we need a simple way to sort during creation.
    g_bb_ds_temp = (valence_xy_t *) calloc(g_num_confident_valences, sizeof(valence_xy_t));
    if (g_bb_ds_temp == 0)
    {
        syslog(LOG_CRIT, "FATAL ERROR: Out of memory when creating bb_ds_temp.");
        return (false);
    }

    // Create the bind_seg.
    g_bind_seg = (bb_ind_t *) calloc((unsigned long) BE.num_elts, sizeof(bb_ind_t));
    if (g_bind_seg == 0)
    {
        syslog(LOG_ERR, "FATAL ERROR: Out of memory when creating bind_seg.");
        return (false);
    }

    // Create the bind_seg_ds.
    g_bind_seg_ds = (bb_ind_t *) calloc((unsigned long) BE.num_elts, sizeof(bb_ind_t));
    if (g_bind_seg_ds == 0)
    {
        syslog(LOG_ERR, "FATAL ERROR: Out of memory when creating bind_seg_ds.");
        return (false);
    }

    // now go populate the DS structures
    populate_ds();

    return (true);
} // end create_DS()


// Create leashes to various structures.
valence_t *bb_leash()
{
    return (g_bb);
}

bb_ind_t  *bind_seg_leash()
{
    return (g_bind_seg);
}

valence_t *bb_ds_leash()
{
    return (g_bb_ds);
}

bb_ind_t  *bind_seg_ds_leash()
{
    return (g_bind_seg_ds);
}

popularity_t *pop_leash()
{
    return (g_pop);
}

int8_t *tiny_slopes_leash()
{
    return (g_tiny_slopes);
}

double *tiny_slopes_inv_leash()
{
    return (g_tiny_slopes_inv);
}

int8_t *tiny_offsets_leash()
{
    return (g_tiny_offsets);
}

rating_t *big_rat_leash()
{
    return (g_big_rat);
}

uint32_t *big_rat_index_leash()
{
    return (g_big_rat_index);
}
