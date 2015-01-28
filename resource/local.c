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


FOUNDATION_DECLARE_THREAD_LOCAL_ARRAY( char, path_buffer, FOUNDATION_MAX_PATHLEN )

static char** _local_paths = 0;


const char* const* resource_local_paths( void )
{
	return (const char* const*)_local_paths;
}


void resource_local_set_paths( const char* const* paths )
{
	int ipath, pathsize;

	string_array_deallocate( _local_paths );

	for( ipath = 0, pathsize = array_size( paths ); ipath < pathsize; ++ipath )
		array_push( _local_paths, path_clean( string_clone( paths[ipath] ), path_is_absolute( paths[ipath] ) ) );
}


void resource_local_add_path( const char* path )
{
	array_push( _local_paths, path_clean( string_clone( path ), path_is_absolute( path ) ) );
}


void resource_local_remove_path( const char* path )
{
	int ipath, pathsize;
	char* cleanpath = path_clean( string_clone( path ), path_is_absolute( path ) );

	for( ipath = 0, pathsize = array_size( _local_paths ); ipath < pathsize; ++ipath )
	{
		char* arrpath = _local_paths[ipath];
		if( string_equal( arrpath, cleanpath ) )
		{
			array_erase( _local_paths, ipath );
			string_deallocate( arrpath );
			break;
		}
	}

	string_deallocate( cleanpath );
}


stream_t* resource_local_open_static( const uuid_t uuid )
{
	stream_t* stream = 0;
	const char* const* paths;
	unsigned int ipath, pathsize;
	
	const char* uuidstr = string_from_uuid_static( uuid );
	char* thread_buffer = get_thread_path_buffer();

	paths = resource_local_paths();
	for( ipath = 0, pathsize = array_size( paths ); ipath < pathsize; ++ipath )
	{
		char* curpath = string_format_buffer( thread_buffer, FOUNDATION_MAX_PATHLEN, "%s/%2s/%2s/%s", paths[ipath], uuidstr, uuidstr + 2, uuidstr );
		stream = stream_open( curpath, STREAM_IN );
		if( stream )
			break;
	}

	return stream;
}


stream_t* resource_local_open_dynamic( const uuid_t uuid )
{
	stream_t* stream = 0;
	const char* const* paths;
	unsigned int ipath, pathsize;
	
	const char* uuidstr = string_from_uuid_static( uuid );
	char* thread_buffer = get_thread_path_buffer();

	paths = resource_local_paths();
	for( ipath = 0, pathsize = array_size( paths ); ipath < pathsize; ++ipath )
	{
		char* curpath = string_format_buffer( thread_buffer, FOUNDATION_MAX_PATHLEN, "%s/%2s/%2s/%s.blob", paths[ipath], uuidstr, uuidstr + 2, uuidstr );
		stream = stream_open( curpath, STREAM_IN );
		if( stream )
			break;
	}

	return stream;
}


#endif
