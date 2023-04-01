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

int g_server_location = TEST_LOC_DEV;
static int g_ratings_scale = 5;
static rating_t *g_big_rat = NULL; // this is the valgen-outputted user ratings
static uint32_t *g_big_rat_index = NULL; // this is a person index into g_big_rat
static bool g_group_test = false;
static size_t g_num_testing_people = 0;
static bool g_test_event_call = false;

//
//
// This binary is an executable that does integration testing on bemorehuman.
//
//

// Create the temp file that holds the passed-in protobuffer.
static void make_tmpfile(char *buf, size_t len, char **pb_fname)
{
    // ok, write the protobuf to a file
    char proto_fname[128];

    // orig: sprintf(proto_fname, "/tmp/scenario_2.pb");
    strcpy(proto_fname, FILE_TEMPLATE);
    int fd = mkstemp(proto_fname);

    if (fd == -1)
    {
        perror("Error creating temporary file");
        exit(EXIT_FAILURE);
    }

    // Write content to the temporary file
    FILE *file = fdopen(fd, "w");
    if (file)
    {
        // Make sure to use fwrite and not fprintff with %s
        fwrite(buf, len, 1, file);
        fflush(file);
        fclose(file);
    }
    else
    {
        perror("Error opening temporary file for writing");
        exit(EXIT_FAILURE);
    }

    // Send back the proto_fname
    *pb_fname = malloc(strlen(proto_fname) + 1);
    strcpy(*pb_fname, proto_fname);
} // end make_tmpfile()


// Make the scenario 2 (recs) file and send back the pb_fname to the caller.
static void make_pb_scen_2_file(uint32_t userid, char **pb_fname)
{
    // Do some protobuf-based calling here (try writing the protobuf to a file)
    Recs message_out = RECS__INIT;
    void *buf;                    // Buffer to store serialized data
    size_t len;                    // Length of serialized data

    message_out.personid = userid;
    message_out.popularity = 7;

    // Finish constructing the protobuf message.
    len = recs__get_packed_size(&message_out); // This is calculated packing length
    buf = malloc (len);                      // Allocate required serialized buffer length
    recs__pack(&message_out, buf); // Pack the data

    // Create the tempfile.
    make_tmpfile(buf, len, pb_fname);

    // cleanup
    free(buf);
} // end scen 2 "recs"


static void make_pb_scen_3_file(uint32_t userid, uint32_t elementid, uint32_t eventid, char **pb_fname)
{
    // Do some protobuf-based calling here (try writing the protobuf to a file)
    Event message_out = EVENT__INIT;
    void *buf;                    // Buffer to store serialized data
    size_t len;                    // Length of serialized data

    message_out.personid = userid;
    message_out.elementid = elementid;
    message_out.eventval = eventid;  // just passing this along; could be 0

    // Finish constructing the protobuf message.
    len = event__get_packed_size(&message_out); // This is calculated packing length
    buf = malloc (len);                      // Allocate required serialized buffer length
    event__pack(&message_out, buf); // Pack the data

    // Create the tempfile.
    make_tmpfile(buf, len, pb_fname);

    // cleanup
    free(buf);
} // end scen 3 "event"


static void make_pb_scen_110_file(uint32_t userid, uint32_t elementid, char **pb_fname)
{
    // Do some protobuf-based calling here (try writing the protobuf to a file)
    InternalSingleRec message_out = INTERNAL_SINGLE_REC__INIT;
    void *buf;                    // Buffer to store serialized data
    size_t len;                    // Length of serialized data

    message_out.personid = (uint32_t) userid;
    message_out.elementid = (uint32_t ) elementid;

    // Finish constructing the protobuf message.
    len = internal_single_rec__get_packed_size(&message_out);
    buf = malloc(len);
    internal_single_rec__pack(&message_out, buf);

    // Create the tempfile.
    make_tmpfile(buf, len, pb_fname);

    // cleanup
    free(buf);
} // end scen 110 "dynamic scan"


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


// Test bemorehuman
void TestAccuracy(void)
{
    /*

     Plan:
     0) iterate over all users for whom we need to generate recs (for steps 1-6)
     1) get ratings
     2) (used to be: create new user but not needed atm)
     3) set aside some of the ratings for that user. We'll use these later for comparison
     4) call server with some /event calls to add some events, just to test /event call
     5) call bemorehuman server and populate MAX_PREDS... elt rec structure in mem with preds for that person
     6) compare what we set aside in 3) with what's in 5) and spit those results out
     7) collate & print results
     8) generate random preds to compare "random" with bemorehuman

     */

    size_t i;
    size_t userCounter = 0;
    long long start, finish, total_time = 0;

    // prepare to iterate over all users for whom we need to generate recs for (for steps 1-7)
    size_t numrows, num_held_back;
    unsigned int curelement_int;
    int userid, currat, held_back[4096];
    int ratings[MAX_PREDS_PER_PERSON], num_found = 0, total_held_back = 0;
    double diff_total;

    double squared, sum_squares = 0.0, rmse = 0.0, xobs = 0.0, nrmse;
    int num_sum_squares = 0;

    srand((unsigned int) time(NULL));
    unsigned int random;
    double random_avg = 0.0;
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
    size_t num_ratings_read, num_indices_read;
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
        char *pb_fname = NULL;
	
        //
        // Scenario: recs
        //
        make_pb_scen_2_file((uint32_t) userids[userCounter], (char **) &pb_fname);

        // need to clear the memory here for the pb_response, then pass in the pointer
        memset(raw_response, 0, RB_RAW_RESPONSE_SIZE_MAX);
        start = current_time_micros();
        len = call_bemorehuman_server(2, pb_fname, (char *) raw_response);
        finish = current_time_micros();
        total_time += (finish - start);
        printf("Time to get recs for user %d with %zu ratings is %lld micros.\n", userid, numrows, finish - start);
        unlink(pb_fname);
        free(pb_fname);
        if (0 == len)
        {
            printf("ERROR: length of response from recs call is 0. Check server logs.\n");
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
                send_counter++;
            }
        }
        printf ("num_held_back for this user is %zu\n", num_held_back);
        printf ("num sent as ratings is %d\n", send_counter);

        //
        // Scenario: /event
        //
        // 4) call server with some /event calls to add some events, just to test /event call

        if (g_test_event_call == true)
        {
            // We want to call /event num_held_back times just to test things out. No particular reason for this number.
            for (i = 0; i < num_held_back; i++)
            {
                // clear out raw_response
                memset(raw_response, 0, sizeof(raw_response));

                // NOTE: test-accuracy can't currently test for non-explicit events, so we must test for ratings.
                size_t made_up_rating = i % (size_t) g_ratings_scale;
                make_pb_scen_3_file((uint32_t) userids[userCounter],
                                    (uint32_t) i + 1,
                                    made_up_rating == 0 ? 1 : (unsigned int) made_up_rating,
                                    (char **) &pb_fname);

                start = current_time_micros();
                len = call_bemorehuman_server(3, pb_fname, (char *) raw_response);
                finish = current_time_micros();
                printf("Time to send event for user %d with %zu ratings is %lld micros.\n", userid, numrows, finish - start);
                unlink(pb_fname);
                free(pb_fname);
                if (0 == len)
                {
                    printf("ERROR: length of response from event call is 0. Check server logs.\n");
                    return;
                }

                EventResponse *message_in2;

                // Now we are ready to decode the message.
                message_in2 = event_response__unpack(NULL, len, raw_response);

                // Must check for NULL
                if (NULL == message_in2)
                {
                    printf("ERROR: len from event call is %lu\n", len);
                    printf("ERROR: response from event call is NULL. Exiting.\n");
                    exit(-1);
                }

                // Print the data contained in the message.
                printf("status from event response is ---%s---\n", message_in2->status);
                assert(! strcmp(message_in2->status, "ok"));
                printf("result from event response is ---%d---\n", message_in2->result);

                // protobuf cleanup
                event_response__free_unpacked(message_in2, NULL);
            } // end for loop over num_held_back
        } // end if we want to test the /event call

        /*
         * Scenario: dynamic scan
         */

        // 5) call bemorehuman server and populate MAX_PREDS... elt rec structure in mem with preds for that person
        // Dynamic scan, similar to dynamic rate. It's scan b/c we're sending one id to the server and saying "what about this element?"
        // It's called Dynamic b/c it changes for each film for which we want to get the pred.

        int recval;
        num_found = 0;
        diff_total = 0.0;

        // We need to call dynamic scan num_held_back times.
        for (i = 0; i < num_held_back; i++)
        {
            // clear out raw_response
            memset(raw_response, 0, sizeof(raw_response));

            make_pb_scen_110_file((uint32_t) userids[userCounter],
                                (uint32_t) i + 1,
                                (char **) &pb_fname);

            // call the server
            len = call_bemorehuman_server(DYNAMIC_SCAN, pb_fname, (char *) raw_response);

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

            unlink(pb_fname);
            free(pb_fname);

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
    } // end iterating over the users

    printf("\nTotal time is %lld micros (%lld millis) to generate a total of %lu strongest recs, or %lld micros per rec.\n",
           total_time,
           total_time / 1000,
           20 * g_num_testing_people,
           (unsigned long long) bmh_round(((double) total_time) / (double) (20 * g_num_testing_people)));

    printf("Overall timing for needle-in-haystack recs used to determine MAE was not calculated.\n");
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

    printf("\n**Across all %zu users we're evaluating, Mean Absolute Error (MAE) is %f, or %f percent for the ones we held back\n",
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
            if (strtol(optarg, NULL, 10) < 1)
            {
                printf("Error: the argument for -n should be > 0 instead of %s. Exiting. ***\n", optarg);
                exit(EXIT_FAILURE);
            }
            g_num_testing_people = (size_t) strtol(optarg, NULL, 10);
            printf("*** Starting test-accuracy with number of testing people: %zu ***\n", g_num_testing_people);
            break;
        case 'p':                              // for "prod"
            g_server_location = TEST_LOC_PROD;
            printf("*** Testing against prod... ***\n");
            break;
        case 'r':                              // for "rating-buckets"
            if (strtol(optarg, NULL, 10) < 2 || strtol(optarg, NULL, 10) > 32)
            {
                printf("Error: the argument for -r should be > 1 and < 33 instead of %s. Exiting. ***\n", optarg);
                exit(EXIT_FAILURE);
            }
            g_ratings_scale = (int) strtol(optarg, NULL, 10);
            printf("*** Starting test-accuracy with ratings using a %d-bucket scale ***\n", g_ratings_scale);
            break;
        case 's':                              // for "stage"
            g_server_location = TEST_LOC_STAGE;
            printf("*** Testing against stage... ***\n");
            break;
        case 't':                              // for "test /event call"
            g_test_event_call = true;
            printf("*** Testing the /event call. NOTE: This will send random ratings to the server. ***\n");
            break;
        default:
            printf("Don't understand. Check args. Need one or more of g, n, p, r, s, or t. \n");
            fprintf(stderr, "Usage: %s [-g | -n num_testing_people | -p | -r ratings_buckets | -s | -t]\n", argv[0]);
            exit(EXIT_FAILURE);
        } // end switch
    } // end while

    TestAccuracy();

    exit(EXIT_SUCCESS);

} // end main ()

