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
static void make_tmpfile(const char *buf, size_t len, char **pb_fname)
{
    // ok, write the protobuf to a file
    char proto_fname[128];

    // orig: sprintf(proto_fname, "/tmp/scenario_2.pb");
    strcpy(proto_fname, FILE_TEMPLATE);
    const int fd = mkstemp(proto_fname);

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
    } else
    {
        perror("Error opening temporary file for writing");
        exit(EXIT_FAILURE);
    }

    // Send back the proto_fname
    *pb_fname = malloc(strlen(proto_fname) + 1);
    strcpy(*pb_fname, proto_fname);
} // end make_tmpfile()


// Take in scenario, recs_request_t *, and return json fname
static void json_serialize(const int scenario, const void *data, char **fname)
{
    char *json = NULL; // Buffer to store serialized data
    size_t len; // Length of serialized data
    switch (scenario)
    {
        case SCENARIO_RECS:
        {
            const recs_request_t *rr = (const recs_request_t *) data;
            // 32 non-data bytes/rating_item_t roughly
            json = (char *) malloc(32 * (unsigned) rr->num_ratings * sizeof(rating_item_t) + 400);

            strcpy(json, "{");
            if (data)
            {
                // add personid
                strcat(json, "\"personid\":");
                char char_int[12];
                itoa((signed) rr->personid, char_int);
                strcat(json, char_int);

                // add popularity
                strcat(json, ",\"popularity\":");
                itoa(rr->popularity, char_int);
                strcat(json, char_int);

                // add ratingslist array of objects
                strcat(json, ",\"ratingslist\":[");
                for (int i = 0; i < rr->num_ratings; i++)
                {
                    strcat(json, "{\"elementid\":");
                    itoa((int) rr->ratings_list[i].elementid, char_int);
                    strcat(json, char_int);
                    strcat(json, ",\"rating\":");
                    itoa((int) rr->ratings_list[i].rating, char_int);
                    strcat(json, char_int);
                    strcat(json, "}");

                    if (i < (rr->num_ratings - 1))
                    {
                        // add comma delimiter
                        strcat(json, ",");
                    }
                } // end for loop across the recs to send
                strcat(json, "]");
            } // end if we have any recs to make

            strcat(json, "}");
            // if (json)
            //    printf("JSON out test: ---%s---\n", json); // {"name":"Mash","star":4,"hits":[2,2,1,3]}
            len = strlen(json);
            break;
        }
        case SCENARIO_SINGLEREC:
        {
            const recs_request_t *rr = (const recs_request_t *) data;
            json = (char *) malloc(sizeof(rating_item_t) + 400);

            strcpy(json, "{");
            if (data)
            {
                // add personid
                strcat(json, "\"personid\":");
                char char_int[12];
                itoa((signed) rr->personid, char_int);
                strcat(json, char_int);

                // add eltid
                strcat(json, ",\"elementid\":");
                itoa((signed) rr->ratings_list->elementid, char_int);
                strcat(json, char_int);
            } // end if we have any ratings to send

            strcat(json, "}");
            // if (json)
            //    printf("JSON out test: ---%s---\n", json); // {"name":"Mash","star":4,"hits":[2,2,1,3]}
            len = strlen(json);
            break;
        }
        case SCENARIO_EVENT:
        {
            const event_t *er = data;
            json = (char *) malloc(sizeof(event_t) + 400);

            strcpy(json, "{");
            if (data)
            {
                // add personid
                strcat(json, "\"personid\":");
                char char_int[12];
                itoa((signed) er->personid, char_int);
                strcat(json, char_int);

                // add eltid
                strcat(json, ",\"elementid\":");
                itoa((signed) er->elementid, char_int);
                strcat(json, char_int);

                // add eventval
                strcat(json, ",\"eventval\":");
                itoa((signed) er->eventval, char_int);
                strcat(json, char_int);
            } // end if we have any event to send

            strcat(json, "}");
            // if (json)
            //    printf("JSON out test: ---%s---\n", json); // {"name":"Mash","star":4,"hits":[2,2,1,3]}
            len = strlen(json);
            break;
        }
        default: return;
    } // end switch across request types

    // Create the tempfile.
    make_tmpfile(json, len, fname);

    // cleanup
    if (json) free(json);
} // end json_serialize()


// Take in POST JSON data and return a prediction_t *, status returned in status param
static void *json_deserialize(const int scenario, const void *data, char **status, const size_t len)
{
    prediction_t *predictions = NULL;
    switch (scenario)
    {
        case SCENARIO_RECS:
        {
            // Read JSON and get root
            yyjson_doc *doc = yyjson_read(data, len, 0);
            yyjson_val *root = yyjson_doc_get_root(doc);

            // what is in data? recslist
            yyjson_val *recslist = yyjson_obj_get(root, "recslist");
            size_t idx, max;

            // Returns the number of key-value pairs in this object.
            // Returns 0 if input is not an object.
            const int num_recs = (int) yyjson_arr_size(recslist);

            yyjson_val *element;

            // malloc a continuous block for all the predictions
            predictions = malloc((unsigned) num_recs * sizeof(prediction_t));

            // We have an array of objects
            yyjson_arr_foreach(recslist, idx, max, element)
            {
                size_t idx2, max2;
                yyjson_val *key, *val;
                yyjson_obj_foreach(element, idx2, max2, key, val)
                {
                    // assign the key, value into our return structure
                    // there's a new prediction now, check the key.
                    const char *key_str = yyjson_get_str(key);
                    if (key_str[0] == 'b' || key_str[0] == 'B') // bmhid (elementid)
                    {
                        predictions[idx].elementid = (uint32_t) yyjson_get_uint(val);
                    } else if (key_str[0] == 'r' || key_str[0] == 'R') // recvalue (rating)
                    {
                        predictions[idx].rating = yyjson_get_int(val);
                    } else
                    {
                        // popularity
                        predictions[idx].rating_count = yyjson_get_int(val); // NOTE: overload
                    }
                } // end iterating over each part of prediction
            } // end iterating over each element in array

            // Get root["status"]
            yyjson_val *json_status = yyjson_obj_get(root, "status");
            *status = malloc(strlen(yyjson_get_str(json_status)) + 1);
            strcpy(*status, yyjson_get_str(json_status));

            // Free the doc
            yyjson_doc_free(doc);
            break;
        } // end case SCENARIO_RECS
        case SCENARIO_SINGLEREC:
        {
            // Read JSON and get root
            yyjson_doc *doc = yyjson_read(data, len, 0);
            yyjson_val *root = yyjson_doc_get_root(doc);

            // malloc a prediction to hold the result
            predictions = malloc(sizeof(prediction_t));
            yyjson_val *res = yyjson_obj_get(root, "result");
            predictions->rating = yyjson_get_int(res);

            // Get root["status"]
            yyjson_val *json_status = yyjson_obj_get(root, "status");
            *status = malloc(strlen(yyjson_get_str(json_status)) + 1);
            strcpy(*status, yyjson_get_str(json_status));

            // Free the doc
            yyjson_doc_free(doc);
            break;
        } // end case SCENARIO_SINGLEREC
        case SCENARIO_EVENT:
        {
            // Read JSON and get root
            yyjson_doc *doc = yyjson_read(data, len, 0);
            yyjson_val *root = yyjson_doc_get_root(doc);

            // malloc a status to hold the result
            // Get root["status"]
            yyjson_val *json_status = yyjson_obj_get(root, "status");
            *status = malloc(strlen(yyjson_get_str(json_status)) + 1);
            strcpy(*status, yyjson_get_str(json_status));

            // Free the doc
            yyjson_doc_free(doc);
            break;
        } // end case SCENARIO_EVENT
        default: return NULL;
    }
    return predictions;
} // end json_deserialize()


#ifdef USE_PROTOBUF
// Make the scenario file and send back the pb_fname to the caller.
static void protobuf_serialize(const int scenario, const void *data, char **fname)
{
    void *buf; // Buffer to store serialized data
    size_t len; // Length of serialized data
    switch (scenario)
    {
        case SCENARIO_RECS:
        {
            // Do some protobuf-based calling here (try writing the protobuf to a file)
            const recs_request_t *rr = data;

            Recs recs_out = RECS__INIT;
            recs_out.personid = rr->personid;
            recs_out.popularity = rr->popularity;

            // Finish constructing the protobuf message.
            len = recs__get_packed_size(&recs_out); // This is calculated packing length
            buf = malloc(len); // Allocate required serialized buffer length
            recs__pack(&recs_out, buf); // Pack the data
            break;
        }

        case SCENARIO_EVENT:
        {
            // Do some protobuf-based calling here (try writing the protobuf to a file)
            const event_t *er = data;

            Event event_out = EVENT__INIT;
            event_out.personid = er->personid;
            event_out.elementid = er->elementid;
            event_out.eventval = er->eventval; // just passing this along; could be 0

            // Finish constructing the protobuf message.
            len = event__get_packed_size(&event_out); // This is calculated packing length
            buf = malloc(len); // Allocate required serialized buffer length
            event__pack(&event_out, buf); // Pack the data
            break;
        }

        case SCENARIO_SINGLEREC:
        {
            // Do some protobuf-based calling here (try writing the protobuf to a file)
            // The single rec request is just user/eltid which is in a rating_t
            const recs_request_t *rr = (const recs_request_t *) data;
            InternalSingleRec singlerec_out = INTERNAL_SINGLE_REC__INIT;
            singlerec_out.personid = (uint32_t) rr->personid;
            singlerec_out.elementid = (uint32_t) rr->ratings_list->elementid;

            // Finish constructing the protobuf message.
            len = internal_single_rec__get_packed_size(&singlerec_out);
            buf = malloc(len);
            internal_single_rec__pack(&singlerec_out, buf);
            break;
        }
        default: return;
    } // end switch across request types

    // Create the tempfile.
    make_tmpfile(buf, len, fname);

    // cleanup
    free(buf);

    return;
} // end protobuf_serialize()


// Take in protobuf message in data, len of msg, and return prediction_t*, status string
static void *protobuf_deserialize(int scenario, const void *data, char **status, const size_t len)
{
    switch (scenario)
    {
        case SCENARIO_RECS:
        {
            // decode the message.
            RecsResponse *recs_response = recs_response__unpack(NULL, len, data);
            if (recs_response == NULL)
            {
                printf("ERROR: Decoding recs has just now failed. :( \n");
                recs_response__free_unpacked(recs_response, NULL);
                return NULL;
            }

            const size_t status_len = strlen(recs_response->status);
            assert(status_len > 0 && status_len < 1024);
            *status = malloc(status_len + 1);

            strcpy(*status, recs_response->status);

            // Print the data contained in the message.
            printf("recslist count is ---%d---\n", (int) recs_response->n_recslist);
            // printf("status from recs_response is ---%s---\n", recs_response->status);
            for (size_t i = 0; i < recs_response->n_recslist; i++)
            {
                printf("recslist at %zu element id is %d, rec value is %d, popularity is %d\n",
                       i,
                       recs_response->recslist[i]->elementid,
                       recs_response->recslist[i]->rating,
                       recs_response->recslist[i]->popularity);
            }
            recs_response__free_unpacked(recs_response, NULL);
            return NULL;
        }

        case SCENARIO_SINGLEREC:
        {
            // Now we are ready to decode the message.
            InternalSingleRecResponse *message_in2 = internal_single_rec_response__unpack(NULL, len, data);

            // Print the data contained in the message.
            // printf("status from internal single rec response is ---%s---\n", message_in2->status);
            assert(! strcmp(message_in2->status, "ok"));
            // the response has the rec value in the "result" line
            // load up the structure with the recs

            prediction_t *message_out = malloc(sizeof(prediction_t));
            message_out->rating = message_in2->result;

            // protobuf cleanup
            internal_single_rec_response__free_unpacked(message_in2, NULL);
            return message_out;
        }

        case SCENARIO_EVENT:
        {
            // Now we are ready to decode the message.
            EventResponse *message_in2 = event_response__unpack(NULL, len, data);

            const size_t status_len = strlen(message_in2->status);
            assert(status_len > 0 && status_len < 1024);
            *status = malloc(status_len + 1);
            strcpy(*status, message_in2->status);

             // protobuf cleanup
            event_response__free_unpacked(message_in2, NULL);
            return NULL;
        }

        default: return NULL;
    } // end switch across scenario
} // end protobuf_deserialize()
#endif

static protocol_interface json_protocol =
{
    .serialize = json_serialize,
    .deserialize = json_deserialize,
};

#ifdef USE_PROTOBUF
static protocol_interface protobuf_protocol =
{
    .serialize = protobuf_serialize,
    .deserialize = protobuf_deserialize,
};
#endif

static protocol_interface *protocol = &json_protocol;

// Provide a random integer in [0, limit)
unsigned int random_uint(unsigned int limit)
{
    union
    {
        unsigned int i;
        unsigned char c[sizeof(unsigned int)];
    } u;

    u.i = 0;

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
    int userid, currat, held_back[4096], elts_to_send[4096], rats_to_send[4096];
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
    FILE *rat_out = fopen(file_to_open, "r");
    size_t num_ratings_read, num_indices_read;
    assert(NULL != rat_out);

    num_ratings_read = fread(g_big_rat, sizeof(rating_t), BE.num_ratings, rat_out);
    printf("Number of ratings read from bin file: %lu and we expected %lu to be read.\n",
           num_ratings_read, BE.num_ratings);
    fclose(rat_out);
    assert(num_ratings_read == BE.num_ratings);
    // end loading big_rat

    double floaty = 32.0 / (double) g_ratings_scale;
    int num1 = 0, num2 = 0, num3 = 0, num4 = 0, num5 = 0, num6 = 0, num7 = 0,
            num8 = 0, num9 = 0, num10 = 0;

    // Do we need to adjust the ratings based on a user-specified scale?
    if (g_ratings_scale != 32)
    {
        /*
        // test the ratings by outputting to test file
        char proto_fname[128];
        strcpy(proto_fname, FILE_TEMPLATE);
        const int fd = mkstemp(proto_fname);
        if (fd == -1)
        {
            perror("Error creating temporary file");
            exit(EXIT_FAILURE);
        }

        FILE *file = fdopen(fd, "w");
        if (!file)
        {
            perror("Error opening temporary ratings file for writing");
            exit(EXIT_FAILURE);
        }
        */
        for (i = 0; i < BE.num_ratings; i++)
        {
            // Ratings in bin file are always 32-based so need to convert to natural scale
            double floaty2 = (double) g_big_rat[i].rating / floaty;
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
            /*
            // Write content to the temporary file
            // Make sure to use fwrite and not fprintff with %s
            char buf[64];
            sprintf(buf, "%d\t%d\t%d\n", g_big_rat[i].userid, g_big_rat[i].elementid, g_big_rat[i].rating);
            size_t len = strlen(buf);
            fwrite(buf, len, 1, file);
            */
        } // end for across all ratings
        /*
        // file cleanup
        fflush(file);
        fclose(file);
        */
    } // end if we're not in 32-scale

    // Read the big_rat_index
    strlcpy(file_to_open, BE.working_dir, sizeof(file_to_open));
    strlcat(file_to_open, "/", sizeof(file_to_open));
    strlcat(file_to_open, RATINGS_BR_INDEX, sizeof(file_to_open));
    rat_out = fopen(file_to_open, "r");
    assert(NULL != rat_out);

    num_indices_read = fread(g_big_rat_index, sizeof(uint32_t), BE.num_people + 1, rat_out);
    printf("Number of index locations read from bin file: %lu and we expected %lu to be read.\n",
           num_indices_read, BE.num_people + 1);
    fclose(rat_out);
    assert(num_indices_read == BE.num_people + 1);
    // end loading big_rat_index

    // begin loading of elements.out which is file of the form: elementid,elementname  (on each line)

    // plan is to malloc the size of the band name when filling array
    char * elements[BE.num_elts + 1];  // + 1 b/c id's start at 1

    // Read the elements file
    strlcpy(file_to_open, BE.working_dir, sizeof(file_to_open));
    strlcat(file_to_open, "/", sizeof(file_to_open));
    strlcat(file_to_open, "elements.out", sizeof(file_to_open));
    FILE *elts_file = fopen(file_to_open, "r");
    assert(NULL != elts_file);

    int eltid;

    // can also use strrchr instead of strtok
    char line[256];
    i = 1;
    while (fgets(line, sizeof(line), elts_file) != 0)
    {
        // errything before the ',' is the eltid
        // errything after the ',' is the band name
        char *token;
        char *search = ",\r\n";

        token = strtok(line, search);
        eltid = atoi(token);

        token = strtok(NULL, search);
        if (NULL != token)
        {
            elements[eltid] = (char *) calloc(strlen(token) + 1, 1); // need to delete \n
            strncat(elements[eltid], token, strlen(token));
        }
        else
            elements[eltid] = 0;
        printf("i: %zu, eltid: %d, elements[eltid] is ---%s---\n", i, eltid, elements[eltid]);
        i++;
    } // end while fgets'ing lines from elements file

    fclose(elts_file);
    // end loading of elements.out which is file of the form: elementid,elementname  (on each line)

    // At this point we can use g_big_rat_index for people id numbers. And we can use the g_big_rat to get ratings.
    int userids[g_num_testing_people];

    // Are we testing for a specific group of people?
    if (g_group_test)
    {
        g_num_testing_people = 4;
        userids[0] = 3071;
        userids[1] = 5218;
        userids[2] = 8099;
        userids[3] = 16986;
    } else // We're testing for a bunch of randos.
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
        printf("*** %zu people werw processed out of %zu for this run. ***\n", userCounter, g_num_testing_people);
        printf("*** %f percent of people have been processed for this run. ***\n",
               floor(100 * (double) userCounter / (double) g_num_testing_people));

        num_held_back = 0;
        numrows = 0;
        for (i = 0; i < num_ratings_read; i++)
            if (g_big_rat[i].userid == userid)
                numrows++;

        // Begin preparing recs request.
        uint8_t raw_response[RB_RAW_RESPONSE_SIZE_MAX] = {0};
        size_t len;
        char *pb_fname = NULL;

        recs_request_t rr;
        // If protocol is protobuf, only need to send personid b/c recgen is aware of all ratings
        // If protocol is JSON, need to send ratings b/c recgen won't know who this is
        // and will need to send ratingslist array with elementid,rating.

        rr.personid = (uint32_t) userids[userCounter];
        rr.popularity = HIGHEST_POP_NUMBER;

        // For json, send errything we don't hold back. We'll need to send these later for the singlerec calls as well.

        // begin holding back some
        size_t send_counter = 0;

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
            } else
            {
                // populate array with the elts to send
                elts_to_send[send_counter] = (signed) curelement_int;
                rats_to_send[send_counter] = currat;
                send_counter++;
            }
        }
        printf("num_held_back for this user is %zu\n", num_held_back);
        printf("num sent as ratings is %lu\n", send_counter);

        // allocate & populate stucture to send
        rating_item_t *ri = malloc((unsigned) send_counter * sizeof(rating_item_t));
        for (i = 0; i < send_counter; i++)
        {
            ri[i].elementid = (uint32_t) elts_to_send[i];
            ri[i].rating = rats_to_send[i];
        }
        rr.ratings_list = ri;
        rr.num_ratings = (int) send_counter;
        // end holding back some

        //
        // Scenario: recs
        //
        protocol->serialize(SCENARIO_RECS, &rr, (char **) &pb_fname);

        // need to clear the memory here for the pb_response, then pass in the pointer
        memset(raw_response, 0, RB_RAW_RESPONSE_SIZE_MAX);
        start = current_time_micros();

        // this is a send/receive part
        if (protocol == &json_protocol)
            len = call_bemorehuman_server(PROTOCOL_JSON, SCENARIO_RECS, pb_fname, (char *) raw_response);
        else
            len = call_bemorehuman_server(PROTOCOL_PROTOBUF, SCENARIO_RECS, pb_fname, (char *) raw_response);
        finish = current_time_micros();
        total_time += (finish - start);
        printf("Time to get recs for user %d with %zu ratings is %lld micros.\n", userid, numrows, finish - start);
        unlink(pb_fname);
        free(pb_fname);
        free(ri);

        if (0 == len)
        {
            printf("ERROR: length of response from recs call is 0. Check server logs.\n");
            return;
        }

        // this is a deserialize part
        char *status = NULL;
        protocol->deserialize(SCENARIO_RECS, raw_response, &status, len);

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
                event_t er;
                er.elementid = (uint32_t) i + 1;
                er.eventval = made_up_rating == 0 ? 1 : (unsigned int) made_up_rating;
                er.personid = (uint32_t) userids[userCounter];

                protocol->serialize(SCENARIO_EVENT, &er, &pb_fname);

                // call the server
                start = current_time_micros();
                if (protocol == &json_protocol)
                    len = call_bemorehuman_server(PROTOCOL_JSON, SCENARIO_EVENT, pb_fname, (char *) raw_response);
                else
                    len = call_bemorehuman_server(PROTOCOL_PROTOBUF, SCENARIO_EVENT, pb_fname, (char *) raw_response);

                finish = current_time_micros();
                printf("Time to send event for user %d with %zu ratings is %lld micros.\n", userid, numrows, finish - start);
                unlink(pb_fname);
                free(pb_fname);
                if (0 == len)
                {
                    printf("ERROR: length of response from event call is 0. Check server logs.\n");
                    return;
                }

                protocol->deserialize(SCENARIO_EVENT, raw_response, &status, len);

                // Must check for NULL
                if (NULL == status)
                {
                    printf("ERROR: deserialized status response from event call is NULL. Exiting.\n");
                    exit(-1);
                }

                // Print the data contained in the message.
                printf("status from event response is ---%s---\n", status);
                assert(! strcmp(status, "ok"));
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
        prediction_t *deserialized_data;

        // We need to call dynamic scan num_held_back times.
        for (i = 0; i < num_held_back; i++)
        {
            // clear out raw_response
            memset(raw_response, 0, sizeof(raw_response));

            rating_item_t *one_ri = malloc(sizeof(rating_item_t));
            one_ri->elementid = (uint32_t) held_back[i];
            // wrong one_ri->elementid = (uint32_t) i + 1;

            rr.personid = (uint32_t) userids[userCounter];
            rr.ratings_list = one_ri;

            protocol->serialize(SCENARIO_SINGLEREC, &rr, (char **) &pb_fname);

            // call the server
            if (protocol == &json_protocol)
                len = call_bemorehuman_server(PROTOCOL_JSON, SCENARIO_SINGLEREC, pb_fname, (char *) raw_response);
            else
                len = call_bemorehuman_server(PROTOCOL_PROTOBUF, SCENARIO_SINGLEREC, pb_fname, (char *) raw_response);

            unlink(pb_fname);
            free(pb_fname);

            recval = 0;
            deserialized_data = NULL;
            deserialized_data = protocol->deserialize(SCENARIO_SINGLEREC, raw_response, &status, len);

            // unpack deserialized_data to get the recval
            if (deserialized_data) recval = deserialized_data->rating;

            printf("For user %d holding back elt \"%-32s\", id %6d, rating %3d, recval %3d, delta %3d\n",
                   userid, elements[held_back[i]], held_back[i], ratings[i], recval, abs(recval - ratings[i]));

            // add an if check here to see if bemorehuman said anything about this one. 0 or -1 maybe? Dunno.
            if (recval > 0)
            {
                num_found++;
                diff_total += abs(ratings[i] - recval);

                // Make a random number
                random = random_uint((unsigned) g_ratings_scale) + 1;

                random_avg += abs(ratings[i] - (int) random);

                squared = pow((ratings[i] - recval), 2);
                sum_squares += squared;
                num_sum_squares++;

                // nrmse (also called coefficient of variation)
                xobs += ratings[i];
            }
        } // end for loop over num_held_back

        // 6) compare what we set aside in 3) with what's in 5) and spit those results out
        //     - "for this user we are on average 1.2 away for the 5's we held back (and store 1.2 for later)

        // what's the difference between num_5_found and num_held_back? this will tell us how many we found in the top max_preds_per_person
        printf("^^^ The diff btwn num_held_back and num_found is %zu - %d = %lu\n", num_held_back, num_found,
               num_held_back - (unsigned long) num_found);

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

    printf(
        "\nTotal time is %lld micros (%lld millis) to generate a total of %lu strongest recs, or %lld micros per rec.\n",
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
        } else
        {
            // bad data! no elts found for this person
            g_num_testing_people--;
        }
    }

    overall_avg = overall_avg / (double) sum_num_found;

    printf(
        "\n**Across all %zu users we're evaluating, Mean Absolute Error (MAE) is %f, or %f percent for the ones we held back\n",
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

    printf("Overall RMSE for this run is %f\n", rmse);
    printf("Total held back: %d\n", total_held_back);
    printf("Num_found is: %d\n", num_found);
    printf("Overall NRMSE for this run is %f\n", nrmse);
    printf("g_num_testing_people is: %zu\n\n", g_num_testing_people);

    if (5 == g_ratings_scale)
        printf("num1 is %d, num2 is %d, num3 is %d, num4 is %d, num5 is %d\n", num1, num2, num3, num4, num5);
    else if (10 == g_ratings_scale)
        printf(
            "num1: %d, num2: %d, num3: %d, num4: %d, num5: %d, num6: %d, num7: %d, num8: %d, num9: %d, num10: %d\n",
            num1, num2, num3, num4, num5, num6, num7, num8, num9, num10);
} // end testAccuracy()


int main(int argc, char **argv)
{
    // NOTE that this client, test-accuracy, is assumed to be invoked on dev, hitting against server process on dev (default)
    // or optionally on stage or production as below.

    // Load the config file.
    load_config_file();

#ifdef USE_PROTOBUF
    protocol = &protobuf_protocol;
    printf("*** Using protobuf data protocol instead of default JSON ***\n");
    syslog(LOG_INFO, "*** Setting test-accuracy to use protobuf instead of json to talk to the server.");
#endif
    // Use getopt to help manage the options on the command line.
    int opt;
    while ((opt = getopt(argc, argv, "egn:pr:s")) != -1)
    {
        switch (opt)
        {
            case 'e': // for "testing /event call"
                g_test_event_call = true;
                printf("*** Testing the /event call. NOTE: This will send random ratings to the server. ***\n");
                break;
            case 'g': // for "group test"
                g_group_test = true;
                printf("*** Testing specific hard-coded group... ***\n");
                break;

            case 'n': // for "number of users"
                if (strtol(optarg, NULL, 10) < 1)
                {
                    printf("Error: the argument for -n should be > 0 instead of %s. Exiting. ***\n", optarg);
                    exit(EXIT_FAILURE);
                }
                g_num_testing_people = (size_t) strtol(optarg, NULL, 10);
                printf("*** Starting test-accuracy with number of testing people: %zu ***\n", g_num_testing_people);
                break;
            case 'p': // for "prod"
                g_server_location = TEST_LOC_PROD;
                printf("*** Testing against prod... ***\n");
                break;
            case 'r': // for "rating-buckets"
                if (strtol(optarg, NULL, 10) < 2 || strtol(optarg, NULL, 10) > 32)
                {
                    printf("Error: the argument for -r should be > 1 and < 33 instead of %s. Exiting. ***\n", optarg);
                    exit(EXIT_FAILURE);
                }
                g_ratings_scale = (int) strtol(optarg, NULL, 10);
                printf("*** Starting test-accuracy with ratings using a %d-bucket scale ***\n", g_ratings_scale);
                break;
            case 's': // for "stage"
                g_server_location = TEST_LOC_STAGE;
                printf("*** Testing against stage... ***\n");
                break;

            default:
                printf("Don't understand. Check args. Need one or more of e, g, j, n, p, r, or s. \n");
                fprintf(stderr, "Usage: %s [-e | -g | -n num_testing_people | -p | -r ratings_buckets | -s]\n",
                        argv[0]);
                exit(EXIT_FAILURE);
        } // end switch
    } // end while

    TestAccuracy();

    exit(EXIT_SUCCESS);
} // end main ()
