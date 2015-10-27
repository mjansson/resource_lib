/* local.h  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
 *
 * This library provides a cross-platform resource I/O library in C11 providing
 * basic resource loading, saving and streaming functionality for projects based
 * on our foundation library.
 *
 * The latest source code maintained by Rampant Pixels is always available at
 *
 * https://github.com/rampantpixels/render_lib
 *
 * The foundation library source code maintained by Rampant Pixels is always available at
 *
 * https://github.com/rampantpixels/foundation_lib
 *
 * This library is put in the public domain; you can redistribute it and/or modify it without any restrictions.
 *
 */

#pragma once

#include <foundation/platform.h>

#include <resource/types.h>

#if RESOURCE_ENABLE_LOCAL_SOURCE

RESOURCE_API resource_source_t*
resource_source_allocate(void);

RESOURCE_API void
resource_source_deallocate(resource_source_t* source);

RESOURCE_API void
resource_source_initialize(resource_source_t* source);

RESOURCE_API void
resource_source_finalize(resource_source_t* source);

RESOURCE_API void
resource_source_set(resource_source_t* source, tick_t timestamp, hash_t key,
                    const char* value, size_t length);

RESOURCE_API bool
resource_source_read(resource_source_t* source, stream_t* stream);

RESOURCE_API bool
resource_source_write(resource_source_t* source, stream_t* stream);

RESOURCE_API hashmap_t*
resource_source_map(resource_source_t* source);

#else

#define resource_source_allocate() nullptr
#define resource_source_deallocate(source) memory_deallocate(source)
#define resource_source_initialize(source) ((void)sizeof(source))
#define resource_source_finalize(source) ((void)sizeof(source))
#define resource_source_set(source, timestamp, key, value, length) ((void)sizeof(source)), ((void)sizeof(timestamp)), ((void)sizeof(key)), ((void)sizeof(value)), ((void)sizeof(length))
#define resource_source_read(source, stream) ((void)sizeof(source)), ((void)sizeof(stream)), false
#define resource_source_write(source, stream) ((void)sizeof(source)), ((void)sizeof(stream)), false
#define resource_source_map(source) nullptr

#endif
