/* platform.c  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
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

#include <resource/platform.h>
#include <resource/internal.h>

#include <foundation/foundation.h>

/* Platform field description

bits    description       variants
0-7     platform          254
8-15    architecture      254
16-23   unused
24-31   render api group  254
32-39   render api        254
40-47   unused
48-55   quality level     254
56-63   custom            254

*/

#define RESOURCE_PLATFORM_SHIFT 0ULL
#define RESOURCE_PLATFORM_MASK ((1ULL << 8ULL) - 1ULL)
#define RESOURCE_PLATFORM_INPLACE (RESOURCE_PLATFORM_MASK << RESOURCE_PLATFORM_SHIFT)

#define RESOURCE_ARCH_SHIFT (RESOURCE_PLATFORM_SHIFT + 8ULL)
#define RESOURCE_ARCH_MASK ((1ULL << 8ULL) - 1ULL)
#define RESOURCE_ARCH_INPLACE (RESOURCE_ARCH_MASK << RESOURCE_ARCH_SHIFT)

#define RESOURCE_RENDERAPIGROUP_SHIFT (RESOURCE_ARCH_SHIFT + 8ULL + 8ULL)
#define RESOURCE_RENDERAPIGROUP_MASK ((1ULL << 8ULL) - 1ULL)
#define RESOURCE_RENDERAPIGROUP_INPLACE (RESOURCE_RENDERAPIGROUP_MASK << RESOURCE_RENDERAPIGROUP_SHIFT)

#define RESOURCE_RENDERAPI_SHIFT (RESOURCE_RENDERAPIGROUP_SHIFT + 8ULL)
#define RESOURCE_RENDERAPI_MASK ((1ULL << 8ULL) - 1ULL)
#define RESOURCE_RENDERAPI_INPLACE (RESOURCE_RENDERAPI_MASK << RESOURCE_RENDERAPI_SHIFT)

#define RESOURCE_QUALITYLEVEL_SHIFT (RESOURCE_RENDERAPI_SHIFT + 8ULL + 8ULL)
#define RESOURCE_QUALITYLEVEL_MASK ((1ULL << 8ULL) - 1ULL)
#define RESOURCE_QUALITYLEVEL_INPLACE (RESOURCE_QUALITYLEVEL_MASK << RESOURCE_QUALITYLEVEL_SHIFT)

#define RESOURCE_CUSTOM_SHIFT (RESOURCE_QUALITYLEVEL_SHIFT + 8ULL)
#define RESOURCE_CUSTOM_MASK ((1ULL << 8ULL) - 1ULL)
#define RESOURCE_CUSTOM_INPLACE (RESOURCE_CUSTOM_MASK << RESOURCE_CUSTOM_SHIFT)

#define RESOURCE_PLATFORM_TO_BITS(platform) (((uint64_t)(platform)&RESOURCE_PLATFORM_MASK) << RESOURCE_PLATFORM_SHIFT)
#define RESOURCE_ARCH_TO_BITS(platform) (((uint64_t)(platform)&RESOURCE_ARCH_MASK) << RESOURCE_ARCH_SHIFT)
#define RESOURCE_RENDERAPIGROUP_TO_BITS(renderapi) \
	(((uint64_t)(renderapi)&RESOURCE_RENDERAPIGROUP_MASK) << RESOURCE_RENDERAPIGROUP_SHIFT)
#define RESOURCE_RENDERAPI_TO_BITS(renderapi) \
	(((uint64_t)(renderapi)&RESOURCE_RENDERAPI_MASK) << RESOURCE_RENDERAPI_SHIFT)
#define RESOURCE_QUALITYLEVEL_TO_BITS(qualitylevel) \
	(((uint64_t)(qualitylevel)&RESOURCE_QUALITYLEVEL_MASK) << RESOURCE_QUALITYLEVEL_SHIFT)
#define RESOURCE_CUSTOM_TO_BITS(custom) (((uint64_t)(custom)&RESOURCE_CUSTOM_MASK) << RESOURCE_CUSTOM_SHIFT)

#define RESOURCE_PLATFORM_FROM_BITS(bits) ((int)(((bits) >> RESOURCE_PLATFORM_SHIFT) & RESOURCE_PLATFORM_MASK))
#define RESOURCE_ARCH_FROM_BITS(bits) ((int)(((bits) >> RESOURCE_ARCH_SHIFT) & RESOURCE_ARCH_MASK))
#define RESOURCE_RENDERAPIGROUP_FROM_BITS(bits) \
	((int)(((bits) >> RESOURCE_RENDERAPIGROUP_SHIFT) & RESOURCE_RENDERAPIGROUP_MASK))
#define RESOURCE_RENDERAPI_FROM_BITS(bits) ((int)(((bits) >> RESOURCE_RENDERAPI_SHIFT) & RESOURCE_RENDERAPI_MASK))
#define RESOURCE_QUALITYLEVEL_FROM_BITS(bits) \
	((int)(((bits) >> RESOURCE_QUALITYLEVEL_SHIFT) & RESOURCE_QUALITYLEVEL_MASK))
#define RESOURCE_CUSTOM_FROM_BITS(bits) ((int)(((bits) >> RESOURCE_CUSTOM_SHIFT) & RESOURCE_CUSTOM_MASK))

#define RESOURCE_PLATFORM_EQUAL_OR_MORE_SPECIFIC(test, ref) \
	(!(ref & RESOURCE_PLATFORM_INPLACE) || ((test & RESOURCE_PLATFORM_INPLACE) == (ref & RESOURCE_PLATFORM_INPLACE)))
#define RESOURCE_ARCH_EQUAL_OR_MORE_SPECIFIC(test, ref) \
	(!(ref & RESOURCE_ARCH_INPLACE) || ((test & RESOURCE_ARCH_INPLACE) == (ref & RESOURCE_ARCH_INPLACE)))
#define RESOURCE_RENDERAPIGROUP_EQUAL_OR_MORE_SPECIFIC(test, ref) \
	(!(ref & RESOURCE_RENDERAPIGROUP_INPLACE) ||                  \
	 ((test & RESOURCE_RENDERAPIGROUP_INPLACE) == (ref & RESOURCE_RENDERAPIGROUP_INPLACE)))
#define RESOURCE_RENDERAPI_EQUAL_OR_MORE_SPECIFIC(test, ref) \
	(!(ref & RESOURCE_RENDERAPI_INPLACE) || ((test & RESOURCE_RENDERAPI_INPLACE) == (ref & RESOURCE_RENDERAPI_INPLACE)))
#define RESOURCE_QUALITYLEVEL_EQUAL_OR_MORE_SPECIFIC(test, ref) \
	(!(ref & RESOURCE_QUALITYLEVEL_INPLACE) ||                  \
	 ((test & RESOURCE_QUALITYLEVEL_INPLACE) == (ref & RESOURCE_QUALITYLEVEL_INPLACE)))
#define RESOURCE_CUSTOM_EQUAL_OR_MORE_SPECIFIC(test, ref) \
	(!(ref & RESOURCE_CUSTOM_INPLACE) || ((test & RESOURCE_CUSTOM_INPLACE) == (ref & RESOURCE_CUSTOM_INPLACE)))

uint64_t
resource_platform(const resource_platform_t declaration) {
	uint64_t compact = 0;
	if ((declaration.platform >= 0) && ((unsigned int)declaration.platform < RESOURCE_PLATFORM_MASK))
		compact |= RESOURCE_PLATFORM_TO_BITS(declaration.platform + 1);
	if ((declaration.arch >= 0) && ((unsigned int)declaration.arch < RESOURCE_ARCH_MASK))
		compact |= RESOURCE_ARCH_TO_BITS(declaration.arch + 1);
	if ((declaration.render_api_group >= 0) &&
	    ((unsigned int)declaration.render_api_group < RESOURCE_RENDERAPIGROUP_MASK))
		compact |= RESOURCE_RENDERAPIGROUP_TO_BITS(declaration.render_api_group + 1);
	if ((declaration.render_api >= 0) && ((unsigned int)declaration.render_api < RESOURCE_RENDERAPI_MASK))
		compact |= RESOURCE_RENDERAPI_TO_BITS(declaration.render_api + 1);
	if ((declaration.quality_level >= 0) && ((unsigned int)declaration.quality_level < RESOURCE_QUALITYLEVEL_MASK))
		compact |= RESOURCE_QUALITYLEVEL_TO_BITS(declaration.quality_level + 1);
	if ((declaration.custom >= 0) && ((unsigned int)declaration.custom < RESOURCE_CUSTOM_MASK))
		compact |= RESOURCE_CUSTOM_TO_BITS(declaration.custom + 1);
	return compact;
}

resource_platform_t
resource_platform_decompose(uint64_t platform) {
	resource_platform_t decl;
	memset(&decl, 0, sizeof(decl));
	decl.platform = RESOURCE_PLATFORM_FROM_BITS(platform) - 1;
	decl.arch = RESOURCE_ARCH_FROM_BITS(platform) - 1;
	decl.render_api_group = RESOURCE_RENDERAPIGROUP_FROM_BITS(platform) - 1;
	decl.render_api = RESOURCE_RENDERAPI_FROM_BITS(platform) - 1;
	decl.quality_level = RESOURCE_QUALITYLEVEL_FROM_BITS(platform) - 1;
	decl.custom = RESOURCE_CUSTOM_FROM_BITS(platform) - 1;
	return decl;
}

bool
resource_platform_is_equal_or_more_specific(uint64_t platform, uint64_t reference) {
	return (RESOURCE_PLATFORM_EQUAL_OR_MORE_SPECIFIC(platform, reference) &&
	        RESOURCE_ARCH_EQUAL_OR_MORE_SPECIFIC(platform, reference) &&
	        RESOURCE_RENDERAPIGROUP_EQUAL_OR_MORE_SPECIFIC(platform, reference) &&
	        RESOURCE_RENDERAPI_EQUAL_OR_MORE_SPECIFIC(platform, reference) &&
	        RESOURCE_QUALITYLEVEL_EQUAL_OR_MORE_SPECIFIC(platform, reference) &&
	        RESOURCE_CUSTOM_EQUAL_OR_MORE_SPECIFIC(platform, reference));
}

uint64_t
resource_platform_reduce(uint64_t platform, uint64_t full_platform) {
	if (platform & RESOURCE_CUSTOM_INPLACE)
		return platform & ~RESOURCE_CUSTOM_INPLACE;
	if (platform & RESOURCE_QUALITYLEVEL_INPLACE) {
		int level = RESOURCE_QUALITYLEVEL_FROM_BITS(platform) - 1;
		return (platform & ~RESOURCE_QUALITYLEVEL_INPLACE) | RESOURCE_QUALITYLEVEL_TO_BITS(level);
	}
	platform |= (full_platform & RESOURCE_CUSTOM_INPLACE) | (full_platform & RESOURCE_QUALITYLEVEL_INPLACE);

	if (platform & RESOURCE_RENDERAPI_INPLACE)
		return platform & ~RESOURCE_RENDERAPI_INPLACE;
	if (platform & RESOURCE_RENDERAPIGROUP_INPLACE)
		return platform & ~RESOURCE_RENDERAPIGROUP_INPLACE;
	platform |= (full_platform & RESOURCE_RENDERAPI_INPLACE) | (full_platform & RESOURCE_RENDERAPIGROUP_INPLACE);

	if (platform & RESOURCE_ARCH_INPLACE)
		return platform & ~RESOURCE_ARCH_INPLACE;
	if (platform & RESOURCE_PLATFORM_INPLACE)
		return platform & ~RESOURCE_PLATFORM_INPLACE;

	return 0;
}

static uint64_t
resource_platform_parse_token(const char* token, size_t length) {
	if (string_equal(token, length, STRING_CONST("ARM8_64")) || string_equal(token, length, STRING_CONST("arm8_64")) ||
	    string_equal(token, length, STRING_CONST("ARM64")) || string_equal(token, length, STRING_CONST("arm64")))
		return (uint64_t)(ARCHITECTURE_ARM8_64 + 1) << RESOURCE_ARCH_SHIFT;
	return 0;
}

uint64_t
resource_platform_parse(const char* decl, size_t length) {
	if (string_find_first_not_of(decl, length, STRING_CONST("0123456789abcdefABCDEF"), 0) == STRING_NPOS)
		return string_to_uint64(decl, length, true);

	uint64_t platform = 0;
	string_const_t tokens[32];
	size_t tokens_count =
	    string_explode(decl, length, STRING_CONST(" ,:;\t"), tokens, sizeof(tokens) / sizeof(tokens[0]), false);
	for (size_t itok = 0; itok < tokens_count; ++itok)
		platform |= resource_platform_parse_token(STRING_ARGS(tokens[itok]));

	return platform;
}
