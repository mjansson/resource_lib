/* sourced.h  -  Resource library  -  Public Domain  -  2016 Mattias Jansson / Rampant Pixels
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

#include <foundation/types.h>
#include <network/types.h>

#define SOURCED_PROTOCOL_VERSION 1

typedef enum sourced_message_id sourced_message_id;
typedef enum sourced_result_id sourced_result_id;

typedef struct sourced_string_t sourced_string_t;
typedef struct sourced_blob_t sourced_blob_t;
typedef struct sourced_change_t sourced_change_t;

typedef struct sourced_message_t sourced_message_t;
typedef struct sourced_lookup_t sourced_lookup_t;
typedef struct sourced_lookup_result_t sourced_lookup_result_t;
typedef struct sourced_reverse_lookup_t sourced_reverse_lookup_t;
typedef struct sourced_reverse_lookup_result_t sourced_reverse_lookup_result_t;
typedef struct sourced_import_t sourced_import_t;
typedef struct sourced_import_result_t sourced_import_result_t;
typedef struct sourced_read_t sourced_read_t;
typedef struct sourced_read_result_t sourced_read_result_t;
typedef struct sourced_get_t sourced_get_t;
typedef struct sourced_get_result_t sourced_get_result_t;
typedef struct sourced_set_t sourced_set_t;
typedef struct sourced_set_result_t sourced_set_result_t;
typedef struct sourced_delete_t sourced_delete_t;
typedef struct sourced_delete_result_t sourced_delete_result_t;
typedef struct sourced_notify_create_t sourced_notify_create_t;
typedef struct sourced_notify_change_t sourced_notify_change_t;
typedef struct sourced_notify_delete_t sourced_notify_delete_t;

enum sourced_message_id {
	SOURCED_LOOKUP = 1,
	SOURCED_LOOKUP_RESULT,

	SOURCED_REVERSE_LOOKUP,
	SOURCED_REVERSE_LOOKUP_RESULT,

	SOURCED_IMPORT,
	SOURCED_IMPORT_RESULT,

	SOURCED_GET,
	SOURCED_GET_RESULT,
	SOURCED_READ,
	SOURCED_READ_RESULT,

	SOURCED_SET,
	SOURCED_SET_RESULT,
	SOURCED_UNSET,
	SOURCED_UNSET_RESULT,

	SOURCED_DELETE,
	SOURCED_DELETE_RESULT,

	SOURCED_NOTIFY_CREATE,
	SOURCED_NOTIFY_CHANGE,
	SOURCED_NOTIFY_DELETE
};

enum sourced_result_id {
	SOURCED_OK = 0,
	SOURCED_FAILED
};

#define SOURCED_DECLARE_MESSAGE \
	uint32_t id; \
	uint32_t size

#define SOURCED_DECLARE_REPLY \
	uint32_t result

struct sourced_message_t {
	SOURCED_DECLARE_MESSAGE;
};

struct sourced_lookup_t {
	SOURCED_DECLARE_MESSAGE;
	char path[];
};

struct sourced_lookup_result_t {
	SOURCED_DECLARE_REPLY;
	uuid_t uuid;
	uint256_t hash;
};

struct sourced_reverse_lookup_t {
	SOURCED_DECLARE_MESSAGE;
	uuid_t uuid;
};

struct sourced_reverse_lookup_result_t {
	SOURCED_DECLARE_REPLY;
	char path[];
};

struct sourced_import_t {
	SOURCED_DECLARE_MESSAGE;
	uuid_t uuid;
	char path[];
};

struct sourced_import_result_t {
	SOURCED_DECLARE_REPLY;
	uuid_t uuid;
	uint32_t flags;
};

struct sourced_get_t {
	SOURCED_DECLARE_MESSAGE;
	uuid_t uuid;
	uint64_t platform;
	hash_t key;
};

struct sourced_blob_t {
	/*! Checksum */
	hash_t checksum;
	/*! Data size */
	uint64_t size;
};

struct sourced_string_t {
	uint64_t offset;
	uint64_t length;
};

struct sourced_change_t {
	/*! Change timestamp */
	tick_t timestamp;
	/*! Key hash */
	hash_t hash;
	/*! Platform */
	uint64_t platform;
	/*! FLags */
	uint32_t flags;
	/*! Value union */
	union {
		/*! String value */
		sourced_string_t value;
		/*! Blob value */
		sourced_blob_t blob;
	} value;
};

struct sourced_get_result_t {
	SOURCED_DECLARE_REPLY;
	sourced_change_t change;
	char payload[];
};

struct sourced_read_t {
	SOURCED_DECLARE_MESSAGE;
	uuid_t uuid;
	uint64_t platform;
};

struct sourced_read_result_t {
	SOURCED_DECLARE_REPLY;
	uint256_t hash;
	uint32_t num_changes;
	char payload[FOUNDATION_FLEXIBLE_ARRAY];
};

struct sourced_set_t {
	SOURCED_DECLARE_MESSAGE;
	uuid_t uuid;
	hash_t key;
	uint64_t platform;
	char value[];
};

struct sourced_set_result_t {
	SOURCED_DECLARE_REPLY;
};

struct sourced_unset_t {
	SOURCED_DECLARE_MESSAGE;
	uuid_t uuid;
	hash_t key;
	uint64_t platform;
};

struct sourced_unset_result_t {
	SOURCED_DECLARE_REPLY;
};

struct sourced_delete_t {
	SOURCED_DECLARE_MESSAGE;
	uuid_t uuid;
};

struct sourced_delete_result_t {
	SOURCED_DECLARE_REPLY;
};

struct sourced_notify_create_t {
	SOURCED_DECLARE_MESSAGE;
	uuid_t uuid;
};

struct sourced_notify_change_t {
	SOURCED_DECLARE_MESSAGE;
	uuid_t uuid;
};

struct sourced_notify_delete_t {
	SOURCED_DECLARE_MESSAGE;
	uuid_t uuid;
};

int
sourced_write_lookup(socket_t* sock, const char* path, size_t length);

int
sourced_write_lookup_reply(socket_t* sock, uuid_t uuid, uint256_t hash);

int
sourced_read_lookup_reply(socket_t* sock, sourced_lookup_result_t* result);

int
sourced_write_read(socket_t* sock, uuid_t uuid);

int
sourced_write_read_reply(socket_t* sock, resource_source_t* source, uint256_t hash);

int
sourced_read_read_reply(socket_t* sock, sourced_read_result_t* result, size_t size);
