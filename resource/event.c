/* event.c  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
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


#include <resource/event.h>
#include <resource/import.h>
#include <resource/internal.h>

#include <foundation/foundation.h>

event_stream_t* _resource_event_stream = 0;

typedef struct {
	uuid_t uuid;
	hash_t token;
} resource_event_payload_t;

uuid_t
resource_event_uuid(const event_t* event) {
	return ((const resource_event_payload_t*)event->payload)->uuid;
}

hash_t
resource_event_token(const event_t* event) {
	return ((const resource_event_payload_t*)event->payload)->token;
}

void
resource_event_post(resource_event_id id, uuid_t uuid, hash_t token) {
	resource_event_payload_t payload = {uuid, token};
	event_post(_resource_event_stream, id, 0, 0, &payload, sizeof(payload));
}

event_stream_t*
resource_event_stream(void) {
	return _resource_event_stream;
}

void
resource_event_handle(event_t* event) {
	resource_autoimport_event_handle(event);
}
