/* compile.h  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
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

RESOURCE_API bool
resource_compile_need_update_static(const uuid_t uuid);

RESOURCE_API bool
resource_compile_need_update_dynamic(const uuid_t uuid);

RESOURCE_API int
resource_compile_update_static(const uuid_t uuid);

RESOURCE_API int
resource_compile_update_dynamic(const uuid_t uuid);

#else

#define resource_compile_need_update_static(uuid) ((void)sizeof( uuid )), false
#define resource_compile_need_update_dynamic(uuid) ((void)sizeof( uuid )), false
#define resource_compile_update_static(uuid) ((void)sizeof( uuid )), 0
#define resource_compile_update_dynamic(uuid) ((void)sizeof( uuid )), 0

#endif
