/* local.c  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
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

#include <resource/local.h>

#include <foundation/foundation.h>


#if RESOURCE_ENABLE_LOCAL_SOURCE


int resource_local_set_source( const char* path )
{
	return 0;
}


#endif


#if RESOURCE_ENABLE_LOCAL_CACHE


const char* const* resource_local_paths( void )
{
	return 0;
}


void resource_local_set_paths( const char* const* paths )
{
}


void resource_local_add_path( const char* path )
{
}


void resource_local_remove_path( const char* path )
{
}


stream_t* resource_local_open_static( const char* path, const uuid_t uuid )
{
	return 0;
}


stream_t* resource_local_open_dynamic( const char* path, const uuid_t uuid )
{
	return 0;
}


#endif
