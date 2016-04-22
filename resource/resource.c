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
#if !RESOURCE_ENABLE_REMOTE_CACHE
	_resource_config.enable_remote_cache = false;
#endif
}

int
resource_module_initialize(const resource_config_t config) {
	if (_resource_module_initialized)
		return 0;

	resource_module_initialize_config(config);

	_resource_event_stream = event_stream_allocate(0);

	size_t iarg, argsize, ipath;
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

	//Make sure we have at least one way of loading resources
	if (!_resource_config.enable_local_cache &&
	        !_resource_config.enable_local_source &&
	        !_resource_config.enable_remote_cache) {
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

	array_deallocate(_resource_importers);
	array_deallocate(_resource_compilers);

	resource_local_clear_paths();

	event_stream_deallocate(_resource_event_stream);

	_resource_importers = 0;
	_resource_compilers = 0;
	_resource_event_stream = 0;
	_resource_module_initialized = false;
}

bool
resource_module_is_initialized(void) {
	return _resource_module_initialized;
}

void
resource_module_parse_config(const char* buffer, size_t size,
                             const json_token_t* tokens, size_t num_tokens) {
	FOUNDATION_UNUSED(size);

	for (size_t tok = num_tokens ? tokens[0].child : 0; tok &&
	        tok < num_tokens; tok = tokens[tok].sibling) {

		string_const_t id = json_token_identifier(buffer, tokens + tok);
		if ((tokens[tok].type == JSON_OBJECT) && string_equal(STRING_ARGS(id), STRING_CONST("resource"))) {

			for (size_t restok = tokens[tok].child; restok &&
			        (restok < num_tokens); restok = tokens[restok].sibling) {

				string_const_t resid = json_token_identifier(buffer, tokens + restok);
				if (tokens[restok].type == JSON_STRING) {
					string_const_t value = json_token_value(buffer, tokens + restok);
					hash_t idhash = hash(STRING_ARGS(resid));

					if (idhash == HASH_LOCAL_PATH)
						resource_local_add_path(STRING_ARGS(value));
					else if (idhash == HASH_SOURCE_PATH)
						resource_source_set_path(STRING_ARGS(value));
				}
			}
		}
	}
}
