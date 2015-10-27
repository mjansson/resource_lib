/* change.c  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
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

#include <resource/change.h>
#include <resource/internal.h>

#include <foundation/foundation.h>

#if RESOURCE_ENABLE_LOCAL_SOURCE

resource_change_data_t*
resource_change_data_allocate(size_t size) {
	resource_change_data_t* data = memory_allocate(HASH_RESOURCE,
	                                               size + sizeof(resource_change_data_t), 0,
	                                               MEMORY_PERSISTENT);
	resource_change_data_initialize(data, pointer_offset(data, sizeof(resource_change_data_t)), size);
	return data;
}

void resource_change_data_deallocate(resource_change_data_t* data) {
	memory_deallocate(data);
}

void
resource_change_data_initialize(resource_change_data_t* data, void* buffer, size_t size) {
	data->data = buffer;
	data->size = size;
	data->used = 0;
	data->next = 0;
}

resource_change_block_t*
resource_change_block_allocate(void) {
	resource_change_block_t* block = memory_allocate(HASH_RESOURCE, sizeof(resource_change_block_t),
	                                                 0, MEMORY_PERSISTENT);
	resource_change_block_initialize(block);
	return block;
}

void
resource_change_block_deallocate(resource_change_block_t* block) {
	resource_change_block_finalize(block);
	memory_deallocate(block);
}

void
resource_change_block_initialize(resource_change_block_t* block) {
	resource_change_data_initialize(&block->fixed.data, block->fixed.fixed, sizeof(block->fixed.fixed));
	block->used = 0;
	block->current_data = &block->fixed.data;
	block->next = 0;
}

void
resource_change_block_finalize(resource_change_block_t* block) {
	resource_change_block_t* next_block = block->next;
	resource_change_data_t* data = block->current_data;
	while (data) {
		resource_change_data_t* next = data->next;
		if (data != &block->fixed.data)
			resource_change_data_deallocate(data);
		data = next;
	}
	if (next_block)
		resource_change_block_deallocate(next_block);
}


#endif
