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
#include <resource/internal.h>

#include <foundation/foundation.h>

#if RESOURCE_ENABLE_LOCAL_SOURCE

string_t _resource_local_source;

bool
resource_local_set_source(const char* path, size_t length) {
	if (!_resource_config.enable_local_source)
		return false;

	string_deallocate(_resource_local_source.str);
	_resource_local_source = string_clone(path, length);

	return true;
}

#endif

#if RESOURCE_ENABLE_LOCAL_CACHE

static string_t* _resource_local_paths = 0;

const string_const_t*
resource_local_paths(void) {
	return (string_const_t*)_resource_local_paths;
}

void
resource_local_set_paths(const string_const_t* paths, size_t num) {
	size_t ipath, pathsize;

	string_array_deallocate(_resource_local_paths);

	for (ipath = 0, pathsize = num; ipath < pathsize; ++ipath)
		resource_local_add_path(STRING_ARGS(paths[ipath]));
}

void
resource_local_add_path(const char* path, size_t length) {
	size_t capacity = length + 2;
	string_t copy = string_copy(string_allocate(length, capacity).str, capacity,
	                            path, length);
	array_push(_resource_local_paths, path_clean(STRING_ARGS(copy), capacity));
}

void resource_local_remove_path(const char* path, size_t length) {
	size_t ipath, pathsize;

	for (ipath = 0, pathsize = array_size(_resource_local_paths); ipath < pathsize; ++ipath) {
		const string_t local_path = _resource_local_paths[ipath];
		if (string_equal(STRING_ARGS(local_path), path, length)) {
			array_erase(_resource_local_paths, ipath);
			string_deallocate(local_path.str);
			break;
		}
	}
}

stream_t*
resource_local_open_static(const uuid_t uuid) {
	stream_t* stream = 0;
	size_t ipath, pathsize;

	string_const_t uuidstr;
	char buffer[BUILD_MAX_PATHLEN];

	if (!_resource_config.enable_local_cache)
		return 0;

	uuidstr = string_from_uuid_static(uuid);

	for (ipath = 0, pathsize = array_size(_resource_local_paths); ipath < pathsize; ++ipath) {
		const string_t local_path = _resource_local_paths[ipath];
		string_t curpath = string_format(buffer, sizeof(buffer), "%.*s/%2s/%2s/%.*s",
		                                 STRING_FORMAT(local_path), uuidstr.str, uuidstr.str + 2,
		                                 STRING_FORMAT(uuidstr));
		stream = stream_open(STRING_ARGS(curpath), STREAM_IN);
		if (stream)
			break;
	}

	return stream;
}

stream_t*
resource_local_open_dynamic(const uuid_t uuid) {
	stream_t* stream = 0;
	size_t ipath, pathsize;

	string_const_t uuidstr;
	char buffer[BUILD_MAX_PATHLEN];

	if (!_resource_config.enable_local_cache)
		return 0;

	uuidstr = string_from_uuid_static(uuid);

	for (ipath = 0, pathsize = array_size(_resource_local_paths); ipath < pathsize; ++ipath) {
		const string_t local_path = _resource_local_paths[ipath];
		string_t curpath = string_format(buffer, sizeof(buffer), "%.*s/%2s/%2s/%.*s.blob",
		                                 STRING_FORMAT(local_path), uuidstr.str, uuidstr.str + 2,
		                                 STRING_FORMAT(uuidstr));
		stream = stream_open(STRING_ARGS(curpath), STREAM_IN);
		if (stream)
			break;
	}

	return stream;
}

#endif
