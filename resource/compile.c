/* compile.c  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
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

#include <resource/remote.h>

#include <foundation/foundation.h>


#if RESOURCE_ENABLE_LOCAL_SOURCE


bool resource_compile_need_update_static( const uuid_t uuid )
{
	return false;
}


bool resource_compile_need_update_dynamic( const uuid_t uuid )
{
	return false;
}


bool resource_compile_update_static( const uuid_t uuid )
{
	return 0;
}


bool resource_compile_update_dynamic( const uuid_t uuid )
{
	return 0;
}


#endif
