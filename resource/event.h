/* event.h  -  Resource library  -  Public Domain  -  2014 Mattias Jansson
 *
 * This library provides a cross-platform resource I/O library in C11 providing
 * basic resource loading, saving and streaming functionality for projects based
 * on our foundation library.
 *
 * The latest source code maintained by Mattias Jansson is always available at
 *
 * https://github.com/mjansson/resource_lib
 *
 * The foundation library source code maintained by Mattias Jansson is always available at
 *
 * https://github.com/mjansson/foundation_lib
 *
 * This library is put in the public domain; you can redistribute it and/or modify it without any
 * restrictions.
 *
 */

#pragma once

#include <foundation/platform.h>

#include <resource/types.h>

RESOURCE_API uuid_t
resource_event_uuid(const event_t* event);

RESOURCE_API uint64_t
resource_event_platform(const event_t* event);

RESOURCE_API hash_t
resource_event_token(const event_t* event);

RESOURCE_API void
resource_event_post(resource_event_id id, uuid_t uuid, uint64_t platform, hash_t token);

RESOURCE_API void
resource_event_post_depends(uuid_t uuid, uint64_t platform, hash_t token);

RESOURCE_API event_stream_t*
resource_event_stream(void);

/*! Handle foundation events. No other event types should be
passed to this function.
\param event Foundation event */
RESOURCE_API void
resource_event_handle(event_t* event);
