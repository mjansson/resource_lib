/* bundle.c  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
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

#include <resource/bundle.h>
#include <resource/internal.h>

#include <foundation/foundation.h>

bool
resource_bundle_load(const uuid_t bundle) {
#if RESOURCE_ENABLE_REMOTE_CACHE
	if (_resource_config.enable_remote_cache) {
	}
#endif

#if RESOURCE_ENABLE_LOCAL_SOURCE
	if (_resource_config.enable_local_source) {
	}
#endif

#if RESOURCE_ENABLE_LOCAL_CACHE
	if (_resource_config.enable_local_cache) {
	}
#endif

	FOUNDATION_UNUSED(bundle);

	return false;
}

stream_t*
resource_bundle_stream(const uuid_t bundle) {
#if RESOURCE_ENABLE_REMOTE_CACHE
	if (_resource_config.enable_remote_cache) {
	}
#endif

#if RESOURCE_ENABLE_LOCAL_SOURCE
	if (_resource_config.enable_local_source) {
	}
#endif

#if RESOURCE_ENABLE_LOCAL_CACHE
	if (_resource_config.enable_local_cache) {
	}
#endif

	FOUNDATION_UNUSED(bundle);

	return nullptr;
}
