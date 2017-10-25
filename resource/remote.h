/* remote.h  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
 *
 * This library provides a cross-platform resource I/O library in C11 providing
 * basic resource loading, saving and streaming functionality for projects based
 * on our foundation library.
 *
 * The latest source code maintained by Rampant Pixels is always available at
 *
 * https://github.com/rampantpixels/resource_lib
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

RESOURCE_API string_const_t
resource_remote_sourced(void);

RESOURCE_API void
resource_remote_sourced_connect(const char* url, size_t length);

RESOURCE_API void
resource_remote_sourced_disconnect(void);

RESOURCE_API bool
resource_remote_sourced_is_connected(void);

RESOURCE_API resource_signature_t
resource_remote_sourced_lookup(const char* path, size_t length);

RESOURCE_API uint256_t
resource_remote_sourced_hash(uuid_t uuid, uint64_t platform);

RESOURCE_API size_t
resource_remote_sourced_dependencies(uuid_t uuid, uint64_t platform, uuid_t* deps, size_t capacity);

RESOURCE_API size_t
resource_remote_sourced_reverse_dependencies(uuid_t uuid, uint64_t platform, uuid_t* deps, size_t capacity);

RESOURCE_API bool
resource_remote_sourced_read(resource_source_t* source, uuid_t uuid);

RESOURCE_API bool
resource_remote_sourced_read_blob(const uuid_t uuid, hash_t key, uint64_t platform,
                                  hash_t checksum, void* data, size_t capacity);

RESOURCE_API string_const_t
resource_remote_compiled(void);

RESOURCE_API void
resource_remote_compiled_connect(const char* url, size_t length);

RESOURCE_API void
resource_remote_compiled_disconnect(void);

RESOURCE_API bool
resource_remote_compiled_is_connected(void);

RESOURCE_API stream_t*
resource_remote_open_static(const uuid_t uuid, uint64_t platform);

RESOURCE_API stream_t*
resource_remote_open_dynamic(const uuid_t uuid, uint64_t platform);
