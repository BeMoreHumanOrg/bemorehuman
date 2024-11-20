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

#ifndef BMH_CONFIG_H
#define BMH_CONFIG_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>


#define ITEM_SIZE 64
#define VALUE_SIZE 64
#define PATH_SIZE 256

typedef struct bemorehumanConfig
{
    uint64_t num_elts;     // number of elements we can possibly recommend
    uint64_t num_ratings;  // total number of ratings
    uint64_t num_people;   // total number of people who ratied stuff
    char events_file[PATH_SIZE];
    char bmh_events_file[PATH_SIZE];
    char ratings_file[PATH_SIZE];
    char working_dir[PATH_SIZE];
    char valence_cache_dir[PATH_SIZE];
    char recgen_socket_location[PATH_SIZE];
} bemorehumanConfig_t;

extern void load_config_file(void);
extern bemorehumanConfig_t BE;
extern long long current_time_millis(void);
extern long long current_time_micros(void);
extern bool check_make_dir(char *);
extern void itoa(int, char[]);
extern long bmh_round(double);

#ifndef HAVE_STRLCAT
#ifdef strlcat
#define HAVE_STRLCAT
#else
// strlcat() - More safely concatenate two strings.
extern
size_t                         // O - Length of string
strlcat(char *restrict,        // O - Destination string
        const char *restrict,  // I - Source string
        size_t);               // I - Size of destination string buffer

#endif // strlcat
#endif // HAVE_STRLCAT

#ifndef HAVE_STRLCPY
#ifdef strlcpy
#define HAVE_STRLCPY
#else
// strlcpy() - More safely copy one string to another.
extern
size_t                         // O - Length of string
strlcpy(char *restrict,        // O - Destination string
        const char *restrict,  // I - Source string
        size_t);               // I - Size of destination string buffer

#endif // strlcpy
#endif // HAVE_STRLCPY

//
// The following typedefs are the data structure interface between the producer (bemorehuman) and
// the consumer you use to get recommendations (e.g. test-accuarcy).
//
typedef struct
{
    uint32_t userid;
    uint32_t elementid;
    uint8_t rating;
    uint8_t padding[3];
} rating_t;

typedef struct
{
    uint32_t elementid;
    int32_t rating;
} rating_item_t;

typedef struct
{
    uint32_t personid;
    uint32_t eltid;
} event_t;

typedef struct
{
    uint32_t personid;
    int32_t popularity;
    int32_t num_ratings;
    rating_item_t *ratings_list;
} recs_request_t;

typedef struct
{
    uint32_t elementid;
    int rating_count;
    int rating;
    uint32_t rating_accum;
} prediction_t;


#endif // BMH_CONFIG_H
