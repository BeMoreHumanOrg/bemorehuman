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

#include "accuracy.h"

int g_test_mode = TEST_MODE_CORE;
int g_server_location = TEST_LOC_DEV;
static int g_ratings_scale = 5;
static rating_t *g_big_rat = NULL; // this is the valgen-outputted user ratings
static uint32_t *g_big_rat_index = NULL; // this is a person index into g_big_rat
static bool g_group_test = false;
static size_t g_num_testing_people = 0;

/*
 
1.12.12 
This guy is an executable that does integration testing on bemorehuman.
 Specify on command line invocation which one to
 execute:
 
bemorehuman (core) testing:  ./test-accuracy --core
 
*/ 

/************ Testing stuff below ****************/

static void make_pb_scen_2_file(uint32_t userid)
{
    // Do some protobuf-based calling here (try writing the protobuf to a file)
    Recs message_out = RECS__INIT;
    void *buf;                    // Buffer to store serialized data
    size_t len;                    // Length of serialized data

    // todo need to put real personid here
    message_out.personid = userid;
    message_out.popularity = 7;

    // Finish constructing the protobuf message.
    len = recs__get_packed_size(&message_out); // This is calculated packing length
    buf = malloc (len);                      // Allocate required serialized buffer length
    recs__pack(&message_out, buf); // Pack the data

    // ok, write the protobuf to a file
    char proto_fname[128];
    sprintf(proto_fname, "./pbfiles/scenario_2.pb");

    FILE *proto_tmpfile = fopen(proto_fname, "w");
    assert(NULL != proto_tmpfile);

    // Make sure to use fwrite and not fprintff with %s
    fwrite(buf, len, 1, proto_tmpfile);
    fclose(proto_tmpfile);

    // cleanup
    free(buf);
}


// Provide a random integer in [0, limit)
unsigned int random_uint(unsigned int limit)
{
    union
    {
        unsigned int i;
        unsigned char c[sizeof(unsigned int)];
    } u;

    do
    {
        if (!RAND_bytes(u.c, sizeof(u.c)))
        {
            printf("Can't get random bytes from openssl. Exiting.\n");
            exit(1);
        }
    } while (u.i < (-limit % limit)); /* u.i < (2**size % limit) */
    return u.i % limit;
} // end random_uint()


// Test bemorehuman core
void TestAccuracy()
{
    /*

     11.02.13
     So, new plan:
     0) iterate over all users for whom we need to generate recs (for steps 1-7)
     1) get MLratings
     2) create new user via API call
     3) set aside half of the 5's for that user. We'll use these later for comparison
     4) call server with "rate" call for a new user
         - if we're testing the testing code, can call bemorehuman with my 66-elt reference. Make sure to do it only once.
     5) call bemorehumanapp server and populate MAX_PREDS... elt rec structure in mem with preds for that person
     6) compare what we set aside in 3) with what's in 5) and spit those results out
         - "for this user we are on average 1.2 away for the 5's we held back (and store 1.2 for later)
     7) delete the user we just created (prolly at db level)

     8) collate & print results
     9) generate random preds to compare "random" with bemorehuman

     */

    /*
     (OLD) Plan is to do the following:

     0) iterate over all users for whom we need to generate recs for (for steps 1-5)
     1) load ratings for a user from db table
     2) set aside half of the 5's for that user. We'll use these later for comparison
     3) call bemorehuman for that user (straight-up http call)
         - if we're testing the testing code, can call bemorehuman with my 66-elt reference. Make sure to do it only once.
     4) populate MAX_PREDS... elt rec structure in mem with preds for that person
     5) compare what we set aside in 2) with what's in 4) and spit those results out
         - "for this user we are on average 1.2 away for the 5's we held back (and store 1.2 for later)

     6) collate & print results
     7) generate random preds to compare "random" with bemorehuman

     */

    size_t i;

    size_t userCounter = 0;

    long long start, finish, total_time = 0;

    // prepare to iterate over all users for whom we need to generate recs for (for steps 1-7)

    size_t numrows = 0, num_held_back = 0;
    unsigned int curelement_int;
    int userid, currat, held_back[4096];
    int ratings[MAX_PREDS_PER_PERSON], num_found = 0, total_held_back = 0;
    double diff_total;
    char suffix[32];

    double squared, sum_squares = 0.0, rmse = 0.0, xobs = 0.0, nrmse = 0.0;
    int num_sum_squares = 0;

    srand((unsigned int) time(NULL));
    unsigned int random = 0;
    double random_avg = 0.0;


    // begin tests for 4 specific people
    // TODO: turn this functionality into a command-line option
    /* we know the fly buys numbers. Use them to get the personid from the person_ids table

    6014351063308314
    6014355727463688
    6014351000635811
    6014351020280416

     */
    // end tests for 4 specific people

    // Begin loading ratings.
    // Create the big_rat.
    g_big_rat = (rating_t *) calloc(BE.num_ratings, sizeof(rating_t));
    if (g_big_rat == 0)
    {
        printf("FATAL ERROR: Out of memory when creating g_big_rat.\n");
        exit(-1);
    }
    // Create the big_rat_index
    g_big_rat_index = (uint32_t *) calloc(BE.num_people + 1, sizeof(uint32_t));
    if (g_big_rat_index == 0)
    {
        printf("FATAL ERROR: Out of memory when creating g_big_rat_index.\n");
        exit(-1);
    }

    // Begin fread-ing the 2 memory structures.

    // Read the big_rat
    char file_to_open[strlen(BE.working_dir) + strlen(RATINGS_BR_INDEX) + 2];

    strlcpy(file_to_open, BE.working_dir, sizeof(file_to_open));
    strlcat(file_to_open, "/", sizeof(file_to_open));
    strlcat(file_to_open, RATINGS_BR, sizeof(file_to_open));
    FILE *rat_out = fopen(file_to_open,"r");
    size_t num_ratings_read = 0, num_indices_read = 0;
    assert(NULL != rat_out);

    num_ratings_read = fread(g_big_rat, sizeof(rating_t), BE.num_ratings, rat_out);
    printf("Number of ratings read from bin file: %lu and we expected %lu to be read.\n",
           num_ratings_read, (unsigned long) BE.num_ratings);
    fclose(rat_out);
    assert(num_ratings_read == BE.num_ratings);
    // end loading big_rat

    double floaty, floaty2;
    int num1 = 0, num2 = 0, num3 = 0, num4 = 0, num5 = 0, num6 = 0, num7 = 0,
            num8 = 0, num9 = 0, num10 = 0;

    // Do we need to adjust the ratings based on a user-specified scale?
    if (g_ratings_scale != 32)
    {
        for (i = 0; i < BE.num_ratings; i++)
        {
            floaty = 32.0 / (double) g_ratings_scale;
            floaty2 = (double) g_big_rat[i].rating / floaty;
            g_big_rat[i].rating = (short) bmh_round(floaty2);
            if (0 == g_big_rat[i].rating) g_big_rat[i].rating = 1;

            if (1 == g_big_rat[i].rating) num1++;
            else if (2 == g_big_rat[i].rating) num2++;
            else if (3 == g_big_rat[i].rating) num3++;
            else if (4 == g_big_rat[i].rating) num4++;
            else if (5 == g_big_rat[i].rating) num5++;
            else if (6 == g_big_rat[i].rating) num6++;
            else if (7 == g_big_rat[i].rating) num7++;
            else if (8 == g_big_rat[i].rating) num8++;
            else if (9 == g_big_rat[i].rating) num9++;
            else if (10 == g_big_rat[i].rating) num10++;
        }
    }


    // Read the big_rat_index
    strlcpy(file_to_open, BE.working_dir, sizeof(file_to_open));
    strlcat(file_to_open, "/", sizeof(file_to_open));
    strlcat(file_to_open, RATINGS_BR_INDEX, sizeof(file_to_open));
    rat_out = fopen(file_to_open,"r");
    assert(NULL != rat_out);

    num_indices_read = fread(g_big_rat_index, sizeof(uint32_t), BE.num_people + 1, rat_out);
    printf("Number of index locations read from bin file: %lu and we expected %lu to be read.\n",
           num_indices_read, BE.num_people + 1);
    fclose(rat_out);
    assert(num_indices_read == BE.num_people + 1);
    // end loading big_rat_index

    // At this point we can use g_big_rat_index for people id numbers. And we can use the g_big_rat to get ratings.
    int userids[g_num_testing_people];

    // Are we testing for a specific group of people?
    if (g_group_test)
    {
        g_num_testing_people = 4;
        userids[0] = 3071; userids[1] = 5218; userids[2] = 8099; userids[3] = 16986;
    }
    else  // We're testing for a bunch of randos.
    {
        for (i = 0; i < g_num_testing_people; i++)
        {
            random = random_uint((unsigned int) BE.num_people + 1);
            userids[i] = (int) random;
        }
    }

    double avg_diff[g_num_testing_people];
    int avg_diff_num_found[g_num_testing_people];

    // 0) iterate over the users
    for (; userCounter < g_num_testing_people; userCounter++)
    {
        userid = userids[userCounter];
        printf("userid is %d\n", userid);

        // Compute how much we have done and print it out.
        printf("*** %zu people have been processed out of %zu for this run. ***\n", userCounter, g_num_testing_people);
        printf("*** %f percent of people have been processed for this run. ***\n", floor(100 * (double) userCounter / (double) g_num_testing_people));

        num_held_back = 0;
        numrows = 0;
        for (i=0; i < num_ratings_read; i++)
            if (g_big_rat[i].userid == userid)
                numrows++;

        // Get the ratings for a user from ratings flat file.
        uint8_t raw_response[RB_RAW_RESPONSE_SIZE_MAX] = {0};
        size_t len;

        //
        // Scenario: recs
        //
        make_pb_scen_2_file((uint32_t) userids[userCounter]);

        // need to clear the memory here for the pb_response, then pass in the pointer
        memset(raw_response, 0, RB_RAW_RESPONSE_SIZE_MAX);
        start = current_time_micros();
        len = (size_t) call_bemorehuman_server(2, (char *) raw_response);
        finish = current_time_micros();
        total_time += (finish - start);
        printf("Time to get recs for user %d with %zu ratings is %lld micros.\n", userid, numrows, finish - start);
        if (0 == len)
        {
            printf("ERROR: length of response from above call is 0.\n");
            return;
        }
        
        RecsResponse *recs_response;

        // decode the message.
        recs_response = recs_response__unpack(NULL, len, raw_response);
        if (recs_response == NULL)
        {
            printf("ERROR: Decoding recs failed.\n");
            recs_response__free_unpacked(recs_response, NULL);
            return;
        }

        // Print the data contained in the message.
        printf("recslist count is ---%d---\n", (int) recs_response->n_recslist);
        // printf("status from recs_response is ---%s---\n", recs_response->status);
        for (i = 0; i < recs_response->n_recslist; i++)
        {
           printf("recslist at %zu element id is %d, rec value is %d, popularity is %d\n",
                   i,
                   recs_response->recslist[i]->elementid,
                   recs_response->recslist[i]->rating,
                   recs_response->recslist[i]->popularity);
        }
        recs_response__free_unpacked(recs_response, NULL);

        // begin pb-specific outputting of data to file
        char fname[128];

        printf ("numrows for this user is %zu\n", numrows);

        int send_counter = 0;

        // Iterate over the ratings for this user.
        // NOTE: old protobuf had a cap of 200
        size_t starter = g_big_rat_index[userid];
        for (i = 0; i < (numrows < 200 ? numrows : 200); i++)
        {
            curelement_int = g_big_rat[starter + i].elementid;
            currat = g_big_rat[starter + i].rating;

            // 3) set aside 33% of all ratings for that user. We'll use these later for comparison with the actual recs
            // Pick some to hold back. The 1 below is arbitrary.
            if (1 == (i % 3))
            {
                // store the currat for later
                ratings[num_held_back] = currat;
                held_back[num_held_back] = (int) curelement_int;
                num_held_back++;
                total_held_back++;
            }
            else
            {
                // add this (elt, rat) pair to the string we'll use to call bemorehuman
                // todo fill up protobuf request here?
                send_counter++;
            }
        }
        printf ("num_held_back for this user is %zu\n", num_held_back);
        printf ("num sent as ratings is %d\n", send_counter);

        /*
         * Scenario: dynamic scan
         */

        // 5) call bemorehumanapp server and populate MAX_PREDS... elt rec structure in mem with preds for that person
        // Dynamic scan, similar to dynamic rate. It's scan b/c we're sending one id to the server and saying "what about this film?"
        // It's called Dynamic b/c it changes for each film for which we want to get the pred.

        void *buf;                    // Buffer to store serialized data
        int recval;
        num_found = 0;
        diff_total = 0.0;

        // We need to call dynamic scan num_held_back times.
        for (i = 0; i < num_held_back; i++)
        {
            // build the outgoing call to bemorehuman
            InternalSingleRec message_out = INTERNAL_SINGLE_REC__INIT;
            message_out.personid = (uint32_t) userids[userCounter];

            // create the dynamic content to send to the server
            message_out.elementid = (uint32_t ) held_back[i];

            // begin pb-specific outputting of data to file
            len = internal_single_rec__get_packed_size(&message_out);
            buf = malloc(len);
            internal_single_rec__pack(&message_out, buf);
            // end pb-specific bit

            // put the pid in the filename b/c we're seeing collision when multiple test-accuracies are run. Duh.
            strcpy(suffix, "");

            // if we're on prod or stage, add a _prod
            if (TEST_LOC_DEV != g_server_location) strcpy(suffix, "_prod");

            sprintf(fname, "./scenario_110%s_%d.pb", suffix, getpid());

            FILE *tmpfile_scan = fopen(fname, "w");
            assert(NULL != tmpfile_scan);

            // make sure to use fwrite and not fprintff with %s
            fwrite(buf, len, 1, tmpfile_scan);
            fclose(tmpfile_scan);

            // free protobuf stuff
            free(buf);

            // clear out raw_response
            memset(raw_response, 0, sizeof(raw_response));

            // call the server
            len = (size_t) call_bemorehuman_server(DYNAMIC_SCAN, (char *) raw_response);

            InternalSingleRecResponse *message_in2;

            // Now we are ready to decode the message.
            message_in2 = internal_single_rec_response__unpack(NULL, len, raw_response);

            // Print the data contained in the message.
            // printf("status from internal single rec response is ---%s---\n", message_in2->status);
            assert(! strcmp(message_in2->status, "ok"));
            // printf("result from internal single rec response is ---%d---\n", message_in2->result);

            // the response has the rec value in the "result" line
            // load up the structure with the recs

            recval = message_in2->result;
            printf("For user %d holding back eltid %6d, rating %3d, recval of %3d\n",
                   userid, held_back[i], ratings[i], recval);

            // add an if check here to see if bemorehuman said anything about this one. 0 or -1 maybe? Dunno.
            if (recval > 0)
            {
                num_found++;
                diff_total += abs( ratings[i] - recval );

                // Make a random number btween 0 and 4 then add 1
                random = random_uint((unsigned int) g_ratings_scale) + 1;

                random_avg += abs(ratings[i] - (int) random);

                squared = pow((ratings[i] - recval), 2)  ;
                sum_squares += squared;
                num_sum_squares++;

                // nrmse (also called coefficient of variation)
                xobs += ratings[i];
            }
            // protobuf cleanup
            internal_single_rec_response__free_unpacked(message_in2, NULL);

            // unlink fname;
            unlink(fname);

        } // end for loop over num_held_back

        // 6) compare what we set aside in 3) with what's in 5) and spit those results out
        //     - "for this user we are on average 1.2 away for the 5's we held back (and store 1.2 for later)

        // what's the difference between num_5_found and num_held_back? this will tell us how many we found in the top max_preds_per_person
        printf("^^^ The diff btwn num_held_back and num_found is %zu - %d = %lu\n", num_held_back, num_found, num_held_back - (unsigned long) num_found);

        // print some summary info
        avg_diff[userCounter] = diff_total / num_found;
        avg_diff_num_found[userCounter] = num_found;
        printf("For the user %d we are on average %f away for the elts we held back\n", userid, avg_diff[userCounter]);

        // if no elts found, mark it for later removal from calculations.
        if (0 == num_found)
        {
            avg_diff[userCounter] = 999;
        }
        //         7) delete the user we just created (prolly at db level)??
    } // end iterating over the users

    printf("\nTotal time is %lld micros (%lld millis) to generate recs for %zu people, or %lld millis per person.\n",
           total_time, total_time / 1000, g_num_testing_people,
           (unsigned long long) bmh_round(((double) total_time / 1000) / (double) g_num_testing_people));

    // 8) collate & print results: specifically, overall average distance from 5 for all the users we're looking at
    double overall_avg = 0.0;
    int sum_num_found = 0;
    for (i = 0; i < g_num_testing_people; i++)
    {
        if (avg_diff[i] < 999)
        {
            // We need to not take simple average but rather the weighted average
            overall_avg += avg_diff[i] * avg_diff_num_found[i];
            sum_num_found += avg_diff_num_found[i];
        }
        else
        {
            // bad data! no elts found for this person
            g_num_testing_people--;
        }
    }

    overall_avg = overall_avg / (double) sum_num_found;

    printf("\n**Across all %zu users we're evaluating, Mean Absoluete Error (MAE) is %f, or %f percent for the ones we held back\n",
           g_num_testing_people, overall_avg, (double) 100 * overall_avg / g_ratings_scale);

    random_avg = random_avg / total_held_back;
    printf("Across all %zu users we're evaluating, random recs have MAE of %f.\n", g_num_testing_people, random_avg);


    // compute the rmse!
     if (0 != num_sum_squares)
         rmse = sum_squares / num_sum_squares;
     rmse = sqrt(rmse);

    // compute the nrmse, normalized to the mean of the observed data
     xobs = xobs / num_sum_squares;
     nrmse = rmse / xobs;

    printf ("Overall RMSE for this run is %f\n", rmse);
    printf ("Total held back: %d\n", total_held_back);
    printf ("Num_found is: %d\n", num_found);
    printf ("Overall NRMSE for this run is %f\n", nrmse);
    printf ("g_num_testing_people is: %zu\n\n", g_num_testing_people);

    if (5 == g_ratings_scale)
        printf("num1 is %d, num2 is %d, num3 is %d, num4 is %d, num5 is %d\n", num1, num2, num3, num4, num5);
    else if (10 == g_ratings_scale)
        printf("num1 is %d, num2 is %d, num3 is %d, num4 is %d, num5 is %d num6 is %d, num7 is %d, num8 is %d, num9 is %d, num10 is %d\n",
               num1, num2, num3, num4, num5, num6, num7, num8, num9, num10);

} // end testAccuracy()


int main(int argc, char **argv)
{
    // NOTE that this client, test-accuracy, is assumed to be invoked on dev, hitting against server process on dev (default)
    // or optionally on stage or production as below.

    // Load the config file.
    load_config_file();

    // Use getopt to help manage the options on the command line.
    int opt;
    while ((opt = getopt(argc, argv, "gn:pr:s")) != -1)
    {
        switch (opt)
        {
        case 'g':                              // for "group test"
            g_group_test = true;
            printf("*** Testing specific hard-coded group... ***\n");
            break;
        case 'n':                              // for "number of users"
            if (atoi(optarg) < 1)
            {
                printf("Error: the argument for -n should be > 0 instead of %s. Exiting. ***\n", optarg);
                exit(EXIT_FAILURE);
            }
            g_num_testing_people = (size_t) atoi(optarg);
            printf("*** Starting test-accuracy with number of testing people: %zu ***\n", g_num_testing_people);
            break;
        case 'p':                              // for "prod"
            g_server_location = TEST_LOC_PROD;
            printf("*** Testing against prod... ***\n");
            break;
        case 'r':                              // for "rating-buckets"
            if (atoi(optarg) < 2 || atoi(optarg) > 32)
            {
                printf("Error: the argument for -r should be > 1 and < 33 instead of %s. Exiting. ***\n", optarg);
                exit(EXIT_FAILURE);
            }
            g_ratings_scale = atoi(optarg);
            printf("*** Starting test-accuracy with ratings using a %d-bucket scale ***\n", g_ratings_scale);
            break;
        case 's':                              // for "stage"
            g_server_location = TEST_LOC_STAGE;
            printf("*** Testing against stage... ***\n");
            break;
        default:
            printf("Don't understand. Check args. Need one or more of g, n, p, r, or s. \n");
            fprintf(stderr, "Usage: %s [-g | -n num_testing_people | -p | -r ratings_buckets | -s]\n", argv[0]);
            exit(EXIT_FAILURE);
        } // end switch
    } // end while

    TestAccuracy();

    exit(EXIT_SUCCESS);

} // end main ()

