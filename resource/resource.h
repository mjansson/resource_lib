/* resource.h  -  Resource library  -  Public Domain  -  2014 Mattias Jansson
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

#include <resource/build.h>
#include <resource/types.h>
#include <resource/hashstrings.h>

#include <resource/event.h>
#include <resource/stream.h>
#include <resource/bundle.h>
#include <resource/compile.h>
#include <resource/local.h>
#include <resource/remote.h>
#include <resource/change.h>
#include <resource/source.h>
#include <resource/import.h>
#include <resource/platform.h>

RESOURCE_API int
resource_module_initialize(const resource_config_t config);

RESOURCE_API void
resource_module_finalize(void);

RESOURCE_API bool
resource_module_is_initialized(void);

RESOURCE_API version_t
resource_module_version(void);

RESOURCE_API resource_config_t
resource_module_config(void);

RESOURCE_API void
resource_module_parse_config(const char* path, size_t path_size, const char* buffer, size_t size,
                             const json_token_t* tokens, size_t tokens_count);
