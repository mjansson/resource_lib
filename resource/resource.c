/* resource.c  -  Resource library  -  Public Domain  -  2014 Mattias Jansson
 *
 * This library provides a cross-platform resource I/O library in C11 providing
 * basic resource loading, saving and streaming functionality for projects based
 * on our foundation library.
 *
 * The latest source code maintained by Mattias Jansson is always available at
 *
 * https://github.com/rampantpixels/resource_lib
 *
 * The foundation library source code maintained by Mattias Jansson is always available at
 *
 * https://github.com/rampantpixels/foundation_lib
 *
 * This library is put in the public domain; you can redistribute it and/or modify it without any restrictions.
 *
 */

#include <resource/resource.h>
#include <resource/internal.h>

#include <foundation/foundation.h>

static resource_config_t _resource_config;
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
#if !RESOURCE_ENABLE_REMOTE_SOURCED
	_resource_config.enable_remote_sourced = false;
#endif
#if !RESOURCE_ENABLE_REMOTE_COMPILED
	_resource_config.enable_remote_compiled = false;
#endif
	if (!_resource_config.enable_local_source)
		_resource_config.enable_local_autoimport = false;
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
		if (string_equal(STRING_ARGS(cmdline[iarg]), STRING_CONST("--resource-remote-sourced")) &&
		    (iarg < (argsize - 1))) {
			++iarg;
			resource_remote_sourced_connect(STRING_ARGS(cmdline[iarg]));
		} else if (string_equal(STRING_ARGS(cmdline[iarg]), STRING_CONST("--resource-remote-compiled")) &&
		           (iarg < (argsize - 1))) {
			++iarg;
			resource_remote_compiled_connect(STRING_ARGS(cmdline[iarg]));
		} else if (string_equal(STRING_ARGS(cmdline[iarg]), STRING_CONST("--resource-source-path")) &&
		           (iarg < (argsize - 1))) {
			++iarg;
			resource_source_set_path(STRING_ARGS(cmdline[iarg]));
		} else if (string_equal(STRING_ARGS(cmdline[iarg]), STRING_CONST("--resource-local-path")) &&
		           (iarg < (argsize - 1))) {
			++iarg;
			string_const_t paths[32];
			size_t paths_count = string_explode(STRING_ARGS(cmdline[iarg]), STRING_CONST(";,"), paths,
			                                    sizeof(paths) / sizeof(paths[0]), false);
			for (ipath = 0; ipath < paths_count; ++ipath)
				resource_local_add_path(STRING_ARGS(paths[ipath]));
		} else if (string_equal(STRING_ARGS(cmdline[iarg]), STRING_CONST("--resource-base-path")) &&
		           (iarg < (argsize - 1))) {
			++iarg;
			resource_import_set_base_path(STRING_ARGS(cmdline[iarg]));
		} else if (string_equal(STRING_ARGS(cmdline[iarg]), STRING_CONST("--resource-autoimport-path")) &&
		           (iarg < (argsize - 1))) {
			++iarg;
			string_const_t paths[32];
			size_t paths_count = string_explode(STRING_ARGS(cmdline[iarg]), STRING_CONST(";,"), paths,
			                                    sizeof(paths) / sizeof(paths[0]), false);
			for (ipath = 0; ipath < paths_count; ++ipath)
				resource_autoimport_watch(STRING_ARGS(paths[ipath]));
		} else if (string_equal(STRING_ARGS(cmdline[iarg]), STRING_CONST("--resource-tool-path")) &&
		           (iarg < (argsize - 1))) {
			++iarg;
			resource_import_register_path(STRING_ARGS(cmdline[iarg]));
			resource_compile_register_path(STRING_ARGS(cmdline[iarg]));
		}
	}

	// Make sure we have at least one way of loading resources
	if (!_resource_config.enable_local_cache && !_resource_config.enable_remote_compiled) {
		log_error(HASH_RESOURCE, ERROR_INVALID_VALUE,
		          STRING_CONST("Invalid config, no way of loading compiled resources"));
		return -1;
	}

	if (resource_import_initialize() < 0)
		return -1;

	if (resource_compile_initialize() < 0)
		return -1;

	if (resource_autoimport_initialize() < 0)
		return -1;

	if (resource_remote_initialize() < 0)
		return -1;

	_resource_module_initialized = true;

	return 0;
}

void
resource_module_finalize(void) {
	if (!_resource_module_initialized)
		return;

	resource_local_clear_paths();

	resource_remote_finalize();
	resource_autoimport_finalize();
	resource_import_finalize();
	resource_compile_finalize();

	event_stream_deallocate(_resource_event_stream);

	_resource_event_stream = 0;
	_resource_module_initialized = false;
}

bool
resource_module_is_initialized(void) {
	return _resource_module_initialized;
}

void
resource_module_parse_config(const char* path, size_t path_size, const char* buffer, size_t size,
                             const json_token_t* tokens, size_t tokens_count) {
	FOUNDATION_UNUSED(size);
	char pathbuf[BUILD_MAX_PATHLEN];

	for (size_t tok = tokens_count ? tokens[0].child : 0; tok && tok < tokens_count; tok = tokens[tok].sibling) {
		string_const_t id = json_token_identifier(buffer, tokens + tok);
		if ((tokens[tok].type == JSON_OBJECT) && string_equal(STRING_ARGS(id), STRING_CONST("resource"))) {
			for (size_t restok = tokens[tok].child; restok && (restok < tokens_count);
			     restok = tokens[restok].sibling) {
				string_const_t resid = json_token_identifier(buffer, tokens + restok);
				hash_t idhash = hash(STRING_ARGS(resid));
				string_const_t sourcedir = path_directory_name(path, path_size);
				if (tokens[restok].type == JSON_STRING) {
					string_const_t value = json_token_value(buffer, tokens + restok);

					string_t fullpath;
					if (!path_is_absolute(STRING_ARGS(value))) {
						fullpath = path_concat(pathbuf, sizeof(pathbuf), STRING_ARGS(sourcedir), STRING_ARGS(value));
						fullpath = path_absolute(STRING_ARGS(fullpath), sizeof(pathbuf));
					} else {
						fullpath = string_copy(pathbuf, sizeof(pathbuf), STRING_ARGS(value));
					}

					if (idhash == HASH_LOCAL_PATH)
						resource_local_add_path(STRING_ARGS(fullpath));
					else if (idhash == HASH_SOURCE_PATH)
						resource_source_set_path(STRING_ARGS(fullpath));
					else if (idhash == HASH_BASE_PATH)
						resource_import_set_base_path(STRING_ARGS(fullpath));
					else if (idhash == HASH_AUTOIMPORT_PATH)
						resource_autoimport_watch(STRING_ARGS(fullpath));
					else if (idhash == HASH_REMOTE_SOURCED)
						resource_remote_sourced_connect(STRING_ARGS(value));
					else if (idhash == HASH_REMOTE_COMPILED)
						resource_remote_compiled_connect(STRING_ARGS(value));
					else if (idhash == HASH_TOOL_PATH) {
						resource_import_register_path(STRING_ARGS(fullpath));
						resource_compile_register_path(STRING_ARGS(fullpath));
					}
				} else if (tokens[restok].type == JSON_ARRAY) {
					if (idhash == HASH_AUTOIMPORT_PATH) {
						size_t arrtok = tokens[restok].child;
						while (arrtok) {
							if (tokens[arrtok].type == JSON_STRING) {
								string_const_t import_path = json_token_value(buffer, tokens + arrtok);
								string_t fullpath;
								if (!path_is_absolute(STRING_ARGS(import_path))) {
									fullpath = path_concat(pathbuf, sizeof(pathbuf), STRING_ARGS(sourcedir),
									                       STRING_ARGS(import_path));
									fullpath = path_absolute(STRING_ARGS(fullpath), sizeof(pathbuf));
								} else {
									fullpath = string_copy(pathbuf, sizeof(pathbuf), STRING_ARGS(import_path));
								}
								resource_autoimport_watch(STRING_ARGS(fullpath));
							}
							arrtok = tokens[arrtok].sibling;
						}
					}
				}
			}
		}
	}
}

resource_config_t
resource_module_config(void) {
	return _resource_config;
}
