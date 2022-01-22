/* import.c  -  Resource library  -  Public Domain  -  2014 Mattias Jansson
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

static resource_import_fn* resource_importers;
static string_t resource_import_path_base;
static string_t* resource_import_path_tool;

int
resource_import_initialize(void) {
	return 0;
}

void
resource_import_finalize(void) {
	array_deallocate(resource_importers);
	string_deallocate(resource_import_path_base.str);
	string_array_deallocate(resource_import_path_tool);

	resource_importers = 0;
	resource_import_path_base = string(0, 0);
}

string_const_t
resource_import_base_path(void) {
	return string_to_const(resource_import_path_base);
}

void
resource_import_set_base_path(const char* path, size_t length) {
	if (resource_import_path_base.str)
		string_deallocate(resource_import_path_base.str);
	resource_import_path_base = length ? string_clone(path, length) : string(0, 0);
}

#if RESOURCE_ENABLE_LOCAL_SOURCE

#if FOUNDATION_PLATFORM_WINDOWS
#define RESOURCE_IMPORTER_PATTERN "^.*import\\.exe$"
#else
#define RESOURCE_IMPORTER_PATTERN "^.*import$"
#endif

bool
resource_import(const char* path, size_t length, const uuid_t uuid) {
	size_t iimp, isize;
	size_t internal = 0;
	size_t external = 0;
	bool was_imported = false;
	stream_t* stream = stream_open(path, length, STREAM_IN);
	if (!stream) {
		log_warnf(HASH_RESOURCE, WARNING_RESOURCE, STRING_CONST("Unable to open input stream for importing: %.*s"),
		          (int)length, path);
		return false;
	}

	size_t streampos = stream_tell(stream);
	uint256_t import_hash = stream_sha256(stream);
	stream_seek(stream, (ssize_t)streampos, STREAM_SEEK_BEGIN);

	for (iimp = 0, isize = array_size(resource_importers); !was_imported && (iimp != isize); ++iimp) {
		stream_seek(stream, 0, STREAM_SEEK_BEGIN);
		was_imported |= (resource_importers[iimp](stream, uuid) == 0);
		++internal;
	}
	stream_deallocate(stream);

	// Try external tools until imported successfully
	for (size_t ipath = 0, psize = array_size(resource_import_path_tool); !was_imported && (ipath != psize); ++ipath) {
		string_t* tools = fs_matching_files(STRING_ARGS(resource_import_path_tool[ipath]),
		                                    STRING_CONST(RESOURCE_IMPORTER_PATTERN), true);
		for (size_t itool = 0, tsize = array_size(tools); !was_imported && (itool != tsize); ++itool) {
			char buffer[BUILD_MAX_PATHLEN];
			string_t fullpath = path_concat(buffer, sizeof(buffer), STRING_ARGS(resource_import_path_tool[ipath]),
			                                STRING_ARGS(tools[itool]));

			process_t proc;
			process_initialize(&proc);

			string_const_t wd = environment_current_working_directory();
			process_set_working_directory(&proc, STRING_ARGS(wd));
			process_set_executable_path(&proc, STRING_ARGS(fullpath));

			string_const_t* args = nullptr;
			array_push(args, string_const(path, length));
			array_push(args, string_const(STRING_CONST("--")));

			string_const_t local_source = resource_source_path();
			if (local_source.length) {
				array_push(args, string_const(STRING_CONST("--resource-source-path")));
				array_push(args, local_source);
			}
			string_const_t base_path = resource_import_base_path();
			if (base_path.length) {
				array_push(args, string_const(STRING_CONST("--resource-base-path")));
				array_push(args, base_path);
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
				log_debugf(HASH_RESOURCE, STRING_CONST("Imported with external tool: %.*s"),
				           STRING_FORMAT(tools[itool]));
				was_imported = true;
			} else {
				log_debugf(HASH_RESOURCE, STRING_CONST("Failed importing with external tool: %.*s (%d)"),
				           STRING_FORMAT(tools[itool]), exit_code);
			}

			process_finalize(&proc);
			array_deallocate(args);

			++external;
		}
		string_array_deallocate(tools);
	}

	if (!was_imported) {
		log_warnf(HASH_RESOURCE, WARNING_RESOURCE,
		          STRING_CONST("Unable to import: %.*s (%" PRIsize " internal, %" PRIsize " external)"), (int)length,
		          path, internal, external);
	} else {
		resource_source_set_import_hash(uuid, import_hash);
		log_infof(HASH_RESOURCE, STRING_CONST("Imported: %.*s"), (int)length, path);
	}
	return was_imported;
}

void
resource_import_register(resource_import_fn importer) {
	size_t iimp, isize;
	for (iimp = 0, isize = array_size(resource_importers); iimp != isize; ++iimp) {
		if (resource_importers[iimp] == importer)
			return;
	}
	array_push(resource_importers, importer);
}

void
resource_import_register_path(const char* path, size_t length) {
	size_t iimp, isize;
	char buffer[BUILD_MAX_PATHLEN];
	string_t pathstr = string_copy(buffer, sizeof(buffer), path, length);
	pathstr = path_clean(STRING_ARGS(pathstr), sizeof(buffer));
	for (iimp = 0, isize = array_size(resource_import_path_tool); iimp != isize; ++iimp) {
		if (string_equal(STRING_ARGS(resource_import_path_tool[iimp]), STRING_ARGS(pathstr)))
			return;
	}
	pathstr = string_clone(STRING_ARGS(pathstr));
	array_push(resource_import_path_tool, pathstr);
}

void
resource_import_unregister(resource_import_fn importer) {
	size_t iimp, isize;
	for (iimp = 0, isize = array_size(resource_importers); iimp != isize; ++iimp) {
		if (resource_importers[iimp] == importer) {
			array_erase(resource_importers, iimp);
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
	for (iimp = 0, isize = array_size(resource_import_path_tool); iimp != isize; ++iimp) {
		if (string_equal(STRING_ARGS(resource_import_path_tool[iimp]), STRING_ARGS(pathstr))) {
			string_deallocate(resource_import_path_tool[iimp].str);
			array_erase(resource_import_path_tool, iimp);
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
		string_t map_path = path_concat(buffer, sizeof(buffer), STRING_ARGS(path), STRING_CONST(RESOURCE_IMPORT_MAP));
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
		string_t map_path = path_concat(buffer, sizeof(buffer), STRING_ARGS(path), STRING_CONST(RESOURCE_IMPORT_MAP));
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
	char buffer[BUILD_MAX_PATHLEN + 64];
	string_t line;
	resource_signature_t sig = {uuid_null(), uint256_null()};
	// TODO: This needs to be a DB as number of imported files grow
	while (!stream_eos(map) && uuid_is_null(sig.uuid)) {
		hash_t linehash;
		string_const_t linepath;
		size_t streampos = stream_tell(map);

		line = stream_read_line_buffer(map, buffer, sizeof(buffer), '\n');
		if (line.length < 120)
			continue;
		if (line.str[line.length - 1] == '\r')
			--line.length;

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
	// TODO: Implement
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

static mutex_t* resource_autoimport_lock;
static string_t* resource_autoimport_dir;
static atomic64_t resource_autoimport_token_value;

int
resource_autoimport_initialize(void) {
	resource_autoimport_lock = mutex_allocate(STRING_CONST("resource-autoimport"));
	return 0;
}

void
resource_autoimport_finalize(void) {
	resource_autoimport_clear();
	mutex_deallocate(resource_autoimport_lock);
}

static hash_t
resource_autoimport_token(void) {
	return (hash_t)atomic_incr64(&resource_autoimport_token_value, memory_order_acq_rel);
}

string_t
resource_autoimport_reverse_lookup(const uuid_t uuid, char* buffer, size_t capacity) {
	// TODO: Improve
	string_t result = (string_t){buffer, 0};
	regex_t* regex;
	size_t ipath, psize;
	size_t imap, msize;
	uuid_t siguuid;
	char linebuffer[BUILD_MAX_PATHLEN];

	regex = regex_compile(STRING_CONST("^" RESOURCE_IMPORT_MAP "$"));

	for (ipath = 0, psize = array_size(resource_autoimport_dir); !result.length && (ipath < psize); ++ipath) {
		string_t* maps = fs_matching_files_regex(STRING_ARGS(resource_autoimport_dir[ipath]), regex, true);
		for (imap = 0, msize = array_size(maps); !result.length && (imap < msize); ++imap) {
			string_t mappath = path_concat(linebuffer, sizeof(linebuffer), STRING_ARGS(resource_autoimport_dir[ipath]),
			                               STRING_ARGS(maps[imap]));
			stream_t* map = stream_open(STRING_ARGS(mappath), STREAM_IN);

			while (map && !stream_eos(map)) {
				string_t line = stream_read_line_buffer(map, linebuffer, sizeof(linebuffer), '\n');
				if (line.length < 120)
					continue;
				if (line.str[line.length - 1] == '\r')
					--line.length;

				siguuid = string_to_uuid(line.str + 17, 37);
				if (uuid_equal(uuid, siguuid)) {
					string_const_t linepath = string_substr(STRING_ARGS(line), 119, line.length);
					string_const_t mapdir = stream_path(map);
					mapdir = path_directory_name(STRING_ARGS(mapdir));
					result = path_concat(buffer, capacity, STRING_ARGS(mapdir), STRING_ARGS(linepath));
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

	mutex_lock(resource_autoimport_lock);
	char buffer[BUILD_MAX_PATHLEN];
	string_t path = resource_autoimport_reverse_lookup(uuid, buffer, sizeof(buffer));
	mutex_unlock(resource_autoimport_lock);

	string_const_t uuidstr = string_from_uuid_static(uuid);
	if (!path.length) {
		log_warnf(HASH_RESOURCE, WARNING_RESOURCE, STRING_CONST("Autoimport failed, no reverse path for %.*s"),
		          STRING_FORMAT(uuidstr));
		return false;
	}

	log_debugf(HASH_RESOURCE, STRING_CONST("Autoimport: %.*s -> %.*s"), STRING_FORMAT(uuidstr), STRING_FORMAT(path));
	return resource_import(STRING_ARGS(path), uuid);
}

static bool
resource_autoimport_source_changed(const char* path, size_t length, uint256_t map_hash, uint256_t import_hash,
                                   uint256_t* newhash) {
	stream_t* stream = stream_open(path, length, STREAM_IN);
	if (!stream)
		return false;
	uint256_t testhash = stream_sha256(stream);
	stream_deallocate(stream);
	if (newhash)
		*newhash = testhash;
	return !uint256_equal(map_hash, testhash) || !uint256_equal(import_hash, testhash);
}

bool
resource_autoimport_need_update(const uuid_t uuid, uint64_t platform) {
	FOUNDATION_UNUSED(platform);
	union {
		char path[BUILD_MAX_PATHLEN];
		resource_dependency_t deps[BUILD_MAX_PATHLEN / sizeof(resource_dependency_t)];
	} buffer;

	if (!resource_module_config().enable_local_autoimport)
		return false;
	if (resource_remote_sourced_is_connected())
		return false;

	if (!resource_source_read(nullptr, uuid)) {
		string_const_t uuidstr = string_from_uuid_static(uuid);
		log_debugf(HASH_RESOURCE, STRING_CONST("Autoimport needed, source file missing: %.*s"), STRING_FORMAT(uuidstr));
		return true;
	}

	mutex_lock(resource_autoimport_lock);
	string_t path = resource_autoimport_reverse_lookup(uuid, buffer.path, sizeof(buffer.path));
	mutex_unlock(resource_autoimport_lock);
	if (path.length) {
		resource_signature_t sig = resource_import_map_lookup(STRING_ARGS(path));
		// Check if import map hash differs from imported asset file hash, or if source
		// import hash differs from imported asset file hash -> if so, need reimport. This ensures
		// all three components of the resource (imported asset, import map signature and source)
		// are up to date and in sync.
		uint256_t import_hash = resource_source_import_hash(uuid);
		if (resource_autoimport_source_changed(STRING_ARGS(path), sig.hash, import_hash, nullptr)) {
			string_const_t uuidstr = string_from_uuid_static(uuid);
			log_debugf(HASH_RESOURCE, STRING_CONST("Autoimport needed, source hash changed: %.*s"),
			           STRING_FORMAT(uuidstr));
			return true;
		}
	}

	return false;
}

static void
resource_autoimport_unwatch_dir(const char* path, size_t length) {
	ssize_t idx = string_array_find((const string_const_t*)resource_autoimport_dir, array_size(resource_autoimport_dir),
	                                path, length);
	if (idx < 0)
		return;

	log_debugf(HASH_RESOURCE, STRING_CONST("Autoimport unwatch dir: %.*s"), (int)length, path);

	fs_unmonitor(path, length);
	string_deallocate(resource_autoimport_dir[idx].str);
	array_erase(resource_autoimport_dir, idx);
}

static void
resource_autoimport_watch_dir(const char* path, size_t length) {
	size_t ipath, psize;
	for (ipath = 0, psize = array_size(resource_autoimport_dir); ipath < psize; ++ipath) {
		// Check if something is already watching this dir or any parent
		if (string_equal(path, length, STRING_ARGS(resource_autoimport_dir[ipath])) ||
		    path_subpath(path, length, STRING_ARGS(resource_autoimport_dir[ipath])).length) {
			log_debugf(HASH_RESOURCE, STRING_CONST("Autoimport already watching dir: %.*s (%.*s)"), (int)length, path,
			           STRING_FORMAT(resource_autoimport_dir[ipath]));
			break;
		}
	}
	if (ipath == psize) {
		// Check if we will replace a more specific monitor
		for (ipath = 0, psize = array_size(resource_autoimport_dir); ipath < psize;) {
			if (path_subpath(STRING_ARGS(resource_autoimport_dir[ipath]), path, length).length) {
				resource_autoimport_unwatch_dir(STRING_ARGS(resource_autoimport_dir[ipath]));
				psize = array_size(resource_autoimport_dir);
			} else {
				++ipath;
			}
		}
		log_debugf(HASH_RESOURCE, STRING_CONST("Autoimport watch dir: %.*s"), (int)length, path);
		if (fs_monitor(path, length))
			array_push(resource_autoimport_dir, string_clone(path, length));
	}
}

void
resource_autoimport_watch(const char* path, size_t length) {
	if (!resource_module_config().enable_local_autoimport)
		return;
	mutex_lock(resource_autoimport_lock);
	if (fs_is_directory(path, length)) {
		resource_autoimport_watch_dir(path, length);
	} else if (fs_is_file(path, length)) {
		string_const_t filename = path_file_name(path, length);
		if (string_equal(STRING_ARGS(filename), STRING_CONST(RESOURCE_IMPORT_MAP))) {
			string_const_t dir = path_directory_name(path, length);
			resource_autoimport_watch_dir(STRING_ARGS(dir));
		}
	}
	mutex_unlock(resource_autoimport_lock);
}

void
resource_autoimport_unwatch(const char* path, size_t length) {
	if (!resource_module_config().enable_local_autoimport)
		return;
	mutex_lock(resource_autoimport_lock);
	if (fs_is_directory(path, length)) {
		resource_autoimport_unwatch_dir(path, length);
	} else if (fs_is_file(path, length)) {
		string_const_t filename = path_file_name(path, length);
		if (string_equal(STRING_ARGS(filename), STRING_CONST(RESOURCE_IMPORT_MAP))) {
			string_const_t dir = path_directory_name(path, length);
			resource_autoimport_unwatch_dir(STRING_ARGS(dir));
		}
	}
	mutex_unlock(resource_autoimport_lock);
}

void
resource_autoimport_clear(void) {
	size_t ipath, psize;
	mutex_lock(resource_autoimport_lock);
	for (ipath = 0, psize = array_size(resource_autoimport_dir); ipath < psize; ++ipath)
		fs_unmonitor(STRING_ARGS(resource_autoimport_dir[ipath]));
	string_array_deallocate(resource_autoimport_dir);
	mutex_unlock(resource_autoimport_lock);
}

static uuid_t resource_autoimport_last_uuid;
static uint256_t resource_autoimport_last_hash;

void
resource_autoimport_event_handle(event_t* event) {
	if (!resource_module_config().enable_local_autoimport)
		return;

	if ((event->id != FOUNDATIONEVENT_FILE_MODIFIED) && (event->id != FOUNDATIONEVENT_FILE_CREATED))
		return;

	const string_const_t path = fs_event_path(event);
	for (size_t ipath = 0, psize = array_size(resource_autoimport_dir); ipath < psize; ++ipath) {
		if (path_subpath(STRING_ARGS(path), STRING_ARGS(resource_autoimport_dir[ipath])).length) {
			const resource_signature_t sig = resource_import_map_lookup(STRING_ARGS(path));
			uint256_t import_hash = resource_source_import_hash(sig.uuid);
			uint256_t newhash;
			if (!uuid_is_null(sig.uuid) &&
			    resource_autoimport_source_changed(STRING_ARGS(path), sig.hash, import_hash, &newhash)) {
				// Suppress multiple events on same file in sequence
				if (!uuid_equal(sig.uuid, resource_autoimport_last_uuid) ||
				    !uint256_equal(newhash, resource_autoimport_last_hash)) {
					resource_autoimport_last_uuid = sig.uuid;
					resource_autoimport_last_hash = newhash;

					hash_t token = resource_autoimport_token();
#if BUILD_ENABLE_DEBUG_LOG
					size_t reverse_count = resource_source_reverse_dependencies_count(sig.uuid, 0);
					const string_const_t uuidstr = string_from_uuid_static(sig.uuid);
					log_debugf(
					    HASH_RESOURCE,
					    STRING_CONST("Autoimport event trigger: %.*s (%.*s) : %" PRIsize " reverse dependencies"),
					    STRING_FORMAT(path), STRING_FORMAT(uuidstr), reverse_count);
#endif
					resource_event_post(RESOURCEEVENT_MODIFY, sig.uuid, 0, token);
					resource_event_post_depends(sig.uuid, 0, token);
				}
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

resource_signature_t
resource_import_lookup(const char* path, size_t length) {
	resource_signature_t sig;
	memset(&sig, 0, sizeof(sig));
	FOUNDATION_UNUSED(path);
	FOUNDATION_UNUSED(length);
	return sig;
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
