/* protocol.c  -  Resource library  -  Public Domain  -  2016 Mattias Jansson / Rampant Pixels
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

#include "protocol.h"

#include <foundation/uuid.h>
#include <network/socket.h>

int
sourced_write_lookup_reply(socket_t* sock, uuid_t uuid, uint256_t hash) {
	sourced_lookup_result_t reply = {
		SOURCED_LOOKUP_RESULT,
		sizeof(uint32_t) + sizeof(uuid_t) + sizeof(uint256_t),
		!uuid_is_null(uuid) ? SOURCED_OK : SOURCED_FAILED,
		uuid,
		hash};
	return socket_write(sock, &reply, sizeof(reply)) == sizeof(reply) ? 0 : -1;
}
