/* import.c  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
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

#include <resource/resource.h>
#include <resource/internal.h>

#include <foundation/foundation.h>

#if FOUNDATION_PLATFORM_WINDOWS
#  define RESOURCE_IMPORTER_PATTERN "^.*import\\.exe$"
#else
#  define RESOURCE_IMPORTER_PATTERN "^.*import$"
#endif

static resource_import_fn* _resource_importers;
static string_t _resource_import_base_path;
static string_t* _resource_import_tool_path;

int
resource_import_initialize(void) {
	return 0;
}

void
resource_import_finalize(void) {
	array_deallocate(_resource_importers);
	string_deallocate(_resource_import_base_path.str);
	string_array_deallocate(_resource_import_tool_path);

	_resource_importers = 0;
	_resource_import_base_path = string(0, 0);
}

string_const_t
resource_import_base_path(void) {
	return string_to_const(_resource_import_base_path);
}

void
resource_import_set_base_path(const char* path, size_t length) {
	if (_resource_import_base_path.str)
		string_deallocate(_resource_import_base_path.str);
	_resource_import_base_path = length ? string_clone(path, length) : string(0, 0);
}

#if RESOURCE_ENABLE_LOCAL_SOURCE

bool
resource_import(const char* path, size_t length, const uuid_t uuid) {
	size_t iimp, isize;
	bool was_imported = false;
	stream_t* stream = stream_open(path, length, STREAM_IN);
	if (!stream) {
		log_warnf(HASH_RESOURCE, WARNING_RESOURCE,
		          STRING_CONST("Unable to open input stream for importing: %.*s"), (int)length, path);
		return false;
	}
	for (iimp = 0, isize = array_size(_resource_importers); iimp != isize; ++iimp) {
		stream_seek(stream, 0, STREAM_SEEK_BEGIN);
		was_imported |= (_resource_importers[iimp](stream, uuid) == 0);
	}
	stream_deallocate(stream);

	//Try external tools
	for (size_t ipath = 0, psize = array_size(_resource_import_tool_path); !was_imported &&
	        (ipath != psize); ++ipath) {
		string_t* tools = fs_matching_files(STRING_ARGS(_resource_import_tool_path[ipath]),
		                                    STRING_CONST(RESOURCE_IMPORTER_PATTERN), true);
		for (size_t itool = 0, tsize = array_size(tools); !was_imported && (itool != tsize); ++itool) {
			char buffer[BUILD_MAX_PATHLEN];
			string_t fullpath = path_concat(buffer, sizeof(buffer),
			                                STRING_ARGS(_resource_import_tool_path[ipath]),
			                                STRING_ARGS(tools[itool]));

			process_t proc;
			process_initialize(&proc);

			string_const_t wd = environment_current_working_directory();
			process_set_working_directory(&proc, STRING_ARGS(wd));
			process_set_executable_path(&proc, STRING_ARGS(fullpath));

			string_const_t local_source = resource_source_path();
			string_const_t args[] = {
				string_const(STRING_CONST("--debug")),
				string_const(path, length),
				string_const(STRING_CONST("--")),
				string_const(STRING_CONST("--resource-local-source")),
				local_source
			};
			process_set_arguments(&proc, args, sizeof(args) / sizeof(args[0]));
			process_set_flags(&proc, PROCESS_STDSTREAMS);

			if (process_spawn(&proc) == 0) {
				stream_t* out = process_stdout(&proc);
				while (!stream_eos(out)) {
					string_t line = stream_read_line_buffer(out, buffer, sizeof(buffer), '\n');
					log_infof(HASH_RESOURCE, STRING_CONST("Importer: %.*s"), STRING_FORMAT(line));
				}
				stream_deallocate(out);
				int res = process_wait(&proc);
				if (res == 0)
					was_imported = true;
			}

			process_finalize(&proc);
		}
		string_array_deallocate(tools);
	}

	if (!was_imported) {
		log_warnf(HASH_RESOURCE, WARNING_RESOURCE,
		          STRING_CONST("Unable to import: %.*s"), (int)length, path);
	}
	else {
		log_infof(HASH_RESOURCE, STRING_CONST("Imported: %.*s"), (int)length, path);
	}
	return was_imported;
}

void
resource_import_register(resource_import_fn importer) {
	size_t iimp, isize;
	for (iimp = 0, isize = array_size(_resource_importers); iimp != isize; ++iimp) {
		if (_resource_importers[iimp] == importer)
			return;
	}
	array_push(_resource_importers, importer);
}

void
resource_import_register_path(const char* path, size_t length) {
	size_t iimp, isize;
	char buffer[BUILD_MAX_PATHLEN];
	string_t pathstr = string_copy(buffer, sizeof(buffer), path, length);
	pathstr = path_clean(STRING_ARGS(pathstr), sizeof(buffer));
	for (iimp = 0, isize = array_size(_resource_import_tool_path); iimp != isize; ++iimp) {
		if (string_equal(STRING_ARGS(_resource_import_tool_path[iimp]), STRING_ARGS(pathstr)))
			return;
	}
	pathstr = string_clone(STRING_ARGS(pathstr));
	array_push(_resource_import_tool_path, pathstr);
}

void
resource_import_unregister(resource_import_fn importer) {
	size_t iimp, isize;
	for (iimp = 0, isize = array_size(_resource_importers); iimp != isize; ++iimp) {
		if (_resource_importers[iimp] == importer) {
			array_erase(_resource_importers, iimp);
			return;
		}
	}
}

void
resource_import_unregister_path(const char* path, size_t length) {
	size_t iimp, isize;
	char buffer[BUILD_MAX_PATHLEN];
	string_t pathstr = string_copy(buffer, sizeof(buffer), path, length);
	pathstr = path_clean(STRING_ARGS(pathstr), sizeof(buffer));
	for (iimp = 0, isize = array_size(_resource_import_tool_path); iimp != isize; ++iimp) {
		if (string_equal(STRING_ARGS(_resource_import_tool_path[iimp]), STRING_ARGS(pathstr))) {
			string_deallocate(_resource_import_tool_path[iimp].str);
			array_erase(_resource_import_tool_path, iimp);
			break;
		}
	}
}

static stream_t*
resource_import_open_map(const char* cpath, size_t length, bool write) {
	char buffer[BUILD_MAX_PATHLEN];
	string_const_t last_path;
	string_const_t path = path_directory_name(cpath, length);
	while (path.length > 1) {
		string_t map_path = path_concat(buffer, sizeof(buffer),
		                                STRING_ARGS(path), STRING_CONST(RESOURCE_IMPORT_MAP));
		stream_t* stream = stream_open(STRING_ARGS(map_path), STREAM_IN | (write ? STREAM_OUT : 0));
		if (stream)
			return stream;
		last_path = path;
		path = path_directory_name(STRING_ARGS(path));
		if (path.length >= last_path.length)
			break;
	}
	if (write) {
		path = path_directory_name(cpath, length);
		string_t map_path = path_concat(buffer, sizeof(buffer),
		                                STRING_ARGS(path), STRING_CONST(RESOURCE_IMPORT_MAP));
		stream_t* stream = stream_open(STRING_ARGS(map_path), STREAM_IN | STREAM_OUT | STREAM_CREATE);
		if (stream)
			return stream;
	}
	return nullptr;
}

static FOUNDATION_NOINLINE string_const_t
resource_import_map_subpath(stream_t* map, const char* path, size_t length) {
	string_const_t subpath;
	string_const_t mappath;
	mappath = stream_path(map);
	mappath = path_directory_name(STRING_ARGS(mappath));
	subpath = path_subpath(path, length, STRING_ARGS(mappath));
	if (!subpath.length)
		subpath = string_const(path, length);
	return subpath;
}

static FOUNDATION_NOINLINE resource_signature_t
resource_import_map_read_and_update(stream_t* map, hash_t pathhash, const char* path, size_t length,
                                    uint256_t update_hash) {
	char buffer[BUILD_MAX_PATHLEN+64];
	string_t line;
	resource_signature_t sig = {uuid_null(), uint256_null()};
	//TODO: This needs to be a DB as number of imported files grow
	while (!stream_eos(map) && uuid_is_null(sig.uuid)) {
		hash_t linehash;
		string_const_t linepath;
		size_t streampos = stream_tell(map);

		line = stream_read_line_buffer(map, buffer, sizeof(buffer), '\n');
		if (line.length < 120)
			continue;

		linehash = string_to_uint64(STRING_ARGS(line), true);
		if (linehash != pathhash)
			continue;

		linepath = string_substr(STRING_ARGS(line), 119, line.length);
		if (!string_equal(STRING_ARGS(linepath), path, length))
			continue;

		sig.uuid = string_to_uuid(line.str + 17, 37);
		sig.hash = string_to_uint256(line.str + 54, 64);

		if (!uint256_is_null(update_hash) && !uint256_equal(sig.hash, update_hash)) {
			if (map->mode & STREAM_OUT) {
				string_const_t token = string_from_uint256_static(update_hash);
				stream_seek(map, (ssize_t)streampos + 54, STREAM_SEEK_BEGIN);
				stream_write(map, STRING_ARGS(token));
				sig.hash = update_hash;
			}
		}
	}
	return sig;
}

uuid_t
resource_import_map_store(const char* path, size_t length, uuid_t uuid, uint256_t sighash) {
	string_const_t subpath;
	hash_t pathhash;
	resource_signature_t sig;

	stream_t* map = resource_import_open_map(path, length, true);
	if (!map) {
		log_warn(HASH_RESOURCE, WARNING_SUSPICIOUS, STRING_CONST("No map to store in"));
		return uuid_null();
	}

	subpath = resource_import_map_subpath(map, path, length);
	pathhash = hash(STRING_ARGS(subpath));
	sig = resource_import_map_read_and_update(map, pathhash, STRING_ARGS(subpath), sighash);

	if (uuid_is_null(sig.uuid)) {
		string_const_t token;
		char separator = ' ';

		stream_seek(map, 0, STREAM_SEEK_END);

		token = string_from_uint_static(pathhash, true, 16, '0');
		stream_write(map, STRING_ARGS(token));
		stream_write(map, &separator, 1);

		token = string_from_uuid_static(uuid);
		stream_write(map, STRING_ARGS(token));
		stream_write(map, &separator, 1);

		token = string_from_uint256_static(sighash);
		stream_write(map, STRING_ARGS(token));
		stream_write(map, &separator, 1);

		stream_write(map, STRING_ARGS(subpath));
		stream_write_endl(map);

		sig.uuid = uuid;
	}

	stream_deallocate(map);

	return sig.uuid;
}

bool
resource_import_map_purge(const char* path, size_t length) {
	//TODO: Implement
	FOUNDATION_UNUSED(path);
	FOUNDATION_UNUSED(length);
	return false;
}

static resource_signature_t
resource_import_map_lookup(const char* path, size_t length) {
	resource_signature_t sig = {uuid_null(), uint256_null()};
	char buffer[BUILD_MAX_PATHLEN];

	string_t pathstr = string_copy(buffer, sizeof(buffer), path, length);
	pathstr = path_absolute(STRING_ARGS(pathstr), sizeof(buffer));

	stream_t* map = resource_import_open_map(STRING_ARGS(pathstr), false);
	if (!map)
		return sig;

	string_const_t subpath = resource_import_map_subpath(map, STRING_ARGS(pathstr));
	hash_t pathhash = hash(STRING_ARGS(subpath));
	sig = resource_import_map_read_and_update(map, pathhash, STRING_ARGS(subpath), uint256_null());

	stream_deallocate(map);

	return sig;
}

resource_signature_t
resource_import_lookup(const char* path, size_t length) {
	if (resource_remote_sourced_is_connected()) {
		string_const_t base_path = resource_import_base_path();
		string_const_t subpath = string_null();
		if (path_is_absolute(path, length) && base_path.length)
			subpath = path_subpath(path, length, STRING_ARGS(base_path));
		if (!subpath.length)
			subpath = string_const(path, length);
		resource_signature_t sig = resource_remote_sourced_lookup(STRING_ARGS(subpath));
		if (!uuid_is_null(sig.uuid))
			return sig;
	}

	return resource_import_map_lookup(path, length);
}

static mutex_t* _resource_autoimport_lock;
static string_t* _resource_autoimport_dir;

int
resource_autoimport_initialize(void) {
	_resource_autoimport_lock = mutex_allocate(STRING_CONST("resource-autoimport"));
	return 0;
}

void
resource_autoimport_finalize(void) {
	resource_autoimport_clear();
	mutex_deallocate(_resource_autoimport_lock);
}

static string_t
resource_autoimport_reverse_lookup(const uuid_t uuid, char* buffer, size_t capacity) {
	//TODO: Improve
	string_t result = (string_t) {buffer, 0};
	regex_t* regex;
	size_t ipath, psize;
	size_t imap, msize;
	uuid_t siguuid;
	char linebuffer[BUILD_MAX_PATHLEN];

	regex = regex_compile(STRING_CONST("^" RESOURCE_IMPORT_MAP "$"));

	for (ipath = 0, psize = array_size(_resource_autoimport_dir);
	        !result.length && (ipath < psize); ++ipath) {

		string_t* maps = fs_matching_files_regex(STRING_ARGS(_resource_autoimport_dir[ipath]), regex, true);
		for (imap = 0, msize = array_size(maps); !result.length && (imap < msize); ++imap) {
			string_t mappath = path_concat(linebuffer, sizeof(linebuffer),
			                               STRING_ARGS(_resource_autoimport_dir[ipath]), STRING_ARGS(maps[imap]));
			stream_t* map = stream_open(STRING_ARGS(mappath), STREAM_IN);

			while (map && !stream_eos(map)) {
				string_t line = stream_read_line_buffer(map, linebuffer, sizeof(linebuffer), '\n');
				if (line.length < 120)
					continue;

				siguuid = string_to_uuid(line.str + 17, 37);
				if (uuid_equal(uuid, siguuid)) {
					string_const_t linepath = string_substr(STRING_ARGS(line), 119, line.length);
					string_const_t mapdir = stream_path(map);
					mapdir = path_directory_name(STRING_ARGS(mapdir));

					result = path_concat(buffer, capacity, STRING_ARGS(mapdir), STRING_ARGS(linepath));

					string_const_t uuidstr = string_from_uuid_static(uuid);
					log_debugf(HASH_RESOURCE, STRING_CONST("Autoimport reversed lookup: %.*s -> %.*s"),
					           STRING_FORMAT(uuidstr), STRING_FORMAT(result));
					break;
				}
			}

			stream_deallocate(map);
		}

		string_array_deallocate(maps);
	}

	regex_deallocate(regex);

	return result;
}

bool
resource_autoimport(const uuid_t uuid) {
	if (!resource_module_config().enable_local_autoimport)
		return false;

	string_const_t uuidstr = string_from_uuid_static(uuid);
	log_debugf(HASH_RESOURCE, STRING_CONST("Autoimport: %.*s"), STRING_FORMAT(uuidstr));

	mutex_lock(_resource_autoimport_lock);
	char buffer[BUILD_MAX_PATHLEN];
	string_t path = resource_autoimport_reverse_lookup(uuid, buffer, sizeof(buffer));
	mutex_unlock(_resource_autoimport_lock);
	if (path.length)
		return resource_import(STRING_ARGS(path), uuid);
	return false;
}

bool
resource_autoimport_need_update(const uuid_t uuid, uint64_t platform) {
	union {
		char path[BUILD_MAX_PATHLEN];
		uuid_t deps[BUILD_MAX_PATHLEN/sizeof(uuid_t)];
	} buffer;

	if (!resource_module_config().enable_local_autoimport)
		return false;

	string_const_t uuidstr = string_from_uuid_static(uuid);
	log_debugf(HASH_RESOURCE, STRING_CONST("Autoimport check: %.*s"), STRING_FORMAT(uuidstr));

	mutex_lock(_resource_autoimport_lock);
	string_t path = resource_autoimport_reverse_lookup(uuid, buffer.path, sizeof(buffer.path));
	mutex_unlock(_resource_autoimport_lock);
	if (path.length) {
		resource_signature_t sig = resource_import_map_lookup(STRING_ARGS(path));
		stream_t* stream = stream_open(STRING_ARGS(path), STREAM_IN);
		uint256_t newhash = stream_sha256(stream);
		stream_deallocate(stream);
		if (!uint256_equal(sig.hash, newhash)) {
			uuidstr = string_from_uuid_static(uuid);
			log_debugf(HASH_RESOURCE, STRING_CONST("Autoimport check, source hash changed: %.*s"),
			           STRING_FORMAT(uuidstr));
			return true;
		}

		uuid_t* localdeps = buffer.deps;
		size_t capacity = sizeof(buffer.deps) / sizeof(uuid_t);
		size_t numdeps = resource_source_num_dependencies(uuid, platform);
		if (numdeps) {
			bool need_import = false;
			uuid_t* deps = localdeps;
			log_debugf(HASH_RESOURCE, STRING_CONST("Autoimport check, %" PRIsize
			                                       " source dependency checks: %.*s"),
			           numdeps, STRING_FORMAT(uuidstr));
			if (numdeps > capacity)
				deps = memory_allocate(HASH_RESOURCE, sizeof(uuid_t) * numdeps, 16, MEMORY_PERSISTENT);
			resource_source_dependencies(uuid, platform, deps, numdeps);
			for (size_t idep = 0; idep < numdeps; ++idep) {
				if (resource_autoimport_need_update(deps[idep], platform)) {
					need_import = true;
					uuidstr = string_from_uuid_static(uuid);
					log_debugf(HASH_RESOURCE, STRING_CONST("Autoimport check, source dependency changed: %.*s"),
					           STRING_FORMAT(uuidstr));
					break;
				}
			}
			if (deps != localdeps)
				memory_deallocate(deps);
			if (need_import)
				return true;
		}
	}
	return false;
}

static void
resource_autoimport_unwatch_dir(const char* path, size_t length) {
	ssize_t idx = string_array_find((const string_const_t*)_resource_autoimport_dir,
	                                array_size(_resource_autoimport_dir), path, length);
	if (idx < 0)
		return;

	log_debugf(HASH_RESOURCE, STRING_CONST("Autoimport unwatch dir: %.*s"), (int)length, path);

	fs_unmonitor(path, length);
	string_deallocate(_resource_autoimport_dir[idx].str);
	array_erase(_resource_autoimport_dir, idx);
}

static void
resource_autoimport_watch_dir(const char* path, size_t length) {
	size_t ipath, psize;
	for (ipath = 0, psize = array_size(_resource_autoimport_dir); ipath < psize; ++ipath) {
		//Check if something is already watching this dir or any parent
		if (string_equal(path, length, STRING_ARGS(_resource_autoimport_dir[ipath])) ||
		        path_subpath(path, length, STRING_ARGS(_resource_autoimport_dir[ipath])).length) {
			log_debugf(HASH_RESOURCE, STRING_CONST("Autoimport already watching dir: %.*s (%.*s)"), (int)length,
			           path, STRING_FORMAT(_resource_autoimport_dir[ipath]));
			break;
		}
	}
	if (ipath == psize) {
		//Check if we will replace a more specific monitor
		for (ipath = 0, psize = array_size(_resource_autoimport_dir); ipath < psize;) {
			if (path_subpath(STRING_ARGS(_resource_autoimport_dir[ipath]), path, length).length) {
				resource_autoimport_unwatch_dir(STRING_ARGS(_resource_autoimport_dir[ipath]));
				psize = array_size(_resource_autoimport_dir);
			}
			else {
				++ipath;
			}
		}
		log_debugf(HASH_RESOURCE, STRING_CONST("Autoimport watch dir: %.*s"), (int)length, path);
		if (fs_monitor(path, length))
			array_push(_resource_autoimport_dir, string_clone(path, length));
	}
}

void
resource_autoimport_watch(const char* path, size_t length) {
	if (!resource_module_config().enable_local_autoimport)
		return;
	mutex_lock(_resource_autoimport_lock);
	if (fs_is_directory(path, length)) {
		resource_autoimport_watch_dir(path, length);
	}
	else if (fs_is_file(path, length)) {
		string_const_t filename = path_file_name(path, length);
		if (string_equal(STRING_ARGS(filename), STRING_CONST(RESOURCE_IMPORT_MAP))) {
			string_const_t dir = path_directory_name(path, length);
			resource_autoimport_watch_dir(STRING_ARGS(dir));
		}
	}
	mutex_unlock(_resource_autoimport_lock);
}

void
resource_autoimport_unwatch(const char* path, size_t length) {
	if (!resource_module_config().enable_local_autoimport)
		return;
	mutex_lock(_resource_autoimport_lock);
	if (fs_is_directory(path, length)) {
		resource_autoimport_unwatch_dir(path, length);
	}
	else if (fs_is_file(path, length)) {
		string_const_t filename = path_file_name(path, length);
		if (string_equal(STRING_ARGS(filename), STRING_CONST(RESOURCE_IMPORT_MAP))) {
			string_const_t dir = path_directory_name(path, length);
			resource_autoimport_unwatch_dir(STRING_ARGS(dir));
		}
	}
	mutex_unlock(_resource_autoimport_lock);
}

void
resource_autoimport_clear(void) {
	size_t ipath, psize;
	mutex_lock(_resource_autoimport_lock);
	for (ipath = 0, psize = array_size(_resource_autoimport_dir); ipath < psize; ++ipath)
		fs_unmonitor(STRING_ARGS(_resource_autoimport_dir[ipath]));
	string_array_deallocate(_resource_autoimport_dir);
	mutex_unlock(_resource_autoimport_lock);
}

void
resource_autoimport_event_handle(event_t* event) {
	if (!resource_module_config().enable_local_autoimport)
		return;

	if (event->id != FOUNDATIONEVENT_FILE_MODIFIED)
		return;

	const string_const_t path = fs_event_path(event);

	for (size_t ipath = 0, psize = array_size(_resource_autoimport_dir); ipath < psize; ++ipath) {
		if (path_subpath(STRING_ARGS(path), STRING_ARGS(_resource_autoimport_dir[ipath])).length) {
			const resource_signature_t sig = resource_import_map_lookup(STRING_ARGS(path));
			if (!uuid_is_null(sig.uuid)) {
				const string_const_t uuidstr = string_from_uuid_static(sig.uuid);
				log_debugf(HASH_RESOURCE, STRING_CONST("Autoimport event trigger: %.*s (%.*s)"),
				           STRING_FORMAT(path), STRING_FORMAT(uuidstr));
				resource_event_post(RESOURCEEVENT_MODIFY, sig.uuid);
			}
		}
	}
}

#else

bool
resource_import(const char* path, size_t length, const uuid_t uuid) {
	FOUNDATION_UNUSED(path);
	FOUNDATION_UNUSED(length);
	FOUNDATION_UNUSED(uuid);
	return false;
}

void
resource_import_register(resource_import_fn importer) {
	FOUNDATION_UNUSED(importer);
}

void
resource_import_register_path(const char* path, size_t length) {
	FOUNDATION_UNUSED(path);
	FOUNDATION_UNUSED(length);
}

void
resource_import_unregister(resource_import_fn importer) {
	FOUNDATION_UNUSED(importer);
}

void
resource_import_unregister_path(const char* path, size_t length) {
	FOUNDATION_UNUSED(path);
	FOUNDATION_UNUSED(length);
}

resource_signature_t
resource_import_map_lookup(const char* path, size_t length) {
	resource_signature_t sig = {uuid_null(), uint256_null()};
	FOUNDATION_UNUSED(path);
	FOUNDATION_UNUSED(length);
	return sig;
}

uuid_t
resource_import_map_store(const char* path, size_t length, uuid_t uuid, uint256_t sighash) {
	FOUNDATION_UNUSED(path);
	FOUNDATION_UNUSED(length);
	FOUNDATION_UNUSED(uuid);
	FOUNDATION_UNUSED(sighash);
	return uuid_null();
}

bool
resource_import_map_purge(const char* path, size_t length) {
	FOUNDATION_UNUSED(path);
	FOUNDATION_UNUSED(length);
	return false;
}

int
resource_autoimport_initialize(void) {
	return 0;
}

void
resource_autoimport_finalize(void) {
}

bool
resource_autoimport(const uuid_t uuid) {
	FOUNDATION_UNUSED(uuid);
	return false;
}

bool
resource_autoimport_need_update(const uuid_t uuid, uint64_t platform) {
	FOUNDATION_UNUSED(uuid);
	FOUNDATION_UNUSED(platform);
	return false;
}

void
resource_autoimport_watch(const char* path, size_t length) {
	FOUNDATION_UNUSED(path);
	FOUNDATION_UNUSED(length);
}

void
resource_autoimport_unwatch(const char* path, size_t length) {
	FOUNDATION_UNUSED(path);
	FOUNDATION_UNUSED(length);
}

void
resource_autoimport_clear(void) {
}

void
resource_autoimport_event_handle(event_t* event) {
	FOUNDATION_UNUSED(event);
}

#endif
