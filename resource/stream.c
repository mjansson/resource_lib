/* stream.c  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
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

#include <foundation/foundation.h>

stream_t*
resource_stream_open_static(const uuid_t res, uint64_t platform) {
	stream_t* stream;

	if (resource_autoimport_need_update(res, platform)) {
		string_const_t uuidstr = string_from_uuid_static(res);
		log_debugf(HASH_RESOURCE, STRING_CONST("Reimporting resource %.*s (open static)"),
		           STRING_FORMAT(uuidstr));
		resource_autoimport(res);
	}

	if (resource_compile_need_update(res, platform)) {
		string_const_t uuidstr = string_from_uuid_static(res);
		log_debugf(HASH_RESOURCE, STRING_CONST("Recompiling resource %.*s (open static)"),
		           STRING_FORMAT(uuidstr));
		resource_compile(res, platform);
	}

	stream = resource_local_open_static(res, platform);
	if (stream)
		return stream;

	stream = resource_remote_open_static(res, platform);
	if (stream)
		return stream;

	string_const_t uuidstr = string_from_uuid_static(res);
	log_warnf(HASH_RESOURCE, WARNING_RESOURCE,
	          STRING_CONST("Unable to open static stream for resource: %.*s"),
	          STRING_FORMAT(uuidstr));

	return 0;
}

stream_t*
resource_stream_open_dynamic(const uuid_t res, uint64_t platform) {
	stream_t* stream;

	if (resource_autoimport_need_update(res, platform)) {
		string_const_t uuidstr = string_from_uuid_static(res);
		log_debugf(HASH_RESOURCE, STRING_CONST("Reimporting resource %.*s (open dynamic)"),
		           STRING_FORMAT(uuidstr));
		resource_autoimport(res);
	}

	if (resource_compile_need_update(res, platform)) {
		string_const_t uuidstr = string_from_uuid_static(res);
		log_debugf(HASH_RESOURCE, STRING_CONST("Recompiling resource %.*s (open dynamic)"),
		           STRING_FORMAT(uuidstr));
		resource_compile(res, platform);
	}

	stream = resource_local_open_dynamic(res, platform);
	if (stream)
		return stream;

	stream = resource_remote_open_dynamic(res, platform);
	if (stream)
		return stream;

	string_const_t uuidstr = string_from_uuid_static(res);
	log_warnf(HASH_RESOURCE, WARNING_RESOURCE,
	          STRING_CONST("Unable to open dynamic stream for resource: %.*s"),
	          STRING_FORMAT(uuidstr));

	return 0;
}

string_t
resource_stream_make_path(char* buffer, size_t capacity, const char* base, size_t base_length,
                          const uuid_t res) {
	string_const_t uuidstr = string_from_uuid_static(res);
	return string_format(buffer, capacity, STRING_CONST("%.*s/%.2s/%.2s/%.*s"),
	                     (int)base_length, base, uuidstr.str, uuidstr.str + 2,
	                     STRING_FORMAT(uuidstr));
}

void
resource_stream_write_header(stream_t* stream, const resource_header_t header) {
	stream_write_uint64(stream, header.type);
	stream_write_uint32(stream, header.version);
	stream_write_uint64(stream, header.source_hash.word[0]);
	stream_write_uint64(stream, header.source_hash.word[1]);
	stream_write_uint64(stream, header.source_hash.word[2]);
	stream_write_uint64(stream, header.source_hash.word[3]);
}

resource_header_t
resource_stream_read_header(stream_t* stream) {
	resource_header_t header;
	header.type = stream_read_uint64(stream);
	header.version = stream_read_uint32(stream);
	header.source_hash.word[0] = stream_read_uint64(stream);
	header.source_hash.word[1] = stream_read_uint64(stream);
	header.source_hash.word[2] = stream_read_uint64(stream);
	header.source_hash.word[3] = stream_read_uint64(stream);
	return header;
}
