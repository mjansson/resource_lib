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

typedef struct resource_base_t   resource_base_t;
typedef struct resource_config_t resource_config_t;
typedef struct resource_event_t  resource_event_t;

struct resource_config_t {
	bool enable_local_cache;
	bool enable_local_source;
	bool enable_remote_source;
};

#define RESOURCE_DECLARE_EVENT   \
	FOUNDATION_DECLARE_EVENT;    \
	uuid_t      uuid

struct resource_event_t {
	RESOURCE_DECLARE_EVENT;
};

#define RESOURCE_DECLARE_OBJECT   \
	FOUNDATION_DECLARE_OBJECT;    \
	uuid_t    uuid

FOUNDATION_ALIGNED_STRUCT(resource_base_t, 8) {
	RESOURCE_DECLARE_OBJECT;
};

