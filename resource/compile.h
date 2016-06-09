/* compile.h  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
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

#if RESOURCE_ENABLE_LOCAL_SOURCE && RESOURCE_ENABLE_LOCAL_CACHE

RESOURCE_API void
resource_compile_register(resource_compile_fn compiler);

RESOURCE_API void
resource_compile_unregister(resource_compile_fn compiler);

RESOURCE_API bool
resource_compile_need_update(const uuid_t uuid, uint64_t platform);

RESOURCE_API bool
resource_compile(const uuid_t uuid, uint64_t platform);

#else

#define resource_compile_register(compiler)
#define resource_compile_unregister(compiler)
#define resource_compile_need_update(uuid, platform) ((void)sizeof(uuid)), ((void)sizeof(platform)), false
#define resource_compile(uuid, platform) ((void)sizeof(uuid)), ((void)sizeof(platform)), 0

#endif
