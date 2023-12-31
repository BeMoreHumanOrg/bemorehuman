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

#ifndef RECGEN_H
#define RECGEN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#ifdef USE_FCGI
#include <fcgiapp.h>
#endif
#include <pthread.h>
#include "recgen.pb-c.h"
#include "bmh-config.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <sys/wait.h>
#include <yyjson.h>

//
// Constants
//
#define LOG_MODULE_STRING "recgen"

// These are the 4 files that we load quick-fast in a hurry into the corresponding data structures.
#define VALENCES_BB "bb.bin"
#define VALENCES_BB_SEG "bb_seg.bin"
#define VALENCES_BB_DS "bb_ds.bin"
#define VALENCES_BB_SEG_DS "bb_seg_ds.bin"

#define RATINGS_BR "big_rat.bin"
#define RATINGS_BR_INDEX "big_rat_index.bin"

#define POP_OUTFILE "pop.out"
#define SO_COMP_OUTFILE "so_compressed.out"
#define NUM_CONF_OUTFILE "num_confident_valences.out"

// These are the ways we can read the valences
#define LOAD_VALENCES_FROM_VALGEN 1
#define LOAD_VALENCES_FROM_BEAST_EXPORT 2

#define RECGEN_LOG_MASK LOG_INFO
#define MIN_VALENCES_FOR_PREDICTIONS 1
#define RATINGS_BOUND_LOWER 10  // keeping things multiplied by FLOAT_TO_SHORT_MULT until the last possible moment
#define RATINGS_BOUND_UPPER 320  // keeping things multiplied by FLOAT_TO_SHORT_MULT until the last possible moment
#define MAX_PREDS_PER_PERSON 200
#define MAX_RATS_PER_PERSON 1000
#define FLOAT_TO_SHORT_MULT 10
#define FLOAT_TO_SHORT_MULT_SQ 100
#define NUM_SO_BUCKETS 16       // How many slope/offset buckets do we want?
#define RECS_BUCKET_SIZE 20

#define FCGX_MAX_INPUT_STREAM_SIZE 20480
#define STATUS_LEN 100
#define EXIT_MEMLOAD 1
#define EXIT_NULLRATS 6
#define EXIT_NULLPREDS 7

#define MAX_STACK 128              // stack size for max 2^(128/2) array elements when sorting
#define EVENTS_TO_PERSIST_MAX 100   // how many incoming events to store in RAM before persisting to disk?

#define HUM_BUFFER_SIZE 4096
#define HUM_DEFAULT_PORT 8888

#define LOG_HUM_STRING "hum"
#define HUM_LOG_MASK LOG_INFO

// This expects a valence_xy_t for both args.
#define ASSIGN(a,b) do { \
    a.x[0] = b.x[0]; \
    a.x[1] = b.x[1]; \
    a.x[2] = b.x[2]; \
    a.eltid[0] = b.eltid[0]; \
    a.eltid[1] = b.eltid[1]; \
    a.eltid[2] = b.eltid[2]; \
    a.soindex = b.soindex; \
} while (0)

// These 2 defines tackle assignment and unassignment of an element id to Element_id_t.
#define COMPACT(a, b) do { \
    a[2] = (b) & 0xff; \
    a[1] = (b >> 8) & 0xff; \
    a[0] = (b >> 16) & 0xff; \
} while (0)

#define EXPAND(a,b) do { \
	b = ((unsigned) a[0] << (unsigned) 16); \
	b += ((unsigned) a[1] << (unsigned) 8); \
	b += (a[2]); \
} while (0)

// Use this before SETHIBITS.
#define SETELT(a, b) do { \
    a[2] = (b) & 0xff; \
    a[1] = (b >> 8) & 0xff; \
    a[0] = (b >> 16) & 0x0f; \
} while (0)

// Use this before SETLOBITS.
#define SETHIBITS(a, b) do { \
a = ((unsigned) b << (unsigned) 4); \
} while (0)

#define SETLOBITS(a, b) do { \
a += ((unsigned) b); \
} while (0)

#define GET_ELT(a) (((uint32_t)a[0] << 16) | ((uint32_t)a[1] << 8) | a[2])
#define GET_LOW_4_BITS(byte) ((byte) & 0x0F)
#define GET_HIGH_4_BITS(byte) ((byte) >> 4)

//
// Typedefs
//
typedef uint8_t element_id_t[3];     // 24 bits for y (in the x,y pair -- x is in the bind_seg)
typedef uint32_t exp_elt_t;          // convenient way to deal with element id's
typedef int64_t bb_ind_t;            // type of each elt in the bb, -1 value means no value
typedef struct                       // slope/offset compression helper type
{
    int8_t guy;
    uint32_t count;
} guy_t;

typedef struct
{
    element_id_t eltid;	      // half of the x,y pair. the other half is in the bind_seg. for bb, this is y. for bb_ds this is x.
    uint8_t soindex;          // slope-offset index. hi fewbits are index to the slope, lower 4 bits are the index to the offset
} valence_t;

typedef struct
{
    element_id_t x;	          // half of the x,y pair. (x)
    element_id_t eltid;	      // other half of the x,y pair. (y)
    uint8_t soindex;          // slope-offset index. hi four bits are index to the slope, lower 4 bits are the index to the offset
} valence_xy_t;

typedef struct
{
    uint8_t fewbit;           // this is the few-bit representation of the value (3 bits for 8 values, 4 bits for 16 values)
	int8_t value;
} fewbit_t;

typedef uint8_t popularity_t; // range is 1-7 where 1 is very popular and 7 is obscure

typedef struct
{
    int userid;
    exp_elt_t  elementid;
    short rating;
    char padding[2];
} rating_t;

typedef struct
{
    exp_elt_t personid;
    exp_elt_t elementid;
    exp_elt_t eventval;
} event_t;

typedef struct
{
    exp_elt_t elementid;
    int rating_count;
    int rating;
    uint32_t rating_accum;
} prediction_t;

typedef struct
{
    int fcgi_fd;
} FCGI_info_t;

// Hum server structures
typedef struct
{
    uint8_t type_status;     // This is either the request type or return status.
    uint32_t content_length;
    uint8_t content[HUM_BUFFER_SIZE];
} hum_record;

typedef struct
{
    hum_record *in;
    hum_record *out;
} hum_request;

enum
{
    HUM_REQUEST_URI = 1,
    HUM_POST_DATA = 2,
    HUM_RESPONSE_OK = 10,
    HUM_RESPONSE_ERROR = 11,
};

// This is the function pointer interface for communication protocols like JSON and protobuf.
typedef struct
{
    void *(*serialize)(const void *data, const char *status, size_t *len);       // conversion
    void *(*deserialize)(const size_t, const void *data, int *status);     // conversion
    int (*send)(int sockfd, const void *data, size_t len);   // network op
    int (*receive)(int sockfd, void *buf, size_t len);       // network op
} protocol_interface;

typedef struct
{
    uint32_t elementid;
    int32_t rating;
} rating_item_t;

typedef struct
{
    uint32_t personid;
    int32_t popularity;
    int32_t num_ratings;
    rating_item_t *ratings_list;
} recs_request_t;

// error messages
enum {STATUS_OK,
      PROTOBUF_DECODE_FAILED_FOR_RECS,
      PERSONID_FROM_CLIENT_INCORRECT,
      NO_RATINGS_FOR_USER};

extern const char *error_strings[];


extern size_t g_num_confident_valences;
extern uint8_t g_output_scale;

//
// Prototypes of things used outside the function's own source file
//

// in big_mem.c
extern bool pop_load(void);

extern bool load_beast(int, bool);

extern bool big_rat_load(void);

extern bool create_ds(void);

extern void export_beast(void);

extern void export_ds(void);

extern valence_t *bb_leash(void);

extern bb_ind_t *bind_seg_leash(void);

extern valence_t *bb_ds_leash(void);

extern bb_ind_t *bind_seg_ds_leash(void);

extern popularity_t *pop_leash(void);

extern int8_t *tiny_slopes_leash(void);

extern double *tiny_slopes_inv_leash(void);

extern int8_t *tiny_offsets_leash(void);

extern rating_t *big_rat_leash(void);

extern uint32_t *big_rat_index_leash(void);

// in predictions.c
extern void create_workingset(size_t);

extern bool predictions(rating_t[], int, prediction_t[], int, int, popularity_t);

// in main.c
extern void gen_valence_cache(void);

extern void gen_valence_cache_ds_only(void);

#endif // RECGEN_H
