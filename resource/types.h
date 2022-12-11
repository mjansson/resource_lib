/* types.h  -  Resource library  -  Public Domain  -  2014 Mattias Jansson
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

#pragma once

#include <foundation/platform.h>
#include <foundation/types.h>
#include <blake3/types.h>

#include <resource/build.h>

#define RESOURCE_PLATFORM_ALL ((uint64_t)-1)

typedef enum resource_event_id {
	/*! Resource was created */
	RESOURCEEVENT_CREATE = 0,
	/*! Resource source was modified */
	RESOURCEEVENT_MODIFY,
	/*! Resource dependency was modified */
	RESOURCEEVENT_DEPENDS,
	/*! Resource was deleted */
	RESOURCEEVENT_DELETE,
	/*! Resource was successfully compiled */
	RESOURCEEVENT_COMPILE,
	RESOURCEEVENT_LAST_RESERVED = 32
} resource_event_id;

#define RESOURCE_SOURCEFLAG_UNSET 0
#define RESOURCE_SOURCEFLAG_VALUE 1
#define RESOURCE_SOURCEFLAG_BLOB 2

typedef struct resource_config_t resource_config_t;
typedef union resource_change_value_t resource_change_value_t;
typedef struct resource_change_t resource_change_t;
typedef struct resource_change_data_t resource_change_data_t;
typedef struct resource_change_data_fixed_t resource_change_data_fixed_t;
typedef struct resource_change_block_t resource_change_block_t;
typedef struct resource_change_map_t resource_change_map_t;
typedef struct resource_source_t resource_source_t;
typedef struct resource_blob_t resource_blob_t;
typedef struct resource_platform_t resource_platform_t;
typedef struct resource_header_t resource_header_t;
typedef struct resource_signature_t resource_signature_t;
typedef struct resource_dependency_t resource_dependency_t;

typedef int (*resource_import_fn)(stream_t*, const uuid_t);
typedef int (*resource_compile_fn)(const uuid_t, uint64_t, resource_source_t*, const blake3_hash_t, const char*,
                                   size_t);
typedef resource_change_t* (*resource_source_map_reduce_fn)(resource_change_t*, resource_change_t*, void*);
typedef int (*resource_source_map_iterate_fn)(resource_change_t*, void*);

/*! Resource library configuration */
struct resource_config_t {
	/*! Enable use of in-process auto import of raw assets to resource source files */
	bool enable_local_autoimport;
	/*! Enable use of remote source daemon for managing imports and resource source files */
	bool enable_remote_sourced;
	/*! Enable use of locally stored resource source files */
	bool enable_local_source;
	/*! Enable use of locally stored compiled resources and bundles */
	bool enable_local_cache;
	/*! Enable use of remote compile daemon for managing compiled resources and bundles */
	bool enable_remote_compiled;
};

/*! Decomposed platform specification */
struct resource_platform_t {
	//! Platform identifier, 8 bits, [0..254]
	int platform;
	//! Architecture identifier, 8 bits, [0..254]
	int arch;
	//! Render API group identifier, 8 bits, [0..254]
	int render_api_group;
	//! Render API identifier, 8 bits, [0..254]
	int render_api;
	//! Quality level identifier, 8 bits, [0..254]
	int quality_level;
	//! Custom identifier, 8 bits, [0..254]
	int custom;
};

/*! Dependency data for a resource to another resource */
struct resource_dependency_t {
	//! Dependent resource UUID
	uuid_t uuid;
	//! Resource platform
	uint64_t platform;
};

/*! Representation of metadata for a binary data blob */
struct resource_blob_t {
	/*! Checksum */
	hash_t checksum;
	/*! Data size */
	size_t size;
};

/*! Value union */
union resource_change_value_t {
	/*! String value */
	string_const_t value;
	/*! Blob value */
	resource_blob_t blob;
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
	resource_change_value_t value;
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
	/*! Flag if source was read as binary */
	bool read_binary;
};

/*! Header for single resource file */
struct resource_header_t {
	/*! Type hash */
	hash_t type;
	/*! Version */
	uint32_t version;
	/*! Source hash */
	blake3_hash_t source_hash;
};

/*! Signature for a resource source file */
struct resource_signature_t {
	/*! Resource UUID */
	uuid_t uuid;
	/*! Source file hash */
	blake3_hash_t hash;
};

static FOUNDATION_FORCEINLINE hash_t
resource_uuid_hash(const uuid_t uuid);

// Implementation

static FOUNDATION_FORCEINLINE hash_t
resource_uuid_hash(const uuid_t uuid) {
	return uuid.word[0] ^ uuid.word[1];
}
