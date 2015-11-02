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

RESOURCE_API string_const_t
resource_source_path(void);

RESOURCE_API bool
resource_source_set_path(const char* path, size_t length);

RESOURCE_API resource_source_t*
resource_source_allocate(void);

RESOURCE_API void
resource_source_deallocate(resource_source_t* source);

RESOURCE_API void
resource_source_initialize(resource_source_t* source);

RESOURCE_API void
resource_source_finalize(resource_source_t* source);

RESOURCE_API bool
resource_source_read(resource_source_t* source, const uuid_t uuid);

RESOURCE_API bool
resource_source_write(resource_source_t* source, const uuid_t uuid, bool binary);

RESOURCE_API void
resource_source_set(resource_source_t* source, tick_t timestamp, hash_t key,
                    uint64_t platform, const char* value, size_t length);

RESOURCE_API void
resource_source_unset(resource_source_t* source, tick_t timestamp, hash_t key,
                      uint64_t platform);

RESOURCE_API void
resource_source_set_blob(resource_source_t* source, tick_t timestamp, hash_t key,
                         uint64_t platform, hash_t checksum, size_t size);

RESOURCE_API bool
resource_source_read_blob(const uuid_t uuid, hash_t key,
                          uint64_t platform, hash_t checksum, void* data, size_t capacity);

RESOURCE_API bool
resource_source_write_blob(const uuid_t uuid, tick_t timestamp, hash_t key,
                           uint64_t platform, hash_t checksum, const void* data, size_t size);

RESOURCE_API void
resource_source_collapse_history(resource_source_t* source);

RESOURCE_API void
resource_source_clear_blob_history(resource_source_t* source, const uuid_t uuid);

RESOURCE_API void
resource_source_map(resource_source_t* source, uint64_t platform, hashmap_t* map);

#else

#define resource_source_set_path(path) ((void)sizeof(path)), false
#define resource_source_path() string_empty()
#define resource_source_allocate() nullptr
#define resource_source_deallocate(source) memory_deallocate(source)
#define resource_source_initialize(source) ((void)sizeof(source))
#define resource_source_finalize(source) ((void)sizeof(source))
#define resource_source_set(source, timestamp, key, value, length) ((void)sizeof(source)), ((void)sizeof(timestamp)), ((void)sizeof(key)), ((void)sizeof(value)), ((void)sizeof(length))
#define resource_source_read(source, uuid) ((void)sizeof(source)), ((void)sizeof(uuid)), false
#define resource_source_write(source, uuid) ((void)sizeof(source)), ((void)sizeof(uuid)), false
#define resource_source_map(source) nullptr

#endif
