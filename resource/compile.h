/* compile.h  -  Resource library  -  Public Domain  -  2014 Mattias Jansson
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

RESOURCE_API bool
resource_compile_need_update(const uuid_t uuid, uint64_t platform);

RESOURCE_API uint256_t
resource_compile_hash(const uuid_t uuid, uint64_t platform);

RESOURCE_API bool
resource_compile(const uuid_t uuid, uint64_t platform);

RESOURCE_API void
resource_compile_register(resource_compile_fn compiler);

RESOURCE_API void
resource_compile_register_path(const char* path, size_t length);

RESOURCE_API void
resource_compile_unregister(resource_compile_fn compiler);

RESOURCE_API void
resource_compile_unregister_path(const char* path, size_t length);

RESOURCE_API void
resource_compile_clear(void);

RESOURCE_API void
resource_compile_clear_path(void);
