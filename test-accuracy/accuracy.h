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

#ifndef _TESTACCURACY_H_
#define _TESTACCURACY_H_

/*
 * This file holds all the commonly used stuff
 */

/*
 * Includes
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/time.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>
#include "../recgen/recgen.pb-c.h"
#include "bmh-config.h"
#include <openssl/rand.h>


/*
 * Constants. These are the bits in the code that are likely to change with a change in what's being recommended.
 */
#define QLEN 2047
#define TEST_MODE_CORE 0
#define RB_URL_LENGTH 2048
#define RB_CURL_PB_PREFIX "curl -X POST --silent --data-binary @- -H 'Content-Type: application/octet-stream' -H 'accept: application/octet-stream' http://%s/bmh/%s < ./pbfiles/scenario_%d%s.pb"
#define RB_LOCAL_CURL_PB_PREFIX "curl -X POST --silent --data-binary @- -H 'Content-Type: application/octet-stream' http://%s/bmh/%s < ./scenario_%d%s.pb"
#define STAGE_SERVER_STRING "fee.stage:4566"
#define PROD_SERVER_STRING "foo.production:4567"

#define TEST_LOC_DEV 0
#define TEST_LOC_STAGE 1
#define TEST_LOC_PROD 2
#define RB_BUCKET_SIZE 20
#define RB_LINE 8192
#define RB_LABEL_SIZE 50
#define RB_RAW_RESPONSE_SIZE_MAX 32768

#define MIN_VALENCES_FOR_PREDICTIONS 1
#define MAX_PREDS_PER_PERSON 12000
#define MAX_RATN_FOR_VALGEN 48
#define MAX_RATS_PER_PERSON 1000
#define DYNAMIC_RATE 100
#define DYNAMIC_SCAN 110

#define str5cmp(m, c0, c1, c2, c3, c4)                                    \
    m[0] == c0 && m[1] == c1 && m[2] == c2 && m[3] == c3 && m[4] == c4
#define str4cmp(m, c0, c1, c2, c3)                                    \
    m[0] == c0 && m[1] == c1 && m[2] == c2 && m[3] == c3

/*
 * Exit failure situations
 */
#define EXIT_MEMLOAD 1
#define EXIT_NULLBEAST 5
#define EXIT_NULLRATS 6
#define EXIT_NULLPREDS 7
#define EXIT_CHMODFAIL 8

/*
 * general status
 */
#define STATUS_OK 0
#define STATUS_NOT_OK 1

#define RATINGS_BR "big_rat.bin"
#define RATINGS_BR_INDEX "big_rat_index.bin"


/*
 * Typedefs
 */

typedef struct
{
    int userid;
    uint32_t  elementid;
    short rating;
    char padding[2];
} rating_t;

typedef struct {
	int elementId;
	float rating;
	float ratingAccum;
	int ratingCount;
} Prediction;

typedef char *String;

//
// Prototypes of things used outside the function's own source file
//

// in accuracy.c
// test stuff
extern void TestAccuracy(void);
extern unsigned int random_uint(unsigned int);

// in helpers.c
extern unsigned long call_bemorehuman_server(int, char *);

// globals
extern int g_server_location;


#endif /* _TESTACCURACY_ */
