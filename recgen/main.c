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
static rating_t *g_big_rat;
static uint32_t *g_big_rat_index;
static uint32_t g_event_counter = 0;
static event_t g_events_to_persist[EVENTS_TO_PERSIST_MAX];
static bool wait_for_valence_reload = false;

uint8_t g_output_scale = 5;


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
#ifdef USE_FCGI
    FCGX_Request *f_req;
    f_req = (FCGX_Request *) request;
#else
    hum_request *h_req;
    h_req = (hum_request *) request;
#endif

    size_t post_len;
    uint8_t post_data[FCGX_MAX_INPUT_STREAM_SIZE];

    InternalSingleRec *message_in;

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
    // Now we are ready to decode the message.
    message_in = internal_single_rec__unpack(NULL, post_len, post_data);

    // Check for errors.
    if (!message_in)
    {
        syslog(LOG_ERR, "Protobuf decoding failed for internal singlerec.");
        syslog(LOG_ERR, "post_len: %lu, post_data[0]: %x, post_data[1]: %x", post_len,
               post_data[0], post_data[1]);
        return;
    }

    bool error_state = false;
    int target_id;

    InternalSingleRecResponse message_out = INTERNAL_SINGLE_REC_RESPONSE__INIT;

    // Does the passed-in personid match a user we know about?
    if (message_in->personid > 0 && message_in->personid <= BE.num_people && 0 != g_big_rat_index[message_in->personid])
    {
        // We can generate recs for this person
        target_id = (int) message_in->elementid;

        // Begin making the recs real!
        // 1. get the ratings for the user
        int num_rats;

        // Check if we're at the max person_id first.
        if (BE.num_people != message_in->personid)
            num_rats = g_big_rat_index[message_in->personid + 1] - g_big_rat_index[message_in->personid];
        else
            num_rats = BE.num_ratings - g_big_rat_index[message_in->personid];

        // Limit what we care about to MAX_RATS_PER_PERSON.
        if (num_rats > MAX_RATS_PER_PERSON) num_rats = MAX_RATS_PER_PERSON;

        rating_t *ratings;
        prediction_t *recs;

        // This is the list of ratings given by the current user.
        ratings = (rating_t*)calloc((size_t) (num_rats) + 1, sizeof(rating_t));

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
            if (target_id != ratings[i].elementid)
            {
                ratings[i].elementid = g_big_rat[g_big_rat_index[message_in->personid] + i].elementid;
                ratings[i].rating = g_big_rat[g_big_rat_index[message_in->personid] + i].rating;
                i++;
            }
            else
            {
                // We found it in the ratings, so decrease the num_rats
                num_rats--;
            }
            if (i == num_rats) done = true;
        } // end while not done

        // 2. Pass ratings to recgen core.
        popularity_t max_obscurity = 7;  // 1 is most popular, 7 is most obscure. 7 includes 1-6, 3 includes 1-2, etc.
        if (predictions(ratings, num_rats, recs, 1, target_id, max_obscurity))
        {
            // Do something with recs, like print 'em out.
            // syslog(LOG_INFO, "element: %d recvalue: %d.%.6d accum: %d.%.6d count:%d",
            //       recs[0].elementid,
            //       (int) recs[0].rating,
            //       (int) ((recs[0].rating - (int) recs[0].rating) * 1000000),
            //       (int) recs[0].rating_accum,
            //       (int) ((recs[0].rating_accum - (int) recs[0].rating_accum) * 1000000),
            //       recs[0].rating_count);

            double current_pred_value = (double) recs[0].rating;
            int int_result;
            double floaty;

            // Scale what we return from 32 to g_output_scale.
            // orig current_pred_value = current_pred_value / (32 / g_output_scale);
            floaty = 32.0 / (double) g_output_scale;
            current_pred_value = (double) current_pred_value / floaty;
            int_result = (int) bmh_round(current_pred_value);
            if (0 == int_result) int_result = 1;
            message_out.result = int_result;
        }// end if successful return from Predictions() call
        else
        {
            syslog(LOG_ERR, "No predictions generated for user %d",
                   message_in->personid);
            error_state = true;
        }
        // end making the recs real! This is the merging of recgen server and real recs.

        // Clear out stuff for next user.
        free(ratings);
        free(recs);
    } // end check for valid personid
    else
    {
        error_state = true;
        syslog(LOG_ERR,
               "Error: received personid that's not in the db for situation internal_singlerec: %d",
               message_in->personid);
    }

    // Finalize stuff.
    message_out.status = malloc(sizeof(char) * STATUS_LEN);
    if (error_state)
    {
        strlcpy(message_out.status, "error", sizeof(message_out.status));
    }
    else
    {
        strlcpy(message_out.status, "ok", sizeof(message_out.status));
    }

    // Continue constructing the protobuf response.
    void *buf;                    // buffer to store serialized data
    size_t len;                   // length of serialized data

    // Finish constructing the protobuf message.
    len = internal_single_rec_response__get_packed_size(&message_out);  // this is calculated packing length
    buf = malloc(len);                                                  // Allocate required serialized buffer length
    internal_single_rec_response__pack(&message_out, buf);              // Pack the data

    // Sent the protobuf to the client.
    int bytes_to_fcgi;

    bytes_to_fcgi = (int) len;

#ifdef USE_FCGI
    bytes_to_fcgi = FCGX_PutStr((const char *) buf, (int) len, f_req->out);
#else
    h_req->out->content_length = len;
    h_req->out->type_status = HUM_RESPONSE_OK;
    strncpy((char *) h_req->out->content, (const char *) buf, (int) len);
#endif

    if (bytes_to_fcgi != (int) len)
        syslog(LOG_ERR, "ERROR: bytes_to_fcgi is %d while message_length is %lu", bytes_to_fcgi, len);

    // Clean up.
    free(message_out.status);
    free(buf);
    internal_single_rec__free_unpacked(message_in, NULL);

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

#ifdef USE_FCGI
    FCGX_Request *f_req;
    f_req = (FCGX_Request *) request;
#else
    hum_request *h_req;
    h_req = (hum_request *) request;
#endif

    RecsResponse pb_response = RECS_RESPONSE__INIT;              // declare the response
    void *buffer = NULL;                                         // this is the output buffer
    size_t len = 0;
    RecItem **recitems = NULL;
    pb_response.status = malloc(sizeof(char) * STATUS_LEN);
    popularity_t *pop = pop_leash();
    int bytes_to_fcgi;

    uint8_t post_data[FCGX_MAX_INPUT_STREAM_SIZE];

    // Get the POSTed protobuf.
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
    uint32_t personid;
    popularity_t popularity;
    int num_rats;

    // 1. Get the ratings for the person.

    Recs *message_in;
    message_in = recs__unpack(NULL, post_len, post_data);
    // Check for errors
    if (message_in == NULL)
    {
        syslog(LOG_ERR, "Protobuf decoding failed for recs() invocation.");
        strlcpy(pb_response.status, "Protobuf decoding failed for recs() invocation.", sizeof(pb_response.status));
        len = recs_response__get_packed_size(&pb_response);
        buffer = malloc(len);
        recs_response__pack(&pb_response, buffer);
        goto finish_up;
    }
    // Does the passed-in personid not match any people we know about?
    if (message_in->personid < 0 || message_in->personid > BE.num_people)
    {
        syslog(LOG_ERR, "personid from client is incorrect: ---%d---", message_in->personid);
        sprintf(pb_response.status, "personid from client is incorrect: %d", message_in->personid);
        len = recs_response__get_packed_size(&pb_response);
        buffer = malloc(len);
        recs_response__pack(&pb_response, buffer);
        goto finish_up;
    }
    personid = message_in->personid;
    popularity = (popularity_t) message_in->popularity;
    recs__free_unpacked(message_in, NULL);

    // Check if we're at the max person_id first.
    if (BE.num_people != personid)
        num_rats = g_big_rat_index[personid + 1] - g_big_rat_index[personid];
    else
        num_rats = BE.num_ratings - g_big_rat_index[personid];

    // Limit what we care about to MAX_RATS_PER_PERSON.
    if (num_rats > MAX_RATS_PER_PERSON) num_rats = MAX_RATS_PER_PERSON;

    int i;

    if (0 == num_rats)
    {
        // No ratings for this person. Problem!
        strlcpy(pb_response.status, "no ratings for this user", sizeof(pb_response.status));
        goto finish_up;
    }

    // We can generate recs for this person.
    rating_t *ratings;
    prediction_t *recs;

    // This is the list of ratings given by the current user.
    ratings = (rating_t *) calloc(MAX_RATS_PER_PERSON + 1, sizeof(rating_t));

    // Bail roughly if we can't get any mem.
    if (!ratings)
        exit(EXIT_NULLRATS);

    // This is the list of recommendations for this user.
    recs = (prediction_t *) calloc(MAX_PREDS_PER_PERSON, sizeof(prediction_t));

    // Bail roughly if we can't get any mem.
    if (!recs)
        exit(EXIT_NULLPREDS);

    // Get personid ratings.
    for (i = 0; i < num_rats; i++)
    {
        ratings[i].elementid = g_big_rat[g_big_rat_index[personid] + i].elementid;
        ratings[i].rating =    g_big_rat[g_big_rat_index[personid] + i].rating;
    }

    // 2. Pass ratings to recgen core.
    if (predictions(ratings, num_rats, recs, RECS_BUCKET_SIZE, 0, popularity))
    {
        // Begin protobuf encapsulation.
        recitems = malloc(sizeof(RecItem *) * RECS_BUCKET_SIZE);
        for (i = 0; i < RECS_BUCKET_SIZE; i++)
        {
            // add the rec data to the protobuf response
            recitems[i] = malloc(sizeof(RecItem));
            rec_item__init(recitems[i]);
            recitems[i]->elementid = (uint32_t) recs[i].elementid;

            double floaty;

            // Scale what we return from 32 to g_output_scale.
            // orig recs[i].rating = recs[i].rating / (32 / g_output_scale);
            floaty = 32.0 / (double) g_output_scale;
            recs[i].rating = (float) recs[i].rating / floaty;

            recitems[i]->rating = (int32_t) bmh_round((double) recs[i].rating);
            if (0 == recitems[i]->rating) recitems[i]->rating = 1;
            recitems[i]->popularity = pop[recs[i].elementid];
        } // end for loop

        pb_response.n_recslist = (size_t) RECS_BUCKET_SIZE;
        pb_response.recslist = recitems;
        strlcpy(pb_response.status, "ok", sizeof(pb_response.status));
        len = recs_response__get_packed_size(&pb_response);
        buffer = malloc(len);
        recs_response__pack(&pb_response, buffer);

        // end protobuf encapsulation
    } // end if successful return from Predictions() call
    else
    {
        syslog(LOG_ERR,
               "No predictions generated for user %d",
               personid);
    }

    strlcpy(pb_response.status, "ok", sizeof(pb_response.status));

    // Clear out stuff for next user.
    free(ratings);
    free(recs);

    // Send the protobuf to the client.
    finish_up:

    bytes_to_fcgi = (int) len;

#ifdef USE_FCGI
    bytes_to_fcgi = FCGX_PutStr((const char *) buffer, (int) len, f_req->out);
#else
    h_req->out->content_length = len;
    h_req->out->type_status = HUM_RESPONSE_OK;
    strncpy((char *) h_req->out->content, (const char *) buffer, (int) len);
#endif

    if (bytes_to_fcgi != (int) len)
        syslog(LOG_ERR,
               "ERROR: in recs, bytes_to_fcgi is %d while message_length is %lu",
               bytes_to_fcgi, len);

    // Free protobuf allocations.
    free(buffer);                          // Free the allocated serialized buffer
    free(pb_response.status);
    for (size_t j = 0; j < pb_response.n_recslist; j++)
        free(recitems[j]);
    free(recitems);

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
    // 2. Construct & send protobuf output.
    // external webserver means the input type is really *FCGX_Request
    // internal webserver means the input type is really *hum_request

#ifdef USE_FCGI
    FCGX_Request *f_req;
    f_req = (FCGX_Request *) request;
#else
    hum_request *h_req;
    h_req = (hum_request *) request;
#endif

    EventResponse pb_response = EVENT_RESPONSE__INIT;              // declare the response
    void *buffer = NULL;                                         // this is the output buffer
    size_t len;
    pb_response.status = malloc(sizeof(char) * STATUS_LEN);

    int bytes_to_fcgi;

    uint8_t post_data[FCGX_MAX_INPUT_STREAM_SIZE];

    // Get the POSTed protobuf.
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
    event_t event;
    event.personid = 0;
    event.elementid = 0;
    event.eventval = 0;

    // 1. Persist input event to in-mem structure and maybe filesystem.
    Event *message_in;
    message_in = event__unpack(NULL, post_len, post_data);
    // Check for errors
    if (message_in == NULL)
    {
        syslog(LOG_ERR, "Protobuf decoding failed for event() invocation.");
        strlcpy(pb_response.status, "Protobuf decoding failed for event() invocation.", sizeof(pb_response.status));
        len = event_response__get_packed_size(&pb_response);
        buffer = malloc(len);
        event_response__pack(&pb_response, buffer);
        goto finish_up;
    }
    // Does the passed-in personid not match any people we know about?
    if (message_in->personid < 0 || message_in->personid > BE.num_people)
    {
        syslog(LOG_ERR, "personid from client is incorrect: ---%d---", message_in->personid);
        sprintf(pb_response.status, "personid from client is incorrect: %d", message_in->personid);
        len = event_response__get_packed_size(&pb_response);
        buffer = malloc(len);
        event_response__pack(&pb_response, buffer);
        goto finish_up;
    }
    event.personid = message_in->personid;

    // Does the passed-in elementid not match any element we know about?
    if (message_in->elementid < 0 || message_in->elementid > BE.num_elts)
    {
        syslog(LOG_ERR, "elementid from client is incorrect: ---%d---", message_in->elementid);
        sprintf(pb_response.status, "elementid from client is incorrect: %d", message_in->elementid);
        len = event_response__get_packed_size(&pb_response);
        buffer = malloc(len);
        event_response__pack(&pb_response, buffer);
        goto finish_up;
    }
    event.elementid = message_in->elementid;
    if (message_in->eventval) event.eventval = message_in->eventval;
    event__free_unpacked(message_in, NULL);

    // Save the event to our in-mem structure;
    g_events_to_persist[g_event_counter] = event;
    g_event_counter++;

    // Are we ready to persist now?
    if (g_event_counter == EVENTS_TO_PERSIST_MAX)
    {
        char line[512];

        FILE *fp;
        char fname[255];
        strlcpy(fname, BE.bmh_events_file, sizeof(fname));

        // MUST be "a" and not "w" because this file gets appended to.
        fp = fopen(fname,"a");

        if (NULL == fp)
        {
            syslog(LOG_ERR, "ERROR: cannot open output file %s", fname);
            exit(1);
        }

        for (int i = 0; i < g_event_counter; i++)
        {
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

    // 2. Finish constructing response
    strlcpy(pb_response.status, "ok", sizeof(pb_response.status));
    len = event_response__get_packed_size(&pb_response);
    buffer = malloc(len);
    event_response__pack(&pb_response, buffer);

    // Send the protobuf to the client.
    finish_up:

    bytes_to_fcgi = (int) len;

#ifdef USE_FCGI
    bytes_to_fcgi = FCGX_PutStr((const char *) buffer, (int) len, f_req->out);
#else
    h_req->out->content_length = len;
    h_req->out->type_status = HUM_RESPONSE_OK;
    strncpy((char *) h_req->out->content, (const char *) buffer, (int) len);
#endif

    if (bytes_to_fcgi != (int) len)
        syslog(LOG_ERR,
               "ERROR: in event, bytes_to_fcgi is %d while message_length is %lu",
               bytes_to_fcgi, len);

    // Free protobuf allocations.
    free(buffer);                          // Free the allocated serialized buffer
    free(pb_response.status);

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
        FCGX_PutS("Content-Type: application/octet-stream\r\n\r\n", request.out);

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
        ssize_t bytes_read;
        hum_record record_in;
        char uri[256];
        ssize_t len_request_uri;

        // Get a request along with POST data.

        // Now ready to accept. Use a new socket for this request.
        int cl_fd;

        // We don't care atm about the address & len of the connecting peer. Make them NULL.
        // This connection will be on a new socket, cl_fd. The listening socket hum_fd remains
        // open and can accept further connections.
        // This will block.
        cl_fd = accept(hum_fd, NULL, NULL);

        // Read field-by-field
        // Get the first byte which is the type.
        bytes_read = read(cl_fd, &record_in.type_status, sizeof(uint8_t));
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
            bytes_read = read(cl_fd, &record_in.content_length, sizeof(uint32_t));

            // Get the content.
            bytes_read = read(cl_fd, &record_in.content, record_in.content_length);
            if (bytes_read != record_in.content_length)
            {
                syslog(LOG_ERR, "bytes_read %zd is not the same as content_length %d", bytes_read, record_in.content_length);
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
            bytes_read = read(cl_fd, &record_in.content_length, sizeof(uint32_t));

            // Get the content.
            bytes_read = read(cl_fd, &record_in.content, record_in.content_length);
            if (bytes_read != record_in.content_length)
            {
                syslog(LOG_ERR, "bytes_read %zd for POST is not the same as content_length %d", bytes_read, record_in.content_length);
                exit(EXIT_FAILURE);
            }
        }

        // So now we have uri, and POST data in record_in.content of length: record_in.content_length

        len_request_uri = (ssize_t) strlen(uri);

        // Do we need to wait for valences to reload?
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
        }
        else if (request.out->type_status == HUM_RESPONSE_ERROR)
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
    FILE *fp;
    strlcpy(filename, BE.working_dir, sizeof(filename));
    strlcat(filename, "/", sizeof(filename));
    strlcat(filename, NUM_CONF_OUTFILE, sizeof(filename));

    fp = fopen(filename, "r");
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
    // Declare time variables.
    long long start, finish;

    // Get the num_confident_valences.
    populate_ncv();

    // Begin beast stuff.
    syslog(LOG_INFO, "Begin timing for loading valences.");
    start = current_time_millis();

    bool retval;

    // Load up Beast with valences and load the DS.
    retval = load_beast(LOAD_VALENCES_FROM_BEAST_EXPORT, true);
    if (true != retval)
        exit(EXIT_MEMLOAD);

    finish = current_time_millis();
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
    setlogmask(LOG_UPTO (RECGEN_LOG_MASK));
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
    syslog(LOG_INFO, "*** Setting recgen to use FastCGI to talk to external webserver");
#endif

    // Use getopt to help manage the options on the command line.
    int opt;
    while ((opt = getopt(argc, argv, "cdb:")) != -1)
    {
        switch (opt)
        {
            case 'c':   // for "cache generation"
                printf("*** Generating valence cache ***\n");
                gen_valence_cache();
                syslog(LOG_INFO, "*** End recgen valence cache generation");
                exit(EXIT_SUCCESS);
            case 'd':   // for "DS cache generation"
                printf("*** Generating DS only valence cache ***\n");
                gen_valence_cache_ds_only();
                syslog(LOG_INFO, "*** End recgen valence cache DS generation");
                exit(EXIT_SUCCESS);
            case 'b':   // for "recommendation-buckets"
                if (strtol(optarg, NULL, 10) < 2 || strtol(optarg, NULL, 10) > 32)
                {
                    printf("Error: the argument for -b should be > 1 and < 33 instead of %s. Exiting. ***\n", optarg);
                    syslog(LOG_ERR, "The argument for -b should be > 1 and < 33, instead of %s. Exiting. ***\n", optarg);
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

    printf("*** Starting the live recommender with recs using a %d-bucket scale ***\n", g_output_scale);
    syslog(LOG_INFO, "*** Start recgen live recommender with recs using a %d-bucket scale ***", g_output_scale);

    // Populate the beast, g_pop, and ratings structures.
    initialize_structures();

    // Begin connecting to socket.
    // Strip off the filename from BE.recgen_socket_location.
    size_t len = strlen(BE.recgen_socket_location);
    char path[len+1];
    strlcpy(path, BE.recgen_socket_location, sizeof(path));
    char *fstart = strrchr(path, '/');
    if (NULL == fstart)
    {
        path[0] = '.';
        path[1] = '\0';
    }
    else
    {
        size_t flen = strlen(fstart);
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
            syslog(LOG_ERR, "Killall failed to get rid of previous hum process(es) because of a permissions issue. Exiting.");
            exit(EXIT_FAILURE);
        }
    }

    // Start double-forking code to get hum to be its own independent process.
    printf("forking process pid: %d\n", getpid());
    pid_t p1 = fork();

    // NOTE: if pid == 0 it's the child process
    if (p1 != 0)
    {
        printf("p1 process id is %d\n", getpid());
        int status;
        wait(&status);
        system("ps");
    }
    else
    {
        pid_t p2 = fork();
        int pid = getpid();

        if (p2 != 0)
        {
            printf("p2 process id is %d\n", pid);
            exit(0);
        }
        else
        {
            printf("p3 process id is %d\n", pid);
        }
        printf("I'm the grandchild with pid %d.\n", getpid());

        // Start the hum server.
        char portstr[6];
        itoa(HUM_DEFAULT_PORT, portstr);
        execlp("hum", "hum", "-p", portstr, (char *) NULL);
    }
    // end double-forking stuff

    // Open socket that will talk to our hum server.
    int hum_fd;
    struct sockaddr_un process_address;

    hum_fd = socket(AF_UNIX, SOCK_STREAM, 0);
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
    if(bind(hum_fd, (struct sockaddr *) &process_address, sizeof(struct sockaddr_un)) < 0
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
    // Declare time variables:
    long long start, finish;
    
    printf("Begin timing for generating valence cache.\n");
    start = current_time_millis();
    bool retval;

    populate_ncv();
    
    // Load up BigMem with valences and don't load the DS. We need to stagger loading the Beast and DS b/c RAM limitations during DS creation.
    retval = load_beast(LOAD_VALENCES_FROM_VALGEN, false);
    if (true != retval)
        exit(EXIT_MEMLOAD);
    
    // Export the bb stuff to binary file.
    export_beast();

    // Can't create the DS stuff b/c free'ing isn't really putting the mem back on the heap, so further work here will crash.
    
    // Record the end time.
    finish = current_time_millis();
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
    long long start, finish;
    
    printf("Begin timing for generating DS valence cache.\n");
    start = current_time_millis();

    populate_ncv();

    // Create the DS stuff.
    create_ds();
    
    // Export the DS stuff to binary file.
    export_ds();

    // Record the end time.
    finish = current_time_millis();
    printf("Time to generate valence cache: %d milliseconds.\n", (int) (finish - start));
    
} // end gen_valence_cache_ds_only()
