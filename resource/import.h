/* import.h  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
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

#if BUILD_ENABLE_LOCAL_SOURCE

RESOURCE_API bool
resource_import(const char* path, size_t length);

RESOURCE_API void
resource_import_register(resource_import_fn importer);

RESOURCE_API void
resource_import_unregister(resource_import_fn importer);

#else

#define resource_import(path, length) ((void)sizeof(path)), ((void)sizeof(length))
#define resource_import_register(importer) ((void)sizeof(importer))
#define resource_import_unregister(importer) ((void)sizeof(importer))

#endif
