/* resource.c  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
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
#include <resource/internal.h>

#include <foundation/foundation.h>


static bool _resource_initialized = false;


int resource_initialize( void )
{
	const char* remote_url;
	const char* local_source;
	const char* local_cache;
	char** paths;
	const char* const* cmdline;
	unsigned int iarg, argsize;

	if( _resource_initialized )
		return 0;

	_resource_event_stream = event_stream_allocate( 1024 );

	remote_url = config_string( HASH_RESOURCE, HASH_REMOTE_URL );
	if( remote_url )
		resource_remote_set_url( remote_url );

	local_source = config_string( HASH_RESOURCE, HASH_LOCAL_SOURCE );
	if( local_source )
		resource_local_set_source( local_source );

	local_cache = config_string( HASH_RESOURCE, HASH_LOCAL_CACHE );
	if( local_cache )
	{
		paths = string_explode( local_cache, ";,", false );
		resource_local_set_paths( (const char* const*)paths );
		string_array_deallocate( paths );
	}

	cmdline = environment_command_line();
	for( iarg = 0, argsize = array_size( cmdline ); iarg < argsize; ++iarg )
	{
		if( string_equal( cmdline[iarg], "--resource-remote-url" ) && ( iarg < ( argsize - 1 ) ) )
		{
			++iarg;
			resource_remote_set_url( cmdline[iarg] );	
		}
		else if( string_equal( cmdline[iarg], "--resource-local-source" ) && ( iarg < ( argsize - 1 ) ) )
		{
			++iarg;
			resource_local_set_source( cmdline[iarg] );	
		}
		else if( string_equal( cmdline[iarg], "--resource-local-cache" ) && ( iarg < ( argsize - 1 ) ) )
		{
			++iarg;
			paths = string_explode( cmdline[iarg], ";,", false );
			resource_local_set_paths( (const char* const*)paths );
			string_array_deallocate( paths );
		}
	}

	_resource_initialized = true;

	return 0;
}


void resource_shutdown( void )
{
	if( !_resource_initialized )
		return;

	event_stream_deallocate( _resource_event_stream );

	_resource_event_stream = 0;
	_resource_initialized = false;
}


bool resource_is_initialized( void )
{
	return _resource_initialized;
}
