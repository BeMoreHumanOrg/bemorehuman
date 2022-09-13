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
// This is the valence generator.
//
// Read ratings from flat file then create a
// valence flat file where each line is like:
//
// "15","392","6","-0.800000","5.800000","-0.309839","f"
//

// Globals.
uint8_t g_ratings_scale = 32;

// Sanity test for spearman math.
static void test_spearman()
{
    double spear;
    int n,  elt1, elt2;

    // Elt pair we're going to test for has 10 ratings pairs.
    elt1 = 1;
    elt2 = 2;
    n = 10;
    uint8_t bx[20] =
        {1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 2, 3, 5, 4, 3, 3, 5, 4, 1, 1};
    spear = spearman(n, bx);
    syslog(LOG_INFO, "spearman for (%d,%d) is %8.6f", elt1, elt2, spear);
    assert (fabs((double) 0.916534 - spear) < 0.000001);

    // Elt pair we're going to test for has 16 ratings pairs.
    elt1 = 1;
    elt2 = 3;
    n = 16;
    uint8_t cx[32] =
        {4, 4, 1, 2, 1, 2, 4, 3, 1, 1, 3, 3, 3, 3, 4, 4, 4, 1, 5, 5, 1, 1, 3, 2, 5, 4, 1, 2, 4, 4, 1, 1};
    spear = spearman(n, cx);
    syslog(LOG_INFO, "spearman for (%d,%d) is %8.6f", elt1, elt2, spear);
    assert (fabs((double) 0.770305 - spear) < 0.000001);

    // Elt pair we're going to test for has 7 ratings pairs.
    elt1 = 1;
    elt2 = 11;
    n = 7;
    uint8_t dx[14] =
        {1, 1, 1, 1, 5, 4, 3, 3, 5, 3, 1, 2, 1, 1};
    spear = spearman(n, dx);
    syslog(LOG_INFO, "spearman for (%d,%d) is %8.6f", elt1, elt2, spear);
    assert (fabs((double) 0.908025 - spear) < 0.000001);

    // Elt pair we're going to test for has 48 ratings pairs.
    elt1 = 199;
    elt2 = 270;
    n = 48;
    uint8_t ex[96] =
        {4, 5, 5, 5, 4, 4, 3, 5, 1, 5, 4, 3, 2, 4, 4, 2, 4, 4, 3, 5, 3, 5, 3, 5, 3, 4, 4, 4, 4, 5, 5, 5, 4, 4,
         5, 5, 5, 3, 4, 5, 2, 5, 5, 4, 4, 4, 4, 5, 4, 1, 4, 3, 3, 5, 4, 5, 4,
         5, 4, 4, 5, 4, 2, 5, 4, 4, 4, 4, 1, 5, 4, 5, 5, 5, 4, 3, 3, 5, 3, 5, 4, 5, 5, 5, 3, 5, 4, 4, 3, 4, 3,
         5, 5, 5, 4, 2};
    spear = spearman(n, ex);
    syslog(LOG_INFO, "spearman for (%d,%d) is %8.6f", elt1, elt2, spear);
    assert (fabs(-0.221844 - spear) < 0.000001);
} // end test_spearman()


//
// Begin thread-specific stuff.
//
static uint32_t elts_per_thread;

Rating *br, *brds;       // brds is the Differently Sorted BR
uint32_t *br_index;      // index into big_rat


// Each thread computes a portion of the g_elements and subsequent valences.
static void *partial_elements(void *args)
{
    uint32_t *myid = (uint32_t *) args;
    uint32_t el1, start_elt, end_elt;

    syslog(LOG_INFO,"in partial_elements, *myid is %d", *myid);

    // The +1 is because we start indexing at 1, not 0 for g_elements.
    start_elt = *myid * elts_per_thread + 1;

    // If we are the last thread we might have a bit more work to do.
    if (*myid == (NTHREADS - 1))
        end_elt = (uint32_t) BE.num_elts + 1;
    else
        end_elt = start_elt + elts_per_thread; // NOTE: end_elt is the upper exclusive bound
        
    // Iterate over the x-valeud-valences
    for (el1 = start_elt; el1 < end_elt; el1++)
    {
        buildValsInit(*myid);

        // Build (x,y) elts.
        build_pairs(*myid, el1);

        // Build (x.y) valences & output them to csv.
        buildValences(*myid, el1);
    }
    return (void *) NULL;
} // end partial_elements()


// Generate the valences.
int main(int argc, char **argv)
{
    // Set up logging.
    openlog("valgen", LOG_PID | LOG_NDELAY, LOG_LOCAL0);
    setlogmask(LOG_UPTO (LOG_INFO));

    printf("*** Starting valence generation...\n");
    syslog(LOG_INFO, "*** Beginning valgen's main:  Begin loading ratings data.");

    // Do some statistics sanity checking
    test_spearman();

    // Set up globals we'll need.
    load_config_file();

    // Use getopt to help manage the options on the command line.
    int opt;
    while ((opt = getopt(argc, argv, "r:")) != -1)
    {
        switch (opt)
        {
            case 'r':   // for "rating-buckets"
                if (atoi(optarg) < 2 || atoi(optarg) > 32)
                {
                    printf("Error: the argument for -r should be > 1 and < 33 instead of %s. Exiting. ***\n", optarg);
                    exit(EXIT_FAILURE);
                }
                g_ratings_scale = (uint8_t) atoi(optarg);
                printf("*** Starting valgen with ratings using a %d-bucket scale ***\n", g_ratings_scale);
                goto forreal;
            default:
                printf("Don't understand. Check args. \n");
                fprintf(stderr, "Usage: %s [-r ratingsbuckets]\n", argv[0]);
                exit(EXIT_FAILURE);
        } // end switch
    } // end while

    forreal:

    if (BE.num_elts == 0)
    {
        syslog(LOG_CRIT, "*** Exiting valgen because num_elts is a null pointer or 0.");
        exit(-1);
    }

    // Declare time vars.
    long long start, finish;

    syslog(LOG_INFO, "BE.num_elts is %lu", BE.num_elts);

    // Begin big_rat stuff: initialise the big_rat.
    big_rat_init();

    // Record the start time.
    start = current_time_millis();

    // This call loads up big_rat and big_rat_ds with ratings.
    big_rat_pull_from_flat_file();

    // This will dump the big_rat and big_rat_index in binary form for later reading by recgen.
    export_br();

    if (NULL == br || NULL == brds)
    {
        syslog(LOG_CRIT, "ERROR in main() where at least one of the big_rat structures wasn't allocated. Exiting.");
        exit(-1);
    }
    // Record the end time.
    finish = current_time_millis();
    syslog(LOG_INFO, "Time to do big_rat and big_rat_ds load: %d milliseconds", (int) (finish - start));
    // End big_rat stuff: initialise the big_rat.

    // NOTE: The rats in big_rat are sorted by userid then elt which is the required sorting for valgen.
    // The rats in big_rat_ds are sorted by elt then userid.
    // Ignore redundant because it won't matter too much.
    // Call buildVals which is entry into valence generation (Assume no intersection bits.)

    // We can't keep all pairs and valences in RAM simultaneously. So, valgen has now been refactored
    // from: allocate all mem for pairs & vals --> build all pairs --> build all valences --> print out valences
    // to: foreach x (allocate enough mem for (x,y) --> build (x,y) elts --> build (x.y) valences --> print out valences)

    // Allocate g_elements.
    // The +1 below is so we can address the index by the y value and basically ignore the 0th position.
    // NOTE: For multithreading, each thread needs its own g_pairs.
    int i, j;
    for (i = 0; i < NTHREADS; i++)
    {
        g_pairs[i] = (pair_t *) calloc((size_t) BE.num_elts + 1, sizeof(pair_t));

        if (g_pairs[i] == 0)
        {
            syslog(LOG_ERR, "ERROR: Out of memory when creating g_pairs[%d].", i);
            exit(1);
        }
    } // end for loop across number of threads

    // Check if output dir exists and if it doesn't, create it.
    if (!check_make_dir(BE.working_dir))
    {
        syslog(LOG_CRIT, "Can't create directory \"%s\" so exiting. More details might be in stderr.", BE.working_dir);
        exit(-1);
    }

    // Begin multithread support: each thread tackles a portion of the el1 values.
    pthread_t thread_id[NTHREADS];
    int thread_args[NTHREADS];
    
    // Calculate how many elts to work on in each thread.
    elts_per_thread = (uint32_t) BE.num_elts / NTHREADS;
    
    for(i = 0; i < NTHREADS; i++)
    {
        syslog(LOG_INFO,"starting thread %d (should be 0-7)", i);
        thread_args[i] = i;
        pthread_create( &thread_id[i], NULL, partial_elements, &thread_args[i]);
    }
    for(j = 0; j < NTHREADS; j++)
    {
        pthread_join(thread_id[j], NULL);
    }
    // End multithread stuff.
    
    // Clean up

    // Thread-specific cleanup
    char vlist[NTHREADS * 16] = "";
    char vname[16] = "";
    for (i = 0; i < NTHREADS; i++)
    {
        // Free the pairs structure we allocated.
        if (g_pairs[i]) free(g_pairs[i]);

        // Build the string for the big cat command.
        sprintf(vname, "%s%d ", "valences.out", i);
        strlcat(vlist, vname, sizeof(vlist));
    }

    // Need to cd to the dir, cat the files, then remove intermediate files.
    int status;
    char couple[4096];

    // Remove the intermediate files as well.
    sprintf(couple, "cd %s; cat %s> valences.out; rm %s", BE.working_dir, vlist, vlist);

    // Execute the cat command
    status = system(couple);
    if (status)
        syslog(LOG_ERR, "status from ---%s--- is %d", couple, status);

    // Create a small file with the num_confident_valences
    sprintf(couple, "cd %s; grep t valences.out | wc | cut -d ' ' -f 1 > num_confident_valences.out",
            BE.working_dir);

    // Execute the grep command
    status = system(couple);
    if (status)
        syslog(LOG_ERR, "status from ---%s--- is %d", couple, status);

    // Record the end time.
    finish = current_time_millis();
    printf("*** Done generating valences.\n");
    syslog(LOG_INFO, "*** Done generating valences! It took %d seconds.",(int) ((finish - start) / 1000) );

    return (0);
} // end main()
