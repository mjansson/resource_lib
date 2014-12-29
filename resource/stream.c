/* stream.c  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
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

#include <resource/resource.h>

#include <foundation/foundation.h>


stream_t* resource_stream_open_static( uuid_t res )
{
	const char* const* paths;
	unsigned int ipath, pathsize;

	if( resource_remote_need_update_static( res ) )
		return resource_remote_update_static( res );
	
	if( resource_compile_need_update_static( res ) )
		resource_compile_update_static( res );

	paths = resource_local_paths();
	for( ipath = 0, pathsize = array_size( paths ); ipath < pathsize; ++ipath )
	{
		stream_t* stream = resource_local_open_static( paths[ipath], res );
		if( stream )
			return stream;
	}

	log_warnf( HASH_RESOURCE, WARNING_RESOURCE, "Unable to open static stream for resource: %s", string_from_uuid_static( res ) );

	return 0;
}


stream_t* resource_stream_open_dynamic( uuid_t res )
{
	const char* const* paths;
	unsigned int ipath, pathsize;

	if( resource_remote_need_update_dynamic( res ) )
		return resource_remote_update_dynamic( res );

	if( resource_compile_need_update_dynamic( res ) )
		resource_compile_update_dynamic( res );

	paths = resource_local_paths();
	for( ipath = 0, pathsize = array_size( paths ); ipath < pathsize; ++ipath )
	{
		stream_t* stream = resource_local_open_dynamic( paths[ipath], res );
		if( stream )
			return stream;
	}

	log_warnf( HASH_RESOURCE, WARNING_RESOURCE, "Unable to open dynamic stream for resource: %s", string_from_uuid_static( res ) );

	return 0;
}

