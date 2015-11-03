/* types.h  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
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

#pragma once

#include <foundation/platform.h>
#include <foundation/types.h>

#include <resource/build.h>

typedef enum resource_event_id {
	RESOURCEEVENT_UPDATE_STATIC = 0,
	RESOURCEEVENT_UPDATE_DYNAMIC
} resource_event_id;

#define RESOURCE_SOURCEFLAG_UNSET 0
#define RESOURCE_SOURCEFLAG_VALUE 1
#define RESOURCE_SOURCEFLAG_BLOB  2

typedef struct resource_base_t              resource_base_t;
typedef struct resource_config_t            resource_config_t;
typedef struct resource_event_t             resource_event_t;
typedef struct resource_change_t            resource_change_t;
typedef struct resource_change_data_t       resource_change_data_t;
typedef struct resource_change_data_fixed_t resource_change_data_fixed_t;
typedef struct resource_change_block_t      resource_change_block_t;
typedef struct resource_change_map_t        resource_change_map_t;
typedef struct resource_source_t            resource_source_t;
typedef struct resource_blob_t              resource_blob_t;

typedef int (* resource_import_fn)(stream_t*);
typedef resource_change_t* (* resource_source_map_reduce_fn)(resource_change_t*, resource_change_t*,
        void*);

struct resource_config_t {
	bool enable_local_cache;
	bool enable_local_source;
	bool enable_remote_source;
};

#define RESOURCE_DECLARE_EVENT   \
	FOUNDATION_DECLARE_EVENT;    \
	uuid_t uuid

struct resource_event_t {
	RESOURCE_DECLARE_EVENT;
};

#define RESOURCE_DECLARE_OBJECT   \
	FOUNDATION_DECLARE_OBJECT;    \
	uuid_t uuid

FOUNDATION_ALIGNED_STRUCT(resource_base_t, 8) {
	RESOURCE_DECLARE_OBJECT;
};

/*! Representation of metadata for a binary data blob */
struct resource_blob_t {
	/*! Checksum */
	hash_t checksum;
	/*! Data size */
	size_t size;
};

/*! Representation of a single change of a key-value
pair in a resource object */
struct resource_change_t {
	/*! Change timestamp */
	tick_t timestamp;
	/*! Key hash */
	hash_t hash;
	/*! Platform */
	uint64_t platform;
	/*! FLags */
	unsigned int flags;
	/*! Value union */
	union {
		/*! String value */
		string_const_t value;
		/*! Blob value */
		resource_blob_t blob;
	} value;
};

/*! Representation of a block of memory storing change data */
struct resource_change_data_t {
	/*! Data */
	char* data;
	/*! Size of data block */
	size_t size;
	/*! Number of bytes used */
	size_t used;
	/*! Next block */
	resource_change_data_t* next;
};

/*! Representation of a block of memory storing change data
in a fixed block */
struct resource_change_data_fixed_t {
	/*! Data */
	resource_change_data_t data;
	/*! Fixed block */
	char fixed[RESOURCE_CHANGE_BLOCK_DATA_SIZE];
};

/*! Representation of a block of changes in a resource object */
struct resource_change_block_t {
	/*! Changes */
	resource_change_t changes[RESOURCE_CHANGE_BLOCK_SIZE];
	/*! Number of used changes */
	size_t used;
	/*! Change data of fixed size */
	resource_change_data_fixed_t fixed;
	/*! Current change data */
	resource_change_data_t* current_data;
	/*! Next block */
	resource_change_block_t* next;
};

/*! Representation of data of an object as a timestamped
key-value store */
struct resource_source_t {
	/*! Changes */
	resource_change_block_t first;
	/*! Current block */
	resource_change_block_t* current;
};
