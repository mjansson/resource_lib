/* import.c  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
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

#include <resource/import.h>
#include <resource/internal.h>

#include <foundation/foundation.h>

#if BUILD_ENABLE_LOCAL_SOURCE

static resource_import_fn* _resource_importers;

bool
resource_import(const char* path, size_t length) {
	size_t iimp, isize;
	bool was_imported = false;
	stream_t* stream = stream_open(path, length, STREAM_IN);
	for (iimp = 0, isize = array_size(_resource_importers); iimp != isize; ++iimp) {
		stream_seek(stream, 0, STREAM_SEEK_BEGIN);
		was_imported |= (_resource_importers[iimp](stream) == 0);
	}
	return was_imported;
}

void
resource_import_register(resource_import_fn importer) {
	array_push(_resource_importers, importer);
}

void
resource_import_unregister(resource_import_fn importer) {
	size_t iimp, isize;
	for (iimp = 0, isize = array_size(_resource_importers); iimp != isize; ++iimp) {
		if (_resource_importers[iimp] == importer) {
			array_erase(_resource_importers, iimp);
			return;
		}
	}
}

#endif
