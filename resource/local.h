/* local.h  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
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

#if RESOURCE_ENABLE_LOCAL_CACHE

RESOURCE_API const string_const_t*
resource_local_paths(void);

RESOURCE_API void
resource_local_set_paths(const string_const_t* paths, size_t num);

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

#else

#define resource_local_paths() ((const string_const_t*)0)
#define resource_local_set_paths(paths, num) ((void)sizeof(paths)), ((void)sizeof(num))
#define resource_local_add_path(...) do { FOUNDATION_UNUSED_VARARGS(__VA_ARGS__); } while(0)
#define resource_local_remove_path(...) do { FOUNDATION_UNUSED_VARARGS(__VA_ARGS__); } while(0)
#define resource_local_clear_paths() do {} while(0)
#define resource_local_open_static(uuid, platform) ((void)sizeof(uuid)), ((void)sizeof(platform)), 0
#define resource_local_open_dynamic(uuid, platform) ((void)sizeof(uuid)), ((void)sizeof(platform)), 0

#endif

#if RESOURCE_ENABLE_LOCAL_CACHE && RESOURCE_ENABLE_LOCAL_SOURCE

RESOURCE_API stream_t*
resource_local_create_static(const uuid_t uuid, uint64_t platform);

RESOURCE_API stream_t*
resource_local_create_dynamic(const uuid_t uuid, uint64_t platform);

#else

#define resource_local_create_static(uuid, platform) ((void)sizeof(uuid)), ((void)sizeof(platform)), 0
#define resource_local_create_dynamic(uuid, platform) ((void)sizeof(uuid)), ((void)sizeof(platform)), 0

#endif
