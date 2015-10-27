/* main.c  -  Resource source test  -  Public Domain  -  2013 Mattias Jansson / Rampant Pixels
 *
 * This library provides a cross-platform foundation library in C11 providing basic support
 * data types and functions to write applications and games in a platform-independent fashion.
 * The latest source code is always available at
 *
 * https://github.com/rampantpixels/foundation_lib
 *
 * This library is put in the public domain; you can redistribute it and/or modify it without
 * any restrictions.
 */

#include <foundation/foundation.h>
#include <resource/resource.h>
#include <test/test.h>

static application_t
test_source_application(void) {
	application_t app;
	memset(&app, 0, sizeof(app));
	app.name = string_const(STRING_CONST("Resource source tests"));
	app.short_name = string_const(STRING_CONST("test_source"));
	app.config_dir = string_const(STRING_CONST("test_source"));
	app.flags = APPLICATION_UTILITY;
	return app;
}

static memory_system_t
test_source_memory_system(void) {
	return memory_system_malloc();
}

static foundation_config_t
test_source_config(void) {
	foundation_config_t config;
	memset(&config, 0, sizeof(config));
	return config;
}

static int
test_source_initialize(void) {
	resource_config_t config;
	memset(&config, 0, sizeof(config));
	config.enable_local_source = true;
	return resource_module_initialize(config);
}

static void
test_source_finalize(void) {
	resource_module_finalize();
}

DECLARE_TEST(source, set) {
	resource_source_t source;
	resource_source_initialize(&source);

#if RESOURCE_ENABLE_LOCAL_SOURCE

	hashmap_t* map = resource_source_map(&source);
	EXPECT_EQ(hashmap_lookup(map, HASH_TEST), nullptr);

	tick_t timestamp = time_system();
	resource_source_set(&source, timestamp, HASH_TEST, STRING_CONST("test"));
	EXPECT_NE(hashmap_lookup(map, HASH_TEST), nullptr);

#endif

	resource_source_finalize(&source);

	return 0;
}

static void
test_source_declare(void) {
	ADD_TEST(source, set);
}

static test_suite_t test_source_suite = {
	test_source_application,
	test_source_memory_system,
	test_source_config,
	test_source_declare,
	test_source_initialize,
	test_source_finalize
};

#if BUILD_MONOLITHIC

int
test_source_run(void);

int
test_source_run(void) {
	test_suite = test_source_suite;
	return test_run_all();
}

#else

test_suite_t
test_suite_define(void);

test_suite_t
test_suite_define(void) {
	return test_source_suite;
}

#endif
