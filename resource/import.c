/* import.c  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
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

#include <resource/import.h>
#include <resource/internal.h>

#include <foundation/foundation.h>

resource_import_fn* _resource_importers;

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
	return was_imported;
}

void
resource_import_register(resource_import_fn importer) {
	array_push(_resource_importers, importer);
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
				stream_seek(map, streampos + 54, STREAM_SEEK_BEGIN);
				stream_write(map, STRING_ARGS(token));
				sig.hash = update_hash;
			}
		}
	}
	return sig;
}

resource_signature_t
resource_import_map_lookup(const char* path, size_t length) {
	string_const_t subpath;
	hash_t pathhash;
	resource_signature_t sig = {uuid_null(), uint256_null()};

	stream_t* map = resource_import_open_map(path, length, false);
	if (!map)
		return sig;

	subpath = resource_import_map_subpath(map, path, length);
	pathhash = hash(STRING_ARGS(subpath));
	sig = resource_import_map_read_and_update(map, pathhash, STRING_ARGS(subpath), uint256_null());

	stream_deallocate(map);

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

string_t* _resource_autoimport_dir;

static string_t
resource_autoimport_reverse_lookup(const uuid_t uuid, char* buffer, size_t capacity) {
	//... implement ...
	return (string_t){buffer, 0};
}

bool
resource_autoimport(const uuid_t uuid) {
	char buffer[BUILD_MAX_PATHLEN];
	string_t path = resource_autoimport_reverse_lookup(uuid, buffer, sizeof(buffer));
	if (path.length)
		return resource_import(STRING_ARGS(path), uuid);
	return false;
}

bool
resource_autoimport_need_update(const uuid_t uuid) {
	FOUNDATION_UNUSED(uuid);
	char buffer[BUILD_MAX_PATHLEN];
	string_t path = resource_autoimport_reverse_lookup(uuid, buffer, sizeof(buffer));
	if (path.length) {
		resource_signature_t sig = resource_import_map_lookup(STRING_ARGS(path));
		stream_t* stream = stream_open(STRING_ARGS(path), STREAM_IN);
		uint256_t newhash = stream_sha256(stream);
		stream_deallocate(stream);
		return !uint256_equal(sig.hash, newhash);
	}
	return false;
}

static void
resource_autoimport_watch_dir(const char* path, size_t length) {
	size_t ipath, psize;
	for (ipath = 0, psize = array_size(_resource_autoimport_dir); ipath < psize; ++ipath) {
		//Check if something is already watching this dir or any parent
		if (string_equal(path, length, STRING_ARGS(_resource_autoimport_dir[ipath])) ||
		        path_subpath(path, length, STRING_ARGS(_resource_autoimport_dir[ipath])).length)
			break;
	}
	if (ipath == psize) {
		//Check if we will replace a more specific monitor
		for (ipath = 0, psize = array_size(_resource_autoimport_dir); ipath < psize; ++ipath) {
			if (path_subpath(STRING_ARGS(_resource_autoimport_dir[ipath]), path, length).length) {
				//... implement ...
			}
		}
		if (fs_monitor(path, length))
			array_push(_resource_autoimport_dir, string_clone(path, length));
	}
}

static void
resource_autoimport_unwatch_dir(const char* path, size_t length) {
	ssize_t idx = string_array_find((const string_const_t*)_resource_autoimport_dir,
	                                array_size(_resource_autoimport_dir), path, length);
	if (idx < 0)
		return;

	fs_unmonitor(path, length);
	string_deallocate(_resource_autoimport_dir[idx].str);
	array_erase(_resource_autoimport_dir, idx);
}

void
resource_autoimport_watch(const char* path, size_t length) {
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
}

void
resource_autoimport_unwatch(const char* path, size_t length) {
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
}

void
resource_autoimport_clear(void) {
	size_t ipath, psize;
	for (ipath = 0, psize = array_size(_resource_autoimport_dir); ipath < psize; ++ipath)
		fs_unmonitor(STRING_ARGS(_resource_autoimport_dir[ipath]));
	string_array_deallocate(_resource_autoimport_dir);
}

void
resource_autoimport_event_handle(event_t* event) {
	//... implement ...
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
}

void
resource_import_unregister(resource_import_fn importer) {
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

bool
resource_autoimport(const uuid_t uuid) {
	FOUNDATION_UNUSED(uuid);
	return false;
}

bool
resource_autoimport_need_update(const uuid_t uuid) {
	FOUNDATION_UNUSED(uuid);
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
}

#endif
