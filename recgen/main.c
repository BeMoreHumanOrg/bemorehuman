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

#include <signal.h>
#if linux
#include <bits/signum-generic.h>
#endif
#include "recgen.h"

// This is the bemorehuman recommendation engine.

// Globals
const char *error_strings[] = {
    "ok",
    "Protobuf decoding failed for recs() invocation.",
    "Protobuf decoding failed for singlerec() invocation.",
    "Protobuf decoding failed for event() invocation.",
    "personid from client is incorrect.",
    "elementid from client is incorrect.",
    "No ratings for this user."
};

static rating_t *g_big_rat;
static uint32_t *g_big_rat_index;
static uint32_t g_event_counter = 0;
static event_t g_events_to_persist[EVENTS_TO_PERSIST_MAX];
static bool wait_for_valence_reload = false;
uint8_t g_output_scale = 5;
static double conv_to_output_scale;
static const int num_recs_to_make = 5;

static void *json_serialize(const int scenario, const void *data, const char *status, size_t *len);

static void *json_deserialize(const int scenario, const size_t len, const void *data, int *status);

#ifdef USE_PROTOBUF
static void *protobuf_serialize(const int scenario, const void *data, const char *status, size_t *len);

static void *protobuf_deserialize(const int scenario, const size_t len, const void *data, int *status);
#endif

protocol_interface json_protocol =
{
    .serialize = json_serialize,
    .deserialize = json_deserialize,
};

#ifdef USE_PROTOBUF
protocol_interface protobuf_protocol =
{
    .serialize = protobuf_serialize,
    .deserialize = protobuf_deserialize,
};
#endif

// Default protocol is json.
protocol_interface *protocol = &json_protocol;

// Take in prediction_t *, status string, and return json message and len of message
static void *json_serialize(const int scenario, const void *data, const char *status, size_t *len)
{
    const prediction_t *recs_in = (prediction_t *) data;
    // 32 non-data bytes/rating_item_t roughly
    char *json = malloc(32 * num_recs_to_make * sizeof(prediction_t) + 400);
    const popularity_t *pop = pop_leash();

    strcpy(json, "{");
    // Need to create an array of objects
    if (data)
    {
        char char_int[12];
        switch (scenario)
        {
            case SCENARIO_RECS:
            {
                strcat(json, "\"recslist\":[");
                for (int i = 0; i < num_recs_to_make; i++)
                {
                    strcat(json, "{\"bmhid\":");
                    itoa((int) recs_in[i].elementid, char_int);
                    strcat(json, char_int);
                    strcat(json, ",\"recvalue\":");
                    int local_int = bmh_round((double) recs_in[i].rating / conv_to_output_scale);
                    if (local_int == 0) local_int = 1;
                    itoa(local_int, char_int);
                    strcat(json, char_int);
                    strcat(json, ",\"popularity\":");
                    local_int = pop[recs_in[i].elementid];
                    itoa((int) local_int, char_int);
                    strcat(json, char_int);
                    strcat(json, "}");

                    if (i < (num_recs_to_make - 1))
                    {
                        // add comma delimiter
                        strcat(json, ",");
                    }
                } // end for loop across the recs to send
                strcat(json, "],");
                break;
            } // end if we have any recs to make
            case SCENARIO_SINGLEREC:
            {
                strcat(json, "\"result\":");
                int local_int = bmh_round((double) recs_in[0].rating / conv_to_output_scale);
                if (local_int == 0) local_int = 1;
                itoa(local_int, char_int);
                strcat(json, char_int);
                strcat(json, ",");
                break;
            } // end if we're recommending a single rec
            case SCENARIO_EVENT:
            {
                break;
            } // end if we're logging an event
            default: return NULL;
        } // end switch across scenarios
    } // end if we have recs to send
    // add status
    strcat(json, "\"status\":\"");
    strcat(json, status);
    strcat(json, "\"}");
    //if (json)
    //    printf("JSON out test: %s\n", json); // {"name":"Mash","star":4,"hits":[2,2,1,3]}
    *len = strlen(json);
    return (void *) json;
} // end json_serialize()


// Take in POST JSON data and return a recs_request_t, status returned in status param
// For now, either personid or ratings is active. personid from protobuf or ratings
// from JSON. If both are there, prefer personid.
static void *json_deserialize(const int scenario, const size_t len, const void *data, int *status)
{

    // Read JSON and get root
    yyjson_doc *doc = yyjson_read(data, len, 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    switch (scenario)
    {
        case SCENARIO_RECS:
        {
            // create the return structure
            // orig recs_request_t *rr = calloc(1, sizeof(recs_request_t));
            recs_request_t *rr = calloc(1, sizeof(recs_request_t));

            // All functions accept NULL input, and return NULL on error.
            // Get root["personid"]
            yyjson_val *personid = yyjson_obj_get(root, "personid");
            rr->personid = yyjson_get_int(personid);

            // Get root["popularity"]
            yyjson_val *pop = yyjson_obj_get(root, "popularity");
            rr->popularity = yyjson_get_int(pop);

            // what is in data? ratingslist
            yyjson_val *ratingslist = yyjson_obj_get(root, "ratingslist");
            size_t idx, max;

            // Returns the number of key-value pairs in this object.
            // Returns 0 if input is not an object.
            rr->num_ratings = (int) yyjson_arr_size(ratingslist);
            yyjson_val *element;

            // malloc a continuous block for all the ratings of type rating_item_t
            rating_item_t *ratings = malloc(rr->num_ratings * sizeof(rating_item_t));

            // We have an array of objects
            yyjson_arr_foreach(ratingslist, idx, max, element)
            {
                size_t idx2, max2;
                yyjson_val *key, *val;
                yyjson_obj_foreach(element, idx2, max2, key, val)
                {
                    // assign the key, value into our return structure
                    // there's a new rating now, check the key.
                    const char *key_str = yyjson_get_str(key);
                    if (key_str[0] == 'e' || key_str[0] == 'E') // elementid
                        ratings[idx].elementid = yyjson_get_uint(val);
                    else
                        ratings[idx].rating = yyjson_get_int(val);
                } // end iterating over each part of element
            } // end iterating over each element in array
            rr->ratings_list = ratings;

            // Free the doc
            yyjson_doc_free(doc);
            *status = STATUS_OK;
            return rr;
        }
        case SCENARIO_SINGLEREC:
        {
            // create the return structure
            recs_request_t *rr = calloc(1, sizeof(recs_request_t));

            // All functions accept NULL input, and return NULL on error.
            // Get root["personid"]
            yyjson_val *personid = yyjson_obj_get(root, "personid");
            rr->personid = yyjson_get_int(personid);

            // Get root["elementid"]
            yyjson_val *elementid = yyjson_obj_get(root, "elementid");

            // malloc a continuous block for all the ratings of type rating_item_t
            rating_item_t *ri = malloc(sizeof(rating_item_t));
            ri->elementid = yyjson_get_int(elementid);
            rr->ratings_list = ri;

            // Free the doc
            yyjson_doc_free(doc);
            *status = STATUS_OK;
            return rr;
        }
        case SCENARIO_EVENT:
        {
            // create the return structure
            event_t *er = calloc(1, sizeof(event_t));

            // All functions accept NULL input, and return NULL on error.
            // Get root["personid"]
            yyjson_val *personid = yyjson_obj_get(root, "personid");
            er->personid = yyjson_get_int(personid);

            // Get root["elementid"]
            yyjson_val *elementid = yyjson_obj_get(root, "elementid");
            er->elementid = yyjson_get_int(elementid);

            // Get root["eventval"]
            yyjson_val *eventval = yyjson_obj_get(root, "elementid");
            er->eventval = yyjson_get_int(eventval);

            // Free the doc
            yyjson_doc_free(doc);
            *status = STATUS_OK;
            return er;
        }
        default: return NULL;
    } // end switch across scenarios
} // end json_deserialize()

#ifdef USE_PROTOBUF
// Take in prediction_t*, status string, and return protobuf message and len of message
static void *protobuf_serialize(const int scenario, const void *data, const char *status, size_t *len)
{
    const popularity_t *pop = pop_leash();
    void *buffer = NULL;
    switch (scenario)
    {
        case SCENARIO_RECS:
        {
            const prediction_t *recs = (prediction_t *) data;
            RecsResponse pb_response = RECS_RESPONSE__INIT; // declare the response
            pb_response.status = malloc(sizeof(char) * STATUS_LEN);
            // Use protobuf functions to serialize data
            pb_response.n_recslist = (size_t) RECS_BUCKET_SIZE;
            // iterate over the input and copy to target
            // Begin protobuf encapsulation.
            RecItem **recitems = malloc(sizeof(RecItem *) * RECS_BUCKET_SIZE);
            for (int i = 0; i < RECS_BUCKET_SIZE; i++)
            {
                // add the rec data to the recitems array
                recitems[i] = malloc(sizeof(RecItem));
                rec_item__init(recitems[i]);
                recitems[i]->elementid = (uint32_t) recs[i].elementid;

                // Scale what we return from 32 to g_output_scale.
                recitems[i]->rating = (int32_t) bmh_round((double) recs[i].rating / conv_to_output_scale);
                if (0 == recitems[i]->rating) recitems[i]->rating = 1;
                recitems[i]->popularity = pop[recs[i].elementid];
            } // end for loop

            pb_response.recslist = recitems;

            strlcpy(pb_response.status, status, sizeof(pb_response.status));

            *len = recs_response__get_packed_size(&pb_response);
            buffer = malloc(*len);
            recs_response__pack(&pb_response, buffer);

            // free malloced status
            free(pb_response.status);
            break;
        }
        case SCENARIO_SINGLEREC:
        {
            prediction_t *recs = (prediction_t *) data;
            InternalSingleRecResponse message_out = INTERNAL_SINGLE_REC_RESPONSE__INIT;
            message_out.status = malloc(sizeof(char) * STATUS_LEN);
            // Scale what we return from 32 to g_output_scale.
            message_out.result = (int32_t) bmh_round((double) recs->rating / conv_to_output_scale);
            if (0 == recs->rating) recs->rating = 1;

            // Finish constructing the protobuf message.
            strlcpy(message_out.status, status, sizeof(message_out.status));
            *len = internal_single_rec_response__get_packed_size(&message_out); // this is calculated packing length
            buffer = malloc(*len); // Allocate required serialized buffer length
            internal_single_rec_response__pack(&message_out, buffer); // Pack the data

            // Clean up.
            free(message_out.status);
            break;
        }
        case SCENARIO_EVENT:
        {
            EventResponse message_out = EVENT_RESPONSE__INIT;
            message_out.status = malloc(sizeof(char) * STATUS_LEN);

            // Finish constructing the protobuf message.
            strlcpy(message_out.status, status, sizeof(message_out.status));
            *len = event_response__get_packed_size(&message_out); // this is calculated packing length
            buffer = malloc(*len); // Allocate required serialized buffer length
            event_response__pack(&message_out, buffer); // Pack the data

            // Clean up.
            free(message_out.status);
            break;
        }
        default: return NULL;
    } // end switch across scenarios
    return buffer;
} // end protobuf_serialize()


// Take in POST data and return a relevant structure like recs_request_t, status returned in status param
static void *protobuf_deserialize(const int scenario, const size_t len, const void *data, int *status)
{
    // Use protobuf functions to deserialize data

    switch (scenario)
    {
        case SCENARIO_RECS:
        {
            // create the return structure
            recs_request_t *rr = NULL;
            rr = malloc(sizeof(recs_request_t));
            Recs *message_in = recs__unpack(NULL, len, data);
            // Check for errors
            if (message_in == NULL)
            {
                syslog(LOG_ERR, "Protobuf decoding failed for recs() invocation.");
                *status = PROTOBUF_DECODE_FAILED_FOR_RECS;
                return NULL;
            }
            // Does the passed-in personid not match any people we know about?
            if (message_in->personid > BE.num_people)
            {
                syslog(LOG_ERR, "personid from client is incorrect: ---%d---", message_in->personid);
                *status = PERSONID_FROM_CLIENT_INCORRECT;
                return NULL;
            }

            rr->personid = message_in->personid;
            rr->popularity = (popularity_t) message_in->popularity;

            recs__free_unpacked(message_in, NULL);
            *status = STATUS_OK;
            return rr;
        }
        case SCENARIO_SINGLEREC:
        {
            // create the return structure
            recs_request_t *rr = NULL;
            rr = malloc(sizeof(recs_request_t));
            InternalSingleRec *message_in = internal_single_rec__unpack(NULL, len, data);
            // Check for errors.
            if (message_in == NULL)
            {
                syslog(LOG_ERR, "Protobuf decoding failed for internal singlerec.");
                *status = PROTOBUF_DECODE_FAILED_FOR_SINGLEREC;
                return NULL;
            }
            // Does the passed-in personid not match any people we know about?
            if (message_in->personid > BE.num_people)
            {
                syslog(LOG_ERR, "personid from client is incorrect: ---%d---", message_in->personid);
                *status = PERSONID_FROM_CLIENT_INCORRECT;
                return NULL;
            }
            rr->personid = message_in->personid;
            rating_item_t *ri = malloc(sizeof(rating_item_t));
            ri->elementid = message_in->elementid;
            rr->ratings_list = ri;

            internal_single_rec__free_unpacked(message_in, NULL);
            *status = STATUS_OK;
            return rr;
        }
        case SCENARIO_EVENT:
        {
            // create the return structure
            event_t *er = NULL;
            er = malloc(sizeof(event_t));
            Event *message_in = event__unpack(NULL, len, data);
            // Check for errors.
            if (message_in == NULL)
            {
                syslog(LOG_ERR, "Protobuf decoding failed for event call.");
                *status = PROTOBUF_DECODE_FAILED_FOR_EVENT;
                return NULL;
            }
            // Does the passed-in personid not match any people we know about?
            if (message_in->personid > BE.num_people)
            {
                syslog(LOG_ERR, "personid from client is incorrect: ---%d---", message_in->personid);
                *status = PERSONID_FROM_CLIENT_INCORRECT;
                return NULL;
            }
            er->personid = message_in->personid;
            er->elementid = message_in->elementid;
            er->eventval = message_in->eventval;
            event__free_unpacked(message_in, NULL);
            *status = STATUS_OK;
            return er;
        }
        default: return NULL;
    } // end switch across scenarios
} // end protobuf_deserialize()
#endif


// This is used for test-accuracy's -core alg testing only.
// Output: a recommendation (-1 or 1..g_output_scale) for the passed-in element.
static void internal_singlerec(void *request)
{
    // 3 steps:
    // 1. Get ratings for this user.
    // 2. Call prediction() with specific elementid (functionally similar to core testing scenario).
    // 3. Construct & send protobuf output.

    // What type are we really dealing with on input?
    // external webserver means the input type is really *FCGX_Request
    // internal webserver means the input type is really *hum_request

    recs_request_t *deserialized_data = NULL;
    void *serialized_data = NULL;
    prediction_t *recs = NULL;
    rating_t *ratings = NULL;

#ifdef USE_FCGI
    FCGX_Request *f_req;
    f_req = (FCGX_Request *) request;
#else
    const hum_request *h_req = (hum_request *) request;
#endif

    size_t post_len;
    uint8_t post_data[FCGX_MAX_INPUT_STREAM_SIZE];

    // Get the POSTed protobuf.
#ifdef USE_FCGI
    post_len = (size_t) FCGX_GetStr((char *) post_data,
                                        sizeof(post_data),
                                        f_req->in);
#else
    post_len = h_req->in->content_length;
    memcpy((char *) post_data,
           (char *) h_req->in->content,
           post_len);
#endif

    // Deserialize the request.
    int status = 0;
    size_t len = 0;
    deserialized_data = protocol->deserialize(SCENARIO_SINGLEREC, post_len, post_data, &status);

    // Now we should have a structure of personid, popularity, num_ratings, array of possible elt/rating pairs
    // if there was no error. Check status for errors.
    if (status)
    {
        printf("Problem from the deserializer! Bailing on this user.\n");
        goto finish_up;
    }

    // Does the passed-in personid match a user we know about?
    if (0 == g_big_rat_index[deserialized_data->personid])
    {
        status = PERSONID_FROM_CLIENT_INCORRECT;
        printf("Person %d not found. Bailing on this person.\n", deserialized_data->personid);
        syslog(LOG_ERR,
               "Error: received personid that's not in the db or has no ratings for situation internal_singlerec: %d",
               deserialized_data->personid);
        goto finish_up;
    }
    // We can generate recs for this person
    const int target_id = (int) deserialized_data->ratings_list->elementid;

    // Begin making the recs real!
    // 1. get the ratings for the user
    int num_rats;

    // Check if we're at the max person_id first.
    if (BE.num_people != deserialized_data->personid)
        num_rats = (int) (g_big_rat_index[deserialized_data->personid + 1] -
                          g_big_rat_index[deserialized_data->personid]);
    else
        num_rats = (int) (BE.num_ratings - g_big_rat_index[deserialized_data->personid]);

    // Limit what we care about to MAX_RATS_PER_PERSON.
    if (num_rats > MAX_RATS_PER_PERSON) num_rats = MAX_RATS_PER_PERSON;

    // This is the list of ratings given by the current user.
    ratings = (rating_t *) calloc((size_t) (num_rats) + 1, sizeof(rating_t));

    // Bail roughly if we can't get any mem.
    if (!ratings) exit(EXIT_NULLRATS);

    // This is the list of recommendations for this user.
    recs = (prediction_t *) calloc(1, sizeof(prediction_t));

    // Bail roughly if we can't get any mem.
    if (!recs) exit(EXIT_NULLPREDS);

    int i = 0;
    bool done = false;

    // Make sure we do NOT select the rating for the target_id if it exists. Put another way, if person
    // has rated the element she's asking about, we ignore that rating.
    // Make sure we can deterministically get the ratings when num rats may be > MAX_RATS so include an ORDER BY clause.
    // This determinism is desired for repeatable accuracy testing. Prolly less desired in production where we embrace more randomness.
    while (!done)
    {
        if ((unsigned int) target_id != ratings[i].elementid)
        {
            ratings[i].elementid = g_big_rat[g_big_rat_index[deserialized_data->personid] + i].elementid;
            ratings[i].rating = g_big_rat[g_big_rat_index[deserialized_data->personid] + i].rating;
            i++;
        } else
        {
            // We found it in the ratings, so decrease the num_rats
            num_rats--;
        }
        if (i == num_rats) done = true;
    } // end while not done

    // 2. Pass ratings to recgen core.
    const popularity_t max_obscurity = HIGHEST_POP_NUMBER;
    // 1 is most popular, 7 is most obscure. 7 includes 1-6, 3 includes 1-2, etc.
    if (!predictions(ratings, num_rats, recs, 1, target_id, max_obscurity))
        syslog(LOG_ERR, "No predictions generated for user %d", deserialized_data->personid);

finish_up:
    // Serialize the data
    serialized_data = protocol->serialize(SCENARIO_SINGLEREC, recs, error_strings[status], &len);
#ifdef USE_FCGI
    bytes_to_fcgi = FCGX_PutStr((const char *) serialized_data, (int) len, f_req->out);
    if (bytes_to_fcgi != (int) len)
        syslog(LOG_ERR,
               "ERROR: in recs, bytes_to_fcgi is %d while message_length is %lu",
               bytes_to_fcgi, len);
#else
    h_req->out->content_length = len;
    h_req->out->type_status = HUM_RESPONSE_OK;
    strncpy((char *) h_req->out->content, (const char *) serialized_data, (int) len);
#endif

    // Clear out stuff for next user.
    if (ratings) free(ratings);
    if (recs) free(recs);

    // free the deserialized_data
    if (deserialized_data)
    {
        // free possible ratings list
        if (protocol == &json_protocol && deserialized_data->ratings_list)
            free(deserialized_data->ratings_list);
        free(deserialized_data);
    }

    // Free serialized data if needed
    if (serialized_data)
        free(serialized_data);
} // end scenario: internal_singlerec

//
// Provide recommendations for a person.
// input: *void which can be *FCGX_Request or *hum_request
//
static void recs(void *request)
{
    // input: (inside the protobuf message) userid
    // output: the recs!
    // 3 steps:
    // 1. Get the ratings for the person.
    // 2. Pass ratings to recgen core.
    // 3. Construct & send protobuf output.

    // What type are we really dealing with on input?
    // external webserver means the input type is really *FCGX_Request
    // internal webserver means the input type is really *hum_request

    recs_request_t *deserialized_data = NULL;
    void *serialized_data = NULL;

#ifdef USE_FCGI
    FCGX_Request *f_req;
    f_req = (FCGX_Request *) request;
#else
    const hum_request *h_req = (hum_request *) request;
#endif

    size_t len = 0;

    // Get the POSTed protobuf.
    uint8_t post_data[FCGX_MAX_INPUT_STREAM_SIZE];
    size_t post_len;

#ifdef USE_FCGI
    post_len = (size_t) FCGX_GetStr((char *) post_data,
                                        sizeof(post_data),
                                        f_req->in);
#else
    post_len = h_req->in->content_length;
    memcpy((char *) post_data,
           (char *) h_req->in->content,
           post_len);
#endif

    // Now we are ready to decode the message.
    int num_rats;

    // 1. Get the ratings for the person.

    rating_t *ratings = NULL;
    prediction_t *recs = NULL;

    // Deserialize the request.
    int status = 0;
    deserialized_data = protocol->deserialize(SCENARIO_RECS, post_len, post_data, &status);

    // Now we should have a structure of personid, popularity, num_ratings, array of possible elt/rating pairs
    // if there was no error. Check status for errors.
    if (status)
    {
        printf("Problem from the deserializer! Bailing on this user.\n");
        goto finish_up;
    }

    // Are we in JSON mode? if so, num_rats will be deserialized_data->num_ratings and ratings are there too.
    if (protocol == &json_protocol)
    {
        num_rats = deserialized_data->num_ratings;
    } else // We're in protobuf mode
    {
        // Check if we're at the max person_id first.
        if (BE.num_people != deserialized_data->personid)
            num_rats = (int) (g_big_rat_index[deserialized_data->personid + 1] -
                              g_big_rat_index[deserialized_data->personid]);
        else
            num_rats = (int) (BE.num_ratings - g_big_rat_index[deserialized_data->personid]);
    }

    // Limit what we care about to MAX_RATS_PER_PERSON.
    if (num_rats > MAX_RATS_PER_PERSON) num_rats = MAX_RATS_PER_PERSON;
    int i;
    if (0 == num_rats)
    {
        // No ratings for this person. Problem!
        status = NO_RATINGS_FOR_USER;
        goto finish_up;
    }

    // This is the list of ratings given by the current user.
    ratings = (rating_t *) calloc(num_rats, sizeof(rating_t));

    // Bail roughly if we can't get any mem.
    if (!ratings)
        exit(EXIT_NULLRATS);

    // This is the list of recommendations for this user.
    recs = (prediction_t *) calloc(MAX_PREDS_PER_PERSON, sizeof(prediction_t));

    // Bail roughly if we can't get any mem.
    if (!recs)
        exit(EXIT_NULLPREDS);

    if (protocol == &json_protocol)
    {
        if (deserialized_data->ratings_list)
        {
            // Get this rando's ratings from the deserialized_data
            for (i = 0; i < num_rats; i++)
            {
                ratings[i].elementid = deserialized_data->ratings_list[i].elementid;
                ratings[i].rating = (short) deserialized_data->ratings_list[i].rating;
            }
        } else
        {
            printf("ERROR: ratings_list is empty. Bailing on this user.\n");
            status = NO_RATINGS_FOR_USER;
            goto finish_up;
        }
    } else
    {
        // Get personid's ratings.
        for (i = 0; i < num_rats; i++)
        {
            ratings[i].elementid = g_big_rat[g_big_rat_index[deserialized_data->personid] + i].elementid;
            ratings[i].rating = g_big_rat[g_big_rat_index[deserialized_data->personid] + i].rating;
        }
    }
    // 2. Pass ratings to recgen core.
    len = 0;

    if (!predictions(ratings, num_rats, recs, RECS_BUCKET_SIZE, 0, deserialized_data->popularity))
        syslog(LOG_ERR, "No predictions generated for user %d", deserialized_data->personid);

    status = STATUS_OK;

    // Send the response to the client.
finish_up:

    // Serialize the data
    serialized_data = protocol->serialize(SCENARIO_RECS, recs, error_strings[status], &len);

#ifdef USE_FCGI
    bytes_to_fcgi = FCGX_PutStr((const char *) serialized_data, (int) len, f_req->out);
    if (bytes_to_fcgi != (int) len)
        syslog(LOG_ERR,
               "ERROR: in recs, bytes_to_fcgi is %d while message_length is %lu",
               bytes_to_fcgi, len);
#else
    h_req->out->content_length = len;
    h_req->out->type_status = HUM_RESPONSE_OK;
    strncpy((char *) h_req->out->content, (const char *) serialized_data, (int) len);
#endif

    // Clear out stuff for next user.
    if (ratings) free(ratings);
    if (recs) free(recs);

    // free the deserialized_data
    if (deserialized_data)
    {
        // free possible ratings list
        if (protocol == &json_protocol && deserialized_data->ratings_list)
            free(deserialized_data->ratings_list);
        free(deserialized_data);
    }

    // Free serialized data if needed
    if (serialized_data)
        free(serialized_data);
} // end recs()


//
// Ingest an event such as a rating, listen, purchase, click, etc.
// output: success or failure
//
static void event(void *request)
{
    // input: userid, eltid, (optional) event_value such as a rating
    // output: success or failure
    // 2 steps:
    // 1. Persist input event to in-memory structure and possibly filesystem.
    // 2. Construct & send protobuf or json output.
    // external webserver means the input type is really *FCGX_Request
    // internal webserver means the input type is really *hum_request

#ifdef USE_FCGI
    FCGX_Request *f_req;
    f_req = (FCGX_Request *) request;
#else
    const hum_request *h_req = (hum_request *) request;
#endif

    void *serialized_data = NULL;
    size_t len;

    uint8_t post_data[FCGX_MAX_INPUT_STREAM_SIZE];

    // Get the POSTed protobuf.
    size_t post_len;

#ifdef USE_FCGI
    post_len = (size_t) FCGX_GetStr((char *) post_data,
                                        sizeof(post_data),
                                        f_req->in);
#else
    post_len = h_req->in->content_length;
    memcpy(post_data,
           h_req->in->content,
           post_len);
#endif

    // Now we are ready to decode the message.
    // Deserialize the request.
    int status = 0;
    event_t *deserialized_data = protocol->deserialize(SCENARIO_EVENT, post_len, post_data, &status);

    // Now we should have a structure of personid, popularity, num_ratings, array of possible elt/rating pairs
    // if there was no error. Check status for errors.
    if (status)
    {
        printf("Problem from the deserializer! Bailing on this user.\n");
        goto finish_up;
    }

    // Does the passed-in personid not match any people we know about?
    if (deserialized_data->personid > BE.num_people)
    {
        // new
        status = PERSONID_FROM_CLIENT_INCORRECT;
        printf("Person %d not found. Bailing on this person.\n", deserialized_data->personid);
        syslog(LOG_ERR,
               "Error: received personid that's not in the db or has no ratings for situation event: %d",
               deserialized_data->personid);
        goto finish_up;
    }

    // Does the passed-in elementid not match any element we know about?
    if (deserialized_data->elementid > BE.num_elts)
    {
        syslog(LOG_ERR, "elementid from client is incorrect: ---%d---", deserialized_data->elementid);
        status = ELEMENTID_FROM_CLIENT_INCORRECT;
        goto finish_up;
    }

    // Save the event to our in-mem structure;
    g_events_to_persist[g_event_counter].personid = deserialized_data->personid;
    g_events_to_persist[g_event_counter].elementid = deserialized_data->elementid;
    g_events_to_persist[g_event_counter].eventval = deserialized_data->eventval;
    g_event_counter++;

    // Are we ready to persist now?
    if (g_event_counter == EVENTS_TO_PERSIST_MAX)
    {
        char fname[255];
        strlcpy(fname, BE.bmh_events_file, sizeof(fname));

        // MUST be "a" and not "w" because this file gets appended to.
        FILE *fp = fopen(fname, "a");

        if (NULL == fp)
        {
            syslog(LOG_ERR, "ERROR: cannot open output file %s", fname);
            exit(1);
        }

        for (int i = 0; i < (int) g_event_counter; i++)
        {
            char line[512];
            // If there's an interesting eventval, add it to the output.
            if (g_events_to_persist[i].eventval != 0)
                sprintf(line, "%d,%d,%d\n",
                        g_events_to_persist[i].personid,
                        g_events_to_persist[i].elementid,
                        g_events_to_persist[i].eventval);
            else
                sprintf(line, "%d,%d\n",
                        g_events_to_persist[i].personid,
                        g_events_to_persist[i].elementid);

            // Put the line in the output file
            fwrite(line, strlen(line), 1, fp);
        } // end for loop across events to write out
        fclose(fp);

        // Reset counter.
        g_event_counter = 0;
    } // end if we want to persist the saved events

finish_up:
    // Serialize the data
    serialized_data = protocol->serialize(SCENARIO_EVENT, NULL, error_strings[status], &len);
#ifdef USE_FCGI
    bytes_to_fcgi = FCGX_PutStr((const char *) serialized_data, (int) len, f_req->out);
    if (bytes_to_fcgi != (int) len)
        syslog(LOG_ERR,
               "ERROR: in event, bytes_to_fcgi is %d while message_length is %lu",
               bytes_to_fcgi, len);
#else
    h_req->out->content_length = len;
    h_req->out->type_status = HUM_RESPONSE_OK;
    strncpy((char *) h_req->out->content, (char *) serialized_data, len);
#endif

    if (deserialized_data)
        free(deserialized_data);

    if (serialized_data)
        free(serialized_data);
} // end event()


#ifdef USE_FCGI
// Execute this callback per thread, with an infinite loop inside that will receive requests
// NOTE: clang understands the GCC pragma, but not vice-versa! So do it this way.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
static void *start_fcgi_worker(void *arg)
{
    FCGI_info_t *info = (FCGI_info_t *) arg;
    FCGX_Init();
    FCGX_Request request;
    FCGX_InitRequest(&request, info->fcgi_fd, 0);

    // Thread-specific init stuff.
    create_workingset(BE.num_elts);

    while (1)
    {
        FCGX_Accept_r(&request);

        // Because we're sending raw protobuf-encoded data, use octet-stream.
        if (protocol == protobuf_protocol)
            FCGX_PutS("Content-Type: application/octet-stream\r\n\r\n", request.out);
        else
            FCGX_PutS("Content-Type: application/json\r\n\r\n", request.out);

        const char *request_uri = FCGX_GetParam("REQUEST_URI", request.envp);

        // If the request is empty, all bets are off. Bail.
        if (NULL == request_uri)
        {
            syslog(LOG_ERR, "recgen received empty request and can't continue. Something's broken upstream.");
            exit(EXIT_FAILURE);
        }

        size_t len_request_uri = strlen(request_uri);

        // Do we need to wait for valences to reload?
        while (wait_for_valence_reload)
        {
            syslog(LOG_INFO, "recgen is waiting for valences to reload...");
            sleep(1);
        }

        // /internal-singlerec call
        if ((23 == len_request_uri) && (!strcmp("/bmh/internal-singlerec", request_uri)))
        {
            internal_singlerec(&request);
            goto cleanup;
        }

        // /recs call
        if ((9 == len_request_uri) && (!strcmp("/bmh/recs", request_uri)))
        {
            recs(&request);
            goto cleanup;
        }

        // /event call
        if ((10 == len_request_uri) && (!strcmp("/bmh/event", request_uri)))
        {
            event(&request);
            goto cleanup;
        }

        cleanup:
        FCGX_Finish_r(&request);

    } // end while (1)
} // End start_fcgi_worker callback
#pragma GCC diagnostic pop

#else

// Run this single worker with an infinite loop inside that will receive requests.
// NOTE: clang understands the GCC pragma, but not vice-versa! So do it this way.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
static void *start_hum_worker(int hum_fd)
{
    // Thread-specific init stuff.
    create_workingset(BE.num_elts);

    while (1)
    {
        hum_record record_in;
        char uri[256];

        // Get a request along with POST data.

        // Now ready to accept. Use a new socket for this request.

        // We don't care atm about the address & len of the connecting peer. Make them NULL.
        // This connection will be on a new socket, cl_fd. The listening socket hum_fd remains
        // open and can accept further connections.
        // This will block.
        const int cl_fd = accept(hum_fd, NULL, NULL);

        // Read field-by-field
        // Get the first byte which is the type.
        ssize_t bytes_read = read(cl_fd, &record_in.type_status, sizeof(uint8_t));
        if (bytes_read < 0)
        {
            perror("Error reading request from hum server.");
            close(cl_fd);
            continue;
        }
        if (bytes_read == 0)
        {
            perror("Error bytes_read is 0 from hum server.");
            close(cl_fd);
            continue;
        }
        if (record_in.type_status == HUM_REQUEST_URI)
        {
            // Get the content length
            read(cl_fd, &record_in.content_length, sizeof(uint32_t));

            // Get the content.
            bytes_read = read(cl_fd, &record_in.content, record_in.content_length);
            if (bytes_read != record_in.content_length)
            {
                syslog(LOG_ERR, "bytes_read %zd is not the same as content_length %d", bytes_read,
                       record_in.content_length);
                syslog(LOG_ERR, "record_in.content is ---%s---", record_in.content);
                exit(EXIT_FAILURE);
            }
            strncpy(uri, (char *) record_in.content, record_in.content_length);
            uri[record_in.content_length] = '\0';
        }

        // Read next record_in, which should be the POST part of the request
        bytes_read = read(cl_fd, &record_in.type_status, sizeof(uint8_t));
        if (bytes_read < 0)
        {
            perror("Error reading POST part of request from hum server.");
            close(cl_fd);
            continue;
        }
        if (bytes_read == 0)
        {
            perror("Error bytes_read for POST part is 0 from hum server.");
            close(cl_fd);
            continue;
        }
        if (record_in.type_status == HUM_POST_DATA)
        {
            // Get the content length
            read(cl_fd, &record_in.content_length, sizeof(uint32_t));

            // Get the content.
            bytes_read = read(cl_fd, &record_in.content, record_in.content_length);
            if (bytes_read != record_in.content_length)
            {
                syslog(LOG_ERR, "bytes_read %zd for POST is not the same as content_length %d", bytes_read,
                       record_in.content_length);
                exit(EXIT_FAILURE);
            }
        }

        // So now we have uri, and POST data in record_in.content of length: record_in.content_length

        const ssize_t len_request_uri = strlen(uri);

        // Do we need to wait for valences to reload?
        // ReSharper disable once CppDFALoopConditionNotUpdated
        while (wait_for_valence_reload)
        {
            syslog(LOG_INFO, "recgen is waiting for valences to reload...");
            sleep(1);
        }

        hum_request request;
        request.in = &record_in;
        hum_record record_out;
        request.out = &record_out;

        // /internal-singlerec call
        if ((23 == len_request_uri) && (!strcmp("/bmh/internal-singlerec", uri)))
        {
            internal_singlerec(&request);
            goto finish;
        }

        // /recs call
        if ((9 == len_request_uri) && (!strcmp("/bmh/recs", uri)))
        {
            recs(&request);
            goto finish;
        }

        // /event call
        if ((10 == len_request_uri) && (!strcmp("/bmh/event", uri)))
        {
            event(&request);
            goto finish;
        }

    finish:
        if (request.out->type_status == HUM_RESPONSE_OK)
        {
            // Everything's ok, and we need to send the response data to hum.
            // Check for content-length > 0 first!
            if (request.out->content_length > 0)
            {
                write(cl_fd, &request.out->type_status, sizeof(uint8_t));
                write(cl_fd, &request.out->content_length, sizeof(uint32_t));
                write(cl_fd, &request.out->content, request.out->content_length);
            }
        } else if (request.out->type_status == HUM_RESPONSE_ERROR)
        {
            // check for content-length > 0 first!
            if (request.out->content_length > 0)
                write(cl_fd, request.out->content, request.out->content_length);
        }
        close(cl_fd);
    } // end while (1)
} // End start_hum_worker()
#pragma GCC diagnostic pop
#endif

// Get the num_confident_valences.
static void populate_ncv()
{
    // Get the num_confident_valences from a flat file.
    char filename[strlen(BE.working_dir) + strlen(NUM_CONF_OUTFILE) + 2];
    strlcpy(filename, BE.working_dir, sizeof(filename));
    strlcat(filename, "/", sizeof(filename));
    strlcat(filename, NUM_CONF_OUTFILE, sizeof(filename));

    FILE *fp = fopen(filename, "r");
    if (!fp)
    {
        printf("Can't open file %s. Exiting.\n", filename);
        exit(1);
    }

    char *line = NULL;
    size_t line_capacity = 0;
    ssize_t line_length;
    while ((line_length = getline(&line, &line_capacity, fp)) != -1)
    {
        // Is the file coming from linux/unix or pre Mac OSX?
        if (line_length && (line[line_length - 1] == '\n' || line[line_length - 1] == '\r'))
            line[--line_length] = '\0';
        // Is the file coming from windows?
        if (line_length && (line[line_length - 1] == '\r'))
            line[--line_length] = '\0';

        // Convert line to a size_t.
        g_num_confident_valences = (size_t) strtol(line, NULL, 10);
        syslog(LOG_INFO, "num_confident_valences is %lu", g_num_confident_valences);
    }
    free(line);
    fclose(fp);
} // end populate_ncv()

static void initialize_structures()
{
    // Get the num_confident_valences.
    populate_ncv();

    // Begin beast stuff.
    syslog(LOG_INFO, "Begin timing for loading valences.");
    long long start = current_time_millis();

    // Load up Beast with valences and load the DS.
    bool retval = load_beast(LOAD_VALENCES_FROM_BEAST_EXPORT, true);
    if (true != retval)
        exit(EXIT_MEMLOAD);

    long long finish = current_time_millis();
    syslog(LOG_INFO, "Time to do Beast load: %d milliseconds.", (int) (finish - start));

    // Load up big_rat and br_index ratings structure.
    syslog(LOG_INFO, "Begin timing for loading big_rat into recgen.");
    start = current_time_millis();

    retval = big_rat_load();
    if (true != retval)
        exit(EXIT_MEMLOAD);

    finish = current_time_millis();
    syslog(LOG_INFO, "Time to do big_rat load: %d milliseconds.", (int) (finish - start));

    // Load up the element popularities to help with preferred obscurity for recommendations.
    retval = pop_load();
    if (true != retval)
        exit(EXIT_FAILURE);

    // Set up big_rat.
    g_big_rat = big_rat_leash();
    g_big_rat_index = big_rat_index_leash();

    // end initializations before spawning threads
} // end initialize_structures()

void sig_handler(int signo)
{
    // Did some process tell us to reload the valence caches?
    if (signo == SIGUSR1)
    {
        syslog(LOG_INFO, "recgen received SIGUSR1.");

        // Block all threads from accessing beast and beast ds.
        wait_for_valence_reload = true;

        // Remove the beast, or rather, reload beast and friends.
        initialize_structures();

        // Unblock other threads from accessing beast and beast ds
        wait_for_valence_reload = false;
    }
} // end sig_handler()


int main(int argc, char **argv)
{
    // Set up logging.
    openlog(LOG_MODULE_STRING, LOG_PID, LOG_LOCAL0);
    setlogmask(LOG_UPTO(RECGEN_LOG_MASK));
    syslog(LOG_INFO, "*** Begin recgen invocation.");

    // Load the config file.
    load_config_file();

    // Register signal handler.
    if (signal(SIGUSR1, sig_handler) == SIG_ERR)
    {
        syslog(LOG_ERR, "Can't catch SIGUSR1 in recgen. Exiting.");
        exit(EXIT_FAILURE);
    }

#ifdef USE_FCGI
    printf("*** Using FastCGI to talk to external web server ***\n");
    syslog(LOG_INFO, "*** Setting recgen to use FastCGI to talk to external webserver.");
#endif

#ifdef USE_PROTOBUF
    protocol = &protobuf_protocol;
    printf("*** Using protobuf data protocol instead of default JSON ***\n");
    syslog(LOG_INFO, "*** Setting recgen to use protobuf instead of json to talk to clients.");
#endif

    // Use getopt to help manage the options on the command line.
    int opt;
    while ((opt = getopt(argc, argv, "cdb:")) != -1)
    {
        switch (opt)
        {
            case 'c': // for "cache generation"
                printf("*** Generating valence cache ***\n");
                gen_valence_cache();
                syslog(LOG_INFO, "*** End recgen valence cache generation");
                exit(EXIT_SUCCESS);
            case 'd': // for "DS cache generation"
                printf("*** Generating DS only valence cache ***\n");
                gen_valence_cache_ds_only();
                syslog(LOG_INFO, "*** End recgen valence cache DS generation");
                exit(EXIT_SUCCESS);
            case 'b': // for "recommendation-buckets"
                if (strtol(optarg, NULL, 10) < 2 || strtol(optarg, NULL, 10) > 32)
                {
                    printf("Error: the argument for -b should be > 1 and < 33 instead of %s. Exiting. ***\n", optarg);
                    syslog(LOG_ERR, "The argument for -b should be > 1 and < 33, instead of %s. Exiting. ***\n",
                           optarg);
                    exit(EXIT_FAILURE);
                }
                g_output_scale = strtol(optarg, NULL, 10);
                break;
            default:
                printf("Don't understand. Check args. Need one of c, d, or b. \n");
                fprintf(stderr, "Usage: %s [-c] [-d] [-b buckets]\n", argv[0]);
                exit(EXIT_FAILURE);
        } // end switch
    } // end while

    conv_to_output_scale = 32.0 / (double) g_output_scale;

    printf("*** Starting the live recommender with recs using a %d-bucket scale ***\n", g_output_scale);
    syslog(LOG_INFO, "*** Start recgen live recommender with recs using a %d-bucket scale ***", g_output_scale);

    // Populate the beast, g_pop, and ratings structures.
    initialize_structures();

    // Begin connecting to socket.
    // Strip off the filename from BE.recgen_socket_location.
    size_t len = strlen(BE.recgen_socket_location);
    char path[len + 1];
    strlcpy(path, BE.recgen_socket_location, sizeof(path));
    const char *fstart = strrchr(path, '/');
    if (NULL == fstart)
    {
        path[0] = '.';
        path[1] = '\0';
    } else
    {
        const size_t flen = strlen(fstart);
        path[len - flen] = '\0';
    }

    // Check to see if the dir exists.
    if (!check_make_dir(path))
    {
        syslog(LOG_CRIT, "Can't access or create the socket dir at %s. Exiting.", path);
        exit(EXIT_FAILURE);
    }

    // Here is where we diverge if we're using hum or FastCGI.
#ifdef USE_FCGI
    int fcgi_fd = FCGX_OpenSocket(BE.recgen_socket_location, 128);
    if (0 > fcgi_fd)
    {
        syslog(LOG_ERR, "Can't open socket for communication with web server. Exiting.");
        exit(EXIT_FAILURE);
    }

    // Allow web server to write to the socket we just opened.
    int returnval;
    char system_string[len + 32];
    strlcpy(system_string, "exec chmod o+w ", sizeof(system_string));
    strlcat(system_string, BE.recgen_socket_location, sizeof(system_string));
    returnval = system(system_string);
    if (returnval != 0)
    {
        syslog(LOG_ERR, "Can't set permissions for unix socket. Exiting.");
        exit(EXIT_FAILURE);
    }
    syslog(LOG_INFO, "successfully set socket %s to allow web server to write to it.", BE.recgen_socket_location);

    const unsigned int n_threads = 2;

    pthread_t threads[n_threads];

    FCGI_info_t info;
    info.fcgi_fd = fcgi_fd;

    for (unsigned int i = 0; i < n_threads; i++)
        pthread_create(&threads[i], NULL, start_fcgi_worker, (void *) &info);

    // Wait indefinitely.
    for (unsigned int i = 0; i < n_threads; i++)
    {
        pthread_join(threads[i], NULL);
    }
    // end fastcgi connectivity
#else
    // Ok, we're not using an external webserver; we're using our internal hum server.

    // Kill any lingering hum processes.
    int returnval = system("killall hum");
    if (returnval != 0)
    {
        // Was there a problem with permissions?
        if (errno == EPERM)
        {
            syslog(
                LOG_ERR,
                "Killall failed to get rid of previous hum process(es) because of a permissions issue. Exiting.");
            exit(EXIT_FAILURE);
        }
    }

    // Start double-forking code to get hum to be its own independent process.
    printf("forking process pid: %d\n", getpid());
    const pid_t p1 = fork();

    // NOTE: if pid == 0 it's the child process
    if (p1 != 0)
    {
        printf("p1 process id is %d\n", getpid());
        int status;
        wait(&status);
        system("ps");
    } else
    {
        const pid_t p2 = fork();
        const int pid = getpid();

        if (p2 != 0)
        {
            printf("p2 process id is %d\n", pid);
            exit(0);
        }
        printf("p3 process id is %d\n", pid);
        printf("I'm the grandchild with pid %d.\n", getpid());

        // Start the hum server.
        char portstr[6];
        itoa(HUM_DEFAULT_PORT, portstr);
        execlp("hum", "hum", "-p", portstr, (char *) NULL);
    }
    // end double-forking stuff

    // Open socket that will talk to our hum server.
    struct sockaddr_un process_address;

    const int hum_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (hum_fd < 0)
    {
        perror("Error creating socket for process.");
        exit(EXIT_FAILURE);
    }

    // Check that length of path & file isn't too long.
    if (strlen(BE.recgen_socket_location) > sizeof(process_address.sun_path) - 1)
    {
        perror("Server socket path too long.");
        exit(EXIT_FAILURE);
    }

    process_address.sun_family = AF_UNIX;
    strcpy(process_address.sun_path, BE.recgen_socket_location);

    unlink(process_address.sun_path);

    // Now bind the listening socket.
    if (bind(hum_fd, (struct sockaddr *) &process_address, sizeof(struct sockaddr_un)) < 0
        || listen(hum_fd, 5) < 0)
    {
        perror("bind/listen");
        exit(EXIT_FAILURE);
    }

    syslog(LOG_INFO, "successfully set socket %s to allow web server to write to it.", BE.recgen_socket_location);

    start_hum_worker(hum_fd);
    // end else we're talking to hum server
#endif
    return (0);
} // end main()


//
// This guy generates the valence cache which is a binary file representation of the bb data structure.

// To use, invoke the recgen executable with "--valence-cache-gen".
// No need to worry about ports, webserving, nor a database.
//
void gen_valence_cache()
{
    printf("Begin timing for generating valence cache.\n");
    const long long start = current_time_millis();

    populate_ncv();

    // Load up BigMem with valences and don't load the DS. We need to stagger loading the Beast and DS b/c RAM limitations during DS creation.
    const bool retval = load_beast(LOAD_VALENCES_FROM_VALGEN, false);
    if (true != retval)
        exit(EXIT_MEMLOAD);

    // Export the bb stuff to binary file.
    export_beast();

    // Can't create the DS stuff b/c free'ing isn't really putting the mem back on the heap, so further work here will crash.

    // Record the end time.
    const long long finish = current_time_millis();
    printf("Time to generate valence cache: %d milliseconds.\n", (int) (finish - start));
} // end gen_valence_cache()


//
// This guy generates the DS valence cache which is a binary file representation of the bb data structure.
//
// To use, invoke the recgen executable with "-d".
// No need to worry about ports, webserving, nor a database.

void gen_valence_cache_ds_only()
{
    // Declare time variables.

    printf("Begin timing for generating DS valence cache.\n");
    const long long start = current_time_millis();

    populate_ncv();

    // Create the DS stuff.
    create_ds();

    // Export the DS stuff to binary file.
    export_ds();

    // Record the end time.
    const long long finish = current_time_millis();
    printf("Time to generate valence cache is: %d milliseconds.\n", (int) (finish - start));
} // end gen_valence_cache_ds_only()
