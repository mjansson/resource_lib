/* remote.h  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
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

#if RESOURCE_ENABLE_REMOTE_SOURCE

RESOURCE_API string_const_t
resource_remote_url(void);

RESOURCE_API void
resource_remote_set_url(const char* url, size_t length);

RESOURCE_API bool
resource_remote_need_update_static(const uuid_t uuid, uint64_t platform);

RESOURCE_API bool
resource_remote_need_update_dynamic(const uuid_t uuid, uint64_t platform);

RESOURCE_API stream_t*
resource_remote_update_static(const uuid_t uuid, uint64_t platform);

RESOURCE_API stream_t*
resource_remote_update_dynamic(const uuid_t uuid, uint64_t platform);

#else

#define resource_remote_url() ((const char*)0)
#define resource_remote_set_url(...) do { FOUNDATION_UNUSED_VARARGS(__VA_ARGS__); } while(0)
#define resource_remote_need_update_static(uuid, platform) ((void)sizeof(uuid)), ((void)sizeof(platform)), false
#define resource_remote_need_update_dynamic(uuid, platform) ((void)sizeof(uuid)), ((void)sizeof(platform)), false
#define resource_remote_update_static(uuid, platform) ((void)sizeof(uuid)), ((void)sizeof(platform)), (void*)0
#define resource_remote_update_dynamic(uuid, platform) ((void)sizeof(uuid)), ((void)sizeof(platform)), (void*)0

#endif
