/* remote.c  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
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

#if RESOURCE_ENABLE_REMOTE_SOURCE

static string_t _remote_url = 0;

string_const_t
resource_remote_url(void) {
	return string_const(STRING_ARGS(_remote_url));
}

void
resource_remote_set_url(const char* url, size_t length) {
	string_deallocate(_remote_url.str);
	_remote_url = string_clone(url, length);
}

bool
resource_remote_need_update_static(const uuid_t uuid) {
	return false;
}

bool
resource_remote_need_update_dynamic(const uuid_t uuid) {
	return false;
}

stream_t*
resource_remote_update_static(const uuid_t uuid) {
	return 0;
}

stream_t*
resource_remote_update_dynamic(const uuid_t uuid) {
	return 0;
}


#endif
