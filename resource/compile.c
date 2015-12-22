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

#include <resource/compile.h>
#include <resource/source.h>
#include <resource/change.h>
#include <resource/internal.h>

#include <foundation/foundation.h>

#if RESOURCE_ENABLE_LOCAL_SOURCE

static resource_compile_fn* _resource_compilers;

bool
resource_compile_need_update(const uuid_t uuid, uint64_t platform) {
	if (!_resource_config.enable_local_source)
		return false;
	FOUNDATION_UNUSED(uuid);
	FOUNDATION_UNUSED(platform);
	return false;
}

bool
resource_compile(const uuid_t uuid, uint64_t platform) {
	size_t icmp, isize;
	resource_source_t source;
	string_const_t type;
	bool success = false;
	if (!_resource_config.enable_local_source)
		return false;

	resource_source_initialize(&source);
	if (resource_source_read(&source, uuid)) {
		resource_change_t* change;
		change = resource_source_get(&source, HASH_RESOURCE_TYPE,
		                             platform != RESOURCE_PLATFORM_ALL ? platform : 0);
		if (change && resource_change_is_value(change)) {
			type = change->value.value;
		}
	}

	for (icmp = 0, isize = array_size(_resource_compilers); !success && (icmp != isize); ++icmp)
		success = (_resource_compilers[icmp](uuid, platform, &source, STRING_ARGS(type)) == 0);

	resource_source_finalize(&source);

	return success;
}

void
resource_compile_register(resource_compile_fn compiler) {
	array_push(_resource_compilers, compiler);
}

void
resource_compile_unregister(resource_compile_fn compiler) {
	size_t icmp, isize;
	for (icmp = 0, isize = array_size(_resource_compilers); icmp != isize; ++icmp) {
		if (_resource_compilers[icmp] == compiler) {
			array_erase(_resource_compilers, icmp);
			return;
		}
	}
}


#endif
