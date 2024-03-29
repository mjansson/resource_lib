/* source.c  -  Resource library  -  Public Domain  -  2014 Mattias Jansson
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
#include <blake3/blake3.h>

#if RESOURCE_ENABLE_LOCAL_SOURCE

static char resource_path_buffer[BUILD_MAX_PATHLEN];
static string_t resource_path_source;

static resource_change_t*
resource_source_change_platform_compare(resource_change_t* change, resource_change_t* best, uint64_t platform);

bool
resource_source_set_path(const char* path, size_t length) {
	if (!resource_module_config().enable_local_source)
		return false;
	resource_path_source = string_copy(resource_path_buffer, sizeof(resource_path_buffer), path, length);
	resource_path_source = path_clean(STRING_ARGS(resource_path_source), sizeof(resource_path_buffer));
	resource_path_source = path_absolute(STRING_ARGS(resource_path_source), sizeof(resource_path_buffer));
	return true;
}

string_const_t
resource_source_path(void) {
	return string_to_const(resource_path_source);
}

static stream_t*
resource_source_open(const uuid_t uuid, unsigned int mode) {
	char buffer[BUILD_MAX_PATHLEN];
	string_t path = resource_stream_make_path(buffer, sizeof(buffer), STRING_ARGS(resource_path_source), uuid);
	if (mode & STREAM_OUT) {
		string_const_t dir_path = path_directory_name(STRING_ARGS(path));
		fs_make_directory(STRING_ARGS(dir_path));
	}
	return stream_open(STRING_ARGS(path), mode);
}

static stream_t*
resource_source_open_hash(const uuid_t uuid, unsigned int mode) {
	char buffer[BUILD_MAX_PATHLEN];
	string_t path = resource_stream_make_path(buffer, sizeof(buffer), STRING_ARGS(resource_path_source), uuid);
	path = string_append(STRING_ARGS(path), sizeof(buffer), STRING_CONST(".hash"));
	if (mode & STREAM_OUT) {
		string_const_t dir_path = path_directory_name(STRING_ARGS(path));
		fs_make_directory(STRING_ARGS(dir_path));
	}
	return stream_open(STRING_ARGS(path), mode);
}

static stream_t*
resource_source_open_deps(const uuid_t uuid, unsigned int mode) {
	char buffer[BUILD_MAX_PATHLEN];
	string_t path = resource_stream_make_path(buffer, sizeof(buffer), STRING_ARGS(resource_path_source), uuid);
	path = string_append(STRING_ARGS(path), sizeof(buffer), STRING_CONST(".deps"));
	if (mode & STREAM_OUT) {
		string_const_t dir_path = path_directory_name(STRING_ARGS(path));
		fs_make_directory(STRING_ARGS(dir_path));
	}
	return stream_open(STRING_ARGS(path), mode);
}

static stream_t*
resource_source_open_reverse_deps(const uuid_t uuid, unsigned int mode) {
	char buffer[BUILD_MAX_PATHLEN];
	string_t path = resource_stream_make_path(buffer, sizeof(buffer), STRING_ARGS(resource_path_source), uuid);
	path = string_append(STRING_ARGS(path), sizeof(buffer), STRING_CONST(".revdeps"));
	if (mode & STREAM_OUT) {
		string_const_t dir_path = path_directory_name(STRING_ARGS(path));
		fs_make_directory(STRING_ARGS(dir_path));
	}
	return stream_open(STRING_ARGS(path), mode);
}

static stream_t*
resource_source_open_blob(const uuid_t uuid, hash_t key, uint64_t platform, hash_t checksum, unsigned int mode) {
	char buffer[BUILD_MAX_PATHLEN];
	char filename[64];
	string_t path = resource_stream_make_path(buffer, sizeof(buffer), STRING_ARGS(resource_path_source), uuid);
	string_t file = string_format(filename, sizeof(filename),
	                              STRING_CONST(".%" PRIhash ".%" PRIx64 ".%" PRIhash ".blob"), key, platform, checksum);
	path = string_append(STRING_ARGS(path), sizeof(buffer), STRING_ARGS(file));
	if (mode & STREAM_OUT) {
		string_const_t dir_path = path_directory_name(STRING_ARGS(path));
		fs_make_directory(STRING_ARGS(dir_path));
	}
	stream_t* stream = stream_open(STRING_ARGS(path), mode);
	if (stream && ((mode & STREAM_IN) != 0)) {
		// Verify checksum
		hash_t current_checksum = 0;
		size_t size = stream_size(stream);
		if (size) {
			void* data = memory_allocate(HASH_RESOURCE, size, 0, MEMORY_PERSISTENT);
			if (stream_read(stream, data, size) == size) {
				current_checksum = hash(data, size);
			}
			memory_deallocate(data);
		}
		if (current_checksum != checksum) {
			log_warnf(HASH_RESOURCE, WARNING_RESOURCE,
			          STRING_CONST("Invalid blob checksum for %.*s: Wanted %" PRIhash ", got %" PRIhash),
			          STRING_FORMAT(path), checksum, current_checksum);
			stream_deallocate(stream);
			stream = nullptr;
		} else {
			stream_seek(stream, 0, STREAM_SEEK_BEGIN);
		}
	}
	return stream;
}

static string_t*
resource_source_get_all_blobs(const uuid_t uuid) {
	char buffer[BUILD_MAX_PATHLEN];
	char patbuffer[128];
	string_t path = resource_stream_make_path(buffer, sizeof(buffer), STRING_ARGS(resource_path_source), uuid);
	string_const_t pathname = path_directory_name(STRING_ARGS(path));
	string_const_t filename = path_file_name(STRING_ARGS(path));
	string_t pattern = string_concat_varg(patbuffer, sizeof(patbuffer), STRING_CONST("^"), STRING_ARGS(filename),
	                                      STRING_CONST(".*\\.blob$"), nullptr);
	return fs_matching_files(STRING_ARGS(pathname), STRING_ARGS(pattern), false);
}

resource_source_t*
resource_source_allocate(void) {
	resource_source_t* source = memory_allocate(HASH_RESOURCE, sizeof(resource_source_t), 0, MEMORY_PERSISTENT);
	resource_source_initialize(source);
	return source;
}

void
resource_source_deallocate(resource_source_t* source) {
	if (source)
		resource_source_finalize(source);
	memory_deallocate(source);
}

void
resource_source_initialize(resource_source_t* source) {
	memset(source, 0, sizeof(resource_source_t));
	resource_change_block_initialize(&source->first);
	source->current = &source->first;
}

void
resource_source_finalize(resource_source_t* source) {
	resource_change_block_finalize(&source->first);
}

static resource_change_t*
resource_source_change_grab(resource_change_block_t** block) {
	resource_change_block_t* cur = *block;
	resource_change_t* change = cur->changes + cur->used++;
	if (cur->used == RESOURCE_CHANGE_BLOCK_SIZE) {
		resource_change_block_t* next = resource_change_block_allocate();
		cur->next = next;
		*block = next;
	}
	return change;
}

static void
resource_source_change_set(resource_change_block_t* block, resource_change_t* change, tick_t timestamp, hash_t key,
                           uint64_t platform, const char* value, size_t length) {
	resource_change_data_t* data = block->current_data;
	if (length > (data->size - data->used)) {
		data = &block->fixed.data;
		while (data && (length > (data->size - data->used)))
			data = data->next;
	}
	if (!data) {
		size_t data_size = RESOURCE_CHANGE_BLOCK_DATA_SIZE;
		if (data_size < length)
			data_size = length;
		data = resource_change_data_allocate(data_size);
		block->current_data->next = data;
		block->current_data = data;
	}

	char* dst = data->data + data->used;
	data->used += length;

	memcpy(dst, value, length);

	change->timestamp = timestamp;
	change->hash = key;
	change->platform = platform;
	change->flags = RESOURCE_SOURCEFLAG_VALUE;
	change->value.value = string_const(dst, length);
}

static void
resource_source_change_set_blob(resource_change_t* change, tick_t timestamp, hash_t key, uint64_t platform,
                                hash_t checksum, size_t size) {
	change->timestamp = timestamp;
	change->hash = key;
	change->platform = platform;
	change->flags = RESOURCE_SOURCEFLAG_BLOB;
	change->value.blob.checksum = checksum;
	change->value.blob.size = size;
}

void
resource_source_set(resource_source_t* source, tick_t timestamp, hash_t key, uint64_t platform, const char* value,
                    size_t length) {
	resource_change_block_t* block = source->current;
	resource_change_t* change = resource_source_change_grab(&source->current);
	resource_source_change_set(block, change, timestamp, key, platform, value, length);
}

void
resource_source_set_blob(resource_source_t* source, tick_t timestamp, hash_t key, uint64_t platform, hash_t checksum,
                         size_t size) {
	resource_change_t* change = resource_source_change_grab(&source->current);
	resource_source_change_set_blob(change, timestamp, key, platform, checksum, size);
}

void
resource_source_unset(resource_source_t* source, tick_t timestamp, hash_t key, uint64_t platform) {
	resource_change_t* change = resource_source_change_grab(&source->current);
	change->timestamp = timestamp;
	change->hash = key;
	change->platform = platform;
	change->flags = RESOURCE_SOURCEFLAG_UNSET;
}

resource_change_t*
resource_source_get(resource_source_t* source, hash_t key, uint64_t platform) {
	resource_change_t* best = 0;
	resource_change_block_t* block = &source->first;
	while (block) {
		size_t ichg, chgsize;
		for (ichg = 0, chgsize = block->used; ichg < chgsize; ++ichg) {
			resource_change_t* change = block->changes + ichg;
			if (change->hash == key)
				best = resource_source_change_platform_compare(change, best, platform);
		}
		block = block->next;
	}
	return best;
}

void
resource_source_map_all(resource_source_t* source, hashmap_t* map, bool all_timestamps) {
	resource_change_block_t* block = &source->first;
	hashmap_clear(map);
	while (block) {
		size_t ichg, chgsize;
		for (ichg = 0, chgsize = block->used; ichg < chgsize; ++ichg) {
			resource_change_t* change = block->changes + ichg;
			void* stored = hashmap_lookup(map, change->hash);
			FOUNDATION_ASSERT(!((uintptr_t)change & (uintptr_t)1));
			if (!stored) {
				hashmap_insert(map, change->hash, change);
			} else if ((uintptr_t)stored & (uintptr_t)1) {
				size_t imap = 0, msize = 0;
				resource_change_t** maparr = (void*)((uintptr_t)stored & ~(uintptr_t)1);
				if (!all_timestamps) {
					for (msize = array_size(maparr); imap < msize; ++imap) {
						if (maparr[imap]->platform == change->platform) {
							if (maparr[imap]->timestamp < change->timestamp) {
								if (change->flags == RESOURCE_SOURCEFLAG_UNSET) {
									array_erase(maparr, imap);
								} else {
									maparr[imap] = change;
								}
							}
							break;
						}
					}
				}
				if (imap == msize) {
					array_push(maparr, change);
					hashmap_insert(map, change->hash, (void*)(((uintptr_t)maparr) | (uintptr_t)1));
				}
			} else {
				resource_change_t* previous = stored;
				if (!all_timestamps && (previous->platform == change->platform)) {
					if (previous->timestamp < change->timestamp)
						hashmap_insert(map, change->hash, (change->flags != RESOURCE_SOURCEFLAG_UNSET) ? change : 0);
				} else {
					resource_change_t** newarr = 0;
					array_push(newarr, previous);
					array_push(newarr, change);
					hashmap_insert(map, change->hash, (void*)(((uintptr_t)newarr) | (uintptr_t)1));
				}
			}
		}
		block = block->next;
	}
}

void
resource_source_map_iterate(resource_source_t* source, hashmap_t* map, void* data,
                            resource_source_map_iterate_fn iterate) {
	size_t ibucket, bsize;
	FOUNDATION_UNUSED(source);
	for (ibucket = 0, bsize = map->bucket_count; ibucket < bsize; ++ibucket) {
		size_t inode, nsize;
		hashmap_node_t* bucket = map->bucket[ibucket];
		for (inode = 0, nsize = array_size(bucket); inode < nsize; ++inode) {
			resource_change_t* change = 0;
			void* stored = bucket[inode].value;
			if (!stored)
				continue;
			else if ((uintptr_t)stored & 1) {
				resource_change_t** maparr = (resource_change_t**)((uintptr_t)stored & ~(uintptr_t)1);
				size_t imap, msize;
				for (imap = 0, msize = array_size(maparr); imap < msize; ++imap) {
					change = maparr[imap];
					if (change->flags == RESOURCE_SOURCEFLAG_UNSET)
						continue;
					if (iterate(change, data) < 0)
						return;
				}
			} else {
				change = stored;
				if (change->flags == RESOURCE_SOURCEFLAG_UNSET)
					continue;
				if (iterate(change, data) < 0)
					return;
			}
		}
	}
}

void
resource_source_map_reduce(resource_source_t* source, hashmap_t* map, void* data,
                           resource_source_map_reduce_fn reduce) {
	size_t ibucket, bsize;
	resource_change_t* best;
	FOUNDATION_UNUSED(source);
	for (ibucket = 0, bsize = map->bucket_count; ibucket < bsize; ++ibucket) {
		size_t inode, nsize;
		hashmap_node_t* bucket = map->bucket[ibucket];
		for (inode = 0, nsize = array_size(bucket); inode < nsize; ++inode) {
			resource_change_t* change = 0;
			void* stored = bucket[inode].value;
			if (!stored)
				continue;
			else if ((uintptr_t)stored & 1) {
				resource_change_t** maparr = (resource_change_t**)((uintptr_t)stored & ~(uintptr_t)1);
				size_t imap, msize;
				best = 0;
				for (imap = 0, msize = array_size(maparr); imap < msize; ++imap) {
					change = maparr[imap];
					if (change->flags == RESOURCE_SOURCEFLAG_UNSET)
						continue;
					best = reduce(change, best, data);
					if ((uintptr_t)best == (uintptr_t)(-1))
						return;
				}
				array_deallocate(maparr);
				bucket[inode].value = best;
			} else {
				change = stored;
				if (change->flags == RESOURCE_SOURCEFLAG_UNSET)
					continue;
				best = reduce(change, 0, data);
				if ((uintptr_t)best == (uintptr_t)(-1))
					return;
				bucket[inode].value = best;
			}
		}
	}
}

void
resource_source_map_clear(hashmap_t* map) {
	size_t ibucket, bsize;
	for (ibucket = 0, bsize = map->bucket_count; ibucket < bsize; ++ibucket) {
		size_t inode, nsize;
		hashmap_node_t* bucket = map->bucket[ibucket];
		for (inode = 0, nsize = array_size(bucket); inode < nsize; ++inode) {
			void* stored = bucket[inode].value;
			if (!stored)
				continue;
			else if ((uintptr_t)stored & 1) {
				resource_change_t** maparr = (resource_change_t**)((uintptr_t)stored & ~(uintptr_t)1);
				array_deallocate(maparr);
			}
		}
	}
	hashmap_clear(map);
}

static resource_change_t*
resource_source_collapse_reduce(resource_change_t* change, resource_change_t* best, void* data) {
	resource_change_block_t** block = data;
	FOUNDATION_UNUSED(best);
	FOUNDATION_ASSERT(best == 0 || change->platform != best->platform);
	resource_change_t* store = resource_source_change_grab(block);
	if (change->flags & RESOURCE_SOURCEFLAG_BLOB)
		resource_source_change_set_blob(store, change->timestamp, change->hash, change->platform,
		                                change->value.blob.checksum, change->value.blob.size);
	else
		resource_source_change_set(*block, store, change->timestamp, change->hash, change->platform,
		                           STRING_ARGS(change->value.value));
	return change;
}

void
resource_source_collapse_history(resource_source_t* source) {
	size_t ichg, chgsize;
	resource_change_t* change;
	resource_change_block_t* block;
	hashmap_fixed_t fixedmap;
	hashmap_t* map = (hashmap_t*)&fixedmap;
	hashmap_initialize(map, sizeof(fixedmap.bucket) / sizeof(fixedmap.bucket[0]), 8);
	resource_source_map_all(source, map, false);

	// Create a new change block structure with changes that are set operations
	block = resource_change_block_allocate();
	resource_change_block_t* first = block;
	resource_source_map_reduce(source, map, &block, resource_source_collapse_reduce);

	// Copy first block data, swap next change block structure and free resources
	resource_change_block_finalize(&source->first);
	memcpy(&source->first, first, sizeof(resource_change_block_t));
	// Patch up first block change data pointers
	source->first.fixed.data.data = source->first.fixed.fixed;
	for (ichg = 0, chgsize = first->used; ichg < chgsize; ++ichg) {
		change = first->changes + ichg;
		if (change->flags == RESOURCE_SOURCEFLAG_VALUE) {
			ptrdiff_t diff = pointer_diff(change->value.value.str, first->fixed.fixed);
			if ((diff >= 0) && (diff < RESOURCE_CHANGE_BLOCK_DATA_SIZE))
				source->first.changes[ichg].value.value.str = source->first.fixed.fixed + diff;
		}
	}
	// Patch up first block current_data pointer
	source->first.current_data = &source->first.fixed.data;
	while (source->first.current_data->next)
		source->first.current_data = source->first.current_data->next;
	// Patch up current block
	source->current = (block == first) ? &source->first : block;
	// Free first block memory
	memory_deallocate(first);

	hashmap_finalize(map);
}

struct resource_source_clear_blob_t {
	string_const_t uuidstr;
	string_t* blobfiles;
	char blobname[128];
};

static resource_change_t*
resource_source_clear_blob_reduce(resource_change_t* change, resource_change_t* best, void* data) {
	struct resource_source_clear_blob_t* clear = (struct resource_source_clear_blob_t*)data;
	FOUNDATION_UNUSED(best);
	if (change->flags == RESOURCE_SOURCEFLAG_UNSET)
		return change;

	if (change->flags & RESOURCE_SOURCEFLAG_BLOB) {
		string_t blobfile = string_format(
		    clear->blobname, sizeof(clear->blobname), STRING_CONST("%.*s.%" PRIhash ".%" PRIx64 ".%" PRIhash ".blob"),
		    STRING_FORMAT(clear->uuidstr), change->hash, change->platform, change->value.blob.checksum);
		size_t ifile, fsize;
		for (ifile = 0, fsize = array_size(clear->blobfiles); ifile < fsize; ++ifile) {
			if (string_equal(STRING_ARGS(clear->blobfiles[ifile]), STRING_ARGS(blobfile))) {
				string_deallocate(clear->blobfiles[ifile].str);
				array_erase(clear->blobfiles, ifile);
				break;
			}
		}
	}
	return change;
}

void
resource_source_clear_blob_history(resource_source_t* source, const uuid_t uuid) {
	size_t ifile, fsize;
	char buffer[BUILD_MAX_PATHLEN];
	struct resource_source_clear_blob_t clear;
	clear.blobfiles = resource_source_get_all_blobs(uuid);
	clear.uuidstr = string_from_uuid_static(uuid);

	hashmap_fixed_t fixedmap;
	hashmap_t* map = (hashmap_t*)&fixedmap;
	hashmap_initialize(map, sizeof(fixedmap.bucket) / sizeof(fixedmap.bucket[0]), 8);
	resource_source_map_all(source, map, true);

	resource_source_map_reduce(source, map, &clear, resource_source_clear_blob_reduce);

	for (ifile = 0, fsize = array_size(clear.blobfiles); ifile < fsize; ++ifile) {
		string_t path = resource_stream_make_path(buffer, sizeof(buffer), STRING_ARGS(resource_path_source), uuid);
		string_const_t pathname = path_directory_name(STRING_ARGS(path));
		ptrdiff_t offset = pointer_diff(pathname.str, buffer);
		string_t fullname = path_append(buffer + offset, pathname.length, sizeof(buffer) - (size_t)offset,
		                                STRING_ARGS(clear.blobfiles[ifile]));
		fs_remove_file(STRING_ARGS(fullname));
	}

	string_array_deallocate(clear.blobfiles);
	hashmap_finalize(map);
}

static bool
resource_source_read_local(resource_source_t* source, const uuid_t uuid) {
	const char op_set = '=';
	const char op_unset = '-';
	const char op_blob = '#';
	stream_t* stream = resource_source_open(uuid, STREAM_IN);
	if (!stream)
		return false;
	if (!source)
		goto exit;
	stream_determine_binary_mode(stream, 16);
	const bool binary = stream_is_binary(stream);
	source->read_binary = binary;

	while (!stream_eos(stream)) {
		char separator, op = 0;
		tick_t timestamp = stream_read_int64(stream);
		hash_t key = stream_read_uint64(stream);
		uint64_t platform = stream_read_uint64(stream);
		stream_read(stream, &op, 1);
		if (op == op_unset) {
			resource_source_unset(source, timestamp, key, platform);
		} else if (op == op_set) {
			string_t value;
			if (binary) {
				value = stream_read_string(stream);
			} else {
				stream_read(stream, &separator, 1);
				value = stream_read_line(stream, '\n');
				if (value.length && (value.str[value.length - 1] == '\r'))
					--value.length;
			}
			resource_source_set(source, timestamp, key, platform, STRING_ARGS(value));
			string_deallocate(value.str);
		} else if (op == op_blob) {
			hash_t checksum = stream_read_uint64(stream);
			size_t size = (size_t)stream_read_uint64(stream);
			resource_source_set_blob(source, timestamp, key, platform, checksum, size);
		}
	}

exit:
	stream_deallocate(stream);
	return true;
}

bool
resource_source_read(resource_source_t* source, const uuid_t uuid) {
	if (source && resource_remote_sourced_is_connected() && resource_remote_sourced_read(source, uuid))
		return true;

	return resource_source_read_local(source, uuid);
}

bool
resource_source_write(resource_source_t* source, const uuid_t uuid, bool binary) {
	const char op_set = '=';
	const char op_unset = '-';
	const char op_blob = '#';
	sha256_t sha;
	stream_t* stream = resource_source_open(uuid, STREAM_OUT | STREAM_CREATE | STREAM_TRUNCATE);
	if (!stream)
		return false;
	stream_set_binary(stream, binary);

	sha256_initialize(&sha);

	resource_change_block_t* block = &source->first;
	while (block) {
		size_t ichg, chgsize;
		for (ichg = 0, chgsize = block->used; ichg < chgsize; ++ichg) {
			resource_change_t* change = block->changes + ichg;
			stream_write_int64(stream, change->timestamp);
			stream_write_separator(stream);
			stream_write_uint64(stream, change->hash);
			stream_write_separator(stream);
			stream_write_uint64(stream, change->platform);
			stream_write_separator(stream);

			sha256_digest(&sha, &change->timestamp, sizeof(change->timestamp));
			sha256_digest(&sha, &change->hash, sizeof(change->hash));
			sha256_digest(&sha, &change->platform, sizeof(change->platform));

			if (change->flags == RESOURCE_SOURCEFLAG_UNSET) {
				stream_write(stream, &op_unset, 1);
			} else {
				if (change->flags & RESOURCE_SOURCEFLAG_BLOB) {
					stream_write(stream, &op_blob, 1);
					stream_write_separator(stream);
					stream_write_uint64(stream, change->value.blob.checksum);
					stream_write_separator(stream);
					stream_write_uint64(stream, change->value.blob.size);

					sha256_digest(&sha, &change->value.blob.checksum, sizeof(change->value.blob.checksum));
					sha256_digest(&sha, &change->value.blob.size, sizeof(change->value.blob.size));
				} else {
					stream_write(stream, &op_set, 1);
					stream_write_separator(stream);
					stream_write_string(stream, STRING_ARGS(change->value.value));

					sha256_digest(&sha, STRING_ARGS(change->value.value));
				}
			}
			stream_write_endl(stream);

			sha256_digest(&sha, &change->flags, sizeof(change->flags));
		}
		block = block->next;
	}

	stream_deallocate(stream);

	sha256_digest_finalize(&sha);

	stream = resource_source_open_hash(uuid, STREAM_OUT | STREAM_CREATE | STREAM_TRUNCATE);
	if (stream) {
		uint256_t hash = sha256_get_digest_raw(&sha);
		string_const_t value = string_from_uint256_static(hash);
		stream_write_string(stream, STRING_ARGS(value));
	}
	stream_deallocate(stream);

	return true;
}

blake3_hash_t
resource_source_hash(const uuid_t uuid, uint64_t platform) {
	blake3_hash_t hash = {0};

	if (resource_remote_sourced_is_connected()) {
		hash = resource_remote_sourced_hash(uuid, platform);
		if (!blake3_hash_is_null(hash))
			return hash;
	}

	stream_t* stream = resource_source_open_hash(uuid, STREAM_IN);
	if (stream) {
		char buffer[BLAKE3_HASH_STRING_LENGTH + 1];
		string_t value = stream_read_string_buffer(stream, buffer, sizeof(buffer));
		hash = string_to_blake3_hash(STRING_ARGS(value));
	}
	stream_deallocate(stream);

	// TODO: Implement adding dependency resource hashes based on platform
	resource_dependency_t localdeps[4];
	size_t capacity = sizeof(localdeps) / sizeof(localdeps[0]);
	size_t deps_count = resource_source_dependencies_count(uuid, platform);
	if (deps_count) {
		blake3_hash_state_t* hash_state = blake3_hash_state_allocate();
		blake3_hash_state_update(hash_state, hash.data, BLAKE3_HASH_LENGTH);
		resource_dependency_t* deps = localdeps;
		if (deps_count > capacity)
			deps = memory_allocate(HASH_RESOURCE, sizeof(resource_dependency_t) * deps_count, 16, MEMORY_PERSISTENT);
		resource_source_dependencies(uuid, platform, deps, deps_count);
		for (size_t idep = 0; idep < deps_count; ++idep) {
			blake3_hash_t dephash = resource_source_hash(deps[idep].uuid, platform);
			blake3_hash_state_update(hash_state, dephash.data, BLAKE3_HASH_LENGTH);
		}
		if (deps != localdeps)
			memory_deallocate(deps);
		hash = blake3_hash_state_finalize(hash_state);
		blake3_hash_state_deallocate(hash_state);
	}

	return hash;
}

static resource_change_t*
resource_source_change_platform_compare(resource_change_t* change, resource_change_t* best, uint64_t platform) {
	if ((change->flags != RESOURCE_SOURCEFLAG_UNSET) &&
	    // Change must be superset of requested platform
	    resource_platform_is_equal_or_more_specific(platform, change->platform) &&
	    // Either no previous result, or
	    //  previous best is platform superset of change platform and
	    //    either platforms are different (change is exclusively more specific), or
	    ///   change is newer (and platforms are equal)
	    (!best || (resource_platform_is_equal_or_more_specific(change->platform, best->platform) &&
	               ((change->platform != best->platform) || (change->timestamp > best->timestamp)))))
		return change;
	return best;
}

static resource_change_t*
resource_source_map_platform_reduce(resource_change_t* change, resource_change_t* best, void* data) {
	return resource_source_change_platform_compare(change, best, *(uint64_t*)data);
}

void
resource_source_map(resource_source_t* source, uint64_t platform, hashmap_t* map) {
	resource_source_map_all(source, map, false);
	resource_source_map_reduce(source, map, &platform, resource_source_map_platform_reduce);
}

bool
resource_source_read_blob(const uuid_t uuid, hash_t key, uint64_t platform, hash_t checksum, void* data,
                          size_t capacity) {
	if (resource_remote_sourced_is_connected() &&
	    resource_remote_sourced_read_blob(uuid, key, platform, checksum, data, capacity))
		return true;

	stream_t* stream = resource_source_open_blob(uuid, key, platform, checksum, STREAM_IN);
	if (!stream)
		return false;
	size_t read = stream_read(stream, data, capacity);
	stream_deallocate(stream);
	return read == capacity;
}

bool
resource_source_write_blob(const uuid_t uuid, tick_t timestamp, hash_t key, uint64_t platform, hash_t checksum,
                           const void* data, size_t size) {
	unsigned int mode = STREAM_OUT | STREAM_BINARY | STREAM_CREATE | STREAM_TRUNCATE;
	stream_t* stream = resource_source_open_blob(uuid, key, platform, checksum, mode);
	FOUNDATION_UNUSED(timestamp);
	if (!stream)
		return false;
	size_t written = stream_write(stream, data, size);
	stream_deallocate(stream);
	return written == size;
}

size_t
resource_source_dependencies_count(const uuid_t uuid, uint64_t platform) {
	return resource_source_dependencies(uuid, platform, nullptr, 0);
}

size_t
resource_source_dependencies(const uuid_t uuid, uint64_t platform, resource_dependency_t* deps, size_t capacity) {
	size_t deps_count_read = 0;
	size_t deps_stored = 0;
	size_t deps_count = 0;

	if (resource_remote_sourced_is_connected())
		return resource_remote_sourced_dependencies(uuid, platform, deps, capacity);

	stream_t* stream = resource_source_open_deps(uuid, STREAM_IN);
	while (stream && !stream_eos(stream)) {
		deps_count_read = stream_read_uint32(stream);
		uint64_t depplatform = stream_read_uint64(stream);
		for (size_t idep = 0; idep < deps_count_read; ++idep) {
			uuid_t depuuid = stream_read_uuid(stream);
			if (!uuid_is_null(depuuid) && resource_platform_is_equal_or_more_specific(platform, depplatform)) {
				if (deps_stored < capacity) {
					deps[deps_stored].uuid = depuuid;
					deps[deps_stored].platform = depplatform;
					++deps_stored;
				}
				++deps_count;
			}
		}
	}
	stream_deallocate(stream);
	return deps_count;
}

void
resource_source_set_dependencies(const uuid_t uuid, uint64_t platform, const resource_dependency_t* deps,
                                 size_t deps_count) {
	stream_t* stream = resource_source_open_deps(uuid, STREAM_IN | STREAM_OUT | STREAM_CREATE);
	size_t size = stream_size(stream);
	resource_dependency_t basedeps[8];
	resource_dependency_t* olddeps = basedeps;
	unsigned int deps_count_old = 0;
	unsigned int idep, iotherdep;
	while (!stream_eos(stream)) {
		ssize_t startofs = (ssize_t)stream_tell(stream);
		unsigned int deps_count_read = stream_read_uint32(stream);
		uint64_t depplatform = stream_read_uint64(stream);
		if (platform == depplatform) {
			if (deps_count_read > (sizeof(basedeps) / sizeof(basedeps[0])))
				olddeps = memory_allocate(HASH_RESOURCE, sizeof(resource_dependency_t) * deps_count_read, 0,
				                          MEMORY_PERSISTENT);
			deps_count_old = deps_count_read;
		}
		for (idep = 0; idep < deps_count_read; ++idep) {
			uuid_t depuuid = stream_read_uuid(stream);
			if (platform == depplatform)
				olddeps[idep].uuid = depuuid;
		}
		stream_skip_whitespace(stream);
		size_t endofs = stream_tell(stream);
		if (platform == depplatform) {
			// Replace line with new line at end
			size_t toread = size - endofs;
			if (toread) {
				char* remain = memory_allocate(HASH_RESOURCE, toread, 0, MEMORY_PERSISTENT);
				size_t read = stream_read(stream, remain, toread);
				stream_seek(stream, startofs, STREAM_SEEK_BEGIN);
				stream_write(stream, remain, read);
				memory_deallocate(remain);
			} else {
				stream_seek(stream, startofs, STREAM_SEEK_BEGIN);
			}
			break;
		}
	}
	stream_write_uint32(stream, (uint32_t)deps_count);
	stream_write_separator(stream);
	stream_write_uint64(stream, platform);
	for (idep = 0; idep < deps_count; ++idep) {
		stream_write_separator(stream);
		stream_write_uuid(stream, deps[idep].uuid);
	}
	stream_write_endl(stream);
	stream_truncate(stream, stream_tell(stream));
	stream_deallocate(stream);

	for (idep = 0; idep < deps_count; ++idep) {
		for (iotherdep = 0; iotherdep < deps_count_old; ++iotherdep) {
			if (uuid_equal(olddeps[iotherdep].uuid, deps[idep].uuid)) {
				olddeps[iotherdep].uuid = uuid_null();
				break;
			}
		}
		if (iotherdep == deps_count_old)
			resource_source_add_reverse_dependency(deps[idep].uuid, platform, uuid);
	}
	for (iotherdep = 0; iotherdep < deps_count_old; ++iotherdep) {
		if (!uuid_is_null(olddeps[iotherdep].uuid))
			resource_source_remove_reverse_dependency(olddeps[iotherdep].uuid, platform, uuid);
	}

	if (olddeps != basedeps)
		memory_deallocate(olddeps);
}

size_t
resource_source_reverse_dependencies_count(const uuid_t uuid, uint64_t platform) {
	return resource_source_reverse_dependencies(uuid, platform, nullptr, 0);
}

size_t
resource_source_reverse_dependencies(const uuid_t uuid, uint64_t platform, resource_dependency_t* deps,
                                     size_t capacity) {
	size_t deps_count_read = 0;
	size_t deps_stored = 0;
	size_t deps_count = 0;

	if (resource_remote_sourced_is_connected())
		return resource_remote_sourced_reverse_dependencies(uuid, platform, deps, capacity);

	stream_t* stream = resource_source_open_reverse_deps(uuid, STREAM_IN);
	while (stream && !stream_eos(stream)) {
		deps_count_read = stream_read_uint32(stream);
		uint64_t depplatform = stream_read_uint64(stream);
		for (size_t idep = 0; idep < deps_count_read; ++idep) {
			uuid_t depuuid = stream_read_uuid(stream);
			if (!uuid_is_null(depuuid) && resource_platform_is_equal_or_more_specific(depplatform, platform)) {
				if (deps_stored < capacity) {
					deps[deps_stored].uuid = depuuid;
					deps[deps_stored].platform = depplatform;
					++deps_stored;
				}
				++deps_count;
			}
		}
	}
	stream_deallocate(stream);
	return deps_count;
}

void
resource_source_add_reverse_dependency(const uuid_t uuid, uint64_t platform, const uuid_t dep) {
	stream_t* stream = resource_source_open_reverse_deps(uuid, STREAM_IN | STREAM_OUT | STREAM_CREATE);
	size_t size = stream_size(stream);
	resource_dependency_t basedeps[8];
	resource_dependency_t* olddeps = basedeps;
	unsigned int deps_count_old = 0;
	unsigned int idep;
	bool hasdep = false;
	while (!stream_eos(stream)) {
		ssize_t startofs = (ssize_t)stream_tell(stream);
		unsigned int deps_count_read = stream_read_uint32(stream);
		uint64_t depplatform = stream_read_uint64(stream);
		if (platform == depplatform) {
			if (deps_count_read > (sizeof(basedeps) / sizeof(basedeps[0])))
				olddeps = memory_allocate(HASH_RESOURCE, sizeof(resource_dependency_t) * deps_count_read, 0,
				                          MEMORY_PERSISTENT);
			deps_count_old = deps_count_read;
		}
		for (idep = 0; idep < deps_count_read; ++idep) {
			uuid_t depuuid = stream_read_uuid(stream);
			if (platform == depplatform) {
				olddeps[idep].uuid = depuuid;
				if (uuid_equal(olddeps[idep].uuid, dep))
					hasdep = true;
			}
		}
		stream_skip_whitespace(stream);
		size_t endofs = stream_tell(stream);
		if (platform == depplatform) {
			if (hasdep)
				break;

			// Replace line with new line at end
			size_t toread = size - endofs;
			if (toread) {
				char* remain = memory_allocate(HASH_RESOURCE, toread, 0, MEMORY_PERSISTENT);
				size_t read = stream_read(stream, remain, toread);
				stream_seek(stream, startofs, STREAM_SEEK_BEGIN);
				stream_write(stream, remain, read);
				memory_deallocate(remain);
			} else {
				stream_seek(stream, startofs, STREAM_SEEK_BEGIN);
			}
			break;
		}
	}
	if (!hasdep) {
		stream_write_uint32(stream, (uint32_t)deps_count_old + 1);
		stream_write_separator(stream);
		stream_write_uint64(stream, platform);
		for (idep = 0; idep < deps_count_old; ++idep) {
			stream_write_separator(stream);
			stream_write_uuid(stream, olddeps[idep].uuid);
		}
		stream_write_separator(stream);
		stream_write_uuid(stream, dep);
		stream_write_endl(stream);
		stream_truncate(stream, stream_tell(stream));
	}
	stream_deallocate(stream);

	if (olddeps != basedeps)
		memory_deallocate(olddeps);
}

void
resource_source_remove_reverse_dependency(const uuid_t uuid, uint64_t platform, const uuid_t dep) {
	stream_t* stream = resource_source_open_reverse_deps(uuid, STREAM_IN | STREAM_OUT | STREAM_CREATE);
	size_t size = stream_size(stream);
	uuid_t basedeps[8];
	uuid_t* olddeps = basedeps;
	unsigned int deps_count_old = 0;
	unsigned int idep;
	bool hasdep = false;
	while (!stream_eos(stream)) {
		ssize_t startofs = (ssize_t)stream_tell(stream);
		unsigned int deps_count_read = stream_read_uint32(stream);
		uint64_t depplatform = stream_read_uint64(stream);
		if (platform == depplatform) {
			if (deps_count_read > (sizeof(basedeps) / sizeof(basedeps[0])))
				olddeps = memory_allocate(HASH_RESOURCE, sizeof(uuid_t) * deps_count_read, 0, MEMORY_PERSISTENT);
			deps_count_old = deps_count_read;
		}
		for (idep = 0; idep < deps_count_read; ++idep) {
			uuid_t depuuid = stream_read_uuid(stream);
			if (platform == depplatform) {
				olddeps[idep] = depuuid;
				if (uuid_equal(olddeps[idep], dep))
					hasdep = true;
			}
		}
		stream_skip_whitespace(stream);
		size_t endofs = stream_tell(stream);
		if (platform == depplatform) {
			if (!hasdep)
				break;
			// Replace line with new line at end
			size_t toread = size - endofs;
			if (toread) {
				char* remain = memory_allocate(HASH_RESOURCE, toread, 0, MEMORY_PERSISTENT);
				size_t read = stream_read(stream, remain, toread);
				stream_seek(stream, startofs, STREAM_SEEK_BEGIN);
				stream_write(stream, remain, read);
				memory_deallocate(remain);
			} else {
				stream_seek(stream, startofs, STREAM_SEEK_BEGIN);
			}
			break;
		}
	}
	if (hasdep && (deps_count_old > 1)) {
		stream_write_uint32(stream, (uint32_t)deps_count_old - 1);
		stream_write_separator(stream);
		stream_write_uint64(stream, platform);
		for (idep = 0; idep < deps_count_old; ++idep) {
			if (!uuid_equal(olddeps[idep], dep)) {
				stream_write_separator(stream);
				stream_write_uuid(stream, olddeps[idep]);
			}
		}
		stream_write_endl(stream);
		stream_truncate(stream, stream_tell(stream));
	}
	stream_deallocate(stream);

	if (olddeps != basedeps)
		memory_deallocate(olddeps);
}

blake3_hash_t
resource_source_import_hash(const uuid_t uuid) {
	char buffer[BUILD_MAX_PATHLEN];
	string_t path = resource_stream_make_path(buffer, sizeof(buffer), STRING_ARGS(resource_path_source), uuid);
	path = string_append(STRING_ARGS(path), sizeof(buffer), STRING_CONST(".importhash"));
	stream_t* hash_stream = stream_open(STRING_ARGS(path), STREAM_IN);
	blake3_hash_t import_hash = {0};
	if (hash_stream)
		stream_read(hash_stream, import_hash.data, BLAKE3_HASH_LENGTH);
	stream_deallocate(hash_stream);
	return import_hash;
}

void
resource_source_set_import_hash(const uuid_t uuid, const blake3_hash_t import_hash) {
	char buffer[BUILD_MAX_PATHLEN];
	string_t path = resource_stream_make_path(buffer, sizeof(buffer), STRING_ARGS(resource_path_source), uuid);
	path = string_append(STRING_ARGS(path), sizeof(buffer), STRING_CONST(".importhash"));
	stream_t* hash_stream = stream_open(STRING_ARGS(path), STREAM_OUT | STREAM_CREATE | STREAM_TRUNCATE);
	if (hash_stream)
		stream_write(hash_stream, import_hash.data, BLAKE3_HASH_LENGTH);
	stream_deallocate(hash_stream);
}

#else

string_const_t
resource_source_path(void) {
	return string_empty();
}

bool
resource_source_set_path(const char* path, size_t length) {
	FOUNDATION_UNUSED(path);
	FOUNDATION_UNUSED(length);
	return false;
}

blake3_hash_t
resource_source_hash(const uuid_t uuid, uint64_t platform) {
	FOUNDATION_UNUSED(uuid);
	FOUNDATION_UNUSED(platform);
	return blake3_hash_null();
}

resource_source_t*
resource_source_allocate(void) {
	return nullptr;
}

void
resource_source_deallocate(resource_source_t* source) {
	memory_deallocate(source);
}

void
resource_source_initialize(resource_source_t* source) {
	memset(source, 0, sizeof(resource_source_t));
}

void
resource_source_finalize(resource_source_t* source) {
	FOUNDATION_UNUSED(source);
}

bool
resource_source_read(resource_source_t* source, const uuid_t uuid) {
	FOUNDATION_UNUSED(source);
	FOUNDATION_UNUSED(uuid);
	return false;
}

bool
resource_source_write(resource_source_t* source, const uuid_t uuid, bool binary) {
	FOUNDATION_UNUSED(source);
	FOUNDATION_UNUSED(uuid);
	FOUNDATION_UNUSED(binary);
	return false;
}

void
resource_source_set(resource_source_t* source, tick_t timestamp, hash_t key, uint64_t platform, const char* value,
                    size_t length) {
	FOUNDATION_UNUSED(source);
	FOUNDATION_UNUSED(timestamp);
	FOUNDATION_UNUSED(key);
	FOUNDATION_UNUSED(platform);
	FOUNDATION_UNUSED(value);
	FOUNDATION_UNUSED(length);
}

void
resource_source_unset(resource_source_t* source, tick_t timestamp, hash_t key, uint64_t platform) {
	FOUNDATION_UNUSED(source);
	FOUNDATION_UNUSED(timestamp);
	FOUNDATION_UNUSED(key);
	FOUNDATION_UNUSED(platform);
}

resource_change_t*
resource_source_get(resource_source_t* source, hash_t key, uint64_t platform) {
	FOUNDATION_UNUSED(source);
	FOUNDATION_UNUSED(key);
	FOUNDATION_UNUSED(platform);
	return nullptr;
}

void
resource_source_set_blob(resource_source_t* source, tick_t timestamp, hash_t key, uint64_t platform, hash_t checksum,
                         size_t size) {
	FOUNDATION_UNUSED(source);
	FOUNDATION_UNUSED(timestamp);
	FOUNDATION_UNUSED(key);
	FOUNDATION_UNUSED(platform);
	FOUNDATION_UNUSED(checksum);
	FOUNDATION_UNUSED(size);
}

bool
resource_source_read_blob(const uuid_t uuid, hash_t key, uint64_t platform, hash_t checksum, void* data,
                          size_t capacity) {
	FOUNDATION_UNUSED(uuid);
	FOUNDATION_UNUSED(key);
	FOUNDATION_UNUSED(platform);
	FOUNDATION_UNUSED(checksum);
	FOUNDATION_UNUSED(data);
	FOUNDATION_UNUSED(capacity);
	return false;
}

bool
resource_source_write_blob(const uuid_t uuid, tick_t timestamp, hash_t key, uint64_t platform, hash_t checksum,
                           const void* data, size_t size) {
	FOUNDATION_UNUSED(uuid);
	FOUNDATION_UNUSED(timestamp);
	FOUNDATION_UNUSED(key);
	FOUNDATION_UNUSED(platform);
	FOUNDATION_UNUSED(checksum);
	FOUNDATION_UNUSED(data);
	FOUNDATION_UNUSED(size);
	return false;
}

void
resource_source_collapse_history(resource_source_t* source) {
	FOUNDATION_UNUSED(source);
}

void
resource_source_clear_blob_history(resource_source_t* source, const uuid_t uuid) {
	FOUNDATION_UNUSED(source);
	FOUNDATION_UNUSED(uuid);
}

void
resource_source_map(resource_source_t* source, uint64_t platform, hashmap_t* map) {
	FOUNDATION_UNUSED(source);
	FOUNDATION_UNUSED(platform);
	FOUNDATION_UNUSED(map);
}

void
resource_source_map_all(resource_source_t* source, hashmap_t* map, bool all_timestamps) {
	FOUNDATION_UNUSED(source);
	FOUNDATION_UNUSED(map);
	FOUNDATION_UNUSED(all_timestamps);
}

void
resource_source_map_iterate(resource_source_t* source, hashmap_t* map, void* data,
                            resource_source_map_iterate_fn iterate) {
	FOUNDATION_UNUSED(source);
	FOUNDATION_UNUSED(map);
	FOUNDATION_UNUSED(data);
	FOUNDATION_UNUSED(iterate);
}

void
resource_source_map_reduce(resource_source_t* source, hashmap_t* map, void* data,
                           resource_source_map_reduce_fn reduce) {
	FOUNDATION_UNUSED(source);
	FOUNDATION_UNUSED(map);
	FOUNDATION_UNUSED(data);
	FOUNDATION_UNUSED(reduce);
}

void
resource_source_map_clear(hashmap_t* map) {
	FOUNDATION_UNUSED(map);
}

size_t
resource_source_dependencies_count(const uuid_t uuid, uint64_t platform) {
	FOUNDATION_UNUSED(uuid);
	FOUNDATION_UNUSED(platform);
	return 0;
}

size_t
resource_source_dependencies(const uuid_t uuid, uint64_t platform, resource_dependency_t* deps, size_t capacity) {
	FOUNDATION_UNUSED(uuid);
	FOUNDATION_UNUSED(platform);
	FOUNDATION_UNUSED(deps);
	FOUNDATION_UNUSED(capacity);
	return 0;
}

void
resource_source_set_dependencies(const uuid_t uuid, uint64_t platform, const resource_dependency_t* deps,
                                 size_t deps_count) {
	FOUNDATION_UNUSED(uuid);
	FOUNDATION_UNUSED(platform);
	FOUNDATION_UNUSED(deps);
	FOUNDATION_UNUSED(deps_count);
}

size_t
resource_source_reverse_dependencies_count(const uuid_t uuid, uint64_t platform) {
	FOUNDATION_UNUSED(uuid);
	FOUNDATION_UNUSED(platform);
	return 0;
}

size_t
resource_source_reverse_dependencies(const uuid_t uuid, uint64_t platform, resource_dependency_t* deps,
                                     size_t capacity) {
	FOUNDATION_UNUSED(uuid);
	FOUNDATION_UNUSED(platform);
	FOUNDATION_UNUSED(deps);
	FOUNDATION_UNUSED(capacity);
	return 0;
}

void
resource_source_add_reverse_dependency(const uuid_t uuid, uint64_t platform, const uuid_t dep) {
	FOUNDATION_UNUSED(uuid);
	FOUNDATION_UNUSED(platform);
	FOUNDATION_UNUSED(dep);
}

void
resource_source_remove_reverse_dependency(const uuid_t uuid, uint64_t platform, const uuid_t dep) {
	FOUNDATION_UNUSED(uuid);
	FOUNDATION_UNUSED(platform);
	FOUNDATION_UNUSED(dep);
}

blake3_hash_t
resource_source_import_hash(const uuid_t uuid) {
	FOUNDATION_UNUSED(uuid);
	return blake3_hash_null();
}

void
resource_source_set_import_hash(const uuid_t uuid, const blake3_hash_t import_hash) {
	FOUNDATION_UNUSED(uuid);
	FOUNDATION_UNUSED(import_hash);
}

#endif
