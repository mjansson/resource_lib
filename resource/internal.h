/* internal.h  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
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
#include <foundation/types.h>
#include <foundation/internal.h>

#include <resource/types.h>
#include <resource/hashstrings.h>

RESOURCE_EXTERN event_stream_t* _resource_event_stream;

RESOURCE_API int
resource_import_initialize(void);

RESOURCE_API void
resource_import_finalize(void);

RESOURCE_API int
resource_autoimport_initialize(void);

RESOURCE_API void
resource_autoimport_finalize(void);

RESOURCE_API int
resource_compile_initialize(void);

RESOURCE_API void
resource_compile_finalize(void);

RESOURCE_API int
resource_remote_initialize(void);

RESOURCE_API void
resource_remote_finalize(void);
