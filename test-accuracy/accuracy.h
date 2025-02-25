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

#ifndef TESTACCURACY_H
#define TESTACCURACY_H

/*
 * This file holds all the commonly used stuff
 */

/*
 * Includes
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>
#include <arpa/inet.h>

#ifdef USE_PROTOBUF
#include "../recgen/recgen.pb-c.h"
#endif

#include "bmh-config.h"
#include <openssl/rand.h>
#include <yyjson.h>
#include <syslog.h>

#ifdef __FreeBSD__
#include <netinet/in.h>
#include <sys/socket.h>
#endif

/*
 * Constants. These are the bits in the code that are likely to change with a change in what's being recommended.
 */
#define RB_URL_LENGTH 2048
#define DEV_SERVER_STRING "127.0.0.1:8888"
#define DEV_SERVER "127.0.0.1"
#define DEV_SERVER_PORT 8888
#define STAGE_SERVER_STRING "fee.stage:4566"
#define STAGE_SERVER "fee.stage"
#define STAGE_SERVER_PORT 4566
#define PROD_SERVER_STRING "foo.production:4567"
#define PROD_SERVER "foo.production"
#define PROD_SERVER_PORT 4567

#define TEST_LOC_DEV 0
#define TEST_LOC_STAGE 1
#define TEST_LOC_PROD 2
#define RB_LINE 8192
#define RB_RAW_RESPONSE_SIZE_MAX 32768

#define MAX_PREDS_PER_PERSON 12000
#define DYNAMIC_RATE 100
#define DYNAMIC_SCAN 110

#define RATINGS_BR "big_rat.bin"
#define RATINGS_BR_INDEX "big_rat_index.bin"

#define FILE_TEMPLATE "/tmp/test-accuracy-XXXXXX"

// These are the different requests we can make to the server.
enum { SCENARIO_RECS, SCENARIO_EVENT, SCENARIO_SINGLEREC };

// These are the different communciation protocols we can use to talk to the server.
enum { PROTOCOL_PROTOBUF, PROTOCOL_JSON };

// Define the range or popularity
#define LOWEST_POP_NUMBER 1
#define HIGHEST_POP_NUMBER 7

// This is the function pointer interface for communication protocols like JSON and protobuf.
typedef struct
{
    void (*serialize)(const int scenario, const void *data, char **body_content, size_t *body_len); // conversion
    void *(*deserialize)(const int scenario, const void *data, char **status, const size_t len); // conversion
} protocol_interface;

typedef struct
{
    int socket;
    struct sockaddr_in server_addr;
} http_client;

//
// Prototypes of things used outside the function's own source file
//

// in accuracy.c
// test stuff
extern void TestAccuracy(void);

extern unsigned int random_uint(unsigned int);

// in helpers.c
extern unsigned long contact_bemorehuman_server(int, int, const char *, size_t, char **);
extern void cleanup_http_client(void);

// globals
extern int g_server_location;


#endif // TESTACCURACY_H
