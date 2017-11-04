/* compiled.h  -  Resource library  -  Public Domain  -  2016 Mattias Jansson / Rampant Pixels
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

#define COMPILED_PROTOCOL_VERSION 1

enum compiled_message_id {
	COMPILED_OPEN_STATIC,
	COMPILED_OPEN_STATIC_RESULT,
	
	COMPILED_OPEN_DYNAMIC,
	COMPILED_OPEN_DYNAMIC_RESULT,
	
	COMPILED_NOTIFY_CREATE,
	COMPILED_NOTIFY_MODIFY,
	COMPILED_NOTIFY_DEPENDS,
	COMPILED_NOTIFY_DELETE
};

enum compiled_result_id {
	COMPILED_OK = 0,
	COMPILED_FAILED
};

typedef enum compiled_message_id compiled_message_id;
typedef enum compiled_result_id compiled_result_id;

typedef struct compiled_message_t compiled_message_t;
typedef struct compiled_open_static_t compiled_open_static_t;
typedef struct compiled_open_dynamic_t compiled_open_dynamic_t;
typedef struct compiled_open_result_t compiled_open_result_t;
typedef struct compiled_notify_t compiled_notify_t;

#define COMPILED_DECLARE_MESSAGE \
	uint32_t id; \
	uint32_t size

#define COMPILED_DECLARE_REPLY \
	uint32_t result; \
	uint32_t flags

struct compiled_message_t {
	COMPILED_DECLARE_MESSAGE;
};

struct compiled_open_static_t {
	COMPILED_DECLARE_MESSAGE;
	uuid_t uuid;
	uint64_t platform;
};

struct compiled_open_dynamic_t {
	COMPILED_DECLARE_MESSAGE;
	uuid_t uuid;
	uint64_t platform;
};

struct compiled_open_result_t {
	COMPILED_DECLARE_REPLY;
	uint64_t stream_size;
};

struct compiled_notify_t {
	COMPILED_DECLARE_MESSAGE;
	uuid_t uuid;
	uint64_t platform;
	uint64_t token;
};

int
compiled_write_open_static(socket_t* sock, uuid_t uuid, uint64_t platform);

int
compiled_write_open_dynamic(socket_t* sock, uuid_t uuid, uint64_t platform);

int
compiled_read_open_static_reply(socket_t* sock, size_t size, compiled_open_result_t* result);

int
compiled_read_open_dynamic_reply(socket_t* sock, size_t size, compiled_open_result_t* result);

int
compiled_write_open_static_reply(socket_t* sock, bool success, size_t size);

int
compiled_write_open_dynamic_reply(socket_t* sock, bool success, size_t size);

int
compiled_write_notify(socket_t* sock, compiled_message_id id, uuid_t uuid, uint64_t platform, hash_t token);

int
compiled_read_notify(socket_t* sock, size_t size, compiled_notify_t* notify);
