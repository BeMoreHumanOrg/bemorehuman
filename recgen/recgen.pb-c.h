// SPDX-FileCopyrightText: 2023 Brian Calhoun <brian@bemorehuman.org>
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

/* Generated by the protocol buffer compiler.  DO NOT EDIT! */
/* Generated from: recgen.proto */

#ifndef PROTOBUF_C_recgen_2eproto__INCLUDED
#define PROTOBUF_C_recgen_2eproto__INCLUDED

#include <protobuf-c/protobuf-c.h>

PROTOBUF_C__BEGIN_DECLS

#if PROTOBUF_C_VERSION_NUMBER < 1003000
# error This file was generated by a newer version of protoc-c which is incompatible with your libprotobuf-c headers. Please update your headers.
#elif 1005000 < PROTOBUF_C_MIN_COMPILER_VERSION
# error This file was generated by an older version of protoc-c which is incompatible with your libprotobuf-c headers. Please regenerate this file with a newer version of protoc-c.
#endif


typedef struct InternalSingleRec InternalSingleRec;
typedef struct InternalSingleRecResponse InternalSingleRecResponse;
typedef struct Recs Recs;
typedef struct RecItem RecItem;
typedef struct RatingItem RatingItem;
typedef struct RecsResponse RecsResponse;
typedef struct Event Event;
typedef struct EventResponse EventResponse;


/* --- enums --- */


/* --- messages --- */

/*
 * Internal Single Rec
 */
struct  InternalSingleRec
{
  ProtobufCMessage base;
  uint32_t personid;
  uint32_t elementid;
};
#define INTERNAL_SINGLE_REC__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&internal_single_rec__descriptor) \
, 0, 0 }


struct  InternalSingleRecResponse
{
  ProtobufCMessage base;
  int32_t result;
  char *status;
};
#define INTERNAL_SINGLE_REC_RESPONSE__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&internal_single_rec_response__descriptor) \
, 0, (char *)protobuf_c_empty_string }


/*
 * Recs
 */
struct  Recs
{
  ProtobufCMessage base;
  uint32_t personid;
  int32_t popularity;
  int32_t numratings;
  size_t n_ratingslist;
  RatingItem **ratingslist;
};
#define RECS__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&recs__descriptor) \
, 0, 0, 0, 0,NULL }


struct  RecItem
{
  ProtobufCMessage base;
  uint32_t elementid;
  int32_t rating;
  uint32_t popularity;
};
#define REC_ITEM__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&rec_item__descriptor) \
, 0, 0, 0 }


struct  RatingItem
{
  ProtobufCMessage base;
  uint32_t elementid;
  int32_t rating;
};
#define RATING_ITEM__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&rating_item__descriptor) \
, 0, 0 }


struct  RecsResponse
{
  ProtobufCMessage base;
  size_t n_recslist;
  RecItem **recslist;
  char *status;
};
#define RECS_RESPONSE__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&recs_response__descriptor) \
, 0,NULL, (char *)protobuf_c_empty_string }


/*
 * Event
 */
struct  Event
{
  ProtobufCMessage base;
  uint32_t personid;
  uint32_t elementid;
};
#define EVENT__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&event__descriptor) \
, 0, 0 }


struct  EventResponse
{
  ProtobufCMessage base;
  int32_t result;
  char *status;
};
#define EVENT_RESPONSE__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&event_response__descriptor) \
, 0, (char *)protobuf_c_empty_string }


/* InternalSingleRec methods */
void   internal_single_rec__init
                     (InternalSingleRec         *message);
size_t internal_single_rec__get_packed_size
                     (const InternalSingleRec   *message);
size_t internal_single_rec__pack
                     (const InternalSingleRec   *message,
                      uint8_t             *out);
size_t internal_single_rec__pack_to_buffer
                     (const InternalSingleRec   *message,
                      ProtobufCBuffer     *buffer);
InternalSingleRec *
       internal_single_rec__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   internal_single_rec__free_unpacked
                     (InternalSingleRec *message,
                      ProtobufCAllocator *allocator);
/* InternalSingleRecResponse methods */
void   internal_single_rec_response__init
                     (InternalSingleRecResponse         *message);
size_t internal_single_rec_response__get_packed_size
                     (const InternalSingleRecResponse   *message);
size_t internal_single_rec_response__pack
                     (const InternalSingleRecResponse   *message,
                      uint8_t             *out);
size_t internal_single_rec_response__pack_to_buffer
                     (const InternalSingleRecResponse   *message,
                      ProtobufCBuffer     *buffer);
InternalSingleRecResponse *
       internal_single_rec_response__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   internal_single_rec_response__free_unpacked
                     (InternalSingleRecResponse *message,
                      ProtobufCAllocator *allocator);
/* Recs methods */
void   recs__init
                     (Recs         *message);
size_t recs__get_packed_size
                     (const Recs   *message);
size_t recs__pack
                     (const Recs   *message,
                      uint8_t             *out);
size_t recs__pack_to_buffer
                     (const Recs   *message,
                      ProtobufCBuffer     *buffer);
Recs *
       recs__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   recs__free_unpacked
                     (Recs *message,
                      ProtobufCAllocator *allocator);
/* RecItem methods */
void   rec_item__init
                     (RecItem         *message);
size_t rec_item__get_packed_size
                     (const RecItem   *message);
size_t rec_item__pack
                     (const RecItem   *message,
                      uint8_t             *out);
size_t rec_item__pack_to_buffer
                     (const RecItem   *message,
                      ProtobufCBuffer     *buffer);
RecItem *
       rec_item__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   rec_item__free_unpacked
                     (RecItem *message,
                      ProtobufCAllocator *allocator);
/* RatingItem methods */
void   rating_item__init
                     (RatingItem         *message);
size_t rating_item__get_packed_size
                     (const RatingItem   *message);
size_t rating_item__pack
                     (const RatingItem   *message,
                      uint8_t             *out);
size_t rating_item__pack_to_buffer
                     (const RatingItem   *message,
                      ProtobufCBuffer     *buffer);
RatingItem *
       rating_item__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   rating_item__free_unpacked
                     (RatingItem *message,
                      ProtobufCAllocator *allocator);
/* RecsResponse methods */
void   recs_response__init
                     (RecsResponse         *message);
size_t recs_response__get_packed_size
                     (const RecsResponse   *message);
size_t recs_response__pack
                     (const RecsResponse   *message,
                      uint8_t             *out);
size_t recs_response__pack_to_buffer
                     (const RecsResponse   *message,
                      ProtobufCBuffer     *buffer);
RecsResponse *
       recs_response__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   recs_response__free_unpacked
                     (RecsResponse *message,
                      ProtobufCAllocator *allocator);
/* Event methods */
void   event__init
                     (Event         *message);
size_t event__get_packed_size
                     (const Event   *message);
size_t event__pack
                     (const Event   *message,
                      uint8_t             *out);
size_t event__pack_to_buffer
                     (const Event   *message,
                      ProtobufCBuffer     *buffer);
Event *
       event__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   event__free_unpacked
                     (Event *message,
                      ProtobufCAllocator *allocator);
/* EventResponse methods */
void   event_response__init
                     (EventResponse         *message);
size_t event_response__get_packed_size
                     (const EventResponse   *message);
size_t event_response__pack
                     (const EventResponse   *message,
                      uint8_t             *out);
size_t event_response__pack_to_buffer
                     (const EventResponse   *message,
                      ProtobufCBuffer     *buffer);
EventResponse *
       event_response__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   event_response__free_unpacked
                     (EventResponse *message,
                      ProtobufCAllocator *allocator);
/* --- per-message closures --- */

typedef void (*InternalSingleRec_Closure)
                 (const InternalSingleRec *message,
                  void *closure_data);
typedef void (*InternalSingleRecResponse_Closure)
                 (const InternalSingleRecResponse *message,
                  void *closure_data);
typedef void (*Recs_Closure)
                 (const Recs *message,
                  void *closure_data);
typedef void (*RecItem_Closure)
                 (const RecItem *message,
                  void *closure_data);
typedef void (*RatingItem_Closure)
                 (const RatingItem *message,
                  void *closure_data);
typedef void (*RecsResponse_Closure)
                 (const RecsResponse *message,
                  void *closure_data);
typedef void (*Event_Closure)
                 (const Event *message,
                  void *closure_data);
typedef void (*EventResponse_Closure)
                 (const EventResponse *message,
                  void *closure_data);

/* --- services --- */


/* --- descriptors --- */

extern const ProtobufCMessageDescriptor internal_single_rec__descriptor;
extern const ProtobufCMessageDescriptor internal_single_rec_response__descriptor;
extern const ProtobufCMessageDescriptor recs__descriptor;
extern const ProtobufCMessageDescriptor rec_item__descriptor;
extern const ProtobufCMessageDescriptor rating_item__descriptor;
extern const ProtobufCMessageDescriptor recs_response__descriptor;
extern const ProtobufCMessageDescriptor event__descriptor;
extern const ProtobufCMessageDescriptor event_response__descriptor;

PROTOBUF_C__END_DECLS


#endif  /* PROTOBUF_C_recgen_2eproto__INCLUDED */
