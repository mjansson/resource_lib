/* local.c  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
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

#include <resource/local.h>
#include <resource/stream.h>
#include <resource/platform.h>
#include <resource/internal.h>

#include <foundation/foundation.h>

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
	char pathbuf[BUILD_MAX_PATHLEN];
	string_t pathstr;
	pathstr = string_copy(pathbuf, sizeof(pathbuf), path, length);
	pathstr = path_clean(STRING_ARGS(pathstr), sizeof(pathbuf));
	pathstr = path_absolute(STRING_ARGS(pathstr), sizeof(pathbuf));
	pathstr = string_clone(STRING_ARGS(pathstr));
	array_push(_resource_local_paths, pathstr);
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

void
resource_local_clear_paths(void) {
	string_array_deallocate(_resource_local_paths);
}

static string_t
resource_local_make_platform_path(char* buffer, size_t capacity, size_t local_path,
                                  const uuid_t uuid, uint64_t platform,
                                  const char* suffix, size_t suffix_length) {
	string_t curpath = resource_stream_make_path(buffer, capacity,
	                                             STRING_ARGS(_resource_local_paths[local_path]),
	                                             uuid);
	string_const_t platformstr = string_from_uint_static(platform, true, 0, '0');
	string_t platformpath = path_append(STRING_ARGS(curpath), capacity, STRING_ARGS(platformstr));
	if (suffix_length)
		platformpath = string_append(STRING_ARGS(platformpath), capacity, suffix, suffix_length);
	return platformpath;
}

static stream_t*
resource_local_open_stream(const uuid_t uuid, uint64_t platform, const char* suffix,
                           size_t suffix_length, unsigned int mode) {
	stream_t* stream = nullptr;
	size_t ipath, pathsize;
	char buffer[BUILD_MAX_PATHLEN];
	uint64_t full_platform = platform;
	bool try_create = (mode & STREAM_CREATE);
	bool tried_create = false;

	if (!_resource_config.enable_local_cache)
		return nullptr;

	//If stream is to be created, first iterate all local paths to find
	//existing file on most specified platform level. If such file does
	//not exist, retry and create a new file at the most specified level
	//in the first local path that succeeds.
	mode &= ~STREAM_CREATE;
	while (!stream) {
retry:
		for (ipath = 0, pathsize = array_size(_resource_local_paths); !stream &&
		        (ipath < pathsize); ++ipath) {
			string_t platformpath = resource_local_make_platform_path(
			                            buffer, sizeof(buffer), ipath,
			                            uuid, platform, suffix, suffix_length);
			if (mode & STREAM_CREATE) {
				string_const_t path = path_directory_name(STRING_ARGS(platformpath));
				fs_make_directory(STRING_ARGS(path));
			}
			stream = stream_open(STRING_ARGS(platformpath), mode);
		}
		if (!stream && try_create) {
			if (tried_create)
				break;
			tried_create = true;
			mode |= STREAM_CREATE;
			goto retry;
		}
		if (!platform)
			break;
		platform = resource_platform_reduce(platform, full_platform);
	}

	return stream;
}

stream_t*
resource_local_open_static(const uuid_t uuid, uint64_t platform) {
	return resource_local_open_stream(uuid, platform, 0, 0, STREAM_IN | STREAM_BINARY);
}

stream_t*
resource_local_open_dynamic(const uuid_t uuid, uint64_t platform) {
	return resource_local_open_stream(uuid, platform, STRING_CONST(".blob"), STREAM_IN | STREAM_BINARY);
}

#endif

#if RESOURCE_ENABLE_LOCAL_CACHE && RESOURCE_ENABLE_LOCAL_SOURCE

stream_t*
resource_local_create_static(const uuid_t uuid, uint64_t platform) {
	return resource_local_open_stream(uuid, platform, 0, 0,
	                                  STREAM_OUT | STREAM_CREATE | STREAM_TRUNCATE | STREAM_BINARY);
}

stream_t*
resource_local_create_dynamic(const uuid_t uuid, uint64_t platform) {
	return resource_local_open_stream(uuid, platform, STRING_CONST(".blob"),
	                                  STREAM_OUT | STREAM_CREATE | STREAM_TRUNCATE | STREAM_BINARY);
}

#endif
