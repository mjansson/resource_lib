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
#include <resource/source.h>
#include <resource/internal.h>

#include <foundation/foundation.h>

event_stream_t* _resource_event_stream = 0;

typedef struct {
	uuid_t uuid;
	uint64_t platform;
	hash_t token;
} resource_event_payload_t;

uuid_t
resource_event_uuid(const event_t* event) {
	return ((const resource_event_payload_t*)event->payload)->uuid;
}

uint64_t
resource_event_platform(const event_t* event) {
	return ((const resource_event_payload_t*)event->payload)->platform;
}

hash_t
resource_event_token(const event_t* event) {
	return ((const resource_event_payload_t*)event->payload)->token;
}

void
resource_event_post(resource_event_id id, uuid_t uuid, uint64_t platform, hash_t token) {
	resource_event_payload_t payload = {uuid, platform, token};
	event_post(_resource_event_stream, (int)id, 0, 0, &payload, sizeof(payload));
}

void
resource_event_post_depends(uuid_t uuid, uint64_t platform, hash_t token) {
	size_t num_reverse = resource_source_num_reverse_dependencies(uuid, platform);
#if BUILD_ENABLE_DEBUG_LOG
	char uuidbuf[33];
	string_t uuidstr = string_from_uuid(uuidbuf, sizeof(uuidbuf), uuid);
	log_debugf(HASH_RESOURCE, STRING_CONST("Dependency event trigger: %.*s platform 0x%" PRIx64 " -> %" PRIsize " reverse dependencies"),
	           STRING_FORMAT(uuidstr), platform, num_reverse);
#endif
	if (!num_reverse)
		return;

	resource_dependency_t basedeps[8];
	resource_dependency_t* reverse_deps = basedeps;
	if (num_reverse > sizeof(basedeps) / sizeof(basedeps[0]))
		reverse_deps = memory_allocate(HASH_RESOURCE, sizeof(resource_dependency_t) * num_reverse, 0, MEMORY_PERSISTENT);
	num_reverse = resource_source_reverse_dependencies(uuid, platform, reverse_deps, num_reverse);
	for (size_t idep = 0; idep < num_reverse; ++idep) {
#if BUILD_ENABLE_DEBUG_LOG
		char revuuidbuf[33];
		string_t revuuidstr = string_from_uuid(revuuidbuf, sizeof(revuuidbuf), reverse_deps[idep].uuid);
		log_debugf(HASH_RESOURCE, STRING_CONST("Dependency event trigger: %.*s -> reverse dependency %.*s platform 0x%" PRIx64),
		           STRING_FORMAT(uuidstr), STRING_FORMAT(revuuidstr), reverse_deps[idep].platform);
#endif
		resource_event_post(RESOURCEEVENT_DEPENDS, reverse_deps[idep].uuid, reverse_deps[idep].platform, token);
		resource_event_post_depends(reverse_deps[idep].uuid, platform/*reverse_deps[idep].platform*/, token);
	}
	if (reverse_deps != basedeps)
		memory_deallocate(reverse_deps);
}

event_stream_t*
resource_event_stream(void) {
	return _resource_event_stream;
}

void
resource_event_handle(event_t* event) {
	resource_autoimport_event_handle(event);
}
