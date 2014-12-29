/* resource.h  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
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
#include <resource/hashstrings.h>

#include <resource/stream.h>
#include <resource/bundle.h>
#include <resource/compile.h>
#include <resource/local.h>
#include <resource/remote.h>


RESOURCE_API int           resource_initialize( void );
RESOURCE_API void          resource_shutdown( void );
RESOURCE_API bool          resource_is_initialized( void );
