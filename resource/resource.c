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
#if !RESOURCE_ENABLE_LOCAL_SOURCE
	_resource_config.enable_local_source = false;
#endif
#if !RESOURCE_ENABLE_LOCAL_CACHE
	_resource_config.enable_local_cache = false;
#endif
#if !RESOURCE_ENABLE_REMOTE_SOURCE
	_resource_config.enable_remote_source = false;
#endif
}

int
resource_module_initialize(const resource_config_t config) {
	if (_resource_module_initialized)
		return 0;

	resource_module_initialize_config(config);

	_resource_event_stream = event_stream_allocate(0);

	config_load(STRING_CONST("resource"), HASH_RESOURCE, true, false);
	{
		size_t ipath;
		const string_const_t remote_url = config_string(HASH_RESOURCE, HASH_REMOTE_URL);
		if (remote_url.length)
			resource_remote_set_url(STRING_ARGS(remote_url));

		const string_const_t local_source = config_string(HASH_RESOURCE, HASH_LOCAL_SOURCE);
		if (local_source.length)
			resource_source_set_path(STRING_ARGS(local_source));

		const string_const_t local_path = config_string(HASH_RESOURCE, HASH_LOCAL_PATH);
		if (local_path.length) {
			string_const_t paths[32];
			size_t numpaths = string_explode(STRING_ARGS(local_path), STRING_CONST(";,"), paths,
			                                 sizeof(paths)/sizeof(paths[0]), false);
			for (ipath = 0; ipath < numpaths; ++ipath)
				resource_local_add_path(STRING_ARGS(paths[ipath]));
		}

		size_t iarg, argsize;
		const string_const_t* cmdline = environment_command_line();
		for (iarg = 0, argsize = array_size(cmdline); iarg < argsize; ++iarg) {
			if (string_equal(STRING_ARGS(cmdline[iarg]), STRING_CONST("--resource-remote-url")) &&
			        (iarg < (argsize - 1))) {
				++iarg;
				resource_remote_set_url(STRING_ARGS(cmdline[iarg]));
			}
			else if (string_equal(STRING_ARGS(cmdline[iarg]), STRING_CONST("--resource-local-source")) &&
			         (iarg < (argsize - 1))) {
				++iarg;
				resource_source_set_path(STRING_ARGS(cmdline[iarg]));
			}
			else if (string_equal(STRING_ARGS(cmdline[iarg]), STRING_CONST("--resource-local-path")) &&
			         (iarg < (argsize - 1))) {
				++iarg;
				string_const_t paths[32];
				size_t numpaths = string_explode(STRING_ARGS(cmdline[iarg]), STRING_CONST(";,"), paths,
				                                 sizeof(paths)/sizeof(paths[0]), false);
				for (ipath = 0; ipath < numpaths; ++ipath)
					resource_local_add_path(STRING_ARGS(paths[ipath]));
			}
		}
	}

	//Make sure we have at least one way of loading resources
	if (!_resource_config.enable_local_cache &&
	        !_resource_config.enable_local_source &&
	        !_resource_config.enable_remote_source) {
		log_error(HASH_RESOURCE, ERROR_INVALID_VALUE,
		          STRING_CONST("Invalid config, no way of loading resources"));
		return -1;
	}

	_resource_module_initialized = true;

	return 0;
}

void
resource_module_finalize(void) {
	if (!_resource_module_initialized)
		return;

	resource_local_clear_paths();

	event_stream_deallocate(_resource_event_stream);

	_resource_event_stream = 0;
	_resource_module_initialized = false;
}

bool
resource_module_is_initialized(void) {
	return _resource_module_initialized;
}
