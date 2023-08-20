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

#ifndef VALGEN_H
#define VALGEN_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/time.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include <syslog.h>
#include <float.h>
#include <time.h>
#include "bmh-config.h"

// Defines

// How many threads?
#define NTHREADS 8

// ratings variables
#define RATINGS_THRESH 3
#define BYTES_RATN_FOR_VALGEN 48
#define MAX_RATN_FOR_VALGEN 48

// spear-specific
#define RANK(I,J) rank[(I)*2 + (J)]
#define BX(I,J) bx[(I)*2 + (J)]

// linefit-specifc
#define INFINITE_SLOPE 1

// big_rat_export-specific
#define BIG_RAT_OUTFILE "big_rat.bin"
#define BIG_RAT_INDEX_OUTFILE "big_rat_index.bin"

// the most number of internal buckets we can have
#define MAX_BUCKETS 32

// Typedefs

typedef struct {
    uint32_t userId;
    uint32_t eltid;
    uint8_t rating;
    uint8_t pad[3];
} Rating;

typedef struct {
    uint8_t rat1[BYTES_RATN_FOR_VALGEN];
    uint8_t rat2[BYTES_RATN_FOR_VALGEN];
    int8_t num_rat;
} pair_t;

typedef struct {
    uint32_t eltid;   // id of element
    uint32_t rcount;  // number of ratings for this element
    uint8_t pop;      // popularity of element, can be a digit from 1-7 with 1 most popular
    char pad[3];      // padding
} popularity_t;

// Bemorehuman-internal globals
extern uint8_t g_ratings_scale;


//
// Prototypes of things used outside the function's own source file
//

// in BigRat.c
extern int big_rat_init(void);
extern void big_rat_pull_from_flat_file(void);
extern void export_br(void);
extern Rating *br, *brds;

// in precursors.c
extern pair_t *g_pairs[NTHREADS];
extern void build_pairs(uint32_t, uint32_t);
extern void buildValsInit(uint32_t);
extern void buildValences(uint32_t, uint32_t);
extern double spearman(int , const uint8_t *);

// in main.c
extern uint32_t *br_index;
#endif // VALGEN_H
