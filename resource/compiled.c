/* compiled.c  -  Resource library  -  Public Domain  -  2016 Mattias Jansson / Rampant Pixels
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

#include <resource/resource.h>
#include <resource/compiled.h>

#include <foundation/foundation.h>
#include <network/socket.h>

int
compiled_write_open_static(socket_t* sock, uuid_t uuid, uint64_t platform) {
	compiled_open_static_t msg = {
		COMPILED_OPEN_STATIC,
		(uint32_t)sizeof(uuid_t) + sizeof(uint64_t),
		uuid,
		platform
	};

	if (socket_write(sock, &msg, sizeof(msg)) == sizeof(msg))
		return 0;
	return -1;
}

int
compiled_write_open_dynamic(socket_t* sock, uuid_t uuid, uint64_t platform) {
	compiled_open_dynamic_t msg = {
		COMPILED_OPEN_DYNAMIC,
		(uint32_t)sizeof(uuid_t) + sizeof(uint64_t),
		uuid,
		platform
	};

	if (socket_write(sock, &msg, sizeof(msg)) == sizeof(msg))
		return 0;
	return -1;
}

int
compiled_read_open_static_reply(socket_t* sock, size_t size, compiled_open_result_t* result) {
	if ((size == sizeof(compiled_open_result_t)) && 
		(socket_read(sock, result, size) == size))
		return 0;
	return -1;
}

int
compiled_read_open_dynamic_reply(socket_t* sock, size_t size, compiled_open_result_t* result) {
	if ((size == sizeof(compiled_open_result_t)) && 
		(socket_read(sock, result, size) == size))
		return 0;
	return -1;
}

int
compiled_write_open_static_reply(socket_t* sock, bool success, size_t size) {
	compiled_message_t msg = {
		COMPILED_OPEN_STATIC_RESULT,
		(uint32_t)sizeof(compiled_open_result_t)
	};
	compiled_open_result_t data = {
		success ? COMPILED_OK : COMPILED_FAILED,
		0,
		size
	};
	if (socket_write(sock, &msg, sizeof(msg)) == sizeof(msg))
		if (socket_write(sock, &data, sizeof(data)) == sizeof(data))
			return 0;
	return -1;
}

int
compiled_write_open_dynamic_reply(socket_t* sock, bool success, size_t size) {
	compiled_message_t msg = {
		COMPILED_OPEN_DYNAMIC_RESULT,
		(uint32_t)sizeof(compiled_open_result_t)
	};
	compiled_open_result_t data = {
		success ? COMPILED_OK : COMPILED_FAILED,
		0,
		size
	};
	if (socket_write(sock, &msg, sizeof(msg)) == sizeof(msg))
		if (socket_write(sock, &data, sizeof(data)) == sizeof(data))
			return 0;
	return -1;
}

int
compiled_write_notify(socket_t* sock, compiled_message_id id, uuid_t uuid, hash_t token) {
	compiled_notify_t msg = {
		id,
		(uint32_t)(sizeof(uuid_t) + sizeof(uint64_t)),
		uuid,
		token
	};
	if (socket_write(sock, &msg, sizeof(msg)) == sizeof(msg))
		return 0;
	return -1;
}

int
compiled_read_notify(socket_t* sock, size_t size, compiled_notify_t* notify) {
	const size_t expected = sizeof(uuid_t) + sizeof(uint64_t);
	if (size != expected)
		return -1;
	if (socket_read(sock, &notify->uuid, expected) == expected)
		return 0;
	return -1;
}
