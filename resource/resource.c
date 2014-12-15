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
	if( _resource_initialized )
		return 0;

	_resource_event_stream = event_stream_allocate( 1024 );

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

