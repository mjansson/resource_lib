/* local.c  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
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

#include <resource/source.h>
#include <resource/change.h>
#include <resource/stream.h>
#include <resource/platform.h>
#include <resource/internal.h>

#include <foundation/foundation.h>

#if RESOURCE_ENABLE_LOCAL_SOURCE

static char _resource_source_path_buffer[BUILD_MAX_PATHLEN];
string_t _resource_source_path;

bool
resource_source_set_path(const char* path, size_t length) {
	if (!_resource_config.enable_local_source)
		return false;
	_resource_source_path = string_copy(_resource_source_path_buffer,
	                                    sizeof(_resource_source_path_buffer), path, length);
	_resource_source_path = path_clean(STRING_ARGS(_resource_source_path),
	                                   sizeof(_resource_source_path_buffer));
	_resource_source_path = path_absolute(STRING_ARGS(_resource_source_path),
	                                      sizeof(_resource_source_path_buffer));
	return true;
}

string_const_t
resource_source_path(void) {
	return string_to_const(_resource_source_path);
}

static stream_t*
resource_source_open(const uuid_t uuid, unsigned int mode) {
	char buffer[BUILD_MAX_PATHLEN];
	string_t path = resource_stream_make_path(buffer, sizeof(buffer),
	                                          STRING_ARGS(_resource_source_path), uuid);
	if (mode & STREAM_OUT) {
		string_const_t dir_path = path_directory_name(STRING_ARGS(path));
		fs_make_directory(STRING_ARGS(dir_path));
	}
	return stream_open(STRING_ARGS(path), mode);
}

static stream_t*
resource_source_open_blob(const uuid_t uuid, hash_t key, uint64_t platform,
                          hash_t checksum, unsigned int mode) {
	char buffer[BUILD_MAX_PATHLEN];
	char filename[64];
	string_t path = resource_stream_make_path(buffer, sizeof(buffer),
	                                          STRING_ARGS(_resource_source_path), uuid);
	string_t file = string_format(filename, sizeof(filename),
	                              STRING_CONST(".%" PRIhash ".%" PRIx64 ".%" PRIhash ".blob"),
	                              key, platform, checksum);
	path = string_append(STRING_ARGS(path), sizeof(buffer), STRING_ARGS(file));
	if (mode & STREAM_OUT) {
		string_const_t dir_path = path_directory_name(STRING_ARGS(path));
		fs_make_directory(STRING_ARGS(dir_path));
	}
	return stream_open(STRING_ARGS(path), mode);
}


static string_t*
resource_source_get_all_blobs(const uuid_t uuid) {
	char buffer[BUILD_MAX_PATHLEN];
	char patbuffer[128];
	string_t path = resource_stream_make_path(buffer, sizeof(buffer),
	                                          STRING_ARGS(_resource_source_path), uuid);
	string_const_t pathname = path_directory_name(STRING_ARGS(path));
	string_const_t filename = path_file_name(STRING_ARGS(path));
	string_t pattern = string_concat_varg(patbuffer, sizeof(patbuffer),
	                                      STRING_CONST("^"), STRING_ARGS(filename),
	                                      STRING_CONST(".*\\.blob$"),
	                                      nullptr);
	return fs_matching_files(STRING_ARGS(pathname), STRING_ARGS(pattern), false);
}

resource_source_t*
resource_source_allocate(void) {
	resource_source_t* source = memory_allocate(HASH_RESOURCE, sizeof(resource_source_t), 0,
	                                            MEMORY_PERSISTENT);
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
resource_source_change_set(resource_change_block_t* block, resource_change_t* change,
                           tick_t timestamp, hash_t key, uint64_t platform,
                           const char* value, size_t length) {
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
resource_source_change_set_blob(resource_change_t* change,
                                tick_t timestamp, hash_t key, uint64_t platform,
                                hash_t checksum, size_t size) {
	change->timestamp = timestamp;
	change->hash = key;
	change->platform = platform;
	change->flags = RESOURCE_SOURCEFLAG_BLOB;
	change->value.blob.checksum = checksum;
	change->value.blob.size = size;
}

void
resource_source_set(resource_source_t* source, tick_t timestamp, hash_t key, uint64_t platform,
                    const char* value, size_t length) {
	resource_change_block_t* block = source->current;
	resource_change_t* change = resource_source_change_grab(&source->current);
	resource_source_change_set(block, change, timestamp, key, platform, value, length);
}

void
resource_source_set_blob(resource_source_t* source, tick_t timestamp, hash_t key,
                         uint64_t platform, hash_t checksum, size_t size) {
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
			}
			else if ((uintptr_t)stored & (uintptr_t)1) {
				size_t imap = 0, msize = 0;
				resource_change_t** maparr = (void*)((uintptr_t)stored & ~(uintptr_t)1);
				if (!all_timestamps) {
					for (msize = array_size(maparr); imap < msize; ++imap) {
						if (maparr[imap]->platform == change->platform) {
							if (maparr[imap]->timestamp < change->timestamp) {
								if (change->flags == RESOURCE_SOURCEFLAG_UNSET) {
									array_erase(maparr, imap);
								}
								else {
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
			}
			else {
				resource_change_t* previous = stored;
				if (!all_timestamps && (previous->platform == change->platform)) {
					if (previous->timestamp < change->timestamp)
						hashmap_insert(map, change->hash, (change->flags != RESOURCE_SOURCEFLAG_UNSET) ? change : 0);
				}
				else {
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
resource_source_map_reduce(resource_source_t* source, hashmap_t* map, void* data,
                           resource_source_map_reduce_fn reduce) {
	size_t ibucket, bsize;
	resource_change_t* best;
	FOUNDATION_UNUSED(source);
	for (ibucket = 0, bsize = map->num_buckets; ibucket < bsize; ++ibucket) {
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
			}
			else {
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
	for (ibucket = 0, bsize = map->num_buckets; ibucket < bsize; ++ibucket) {
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

	//Create a new change block structure with changes that are set operations
	block = resource_change_block_allocate();
	resource_change_block_t* first = block;
	resource_source_map_reduce(source, map, &block, resource_source_collapse_reduce);

	//Copy first block data, swap next change block structure and free resources
	resource_change_block_finalize(&source->first);
	memcpy(&source->first, first, sizeof(resource_change_block_t));
	//Patch up first block change data pointers
	source->first.fixed.data.data = source->first.fixed.fixed;
	for (ichg = 0, chgsize = first->used; ichg < chgsize; ++ichg) {
		change = first->changes + ichg;
		if (change->flags == RESOURCE_SOURCEFLAG_VALUE) {
			ptrdiff_t diff = pointer_diff(change->value.value.str, first->fixed.fixed);
			if ((diff >= 0) && (diff < RESOURCE_CHANGE_BLOCK_DATA_SIZE))
				source->first.changes[ichg].value.value.str = source->first.fixed.fixed + diff;
		}
	}
	//Patch up first block current_data pointer
	source->first.current_data = &source->first.fixed.data;
	while (source->first.current_data->next)
		source->first.current_data = source->first.current_data->next;
	//Patch up current block
	source->current = (block == first) ? &source->first : block;
	//Free first block memory
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
		string_t blobfile = string_format(clear->blobname, sizeof(clear->blobname),
		                                  STRING_CONST("%.*s.%" PRIhash ".%" PRIx64 ".%" PRIhash ".blob"),
		                                  STRING_FORMAT(clear->uuidstr),
		                                  change->hash, change->platform, change->value.blob.checksum);
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
		string_t path = resource_stream_make_path(buffer, sizeof(buffer),
		                                          STRING_ARGS(_resource_source_path), uuid);
		string_const_t pathname = path_directory_name(STRING_ARGS(path));
		ptrdiff_t offset = pointer_diff(pathname.str, buffer);
		string_t fullname = path_append(buffer + offset, pathname.length, sizeof(buffer) - (size_t)offset,
		                                STRING_ARGS(clear.blobfiles[ifile]));
		fs_remove_file(STRING_ARGS(fullname));
	}

	string_array_deallocate(clear.blobfiles);
	hashmap_finalize(map);
}

bool
resource_source_read(resource_source_t* source, const uuid_t uuid) {
	const char op_set = '=';
	const char op_unset = '-';
	const char op_blob = '#';
	stream_t* stream = resource_source_open(uuid, STREAM_IN);
	if (!stream)
		return false;
	stream_determine_binary_mode(stream, 16);
	const bool binary = stream_is_binary(stream);

	while (!stream_eos(stream)) {
		char separator, op = 0;
		tick_t timestamp = stream_read_int64(stream);
		hash_t key = stream_read_uint64(stream);
		uint64_t platform = stream_read_uint64(stream);
		stream_read(stream, &op, 1);
		if (op == op_unset) {
			resource_source_unset(source, timestamp, key, platform);
		}
		else if (op == op_set) {
			string_t value;
			if (binary) {
				value = stream_read_string(stream);
			}
			else {
				stream_read(stream, &separator, 1);
				value = stream_read_line(stream, '\n');
			}
			resource_source_set(source, timestamp, key, platform, STRING_ARGS(value));
			string_deallocate(value.str);
		}
		else if (op == op_blob) {
			hash_t checksum = stream_read_uint64(stream);
			size_t size = (size_t)stream_read_uint64(stream);
			resource_source_set_blob(source, timestamp, key, platform, checksum, size);
		}
	}

	stream_deallocate(stream);

	return true;
}

bool
resource_source_write(resource_source_t* source, const uuid_t uuid, bool binary) {
	const char separator = ' ';
	const char op_set = '=';
	const char op_unset = '-';
	const char op_blob = '#';
	stream_t* stream = resource_source_open(uuid, STREAM_OUT | STREAM_CREATE | STREAM_TRUNCATE);
	if (!stream)
		return false;
	stream_set_binary(stream, binary);

	resource_change_block_t* block = &source->first;
	while (block) {
		size_t ichg, chgsize;
		for (ichg = 0, chgsize = block->used; ichg < chgsize; ++ichg) {
			resource_change_t* change = block->changes + ichg;
			stream_write_int64(stream, change->timestamp);
			if (!binary)
				stream_write(stream, &separator, 1);
			stream_write_uint64(stream, change->hash);
			if (!binary)
				stream_write(stream, &separator, 1);
			stream_write_uint64(stream, change->platform);
			if (!binary)
				stream_write(stream, &separator, 1);
			if (change->flags == RESOURCE_SOURCEFLAG_UNSET) {
				stream_write(stream, &op_unset, 1);
			}
			else {
				if (change->flags & RESOURCE_SOURCEFLAG_BLOB) {
					stream_write(stream, &op_blob, 1);
					if (!binary)
						stream_write(stream, &separator, 1);
					stream_write_uint64(stream, change->value.blob.checksum);
					if (!binary)
						stream_write(stream, &separator, 1);
					stream_write_uint64(stream, change->value.blob.size);
				}
				else {
					stream_write(stream, &op_set, 1);
					if (!binary)
						stream_write(stream, &separator, 1);
					stream_write_string(stream, STRING_ARGS(change->value.value));
				}
			}
			if (!binary)
				stream_write_endl(stream);
		}
		block = block->next;
	}

	stream_deallocate(stream);
	return true;
}

static resource_change_t*
resource_source_map_platform_reduce(resource_change_t* change, resource_change_t* best,
                                    void* data) {
	uint64_t platform = *(uint64_t*)data;
	if ((change->flags != RESOURCE_SOURCEFLAG_UNSET) &&
//Change must be superset of requested platform
	        resource_platform_is_equal_or_more_specific(platform, change->platform) &&
//Either no previous result, or
//  previous best is platform superset of change platform and
//    either platforms are different (change is exclusively more specific), or
///   change is newer (and platforms are equal)
	        (!best || (resource_platform_is_equal_or_more_specific(change->platform, best->platform) &&
	                   ((change->platform != best->platform) || (change->timestamp > best->timestamp)))))
		return change;
	return best;
}

void
resource_source_map(resource_source_t* source, uint64_t platform, hashmap_t* map) {
	resource_source_map_all(source, map, false);
	resource_source_map_reduce(source, map, &platform, resource_source_map_platform_reduce);
}

bool
resource_source_read_blob(const uuid_t uuid, hash_t key, uint64_t platform,
                          hash_t checksum, void* data, size_t capacity) {
	stream_t* stream = resource_source_open_blob(uuid, key, platform, checksum, STREAM_IN);
	if (!stream)
		return false;
	size_t read = stream_read(stream, data, capacity);
	stream_deallocate(stream);
	return read == capacity;
}

bool
resource_source_write_blob(const uuid_t uuid, tick_t timestamp, hash_t key,
                           uint64_t platform, hash_t checksum, const void* data, size_t size) {
	unsigned int mode = STREAM_OUT | STREAM_BINARY | STREAM_CREATE | STREAM_TRUNCATE;
	stream_t* stream = resource_source_open_blob(uuid, key, platform, checksum, mode);
	FOUNDATION_UNUSED(timestamp);
	if (!stream)
		return false;
	size_t written = stream_write(stream, data, size);
	stream_deallocate(stream);
	return written == size;
}

#endif
