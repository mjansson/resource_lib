/* bundle.h  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
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

/*! \file bundle.h
\brief Resource bundles

A resource bundle contains any number of resources of a single type.
The static and dynamic parts of the bundle resources are stored in separate files. */

#include <foundation/platform.h>

#include <resource/types.h>

/*! Load static part of bundle, creating and initializing all
resources stored in the bundle
\param uuid Bundle UUID
\return true if successful, false if error */
RESOURCE_API bool
resource_bundle_load(const uuid_t bundle);

/*! Open stream of dynamic part of bundle, creating and initializing all
resources stored in the bundle
\param uuid Bundle UUID
\return Stream for dynamic resource data, null if error */
RESOURCE_API stream_t*
resource_bundle_stream(const uuid_t bundle);
