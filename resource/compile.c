/* compile.c  -  Resource library  -  Public Domain  -  2014 Mattias Jansson
 *
 * This library provides a cross-platform resource I/O library in C11 providing
 * basic resource loading, saving and streaming functionality for projects based
 * on our foundation library.
 *
 * The latest source code maintained by Mattias Jansson is always available at
 *
 * https://github.com/mjansson/resource_lib
 *
 * The foundation library source code maintained by Mattias Jansson is always available at
 *
 * https://github.com/mjansson/foundation_lib
 *
 * This library is put in the public domain; you can redistribute it and/or modify it without any
 * restrictions.
 *
 */

#include <resource/resource.h>
#include <resource/internal.h>

#include <foundation/foundation.h>

static resource_compile_fn* _resource_compilers;
static string_t* _resource_compile_tool_path;
static atomic64_t _resource_compile_token;

static hash_t
resource_compile_token(void) {
	return (hash_t)atomic_incr64(&_resource_compile_token, memory_order_acq_rel);
}

int
resource_compile_initialize(void) {
	return 0;
}

void
resource_compile_finalize(void) {
	array_deallocate(_resource_compilers);
	string_array_deallocate(_resource_compile_tool_path);

	_resource_compilers = 0;
}

#if (RESOURCE_ENABLE_LOCAL_SOURCE || RESOURCE_ENABLE_REMOTE_SOURCED) && RESOURCE_ENABLE_LOCAL_CACHE

#if FOUNDATION_PLATFORM_WINDOWS
#define RESOURCE_COMPILER_PATTERN "^.*compile\\.exe$"
#else
#define RESOURCE_COMPILER_PATTERN "^.*compile$"
#endif

bool
resource_compile_need_update(const uuid_t uuid, uint64_t platform) {
	uint256_t source_hash;
	stream_t* stream;
	resource_header_t header;

	if (!resource_module_config().enable_local_source && !resource_module_config().enable_remote_sourced)
		return false;

	string_const_t uuidstr = string_from_uuid_static(uuid);
	log_debugf(HASH_RESOURCE, STRING_CONST("Compile check: %.*s (platform 0x%" PRIx64 ")"), STRING_FORMAT(uuidstr),
	           platform);

	resource_dependency_t localdeps[8];
	size_t deps_capacity = sizeof(localdeps) / sizeof(localdeps[0]);
	size_t deps_count = resource_source_dependencies_count(uuid, platform);
	if (deps_count) {
		bool depsuccess = true;
		resource_dependency_t* deps = localdeps;
		if (deps_count > deps_capacity)
			deps = memory_allocate(HASH_RESOURCE, sizeof(resource_dependency_t) * deps_count, 16, MEMORY_PERSISTENT);
		resource_source_dependencies(uuid, platform, deps, deps_count);
		for (size_t idep = 0; idep < deps_count; ++idep) {
			log_debug(HASH_RESOURCE, STRING_CONST("Dependent resource compile check:"));
			if (resource_compile_need_update(deps[idep].uuid, platform)) {
				if (!resource_compile(deps[idep].uuid, platform))
					depsuccess = false;
			}
		}
		if (deps != localdeps)
			memory_deallocate(deps);

		if (!depsuccess) {
			error_context_pop();
			return false;
		}
	}

	if (resource_autoimport_need_update(uuid, platform))
		resource_autoimport(uuid);

	source_hash = resource_source_hash(uuid, platform);
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

	// TODO: Based on resource_type_hash, check expected version
	return !uint256_equal(source_hash, header.source_hash);
}

bool
resource_compile(const uuid_t uuid, uint64_t platform) {
	size_t icmp, isize;
	size_t internal = 0;
	size_t external = 0;
	resource_source_t source;
	string_const_t type = string_null();
	bool success = false;
	if (!resource_module_config().enable_local_source && !resource_module_config().enable_remote_sourced)
		return false;

	char uuidbuf[40];
	const string_t uuidstr = string_from_uuid(uuidbuf, sizeof(uuidbuf), uuid);
	error_context_push(STRING_CONST("compiling resource"), STRING_ARGS(uuidstr));

	size_t deps_count = resource_source_dependencies_count(uuid, platform);
	log_debugf(HASH_RESOURCE, STRING_CONST("Compile: %.*s (platform 0x%" PRIx64 ") %" PRIsize " dependencies"),
	           STRING_FORMAT(uuidstr), platform, deps_count);

	resource_dependency_t localdeps[8];
	size_t deps_capacity = sizeof(localdeps) / sizeof(localdeps[0]);
	if (deps_count) {
		bool depsuccess = true;
		resource_dependency_t* deps = localdeps;
		if (deps_count > deps_capacity)
			deps = memory_allocate(HASH_RESOURCE, sizeof(resource_dependency_t) * deps_count, 16, MEMORY_PERSISTENT);
		resource_source_dependencies(uuid, platform, deps, deps_count);
		for (size_t idep = 0; idep < deps_count; ++idep) {
			char depuuidbuf[40];
			const string_t depuuidstr = string_from_uuid(depuuidbuf, sizeof(depuuidbuf), deps[idep].uuid);
			log_debugf(HASH_RESOURCE, STRING_CONST("Compile: %.*s dependency: %.*s"), STRING_FORMAT(uuidstr),
			           STRING_FORMAT(depuuidstr));
			error_context_push(STRING_CONST("compiling dependent resource"), STRING_ARGS(depuuidstr));
			if (resource_compile_need_update(deps[idep].uuid, platform)) {
				if (!resource_compile(deps[idep].uuid, platform))
					depsuccess = false;
			}
			error_context_pop();
		}
		if (deps != localdeps)
			memory_deallocate(deps);

		if (!depsuccess) {
			error_context_pop();
			return false;
		}
	}

	if (resource_autoimport_need_update(uuid, platform))
		resource_autoimport(uuid);

	resource_source_initialize(&source);
	bool was_read = resource_source_read(&source, uuid);
	if (!was_read) {
		resource_source_finalize(&source);

		// Try reimporting
		resource_autoimport(uuid);

		resource_source_initialize(&source);
		was_read = resource_source_read(&source, uuid);
	}
	if (was_read) {
		uint256_t source_hash;
		resource_change_t* change;

		source_hash = resource_source_hash(uuid, platform);
		if (uint256_is_null(source_hash) && resource_module_config().enable_local_source) {
			// Recreate source hash data
			resource_source_write(&source, uuid, source.read_binary);
			source_hash = resource_source_hash(uuid, platform);
		}

		resource_source_collapse_history(&source);
		change = resource_source_get(&source, HASH_RESOURCE_TYPE, platform != RESOURCE_PLATFORM_ALL ? platform : 0);
		if (change && resource_change_is_value(change)) {
			type = change->value.value;
		}

		for (icmp = 0, isize = array_size(_resource_compilers); !success && (icmp != isize); ++icmp) {
			success = (_resource_compilers[icmp](uuid, platform, &source, source_hash, STRING_ARGS(type)) == 0);
			++internal;
		}
	}
	resource_source_finalize(&source);

	// Try external tools
	for (size_t ipath = 0, psize = array_size(_resource_compile_tool_path); !success && (ipath != psize); ++ipath) {
		string_t* tools = fs_matching_files(STRING_ARGS(_resource_compile_tool_path[ipath]),
		                                    STRING_CONST(RESOURCE_COMPILER_PATTERN), true);
		for (size_t itool = 0, tsize = array_size(tools); !success && (itool != tsize); ++itool) {
			char buffer[BUILD_MAX_PATHLEN];
			string_t fullpath = path_concat(buffer, sizeof(buffer), STRING_ARGS(_resource_compile_tool_path[ipath]),
			                                STRING_ARGS(tools[itool]));

			process_t proc;
			process_initialize(&proc);

			string_const_t wd = environment_current_working_directory();
			process_set_working_directory(&proc, STRING_ARGS(wd));
			process_set_executable_path(&proc, STRING_ARGS(fullpath));

			string_const_t* args = nullptr;
			char platformarr[34];
			array_push(args, string_to_const(uuidstr));
			if (platform) {
				string_t platformstr = string_from_uint(platformarr, sizeof(platformarr), platform, true, 0, 0);
				array_push(args, string_const(STRING_CONST("--platform")));
				array_push(args, string_to_const(platformstr));
			}
			array_push(args, string_const(STRING_CONST("--")));

			const string_const_t* local_paths = resource_local_paths();
			for (size_t ilocal = 0, lsize = array_size(local_paths); ilocal < lsize; ++ilocal) {
				array_push(args, string_const(STRING_CONST("--resource-local-path")));
				array_push(args, local_paths[ilocal]);
			}

			string_const_t local_source = resource_source_path();
			if (local_source.length) {
				array_push(args, string_const(STRING_CONST("--resource-source-path")));
				array_push(args, local_source);
			}

			string_const_t remote_sourced = resource_remote_sourced();
			if (remote_sourced.length) {
				array_push(args, string_const(STRING_CONST("--resource-remote-sourced")));
				array_push(args, remote_sourced);
			}

			process_set_arguments(&proc, args, array_size(args));
			process_set_flags(&proc, PROCESS_STDSTREAMS | PROCESS_DETACHED);
			process_spawn(&proc);

			stream_t* err = process_stderr(&proc);
			stream_finalize(process_stdout(&proc));
			while (!stream_eos(err)) {
				string_t line = stream_read_line_buffer(err, buffer, sizeof(buffer), '\n');
				if (line.length) {
					if (line.str[line.length - 1] == '\r')
						--line.length;
					log_infof(HASH_RESOURCE, STRING_CONST("%.*s: %.*s"), STRING_FORMAT(tools[itool]),
					          STRING_FORMAT(line));
				}
			}
			int exit_code = process_wait(&proc);
			while (exit_code == PROCESS_STILL_ACTIVE) {
				thread_yield();
				exit_code = process_wait(&proc);
			}
			if (exit_code == 0) {
				log_debugf(HASH_RESOURCE, STRING_CONST("Compiled with external tool: %.*s"),
				           STRING_FORMAT(tools[itool]));
				success = true;
			} else {
				log_debugf(HASH_RESOURCE, STRING_CONST("Failed compiling with external tool: %.*s (%d)"),
				           STRING_FORMAT(tools[itool]), exit_code);
			}

			process_finalize(&proc);
			array_deallocate(args);

			++external;
		}
		string_array_deallocate(tools);
	}

	error_context_pop();

	if (!success) {
		log_warnf(HASH_RESOURCE, WARNING_RESOURCE,
		          STRING_CONST("Unable to compile: %.*s (platform 0x%" PRIx64 ") (%" PRIsize " internal, %" PRIsize
		                       " external)"),
		          STRING_FORMAT(uuidstr), platform, internal, external);
	} else {
		log_infof(HASH_RESOURCE, STRING_CONST("Compiled: %.*s (platform 0x%" PRIx64 ")"), STRING_FORMAT(uuidstr),
		          platform);
		hash_t token = resource_compile_token();
		resource_event_post(RESOURCEEVENT_COMPILE, uuid, platform, token);
	}

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
resource_compile_register_path(const char* path, size_t length) {
	size_t iimp, isize;
	char buffer[BUILD_MAX_PATHLEN];
	string_t pathstr = string_copy(buffer, sizeof(buffer), path, length);
	pathstr = path_clean(STRING_ARGS(pathstr), sizeof(buffer));
	for (iimp = 0, isize = array_size(_resource_compile_tool_path); iimp != isize; ++iimp) {
		if (string_equal(STRING_ARGS(_resource_compile_tool_path[iimp]), STRING_ARGS(pathstr)))
			return;
	}
	pathstr = string_clone(STRING_ARGS(pathstr));
	array_push(_resource_compile_tool_path, pathstr);
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

void
resource_compile_unregister_path(const char* path, size_t length) {
	size_t iimp, isize;
	char buffer[BUILD_MAX_PATHLEN];
	string_t pathstr = string_copy(buffer, sizeof(buffer), path, length);
	pathstr = path_clean(STRING_ARGS(pathstr), sizeof(buffer));
	for (iimp = 0, isize = array_size(_resource_compile_tool_path); iimp != isize; ++iimp) {
		if (string_equal(STRING_ARGS(_resource_compile_tool_path[iimp]), STRING_ARGS(pathstr))) {
			string_deallocate(_resource_compile_tool_path[iimp].str);
			array_erase(_resource_compile_tool_path, iimp);
			break;
		}
	}
}

void
resource_compile_clear(void) {
	array_clear(_resource_compilers);
}

void
resource_compile_clear_path(void) {
	string_array_deallocate_elements(_resource_compile_tool_path);
	array_clear(_resource_compile_tool_path);
}

#else

bool
resource_compile_need_update(const uuid_t uuid, uint64_t platform) {
	FOUNDATION_UNUSED(uuid);
	FOUNDATION_UNUSED(platform);
	return false;
}

bool
resource_compile(const uuid_t uuid, uint64_t platform) {
	FOUNDATION_UNUSED(uuid);
	FOUNDATION_UNUSED(platform);
	return true;
}

void
resource_compile_register(resource_compile_fn compiler) {
	FOUNDATION_UNUSED(compiler);
}

void
resource_compile_register_path(const char* path, size_t length) {
	FOUNDATION_UNUSED(path);
	FOUNDATION_UNUSED(length);
}

void
resource_compile_unregister(resource_compile_fn compiler) {
	FOUNDATION_UNUSED(compiler);
}

void
resource_compile_unregister_path(const char* path, size_t length) {
	FOUNDATION_UNUSED(path);
	FOUNDATION_UNUSED(length);
}

void
resource_compile_clear(void) {
}

void
resource_compile_clear_path(void) {
}

#endif
