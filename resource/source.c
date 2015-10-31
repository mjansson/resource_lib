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

void
resource_source_set(resource_source_t* source, tick_t timestamp, hash_t key, uint64_t platform,
                    const char* value, size_t length) {
	resource_change_block_t* block = source->current;
	resource_change_t* change = block->changes + block->used++;
	if (block->used == RESOURCE_CHANGE_BLOCK_SIZE) {
		block->next = resource_change_block_allocate();
		source->current = block->next;
	}

	resource_change_data_t* data = block->current_data;
	while (data) {
		if (length < data->size - data->used)
			break;
		data = data->next;
	}
	if (!data) {
		size_t data_size = RESOURCE_CHANGE_BLOCK_DATA_SIZE;
		if (data_size < length)
			data_size = length;
		data = resource_change_data_allocate(data_size);
		data->next = block->current_data;
		block->current_data = data;
	}

	string_t value_str = string_copy(data->data + data->used, data->size - data->used,
	                                 value, length);

	change->timestamp = timestamp;
	change->hash = key;
	change->platform = platform;
	change->value = string_const(STRING_ARGS(value_str));

	data->used += length;
}

void
resource_source_unset(resource_source_t* source, tick_t timestamp, hash_t key, uint64_t platform) {
	resource_change_block_t* block = source->current;
	resource_change_t* change = block->changes + block->used++;
	if (block->used == RESOURCE_CHANGE_BLOCK_SIZE) {
		block->next = resource_change_block_allocate();
		source->current = block->next;
	}

	change->timestamp = timestamp;
	change->hash = key;
	change->platform = platform;
	change->value = string_const(0, 0);
}

void resource_source_collapse_history(resource_source_t* source) {
	//...
	FOUNDATION_UNUSED(source);
}

static stream_t*
resource_source_open(const uuid_t uuid, unsigned int mode) {
	char buffer[BUILD_MAX_PATHLEN];
	string_t path = resource_stream_make_path(buffer, sizeof(buffer),
	                                          STRING_ARGS(_resource_source_path),
	                                          uuid);
	if (mode & STREAM_OUT) {
		string_const_t dir_path = path_directory_name(STRING_ARGS(path));
		fs_make_directory(STRING_ARGS(dir_path));
	}
	return stream_open(STRING_ARGS(path), mode);
}

bool
resource_source_read(resource_source_t* source, const uuid_t uuid) {
	const char op_set = '=';
	const char op_unset = '-';
	stream_t* stream = resource_source_open(uuid, STREAM_IN);
	if (!stream)
		return false;
	stream_determine_binary_mode(stream, 16);
	const bool binary = stream_is_binary(stream);

	while (!stream_eos(stream)) {
		char op = 0;
		tick_t timestamp = stream_read_int64(stream);
		hash_t key = stream_read_uint64(stream);
		uint64_t platform = stream_read_uint64(stream);
		stream_read(stream, &op, 1);
		if (op == op_unset) {
			resource_source_unset(source, timestamp, key, platform);
		}
		else if (op == op_set) {
			string_t value = binary ? stream_read_string(stream) : stream_read_line(stream, '\n');
			resource_source_set(source, timestamp, key, platform, STRING_ARGS(value));
			string_deallocate(value.str);
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
			if (change->value.str) {
				stream_write(stream, &op_set, 1);
				if (!binary)
					stream_write(stream, &separator, 1);
				stream_write_string(stream, STRING_ARGS(change->value));
			}
			else {
				stream_write(stream, &op_unset, 1);
			}
			if (!binary)
				stream_write_endl(stream);
		}
		block = block->next;
	}

	stream_deallocate(stream);
	return true;
}

void
resource_source_map(resource_source_t* source, uint64_t platform, hashmap_t* map) {

	//Build an array of platform specific changes for each key, with an optimization
	//that single platform values are stored directly tagged with a low bit set
	hashmap_clear(map);
	resource_change_block_t* block = &source->first;
	while (block) {
		size_t ichg, chgsize;
		for (ichg = 0, chgsize = block->used; ichg < chgsize; ++ichg) {
			resource_change_t* change = block->changes + ichg;
			void* stored = hashmap_lookup(map, change->hash);
			if (!stored) {
				hashmap_insert(map, change->hash, (void*)(((uintptr_t)change) | 1));
			}
			else if ((uintptr_t)stored & 1) {
				resource_change_t* previous = (void*)((uintptr_t)stored & ~(uintptr_t)1);
				if (previous->platform == change->platform) {
					if (previous->timestamp < change->timestamp)
						hashmap_insert(map, change->hash, (void*)(((uintptr_t)change) | 1));
				}
				else {
					resource_change_t** newarr = 0;
					array_push(newarr, previous);
					array_push(newarr, change);
					hashmap_insert(map, change->hash, newarr);
				}
			}
			else {
				size_t imap, msize;
				resource_change_t** maparr = stored;
				for	(imap = 0, msize = array_size(maparr); imap < msize; ++imap) {
					if (maparr[imap]->platform == change->platform) {
						if (maparr[imap]->timestamp < change->timestamp)
							maparr[imap] = change;
						break;
					}
				}
				if (imap == msize) {
					array_push(maparr, change);
					hashmap_insert(map, change->hash, maparr);
				}
			}
		}
		block = block->next;
	}

	//Then collapse change array for each key to most specific platform with a set operation
	size_t ibucket, bsize;
	for (ibucket = 0, bsize = map->num_buckets; ibucket < bsize; ++ibucket) {
		size_t inode, nsize;
		hashmap_node_t* bucket = map->bucket[ibucket];
		for (inode = 0, nsize = array_size(bucket); inode < nsize; ++inode) {
			resource_change_t* best = 0;
			void* stored = bucket[inode].value;
			if (!stored)
				continue;
			else if ((uintptr_t)stored & 1) {
				best = (resource_change_t*)((uintptr_t)stored & ~(uintptr_t)1);
				if (!best->value.str)
					best = 0; //Unset operation
			}
			else {
				resource_change_t** maparr = stored;
				size_t imap, msize;
				for (imap = 0, msize = array_size(maparr); imap < msize; ++imap) {
					resource_change_t* change = maparr[imap];
					if (!change->value.str)
						continue; //Unset operation
					if (resource_platform_is_more_specific(platform, change->platform) &&
						(!best || (resource_platform_is_more_specific(change->platform, best->platform))))
						best = change;
				}
				array_deallocate(maparr);
			}
			bucket[inode].value = best;
		}
	}
}

#endif
