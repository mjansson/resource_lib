/* local.h  -  Resource library  -  Public Domain  -  2014 Mattias Jansson
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

RESOURCE_API const string_const_t*
resource_local_paths(void);

RESOURCE_API void
resource_local_set_paths(const string_const_t* paths, size_t paths_count);

RESOURCE_API void
resource_local_add_path(const char* path, size_t length);

RESOURCE_API void
resource_local_remove_path(const char* path, size_t length);

RESOURCE_API void
resource_local_clear_paths(void);

RESOURCE_API stream_t*
resource_local_open_static(const uuid_t uuid, uint64_t platform);

RESOURCE_API stream_t*
resource_local_open_dynamic(const uuid_t uuid, uint64_t platform);

RESOURCE_API stream_t*
resource_local_create_static(const uuid_t uuid, uint64_t platform);

RESOURCE_API stream_t*
resource_local_create_dynamic(const uuid_t uuid, uint64_t platform);
