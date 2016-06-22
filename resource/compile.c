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

	string_const_t uuidstr = string_from_uuid_static(uuid);
	log_debugf(HASH_RESOURCE, STRING_CONST("Compile check: %.*s"), STRING_FORMAT(uuidstr));

	source_hash = resource_source_read_hash(uuid, platform);
	if (uint256_is_null(source_hash)) {
		log_debug(HASH_RESOURCE, STRING_CONST("  no source hash"));
		return true;
	}

	stream = resource_local_open_static(uuid, platform);
	if (!stream) {
		log_debug(HASH_RESOURCE, STRING_CONST("  no source static stream"));
		return true;
	}

	header = resource_stream_read_header(stream);

	stream_deallocate(stream);

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
	bool success = true;
	if (!_resource_config.enable_local_source)
		return false;

#if BUILD_ENABLE_DEBUG_LOG || BUILD_ENABLE_ERROR_CONTEXT
	char uuidbuf[40];
	const string_t uuidstr = string_from_uuid(uuidbuf, sizeof(uuidbuf), uuid);
	error_context_push(STRING_CONST("compiling resource"), STRING_ARGS(uuidstr));
#else
	const string_t uuidstr = {0};
#endif
	log_debugf(HASH_RESOURCE, STRING_CONST("Compile: %.*s"), STRING_FORMAT(uuidstr));

	uuid_t localdeps[4];
	size_t depscapacity = sizeof(localdeps) / sizeof(uuid_t);
	size_t numdeps = resource_source_num_dependencies(uuid, platform);
	if (numdeps) {
		uuid_t* deps = localdeps;
		log_debugf(HASH_RESOURCE, STRING_CONST("Dependency compile check: %.*s"), STRING_FORMAT(uuidstr));
		if (numdeps > depscapacity)
			deps = memory_allocate(HASH_RESOURCE, sizeof(uuid_t) * numdeps, 16, MEMORY_PERSISTENT);
		resource_source_dependencies(uuid, platform, deps, numdeps);
		for (size_t idep = 0; idep < numdeps; ++idep) {
			if (resource_compile_need_update(deps[idep], platform)) {
				if (!resource_compile(deps[idep], platform))
					success = false;
			}
		}
		if (deps != localdeps)
			memory_deallocate(deps);

		if (!success) {
			error_context_pop();
			return false;
		}
	}

	resource_source_initialize(&source);
	if (resource_source_read(&source, uuid)) {
		uint256_t source_hash;
		resource_change_t* change;

		source_hash = resource_source_read_hash(uuid, platform);
		if (uint256_is_null(source_hash)) {
			//Recreate source hash data
			resource_source_write(&source, uuid, source.read_binary);
			source_hash = resource_source_read_hash(uuid, platform);
		}

		resource_source_collapse_history(&source);
		change = resource_source_get(&source, HASH_RESOURCE_TYPE,
		                             platform != RESOURCE_PLATFORM_ALL ? platform : 0);
		if (change && resource_change_is_value(change)) {
			type = change->value.value;
		}

		success = false;
		for (icmp = 0, isize = array_size(_resource_compilers); !success && (icmp != isize); ++icmp)
			success = (_resource_compilers[icmp](uuid, platform, &source, source_hash, STRING_ARGS(type)) == 0);
	}

	resource_source_finalize(&source);

	error_context_pop();

	return success;
}

void
resource_compile_register(resource_compile_fn compiler) {
	size_t icmp, isize;
	for (icmp = 0, isize = array_size(_resource_compilers); icmp != isize; ++icmp) {
		if (_resource_compilers[icmp] == compiler)
			return;
	}
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
