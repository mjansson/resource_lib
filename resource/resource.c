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

resource_config_t _resource_config;
static bool _resource_module_initialized;

static void
resource_module_initialize_config(const resource_config_t config) {
	memcpy(&_resource_config, &config, sizeof(resource_config_t));
}

int
resource_module_initialize(resource_config_t config) {
	if (_resource_module_initialized)
		return 0;

	resource_module_initialize_config(config);

	_resource_event_stream = event_stream_allocate(0);

	/*
	const char* remote_url;
	const char* local_source;
	const char* local_cache;
	char** paths;
	const char* const* cmdline;
	unsigned int iarg, argsize;

	remote_url = config_string(HASH_RESOURCE, HASH_REMOTE_URL);
	if (remote_url)
		resource_remote_set_url(remote_url);

	local_source = config_string(HASH_RESOURCE, HASH_LOCAL_SOURCE);
	if (local_source)
		resource_local_set_source(local_source);

	local_cache = config_string(HASH_RESOURCE, HASH_LOCAL_CACHE);
	if (local_cache) {
		paths = string_explode(local_cache, ";,", false);
		resource_local_set_paths((const char* const*)paths);
		string_array_deallocate(paths);
	}

	cmdline = environment_command_line();
	for (iarg = 0, argsize = array_size(cmdline); iarg < argsize; ++iarg) {
		if (string_equal(cmdline[iarg], "--resource-remote-url") && (iarg < (argsize - 1))) {
			++iarg;
			resource_remote_set_url(cmdline[iarg]);
		}
		else if (string_equal(cmdline[iarg], "--resource-local-source") && (iarg < (argsize - 1))) {
			++iarg;
			resource_local_set_source(cmdline[iarg]);
		}
		else if (string_equal(cmdline[iarg], "--resource-local-cache") && (iarg < (argsize - 1))) {
			++iarg;
			paths = string_explode(cmdline[iarg], ";,", false);
			resource_local_set_paths((const char* const*)paths);
			string_array_deallocate(paths);
		}
	}*/

	//Make sure we have at least one way of loading resources
	if (_resource_config.enable_local_cache ||
		_resource_config.enable_local_source ||
		_resource_config.enable_remote_source) {
		log_error(HASH_RESOURCE, ERROR_INVALID_VALUE, STRING_CONST("Invalid config, no way of loading resources"));
		return -1;
	}

	_resource_module_initialized = true;

	return 0;
}

void
resource_module_shutdown(void) {
	if (!_resource_module_initialized)
		return;

	event_stream_deallocate(_resource_event_stream);

	_resource_event_stream = 0;
	_resource_module_initialized = false;
}

bool
resource_module_is_initialized(void) {
	return _resource_module_initialized;
}
