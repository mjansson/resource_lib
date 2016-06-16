/* compile.c  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
 *
 * This library provides a cross-platform resource I/O library in C11 providing
 * basic resource loading, saving and streaming functionality for projects based
 * on our foundation library.
 *
 * The latest source code maintained by Rampant Pixels is always available at
 *
 * https://github.com/rampantpixels/resource_lib
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
#include <resource/stream.h>
#include <resource/local.h>
#include <resource/internal.h>

#include <foundation/foundation.h>

resource_compile_fn* _resource_compilers;

#if RESOURCE_ENABLE_LOCAL_SOURCE && RESOURCE_ENABLE_LOCAL_CACHE

bool
resource_compile_need_update(const uuid_t uuid, uint64_t platform) {
	uint256_t source_hash;
	stream_t* stream;
	resource_header_t header;

	if (!_resource_config.enable_local_source)
		return false;

	source_hash = resource_source_read_hash(uuid);
	if (uint256_is_null(source_hash))
		return true;

	stream = resource_local_open_static(uuid, platform);
	if (!stream)
		return true;

	header = resource_stream_read_header(stream);

	stream_deallocate(stream);

	string_const_t uuidstr = string_from_uuid_static(uuid);
	log_debugf(HASH_RESOURCE, STRING_CONST("Check compilation for resource %.*s"), STRING_FORMAT(uuidstr));
	string_const_t hashstr = string_from_uint256_static(source_hash);
	log_debugf(HASH_RESOURCE, STRING_CONST("  source: %.*s"), STRING_FORMAT(hashstr));
	hashstr = string_from_uint256_static(header.source_hash);
	log_debugf(HASH_RESOURCE, STRING_CONST("  target: %.*s"), STRING_FORMAT(hashstr));

	//TODO: Based on resource_type_hash, check expected version
	return !uint256_equal(source_hash, header.source_hash);
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
		uint256_t source_hash;
		resource_change_t* change;

		source_hash = resource_source_read_hash(uuid);
		if (uint256_is_null(source_hash)) {
			//Recreate source hash data
			resource_source_write(&source, uuid, source.read_binary);
			source_hash = resource_source_read_hash(uuid);
		}

		resource_source_collapse_history(&source);
		change = resource_source_get(&source, HASH_RESOURCE_TYPE,
		                             platform != RESOURCE_PLATFORM_ALL ? platform : 0);
		if (change && resource_change_is_value(change)) {
			type = change->value.value;
		}

		for (icmp = 0, isize = array_size(_resource_compilers); !success && (icmp != isize); ++icmp)
			success = (_resource_compilers[icmp](uuid, platform, &source, source_hash, STRING_ARGS(type)) == 0);
	}

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
