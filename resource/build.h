/* build.h  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
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

#if defined(RESOURCE_COMPILE) && RESOURCE_COMPILE
#  ifdef __cplusplus
#  define RESOURCE_EXTERN extern "C"
#  define RESOURCE_API extern "C"
#  else
#  define RESOURCE_EXTERN extern
#  define RESOURCE_API extern
#  endif
#else
#  ifdef __cplusplus
#  define RESOURCE_EXTERN extern "C"
#  define RESOURCE_API extern "C"
#  else
#  define RESOURCE_EXTERN extern
#  define RESOURCE_API extern
#  endif
#endif

#if !FOUNDATION_PLATFORM_FAMILY_CONSOLE && !BUILD_DEPLOY
#  define RESOURCE_ENABLE_LOCAL_SOURCE 1
#else
#  define RESOURCE_ENABLE_LOCAL_SOURCE 0
#endif

#if FOUNDATION_PLATFORM_FAMILY_CONSOLE || BUILD_DEPLOY || RESOURCE_ENABLE_LOCAL_SOURCE
#  define RESOURCE_ENABLE_LOCAL_CACHE 1
#else
#  define RESOURCE_ENABLE_LOCAL_CACHE 0
#endif

#if !FOUNDATION_PLATFORM_FAMILY_CONSOLE && !BUILD_DEPLOY
#  define RESOURCE_ENABLE_REMOTE_SOURCED 1
#else
#  define RESOURCE_ENABLE_REMOTE_SOURCED 0
#endif

#if !BUILD_DEPLOY || !RESOURCE_ENABLE_LOCAL_CACHE
#  define RESOURCE_ENABLE_REMOTE_COMPILED 1
#else
#  define RESOURCE_ENABLE_REMOTE_COMPILED 0
#endif

/*! Number of changes in a change block */
#define RESOURCE_CHANGE_BLOCK_SIZE 32

/*! Initial size of change block string data */
#define RESOURCE_CHANGE_BLOCK_DATA_SIZE 1024

/*! Name of import map files */
#define RESOURCE_IMPORT_MAP "import.map"

//Make sure we have at least one way of loading resources
#if !RESOURCE_ENABLE_REMOTE_COMPILED && !RESOURCE_ENABLE_LOCAL_CACHE
#  error Invalid build configuration, no way of loading resources
#endif
