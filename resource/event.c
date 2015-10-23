/* event.c  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
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


#include <resource/event.h>
#include <resource/internal.h>

#include <foundation/foundation.h>

event_stream_t* _resource_event_stream = 0;

uuid_t
resource_event_uuid(const event_t* event) {
	return ((const resource_event_t*)event)->uuid;
}

void
resource_event_post(resource_event_id id, uuid_t uuid) {
	event_post(_resource_event_stream, id, sizeof(uuid_t), 0, &uuid, 0);
}

event_stream_t*
resource_event_stream(void) {
	return _resource_event_stream;
}

