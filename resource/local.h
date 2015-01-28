/* local.h  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
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

RESOURCE_API int   resource_local_set_source( const char* path );

#else

#define resource_local_set_source( path ) ((void)sizeof( path )), 0

#endif


#if RESOURCE_ENABLE_LOCAL_CACHE

RESOURCE_API const char* const*  resource_local_paths( void );
RESOURCE_API void                resource_local_set_paths( const char* const* paths );
RESOURCE_API void                resource_local_add_path( const char* path );
RESOURCE_API void                resource_local_remove_path( const char* path );

RESOURCE_API stream_t*           resource_local_open_static( const uuid_t uuid );
RESOURCE_API stream_t*           resource_local_open_dynamic( const uuid_t uuid );

#else


#define resource_local_paths() ((const char* const*)0)
#define resource_local_set_paths( paths ) ((void)sizeof( paths ))
#define resource_local_add_path( path ) ((void)sizeof( path ))
#define resource_local_remove_path( path ) ((void)sizeof( path ))
#define resource_local_open_static( uuid ) ((void)sizeof( uuid )), 0
#define resource_local_open_dynamic( uuid ) ((void)sizeof( uuid )), 0

#endif
