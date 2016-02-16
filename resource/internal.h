/* internal.h  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
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
#include <foundation/types.h>
#include <foundation/internal.h>

#include <resource/types.h>
#include <resource/hashstrings.h>

RESOURCE_EXTERN event_stream_t* _resource_event_stream;
RESOURCE_EXTERN resource_config_t _resource_config;
RESOURCE_EXTERN string_t _resource_source_path;
RESOURCE_EXTERN resource_compile_fn* _resource_compilers;
