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
	                              STRING_CONST(".%" PRIhash ".%" PRIx64 ".%" PRIhash),
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
	                                      STRING_CONST("\\.[a-f0-9]*\\.[a-f0-9]*\\.[a-f0-9]*$"),
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

//Build an array of platform specific changes for each key, with an optimization
//that single platform values are stored directly tagged with a low bit set
static void
resource_source_map_all_platforms(resource_source_t* source, hashmap_t* map) {
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
				for (imap = 0, msize = array_size(maparr); imap < msize; ++imap) {
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
}

static void
resource_source_map_reduce(resource_source_t* source, hashmap_t* map, void* data,
						   void (*reduce)(resource_source_t*, resource_change_t*, void*)) {
	size_t ibucket, bsize;
	for (ibucket = 0, bsize = map->num_buckets; ibucket < bsize; ++ibucket) {
		size_t inode, nsize;
		hashmap_node_t* bucket = map->bucket[ibucket];
		for (inode = 0, nsize = array_size(bucket); inode < nsize; ++inode) {
			resource_change_t* change = 0;
			void* stored = bucket[inode].value;
			if (!stored)
				continue;
			else if ((uintptr_t)stored & 1) {
				change = (resource_change_t*)((uintptr_t)stored & ~(uintptr_t)1);
				if (change->flags == RESOURCE_SOURCEFLAG_UNSET)
					continue;
				reduce(source, change, data);
			}
			else {
				resource_change_t** maparr = stored;
				size_t imap, msize;
				for (imap = 0, msize = array_size(maparr); imap < msize; ++imap) {
					change = maparr[imap];
					if (change->flags == RESOURCE_SOURCEFLAG_UNSET)
						continue;
					reduce(source, change, data);
				}
				array_deallocate(maparr);
			}
		}
	}
}

static void
resource_source_collapse_reduce(resource_source_t* source, resource_change_t* change, void* data) {
	resource_change_block_t** block = data;
	FOUNDATION_UNUSED(source);
	if (change->flags & RESOURCE_SOURCEFLAG_BLOB) {
		resource_change_t* store = resource_source_change_grab(block);
		resource_source_change_set_blob(store, change->timestamp, change->hash, change->platform,
										change->value.blob.checksum, change->value.blob.size);
	}
	else {
		resource_change_t* store = resource_source_change_grab(block);
		resource_source_change_set(*block, store, change->timestamp, change->hash, change->platform,
								   STRING_ARGS(change->value.value));
	}
}

void
resource_source_collapse_history(resource_source_t* source) {
	hashmap_fixed_t fixedmap;
	hashmap_t* map = (hashmap_t*)&fixedmap;
	hashmap_initialize(map, sizeof(fixedmap.bucket) / sizeof(fixedmap.bucket[0]), 8);
	resource_source_map_all_platforms(source, map);

	//Create a new change block structure with changes that are set operations
	resource_change_block_t* block = resource_change_block_allocate();
	resource_change_block_t* first = block;
	resource_source_map_reduce(source, map, &block, resource_source_collapse_reduce);

	//Swap change block structure and free resources
	resource_change_block_finalize(&source->first);
	memcpy(&source->first, first, sizeof(resource_change_block_t));
	memory_deallocate(first);

	hashmap_finalize(map);
}

struct resource_blob_reduce_t {
	string_const_t uuidstr;
	string_t* blobfiles;
	char blobname[128];
};

static void
resource_source_clear_blob_reduce(resource_source_t* source, resource_change_t* change, void* data) {
	struct resource_blob_reduce_t* reduce = data;
	string_t* blobfiles = reduce->blobfiles;
	string_const_t uuidstr = reduce->uuidstr;
	size_t ifile, fsize;
	FOUNDATION_UNUSED(source);
	if (change->flags & RESOURCE_SOURCEFLAG_BLOB) {
		string_t blobfile = string_format(reduce->blobname, sizeof(reduce->blobname),
										  STRING_CONST("%.*s.%" PRIhash ".%" PRIx64 ".%" PRIhash),
										  STRING_FORMAT(uuidstr),
										  change->hash, change->platform, change->value.blob.checksum);
		for (ifile = 0, fsize = array_size(blobfiles); ifile < fsize; ++ifile) {
			if (string_equal(STRING_ARGS(blobfiles[ifile]), STRING_ARGS(blobfile))) {
				string_deallocate(blobfiles[ifile].str);
				array_erase(blobfiles, ifile);
				break;
			}
		}
	}
}

void
resource_source_clear_blob_history(resource_source_t* source, const uuid_t uuid) {
	string_t* blobfiles = resource_source_get_all_blobs(uuid);

	hashmap_fixed_t fixedmap;
	hashmap_t* map = (hashmap_t*)&fixedmap;
	hashmap_initialize(map, sizeof(fixedmap.bucket) / sizeof(fixedmap.bucket[0]), 8);
	resource_source_map_all_platforms(source, map);

	struct resource_blob_reduce_t arg;
	arg.blobfiles = blobfiles;
	arg.uuidstr = string_from_uuid_static(uuid);
	resource_source_map_reduce(source, map, &arg, resource_source_clear_blob_reduce);

	size_t ifile, fsize;
	for (ifile = 0, fsize = array_size(blobfiles); ifile < fsize; ++ifile) {
		//	delete blob file
	}

	string_array_deallocate(blobfiles);
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

/*static void
resource_source_map_platform_reduce(resource_source_t* source, resource_change_t* change, void* data) {
	resource_change_t** best = data;
	if ((change->flags != RESOURCE_SOURCEFLAG_UNSET) &&
			resource_platform_is_more_specific(platform, change->platform) &&
			(!*best || (resource_platform_is_more_specific(change->platform, (*best)->platform))))
		*best = change;
}*/

void
resource_source_map(resource_source_t* source, uint64_t platform, hashmap_t* map) {
	hashmap_clear(map);
	resource_source_map_all_platforms(source, map);

	//Then collapse change array for each key to most specific platform with a set operation
	resource_change_t* best = 0;
	//resource_source_map_reduce(source, map, &best, resource_source_map_platform_reduce);
	for (ibucket = 0, bsize = map->num_buckets; ibucket < bsize; ++ibucket) {
		size_t inode, nsize;
		hashmap_node_t* bucket = map->bucket[ibucket];
		for (inode = 0, nsize = array_size(bucket); inode < nsize; ++inode) {
			resource_change_t* best = 0;
			void* stored = bucket[inode].value;
			if (!stored)
				continue;
			else if ((uintptr_t)stored & 1) {
				resource_change_t* change = (resource_change_t*)((uintptr_t)stored & ~(uintptr_t)1);
				if ((change->flags != RESOURCE_SOURCEFLAG_UNSET) &&
				        resource_platform_is_more_specific(platform, change->platform))
					best = change;
			}
			else {
				resource_change_t** maparr = stored;
				size_t imap, msize;
				for (imap = 0, msize = array_size(maparr); imap < msize; ++imap) {
					resource_change_t* change = maparr[imap];
					if ((change->flags != RESOURCE_SOURCEFLAG_UNSET) &&
					        resource_platform_is_more_specific(platform, change->platform) &&
					        (!best || (resource_platform_is_more_specific(change->platform, best->platform))))
						best = change;
				}
				array_deallocate(maparr);
			}
			bucket[inode].value = best;
		}
	}
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
