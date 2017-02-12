/* source.h  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
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

RESOURCE_API string_const_t
resource_source_path(void);

RESOURCE_API bool
resource_source_set_path(const char* path, size_t length);

RESOURCE_API uint256_t
resource_source_read_hash(const uuid_t uuid, uint64_t platform);

RESOURCE_API resource_source_t*
resource_source_allocate(void);

RESOURCE_API void
resource_source_deallocate(resource_source_t* source);

RESOURCE_API void
resource_source_initialize(resource_source_t* source);

RESOURCE_API void
resource_source_finalize(resource_source_t* source);

/*! Read source file. If source is null the return value indicates
if file could have been read.
\param source Source to read into
\param uuid Resource UUID
\return true if read successfully, false if failed or error */
RESOURCE_API bool
resource_source_read(resource_source_t* source, const uuid_t uuid);

RESOURCE_API bool
resource_source_write(resource_source_t* source, const uuid_t uuid, bool binary);

RESOURCE_API void
resource_source_set(resource_source_t* source, tick_t timestamp, hash_t key,
                    uint64_t platform, const char* value, size_t length);

RESOURCE_API void
resource_source_unset(resource_source_t* source, tick_t timestamp, hash_t key,
                      uint64_t platform);

RESOURCE_API resource_change_t*
resource_source_get(resource_source_t* source, hash_t key, uint64_t platform);

RESOURCE_API void
resource_source_set_blob(resource_source_t* source, tick_t timestamp, hash_t key,
                         uint64_t platform, hash_t checksum, size_t size);

RESOURCE_API bool
resource_source_read_blob(const uuid_t uuid, hash_t key,
                          uint64_t platform, hash_t checksum, void* data, size_t capacity);

RESOURCE_API bool
resource_source_write_blob(const uuid_t uuid, tick_t timestamp, hash_t key,
                           uint64_t platform, hash_t checksum, const void* data, size_t size);

RESOURCE_API void
resource_source_collapse_history(resource_source_t* source);

RESOURCE_API void
resource_source_clear_blob_history(resource_source_t* source, const uuid_t uuid);

RESOURCE_API void
resource_source_map(resource_source_t* source, uint64_t platform, hashmap_t* map);

/*! Build a map with arrays of platform specific changes for each key, with an optimization
that single platform values are stored directly as pointers and arrays of platform values
are tagged with a low bit set in the pointer. Mostly used in conjunction with
resource_source_map_reduce which will internalize this representation and present clean
resource_change_t* data for inspection. Clears the map before storing data.
\param source Resource source
\param map Map storing results
\param all_timestamps Flag to include all timestamps, not only newest */
RESOURCE_API void
resource_source_map_all(resource_source_t* source, hashmap_t* map, bool all_timestamps);

/*! Iterate of a map of source key-value to perform operations on each change.
The iteration can be aborted by the reduce function returning a marker value of -1 */
RESOURCE_API void
resource_source_map_iterate(resource_source_t* source, hashmap_t* map, void* data,
                            resource_source_map_iterate_fn iterate);

/*! Iterate of a map of source key-value to perform operations on each change and
selecting the best change, thus reducing the map to one change per key/platform.
The iteration can be aborted by the reduce function returning a marker value of -1 */
RESOURCE_API void
resource_source_map_reduce(resource_source_t* source, hashmap_t* map, void* data,
                           resource_source_map_reduce_fn reduce);

/*! Clear hashmap generated by #resource_source_map_all and free resources used. Must
be called for a map not reduced with #resource_source_map_reduce.
\param map Map storing results */
RESOURCE_API void
resource_source_map_clear(hashmap_t* map);

RESOURCE_API size_t
resource_source_num_dependencies(const uuid_t uuid, uint64_t platform);

RESOURCE_API size_t
resource_source_dependencies(const uuid_t uuid, uint64_t platform, uuid_t* deps, size_t capacity);

RESOURCE_API void
resource_source_set_dependencies(const uuid_t uuid, uint64_t platform, const uuid_t* deps, size_t num);
