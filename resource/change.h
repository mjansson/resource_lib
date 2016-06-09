/* change.h  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
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

#pragma once

#include <foundation/platform.h>

#include <resource/types.h>

#if RESOURCE_ENABLE_LOCAL_SOURCE

RESOURCE_API bool
resource_change_is_value(resource_change_t* change);

RESOURCE_API bool
resource_change_is_blob(resource_change_t* change);

RESOURCE_API resource_change_data_t*
resource_change_data_allocate(size_t size);

RESOURCE_API void
resource_change_data_deallocate(resource_change_data_t* data);

RESOURCE_API void
resource_change_data_initialize(resource_change_data_t* data, void* buffer, size_t size);

RESOURCE_API void
resource_change_data_finalize(resource_change_data_t* data);

RESOURCE_API resource_change_block_t*
resource_change_block_allocate(void);

RESOURCE_API void
resource_change_block_deallocate(resource_change_block_t* block);

RESOURCE_API void
resource_change_block_initialize(resource_change_block_t* block);

RESOURCE_API void
resource_change_block_finalize(resource_change_block_t* block);

#else

#define resource_change_data_allocate() nullptr
#define resource_change_data_deallocate(data) memory_deallocate(data)
#define resource_change_data_initialize(data) ((void)sizeof(data))
#define resource_change_data_finalize(data) ((void)sizeof(data))
#define resource_change_block_allocate() nullptr
#define resource_change_block_deallocate(block) memory_deallocate(block)
#define resource_change_block_initialize(block) ((void)sizeof(block))
#define resource_change_block_finalize(block) ((void)sizeof(block))

#endif
