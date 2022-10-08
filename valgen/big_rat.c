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

//
// This file has functions which manage the collection of item ratings.
//

//
// Globals
//

// g_big_rat must store the entire representation of the ratings
static uint64_t g_curr = 0;                    // static counter of ratings added
static Rating *g_big_rat, *g_big_rat_ds;  // static structure to hold ratings, ds is for different sorting
static popularity_t *g_pop;               // static structure to hold popularity structure for each element

//
// Forward declarations
//
void add(Rating);
int ratingcmp(const void *, const void*);
int ratingcmpDS(const void*, const void*);

//
// Create the big chunk of memory. And another one, sorted differently to decrease mem reqs.
//
int big_rat_init(void)
{
    // Declare time variables.
    long long start, finish;
    size_t size;

    size = sizeof(Rating) * BE.num_ratings;
        
    syslog(LOG_INFO, "Begin timing for allocation of big_rats...");

    // Record the start time.
    start = current_time_micros();

    g_big_rat = (Rating *) malloc(size);
    g_big_rat_ds = (Rating *) malloc(size);
    g_pop = (popularity_t *) malloc(sizeof(popularity_t) * (BE.num_elts + 1));  // the + 1 is so we can address this the same way as eltid

    // Record the end time.
    finish = current_time_micros();

    // Print out the difference
    syslog(LOG_INFO, "Time to allocate: %d microseconds", (int) (finish - start));
    syslog(LOG_INFO, "Number of bytes allocated for each of the two structures: %lu", size);

    if (0 == g_big_rat || 0 == g_big_rat_ds || 0 == g_pop)
    {
        syslog(LOG_ERR, "ERROR: Out of memory when creating g_big_rat or g_big_rat_ds or g_pop.");
        return (1);
    }
    syslog(LOG_INFO, "Created the g_big_rat and g_big_rat_ds with %lu elements each.", (unsigned long) BE.num_ratings);
    syslog(LOG_INFO, "Created the g_pop with %lu elements.", (unsigned long) BE.num_elts);

    // Init the g_pop by setting the eltid and ensuring rcount is 0.
    for (unsigned long i=0; i < (BE.num_elts + (unsigned long) 1); i++)
    {
        g_pop[i].eltid = (uint32_t) i;
        g_pop[i].rcount = 0;
    }

    return (0);
} // end big_rat_init()


//
// Put a Rating into the big_rat and big_rat_ds.
//
void add(Rating r)
{
    // Put the rating data in the big_rat.
    if (BE.num_ratings == g_curr)
    {
        syslog(LOG_CRIT, "FATAL ERROR: tried to put more ratings into structure than what we were expecting.");
        exit (1);
    }
    g_big_rat[g_curr].userId = r.userId;
    g_big_rat[g_curr].eltid = r.eltid;
    g_big_rat[g_curr].rating = r.rating;
        
    // Set up additonal Ratings structure, to be sorted differently.
    g_big_rat_ds[g_curr].userId = r.userId;
    g_big_rat_ds[g_curr].eltid = r.eltid;
    g_big_rat_ds[g_curr].rating = r.rating;

    // Update the g_pop.
    g_pop[r.eltid].rcount += 1;

    g_curr++;
    if (0 == r.eltid)
    {
        syslog(LOG_ERR, "Not a happy place: 0 elementid for user %d, rating %d", r.userId, r.rating);
    }
} // end add()


//
// A function to compare any two Ratings for use as a callback function by qsort alternative below.
// We're sorting by userid, then eltid
//
// Returns -1 if the first uid is less than the second || same user but first elt is less than second
//          0 if the two uids are the same AND element ids are equal
//          1 if the first uid is greater than the second || same user but first elt is greater than second
//
int ratingcmp(const void* p1, const void* p2)
{
    Rating x = *(const Rating *) p1, y = *(const Rating *) p2;

    // this test is to see if p1 < p2
    if ((x.userId < y.userId) || ((x.userId == y.userId) && (x.eltid < y.eltid)))
    {
        return (-1);
    }
    else
    {
        // this test is to see if p1 > p2
        if ((x.userId > y.userId) || ((x.userId == y.userId) && (x.eltid > y.eltid)))
        {
            return (1);
        }
        else
        {
            // they're the same (x.user = y.user AND x.element = y.element)
            return (0);
        }
    }
} // end ratingcmp()


//
// A function to compare any two Ratings for use as a callback function by qsort alternative below.
// We're sorting by eltid, then userid (opposite of how ratingcmp sorts)
// Returns -1 if the first eltid is less than the second || same elts but first user is less than second
//          0 if the two eltids are the same AND userids are equal
//          1 if the first eltid is greater than the second || same elts but first user is greater than second
//
int ratingcmpDS(const void* p1, const void* p2)
{
    Rating x = *(const Rating *) p1, y = *(const Rating *) p2;

    // this test is to see if p1 < p2
    if ((x.eltid < y.eltid) || ((x.eltid == y.eltid) && (x.userId < y.userId)))
    {
        return (-1);
    }
    else
    {
        // this test is to see if p1 > p2
        if ((x.eltid > y.eltid) || ((x.eltid == y.eltid) && (x.userId > y.userId)))
        {
            return (1);
        }
        else
        {
            // they're the same (x.user = y.user AND x.element = y.element)
            return (0);
        }
    }
} // end ratingcmpDS()


//
// A function to compare any two popularities for use as a callback function by qsort().
// We're sorting by the popularity's rcount, high to low
//
// Returns -1 if the first rcount is greater than the second.
//          0 if the two rcounts are the same.
//          1 if the first rcount is less than the second.
//
int static pop_count_cmp(const void* p1, const void* p2)
{
    popularity_t x = *(const popularity_t *) p1, y = *(const popularity_t *) p2;

    // Note that the return values are flipped because we want to sort high to low.

    // This test is to see if p1 < p2.
    if (x.rcount < y.rcount)
    {
        return (1);
    }
    else
    {
        // this test is to see if p1 > p2
        if (x.rcount > y.rcount)
        {
            return (-1);
        }
        else
        {
            // they're the same (x.rcount == y.rcount)
            return (0);
        }
    }
} // end pop_count_cmp()


//
// A function to compare any two popularities for use as a callback function by qsort().
// We're sorting by the popularity's eltid, low to high
//
// Returns -1 if the first eltid is less than the second.
//          0 if the two eltids are the same.
//          1 if the first eltid is greater than the second.
//
static int pop_elt_cmp(const void* p1, const void* p2)
{
    popularity_t x = *(const popularity_t *) p1, y = *(const popularity_t *) p2;

    // This test is to see if p1 < p2.
    if (x.eltid < y.eltid)
    {
        return (-1);
    }
    else
    {
        // this test is to see if p1 > p2
        if (x.eltid > y.eltid)
        {
            return (1);
        }
        else
        {
            // they're the same (x.eltid == y.eltid)
            return (0);
        }
    }
} // end pop_elt_cmp()


static bool ds = false;
typedef Rating type;      // array type
#define MAX 64            // stack size for max 2^(64/2) array elements

static void quicksort_iterative(type array[], uint64_t len)
{
    uint64_t left = 0, stack[MAX], pos = 0;
    uint64_t seed = (unsigned) rand(); // NOLINT because we know rand() is pseudorandom and that's ok.
    for ( ; ; )                                          // outer loop
    {
        for (; left+1 < len; len++)                      // sort left to len-1
        {
            if (pos == MAX) len = stack[pos = 0];        // stack overflow, reset
            type pivot = array[left+seed%(len-left)];    // pick random pivot
            seed = seed*69069+1;                         // next pseudorandom number
            stack[pos++] = len;                          // sort right part later
            for (uint64_t right = left-1; ; )            // inner loop: partitioning
            {
                if (ds)
                {
                    while (ratingcmpDS(&(array[++right]), &pivot) < 0); // look for greater element
                    while (ratingcmpDS(&pivot, &(array[--len])) < 0);   // look for smaller element
                }
                else
                {
                    while (ratingcmp(&(array[++right]), &pivot) < 0);   // look for greater element
                    while (ratingcmp(&pivot, &(array[--len])) < 0);     // look for smaller element
                }
                if (right >= len) break;                 // partition point found?
                type temp = array[right];
                array[right] = array[len];               // the only swap
                array[len] = temp;
            } // end for loop
        } // end for loop
        if (pos == 0) break;                             // stack empty?
        left = len;                                      // left to right is sorted
        len = stack[--pos];                              // get next range to sort
    } // end outermost for loop
} // end quicksort_iterative()


// Pull ratings from flat file.
void big_rat_pull_from_flat_file(void)
{
    // Plan is:
    // 1) read the ratings from the flat file
    // 2) once we have userid, and rating, call add() for each rating
    // 3) sort the big guy

    long long start, finish;

    syslog(LOG_INFO, "Begin timing for loading of big_rat...");

    // Record the start time:
    start = current_time_millis();

    Rating r;
    r.rating = 0;
    r.eltid = 0;
    r.userId = 0;

    // Begin reading from flat file.

    FILE *fp;
    u_int64_t i;
    u_int64_t ratings_count = 0;
    char filename[strlen(BE.ratings_file) + 1];
    strlcpy(filename, BE.ratings_file, sizeof(filename));
    fp = fopen(filename, "r");
    if (!fp)
    {
        syslog(LOG_CRIT, "Can't open ratings file %s. Exiting.", filename);
        exit(-11);
    }

    char *line = NULL;
    size_t line_capacity = 0;
    ssize_t line_length;

    // Allow for tab and comma-delimited ratings input file.
    const char delimiter[3] = "\t,";

    while ((line_length = getline(&line, &line_capacity, fp)) != -1)
    {
        // Is the file coming from Linux/Unix or maybe Mac?
        if (line_length && (line[line_length - 1] == '\n' || line[line_length - 1] == '\r'))
            line[--line_length] = '\0';
        // Is the file coming from Windows?
        if (line_length && (line[line_length - 1] == '\r'))
            line[--line_length] = '\0';

        int token_number = 0;

        // Get the first token.
        char *token;
        token = strtok(line, delimiter);

        // Walk through other tokens.
        while( token != NULL )
        {
            switch (token_number)
            {
                case 0:   // person id
                    r.userId = (uint32_t) strtol(token, NULL, 10);
                    break;
                case 1:   // element id
                    r.eltid = (uint32_t) strtol(token, NULL, 10);
                    break;
                case 2:   // rating
                    r.rating = (uint8_t) strtol(token, NULL, 10);

                    // Convert to 32-buckets if not already there
                    if (g_ratings_scale != 32)
                    {
                        double floaty, floaty2;
                        floaty = 32.0 / (double) g_ratings_scale;
                        floaty2 = (double) r.rating * floaty;
                        r.rating = (uint8_t) bmh_round(floaty2);
                        if (r.rating > 32) r.rating = 32;
                    }
                    break;
                default:
                    syslog(LOG_ERR, "ERROR: hit the default case situation when parsing tokens in ratings input file. Why?");
                    syslog(LOG_ERR, "token number in default case is: %d, ratings_count is %lu", token_number, ratings_count);
                    exit(-1);
            } // end switch stmt
            token = strtok(NULL, delimiter);
            token_number++;
        } // end while() parsing tokens

        // Check to make sure the elements and people ids are normalized.
        if (r.eltid > BE.num_elts || r.userId > BE.num_people)
        {
            syslog(LOG_ERR, "Either eltid %d is more than num_elts %lu or userid %d is more than num_people %lu. Userids and element ids must be in sequence. ratings_count is %lu. Exiting.",
                   r.eltid, BE.num_elts, r.userId, BE.num_people, ratings_count);
            exit(-1);
        }

        add(r);

        // Spit something out every 1 M to generally track progress.
        if (0 == (ratings_count % 1000000))
            printf("ratings_count is: %lu \n", ratings_count);
        ratings_count++;
    } // end while we still have lines to process in this file

    printf("final ratings_count: %lu\n", ratings_count);
    free(line);
    fclose(fp);

    // end read from flat file

    // Record the end time.
    finish = current_time_millis();

    // Print out the difference.
    syslog(LOG_INFO, "Time to load: %d milliseconds", (int) (finish - start));

    // Print the number of big_rat elts.
    syslog(LOG_INFO, "Number of big_rat elements is %lu", g_curr);

    // 4) sort the big guy
    // Now sort on uid.
    syslog(LOG_INFO, "Beginning sort of big_rat and big_rat_ds...");

    // DO NOT USE QSORT. It's broken for large data sizes that we sometimes use.
    start = current_time_millis();
    quicksort_iterative(g_big_rat, (BE.num_ratings));
    ds = true;
    quicksort_iterative(g_big_rat_ds, (BE.num_ratings));

    finish = current_time_millis();

    // Print out the difference
    syslog(LOG_INFO, "Time to sort both the big_rat structures: %d milliseconds", (int) (finish - start));

    // At this point, g_big_rat is sorted by userId then eltid, and is not redundant.
    // At this point, g_big_rat_ds is sorted by eltid then userId and is not redundant.
    // The non-redundancy part just so happens to be that way based on data

    // todo: ensure non-redundancy

    // sanity check
    for (i = 0; i < 10; i++)
    {
        syslog(LOG_INFO, "big_rat [%lu].userId: %d, rating: %d, elt: %d", i, g_big_rat[i].userId, g_big_rat[i].rating, g_big_rat[i].eltid);
        syslog(LOG_INFO, "big_rat_ds [%lu].userId: %d, rating: %d, elt: %d", i, g_big_rat_ds[i].userId, g_big_rat_ds[i].rating, g_big_rat_ds[i].eltid);
    }

    br = g_big_rat;
    brds = g_big_rat_ds;

    // Make an array index of the users with values of BR indexes so we don't have to be so dumb in buildElements()

    // Iterate over BR once to populate the br_index
    int curr_user = 0;
    i = 0;

    // Allocate br_index.
    br_index = malloc((BE.num_people + 1) * sizeof(uint32_t));
    if (br_index == NULL)
    {
        syslog(LOG_CRIT, "Out of RAM. Can't allocate br_index. Exiting.");
        exit(-1);
    }

    // Need to speed-optimize the br_index, so make it an array. We just sorted g_big_rat by personid so we can walk it.
    // It can be an array of uint32_t. The position in the array is the walker_user. Just watch the one-off index.
    while (i < BE.num_ratings)
    {
        uint32_t walker_user;
        walker_user = g_big_rat[i].userId;
        if (walker_user != (uint32_t) curr_user)
        {
            (br_index)[walker_user] = (uint32_t) i;
            curr_user = (int) walker_user;
        }
        i++;
    }

    // Now shape up the g_pop.

    // First, sort it by rcount, high to low.
    qsort(g_pop, BE.num_elts + 1, sizeof(popularity_t), pop_count_cmp);

    // Now divide up by 7.
    size_t num_per_group = BE.num_elts / 7;

    // And set the pop value.
    uint8_t cur_pop = 1;
    size_t group_counter = 0;
    for (i = 0; i < BE.num_elts; i++)
    {
        if (group_counter == num_per_group)
        {
            group_counter = 0;
            cur_pop++;
        }
        g_pop[i].pop = cur_pop;
        group_counter++;
    }

    // Now sort g_pop by eltid, low to high
    qsort(g_pop, BE.num_elts + 1, sizeof(popularity_t), pop_elt_cmp);

    // Dump the data structure to the file system.
    char fname[512];
    sprintf(fname, "%s%s", BE.working_dir, "/pop.out");

    fp = fopen(fname,"w");

    if (NULL == fp)
    {
        syslog(LOG_ERR, "ERROR: cannot open output file %s", fname);
        exit(1);
    }

    char out_buffer[3];

    for (i=0; i < (BE.num_elts + 1); i++)
    {
        out_buffer[0] = (char) g_pop[i].pop + '0';
        out_buffer[1] = '\n';
        out_buffer[2] = '\0';
        fwrite(out_buffer, strlen(out_buffer), 1, fp);
    }

    if (fclose(fp) != 0)
    {
        syslog(LOG_ERR, "Error closing output file %s.", fname);
        exit(1);
    }
    free(g_pop);
} // end big_rat_pull_from_flat_file()


// This dumps the big_rat and br_rat_index (2 structures total) to the filesystem for quick loading later.
void export_br(void)
{
    // Now write the loaded big_rat to a file for quick-fast in a hurry loading later.
    char filename[strlen(BE.working_dir) + 24];
    strlcpy(filename, BE.working_dir, sizeof(filename));
    strlcat(filename, "/", sizeof(filename));
    strlcat(filename, BIG_RAT_OUTFILE, sizeof(filename));

    // Check if dir exists and if it doesn't, create it.
    if (!check_make_dir(BE.working_dir))
    {
        syslog(LOG_CRIT, "Can't create directory %s so exiting. More details might be in stderr.", BE.working_dir);
        exit(-1);
    }

    FILE *rat_out = fopen(filename,"w");

    assert(NULL != rat_out);
    size_t num_ratings_written;
    num_ratings_written = fwrite(g_big_rat, sizeof(Rating), BE.num_ratings, rat_out);
    syslog(LOG_INFO, "Number of ratings written to bin file: %lu and we expected %lu to be written.",
           num_ratings_written, BE.num_ratings);
    fclose(rat_out);

    // Now write the br_index to a file for quick-fast in a hurry loading later.
    strlcpy(filename, BE.working_dir, sizeof(filename));
    strlcat(filename, "/", sizeof(filename));
    strlcat(filename, BIG_RAT_INDEX_OUTFILE, sizeof(filename));
    rat_out = fopen(filename,"w");
    assert(NULL != rat_out);

    num_ratings_written = fwrite(br_index, sizeof(uint32_t), BE.num_people + 1, rat_out);
    syslog(LOG_INFO, "Number of br_index entries written to bin file: %lu and we expected %lu to be written.",
           num_ratings_written, BE.num_people + 1);
    fclose(rat_out);
} // end export_br()

