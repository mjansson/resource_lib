/* import.h  -  Resource library  -  Public Domain  -  2014 Mattias Jansson
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

#include <resource/types.h>

RESOURCE_API string_const_t
resource_import_base_path(void);

RESOURCE_API void
resource_import_set_base_path(const char* path, size_t length);

RESOURCE_API bool
resource_import(const char* path, size_t length, const uuid_t uuid);

RESOURCE_API void
resource_import_register(resource_import_fn importer);

RESOURCE_API void
resource_import_register_path(const char* path, size_t length);

RESOURCE_API void
resource_import_unregister(resource_import_fn importer);

RESOURCE_API void
resource_import_unregister_path(const char* path, size_t length);

RESOURCE_API resource_signature_t
resource_import_lookup(const char* path, size_t length);

RESOURCE_API uuid_t
resource_import_map_store(const char* path, size_t length, uuid_t uuid, blake3_hash_t sighash);

RESOURCE_API bool
resource_import_map_purge(const char* path, size_t length);

RESOURCE_API bool
resource_autoimport(const uuid_t uuid);

RESOURCE_API bool
resource_autoimport_need_update(const uuid_t uuid, uint64_t platform);

RESOURCE_API void
resource_autoimport_watch(const char* path, size_t length);

RESOURCE_API void
resource_autoimport_unwatch(const char* path, size_t length);

RESOURCE_API void
resource_autoimport_clear(void);

RESOURCE_API string_t
resource_autoimport_reverse_lookup(const uuid_t uuid, char* buffer, size_t capacity);

/*! Handle foundation events from fs_event_stream event stream.
No other event types should be passed to this function.
\param event Foundation event */
RESOURCE_API void
resource_autoimport_event_handle(event_t* event);
