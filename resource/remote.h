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

#if RESOURCE_ENABLE_REMOTE_SOURCED

RESOURCE_API string_const_t
resource_remote_sourced(void);

RESOURCE_API void
resource_remote_sourced_connect(const char* url, size_t length);

RESOURCE_API void
resource_remote_sourced_disconnect(void);

#else

#define resource_remote_sourced() string_empty()
#define resource_remote_sourced_connect(...) do { FOUNDATION_UNUSED_VARARGS(__VA_ARGS__); } while(0)
#define resource_remote_sourced_disconnect()

#endif

RESOURCE_API resource_signature_t
resource_remote_sourced_lookup(const char* path, size_t length);

#if RESOURCE_ENABLE_REMOTE_COMPILED

RESOURCE_API string_const_t
resource_remote_compiled(void);

RESOURCE_API void
resource_remote_compiled_connect(const char* url, size_t length);

RESOURCE_API void
resource_remote_compiled_disconnect(void);

RESOURCE_API stream_t*
resource_remote_open_static(const uuid_t uuid, uint64_t platform);

RESOURCE_API stream_t*
resource_remote_open_dynamic(const uuid_t uuid, uint64_t platform);

#else

#define resource_remote_compiled() string_empty()
#define resource_remote_compiled_connect(...) do { FOUNDATION_UNUSED_VARARGS(__VA_ARGS__); } while(0)
#define resource_remote_compiled_disconnect() 
#define resource_remote_open_static(uuid, platform) (((void)sizeof(uuid)), ((void)sizeof(platform)), (void*)0)
#define resource_remote_open_dynamic(uuid, platform) (((void)sizeof(uuid)), ((void)sizeof(platform)), (void*)0)

#endif
