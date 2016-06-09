/* platform.h  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
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

#include <foundation/platform.h>

#include <resource/types.h>

/*! Compute a resource platform compact identifier from a platform
declaration structure
\param declaration Declaration
\return Compact identifier */
RESOURCE_API uint64_t
resource_platform(const resource_platform_t declaration);

/*! Decompose a resource platform compact identifier to a platform
declaration structure
\param platform Compact identifier
\return Declaration */
RESOURCE_API resource_platform_t
resource_platform_decompose(uint64_t platform);

/*! Check if platform is equal or more specific than reference
\param platform Platform
\param reference Reference
\return true if platform is equal or more specific than reference, false if not */
RESOURCE_API bool
resource_platform_is_equal_or_more_specific(uint64_t platform, uint64_t reference);

/*! Gradual reduction of platform specification
\param platform Current platform specifier
\param full_platform Original full platform specifier
\return Reduced platform specifier */
RESOURCE_API uint64_t
resource_platform_reduce(uint64_t platform, uint64_t full_platform);
