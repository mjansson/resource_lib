/* stream.h  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
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

RESOURCE_API stream_t*
resource_stream_open_static(const uuid_t res, uint64_t platform);

RESOURCE_API stream_t*
resource_stream_open_dynamic(const uuid_t res, uint64_t platform);

RESOURCE_API string_t
resource_stream_make_path(char* buffer, size_t capacity, const char* base, size_t base_length,
                          const uuid_t res);
