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
#include <resource/internal.h>

#include <foundation/foundation.h>

#if RESOURCE_ENABLE_LOCAL_SOURCE

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
	hashmap_initialize((hashmap_t*)&source->merged, RESOURCE_CHANGE_MAP_BUCKETS,
	                   RESOURCE_CHANGE_MAP_BUCKET_SIZE);
}

void
resource_source_finalize(resource_source_t* source) {
	resource_change_block_finalize(&source->first);
	hashmap_finalize((hashmap_t*)&source->merged);
}

void
resource_source_set(resource_source_t* source, tick_t timestamp, hash_t key, const char* value,
                    size_t length) {
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
	change->value = string_const(STRING_ARGS(value_str));

	data->used += length;

	resource_change_t* stored = hashmap_lookup((hashmap_t*)&source->merged, change->hash);
	if (!stored || (stored->timestamp < change->timestamp))
		hashmap_insert((hashmap_t*)&source->merged, change->hash, change);
}

bool
resource_source_read(resource_source_t* source, stream_t* stream) {
	const bool binary = stream_is_binary(stream);
	while (!stream_eos(stream)) {
		tick_t timestamp = stream_read_uint64(stream);
		tick_t key = stream_read_uint64(stream);
		string_t value = stream_read_line(stream, '\n');
		resource_source_set(source, timestamp, key, STRING_ARGS(value));
		string_deallocate(value.str);
	}

	return true;
}

bool
resource_source_write(resource_source_t* source, stream_t* stream) {
	const bool binary = stream_is_binary(stream);
	const char separator = ' ';
	resource_change_block_t* block = &source->first;
	while (block) {
		size_t ichg, chgsize;
		for (ichg = 0, chgsize = block->used; ichg < chgsize; ++ichg) {
			resource_change_t* change = block->changes + ichg;
			stream_write_uint64(stream, change->timestamp);
			if (!binary)
				stream_write(stream, &separator, 1);
			stream_write_uint64(stream, change->hash);
			if (!binary)
				stream_write(stream, &separator, 1);
			stream_write_string(stream, STRING_ARGS(change->value));
			if (!binary)
				stream_write_endl(stream);
		}
		block = block->next;
	}
	return true;
}

hashmap_t*
resource_source_map(resource_source_t* source) {
	return (hashmap_t*)&source->merged;
}

#endif
