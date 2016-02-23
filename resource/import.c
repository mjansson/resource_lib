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

#define IMPORT_MAP "import.map"

static stream_t*
resource_import_open_map(const char* cpath, size_t length, bool write) {
	char buffer[BUILD_MAX_PATHLEN];
	string_const_t last_path;
	string_const_t path = path_directory_name(cpath, length);
	while (path.length > 1) {
		string_t map_path = path_concat(buffer, sizeof(buffer),
		                                STRING_ARGS(path), STRING_CONST(IMPORT_MAP));
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
		                                STRING_ARGS(path), STRING_CONST(IMPORT_MAP));
		stream_t* stream = stream_open(STRING_ARGS(map_path), STREAM_IN | STREAM_OUT | STREAM_CREATE);
		if (stream)
			return stream;
	}
	return nullptr;
}

static stream_t*
resource_import_create_map(const char* cpath, size_t length) {
	char buffer[BUILD_MAX_PATHLEN];
	string_t map_path = path_concat(buffer, sizeof(buffer),
	                                cpath, length, STRING_CONST(IMPORT_MAP));
	stream_t* stream = stream_open(STRING_ARGS(map_path), STREAM_OUT | STREAM_CREATE);
	return stream;
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

static FOUNDATION_NOINLINE uuid_t
resource_import_map_read(stream_t* map, hash_t pathhash, const char* path, size_t length) {
	char buffer[BUILD_MAX_PATHLEN+64];
	string_t line;
	while (!stream_eos(map)) {
		hash_t linehash;
		string_const_t linepath;

		line = stream_read_line_buffer(map, buffer, sizeof(buffer), '\n');
		if (line.length < 54)
			continue;

		linehash = string_to_uint64(STRING_ARGS(line), true);
		if (linehash != pathhash)
			continue;

		linepath = string_substr(STRING_ARGS(line), 54, line.length);
		if (!string_equal(STRING_ARGS(linepath), path, length))
			continue;

		return string_to_uuid(line.str + 17, 37);
	}
	return uuid_null();
}

uuid_t
resource_import_map_lookup(const char* path, size_t length) {
	string_const_t subpath;
	hash_t pathhash;
	uuid_t uuid;

	stream_t* map = resource_import_open_map(path, length, false);
	if (!map)
		return uuid_null();

	subpath = resource_import_map_subpath(map, path, length);
	pathhash = hash(STRING_ARGS(subpath));
	uuid = resource_import_map_read(map, pathhash, STRING_ARGS(subpath));

	stream_deallocate(map);

	return uuid;
}

bool
resource_import_map_store(const char* path, size_t length, uuid_t* uuid) {
	string_const_t subpath;
	hash_t pathhash;
	uuid_t founduuid;

	stream_t* map = resource_import_open_map(path, length, true);
	if (!map) {
		map = resource_import_create_map(path, length);
		if (!map) {
			log_warn(HASH_RESOURCE, WARNING_SUSPICIOUS, STRING_CONST("No map to store in"));
			return false;
		}
	}

	subpath = resource_import_map_subpath(map, path, length);
	pathhash = hash(STRING_ARGS(subpath));
	founduuid = resource_import_map_read(map, pathhash, STRING_ARGS(subpath));

	if (uuid_is_null(founduuid)) {
		string_const_t token;
		char separator = ' ';

		stream_seek(map, 0, STREAM_SEEK_END);

		token = string_from_uint_static(pathhash, true, 16, '0');
		stream_write(map, STRING_ARGS(token));
		stream_write(map, &separator, 1);

		token = string_from_uuid_static(*uuid);
		stream_write(map, STRING_ARGS(token));
		stream_write(map, &separator, 1);

		stream_write(map, STRING_ARGS(subpath));
		stream_write_endl(map);
	}
	else {
		*uuid = founduuid;
	}

	stream_deallocate(map);

	return true;
}

bool
resource_import_map_purge(const char* path, size_t length) {
	//TODO: Implement
	FOUNDATION_UNUSED(path);
	FOUNDATION_UNUSED(length);
	return false;
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

uuid_t
resource_import_map_lookup(const char* path, size_t length) {
	FOUNDATION_UNUSED(path);
	FOUNDATION_UNUSED(length);
	return uuid_null();
}

bool
resource_import_map_store(const char* path, size_t length, uuid_t* uuid) {
	FOUNDATION_UNUSED(path);
	FOUNDATION_UNUSED(length);
	FOUNDATION_UNUSED(uuid);
	return false;
}

bool
resource_import_map_purge(const char* path, size_t length) {
	FOUNDATION_UNUSED(path);
	FOUNDATION_UNUSED(length);
	return false;
}

#endif
