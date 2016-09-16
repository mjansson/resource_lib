/* sourced.c  -  Resource library  -  Public Domain  -  2016 Mattias Jansson / Rampant Pixels
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
#include <resource/sourced.h>

#include <foundation/foundation.h>
#include <network/socket.h>

int
sourced_write_lookup(socket_t* sock, const char* path, size_t length) {
	sourced_message_t msg = {
		SOURCED_LOOKUP,
		(uint32_t)length
	};
	if (socket_write(sock, &msg, sizeof(msg)) == sizeof(msg)) {
		if (socket_write(sock, path, length) == length)
			return 0;
	}
	return -1;
}

int
sourced_write_lookup_reply(socket_t* sock, uuid_t uuid, uint256_t hash) {
	sourced_message_t msg = {
		SOURCED_LOOKUP_RESULT,
		(uint32_t)sizeof(sourced_lookup_result_t)
	};
	sourced_lookup_result_t reply = {
		!uuid_is_null(uuid) ? SOURCED_OK : SOURCED_FAILED,
		0,
		uuid,
		hash
	};
	if (socket_write(sock, &msg, sizeof(msg)) == sizeof(msg)) {
		if (socket_write(sock, &reply, sizeof(reply)) == sizeof(reply))
			return 0;
	}
	return -1;
}

int
sourced_read_lookup_reply(socket_t* sock, size_t size, sourced_lookup_result_t* result) {
	if (size != sizeof(sourced_lookup_result_t))
		return -1;
	size_t read = socket_read(sock, result, size);
	if (read == size)
		return 0;
	
	log_warnf(HASH_RESOURCE, WARNING_SYSTEM_CALL_FAIL, STRING_CONST("Read partial lookup reply: %" PRIsize " of %" PRIsize),
	          read, size);
	return -1;
}

int
sourced_write_read(socket_t* sock, uuid_t uuid) {
	sourced_read_t msg = {
		SOURCED_READ,
		(uint32_t)(sizeof(uuid_t)),
		uuid
	};
	if (socket_write(sock, &msg, sizeof(msg)) == sizeof(msg))
		return 0;
	return -1;
}

typedef struct {
	uint32_t count;
	size_t size;
	sourced_change_t* change;
	char* payload;
	size_t offset;
} sourced_walker_t;

static int
sourced_count_source(resource_change_t* change, void* data) {
	sourced_walker_t* walker = data;
	++walker->count;
	if (change->flags & RESOURCE_SOURCEFLAG_VALUE)
		walker->size += change->value.value.length;
	return 0;
}

static int
sourced_copy_source(resource_change_t* change, void* data) {
	sourced_walker_t* walker = data;
	sourced_change_t* dest = &walker->change[walker->count++];
	dest->timestamp = change->timestamp;
	dest->hash = change->hash;
	dest->platform = change->platform;
	dest->flags = change->flags;
	if (change->flags & RESOURCE_SOURCEFLAG_BLOB) {
		dest->value.blob.checksum = change->value.blob.checksum;
		dest->value.blob.size = change->value.blob.size;
	}
	else if (change->flags & RESOURCE_SOURCEFLAG_VALUE) {
		memcpy(walker->payload + walker->offset, change->value.value.str, change->value.value.length);
		dest->value.value.offset = walker->offset;
		dest->value.value.length = change->value.value.length;
		walker->offset += change->value.value.length;
	}
	return 0;
}

int
sourced_write_read_reply(socket_t* sock, resource_source_t* source, uint256_t hash) {
	sourced_message_t msg = {
		SOURCED_READ_RESULT,
		0
	};

	void* allocated = nullptr;
	void* reply = nullptr;
	size_t size = 0;
	uint32_t result = SOURCED_OK;

	if (!source) {
		result = SOURCED_FAILED;
		reply = &result;
		size = sizeof(uint32_t);
	}
	else {
		sourced_walker_t walker;
		walker.count = 0;
		walker.size = 0;

		hashmap_fixed_t fixedmap;
		hashmap_t* map = (hashmap_t*)&fixedmap;
		hashmap_initialize(map, sizeof(fixedmap.bucket)/sizeof(fixedmap.bucket[0]), 0);

		resource_source_map_all(source, map, true);
		resource_source_map_iterate(source, map, &walker, sourced_count_source);

		size = sizeof(sourced_read_result_t) + walker.size + (sizeof(sourced_change_t) * walker.count);
		
		sourced_read_result_t* result = memory_allocate(HASH_RESOURCE, size, 0, MEMORY_PERSISTENT);
		result->result = SOURCED_OK;
		result->flags = 0;
		result->hash = hash;
		result->num_changes = walker.count;

		walker.count = 0;
		walker.size = 0;
		walker.change = (sourced_change_t*)result->payload;
		walker.payload = result->payload;
		walker.offset = sizeof(sourced_change_t) * result->num_changes;

		resource_source_map_iterate(source, map, &walker, sourced_copy_source);
		resource_source_map_clear(map);
		hashmap_finalize(map);

		reply = allocated = result;
	}

	msg.size = (uint32_t)size;

	int ret = -1;
	size_t written;
	if ((written = socket_write(sock, &msg, sizeof(msg))) == sizeof(msg)) {
		if ((written = socket_write(sock, reply, size)) == size)
			ret = 0;
	}

	memory_deallocate(allocated);

	return ret;
}

int
sourced_read_read_reply(socket_t* sock, size_t size, sourced_read_result_t* result) {
	size_t read = socket_read(sock, result, size);
	if (read == size)
		return 0;

	log_warnf(HASH_RESOURCE, WARNING_SYSTEM_CALL_FAIL, STRING_CONST("Read partial read reply: %" PRIsize " of %" PRIsize),
	          read, size);
	return -1;
}

int
sourced_write_hash(socket_t* sock, uuid_t uuid, uint64_t platform) {
	sourced_hash_t msg = {
		SOURCED_HASH,
		(uint32_t)(sizeof(uuid_t) + sizeof(uint64_t)),
		uuid,
		platform
	};
	if (socket_write(sock, &msg, sizeof(msg)) == sizeof(msg))
		return 0;
	return -1;
}

int
sourced_read_hash_reply(socket_t* sock, size_t size, sourced_hash_result_t* result) {
	size_t read = socket_read(sock, result, size);
	if (read == size)
		return 0;

	log_warnf(HASH_RESOURCE, WARNING_SYSTEM_CALL_FAIL, STRING_CONST("Read partial hash reply: %" PRIsize " of %" PRIsize),
	          read, size);
	return -1;
}

int
sourced_write_hash_reply(socket_t* sock, uint256_t hash) {
	sourced_message_t msg = {
		SOURCED_HASH_RESULT,
		(uint32_t)sizeof(sourced_hash_result_t)
	};
	sourced_hash_result_t reply = {
		SOURCED_OK,
		0,
		hash
	};
	if (socket_write(sock, &msg, sizeof(msg)) == sizeof(msg)) {
		if (socket_write(sock, &reply, sizeof(reply)) == sizeof(reply))
			return 0;
	}
	return -1;
}

int
sourced_write_dependencies(socket_t* sock, uuid_t uuid, uint64_t platform) {
	sourced_dependencies_t msg = {
		SOURCED_DEPENDENCIES,
		(uint32_t)(sizeof(uuid_t) + sizeof(uint64_t)),
		uuid,
		platform
	};
	if (socket_write(sock, &msg, sizeof(msg)) == sizeof(msg))
		return 0;
	return -1;
}

int
sourced_write_dependencies_reply(socket_t* sock, uuid_t* deps, size_t numdeps) {
	size_t reply_size = sizeof(sourced_dependencies_result_t) + sizeof(uuid_t) * numdeps;
	sourced_message_t msg = {
		SOURCED_DEPENDENCIES_RESULT,
		(uint32_t)reply_size
	};
	sourced_dependencies_result_t* reply = memory_allocate(HASH_RESOURCE, reply_size, 0, MEMORY_PERSISTENT);
	reply->result = SOURCED_OK;
	reply->flags = 0;
	reply->num_deps = numdeps;
	memcpy(reply->deps, deps, sizeof(uuid_t) * numdeps);
	if (socket_write(sock, &msg, sizeof(msg)) == sizeof(msg)) {
		if (socket_write(sock, &reply, reply_size) == reply_size)
			return 0;
	}
	return -1;
}

int
sourced_read_dependencies_reply(socket_t* sock, size_t size, uuid_t* deps, size_t capacity, uint64_t* count) {
	sourced_reply_t header;
	size_t read = socket_read(sock, &header, sizeof(header));
	if (read != sizeof(header)) {
		log_warnf(HASH_RESOURCE, WARNING_SYSTEM_CALL_FAIL, STRING_CONST("Read partial dependencies reply: %" PRIsize " of %" PRIsize),
		          read, sizeof(header));
		*count = 0;
		return -1;
	}

	size -= sizeof(header);
	uint64_t pending = size / sizeof(uuid_t);
	uint64_t limit = pending;
	if (limit > capacity)
		limit = capacity;
	*count = limit;

	read = 0;
	limit *= sizeof(uuid_t);
	while (read < limit) {
		size_t want_read = limit - read;
		size_t this_read = socket_read(sock, pointer_offset(deps, read), want_read);
		read += this_read;
		if (!this_read)
			break;
	}
	if (read != limit) {
		log_warnf(HASH_RESOURCE, WARNING_SYSTEM_CALL_FAIL, STRING_CONST("Read partial dependencies reply: %" PRIsize " of %" PRIsize),
		          read, limit);
		return -1;
	}

	while (read < size) {
		char buffer[256];
		size_t want_read = size - read;
		if (want_read > sizeof(buffer))
			want_read = sizeof(buffer);
		size_t this_read = socket_read(sock, buffer, want_read);
		if (this_read != want_read) {
			log_warnf(HASH_RESOURCE, WARNING_SYSTEM_CALL_FAIL, STRING_CONST("Read partial dependencies reply: %" PRIsize " of %" PRIsize),
			          read + this_read, size);
			return -1;
		}
		read += this_read;
	}

	return 0;
}

int
sourced_write_read_blob(socket_t* sock, uuid_t uuid, uint64_t platform, hash_t key) {
	sourced_read_blob_t msg = {
		SOURCED_READ_BLOB,
		(uint32_t)(sizeof(uuid_t) + sizeof(uint64_t)*2),
		uuid,
		platform,
		key
	};
	if (socket_write(sock, &msg, sizeof(msg)) == sizeof(msg))
		return 0;
	return -1;
}

int 
sourced_write_read_blob_reply(socket_t* sock, hash_t checksum, void* store, size_t size) {
	size_t reply_size = sizeof(sourced_read_blob_reply_t) + size;
	sourced_message_t msg = {
		SOURCED_READ_BLOB_RESULT,
		(uint32_t)reply_size
	};
	sourced_read_blob_reply_t reply = {
		SOURCED_OK,
		0,
		checksum,
		size
	};
	if (socket_write(sock, &msg, sizeof(msg)) == sizeof(msg)) {
		if (socket_write(sock, &reply, sizeof(sourced_read_blob_reply_t)) == sizeof(sourced_read_blob_reply_t)) {
			if (socket_write(sock, store, size) == size)
				return 0;
		}
	}
	return -1;
}

int
sourced_read_read_blob_reply(socket_t* sock, size_t size, sourced_read_blob_reply_t* reply, void* store, size_t capacity) {
	size_t read = socket_read(sock, reply, sizeof(sourced_read_blob_reply_t));
	if (read != sizeof(sourced_read_blob_reply_t)) {
		log_warnf(HASH_RESOURCE, WARNING_SYSTEM_CALL_FAIL, STRING_CONST("Read partial read blob reply: %" PRIsize " of %" PRIsize),
		          read, sizeof(sourced_read_blob_reply_t));
		return -1;
	}
	size -= sizeof(sourced_read_blob_reply_t);

	read = 0;
	size_t limit = size;
	if (limit > capacity)
		limit = capacity;
	while (read < limit) {
		size_t want_read = limit - read;
		size_t this_read = socket_read(sock, pointer_offset(store, read), want_read);
		read += this_read;
		if (!this_read)
			break;
	}
	if (read != limit) {
		log_warnf(HASH_RESOURCE, WARNING_SYSTEM_CALL_FAIL, STRING_CONST("Read partial read blob reply: %" PRIsize " of %" PRIsize),
		          read, limit);
		return -1;
	}

	while (read < size) {
		char buffer[256];
		size_t want_read = size - read;
		if (want_read > sizeof(buffer))
			want_read = sizeof(buffer);
		size_t this_read = socket_read(sock, buffer, want_read);
		if (this_read != want_read) {
			log_warnf(HASH_RESOURCE, WARNING_SYSTEM_CALL_FAIL, STRING_CONST("Read partial read blob reply: %" PRIsize " of %" PRIsize),
			          read + this_read, size);
			return -1;
		}
		read += this_read;
	}

	return 0;
}
