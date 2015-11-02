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
	hashmap_fixed_t fixedmap;
	hashmap_t* map;
	resource_source_t source;

	map = (hashmap_t*)&fixedmap;

	hashmap_initialize(map, sizeof(fixedmap.bucket) / sizeof(fixedmap.bucket[0]), 8);
	resource_source_initialize(&source);

#if RESOURCE_ENABLE_LOCAL_SOURCE

	resource_change_t* change;
	resource_source_map(&source, 0, map);
	change = hashmap_lookup(map, HASH_TEST);
	EXPECT_PTREQ(change, nullptr);

	tick_t timestamp = time_system();
	resource_source_set(&source, timestamp, HASH_TEST, 0, STRING_CONST("test"));

	resource_source_map(&source, 0, map);
	change = hashmap_lookup(map, HASH_TEST);
	EXPECT_PTRNE(change, nullptr);
	EXPECT_TICKEQ(change->timestamp, timestamp);
	EXPECT_HASHEQ(change->hash, HASH_TEST);
	EXPECT_CONSTSTRINGEQ(change->value, string_const(STRING_CONST("test")));

	thread_sleep(100);

	resource_source_set(&source, time_system(), HASH_TEST, 0, STRING_CONST("foobar"));

	resource_source_map(&source, 0, map);
	change = hashmap_lookup(map, HASH_TEST);
	EXPECT_PTRNE(change, nullptr);
	EXPECT_TICKGT(change->timestamp, timestamp);
	EXPECT_HASHEQ(change->hash, HASH_TEST);
	EXPECT_CONSTSTRINGEQ(change->value, string_const(STRING_CONST("foobar")));

	size_t iloop, lsize;
	char buffer[1024];
	for (iloop = 0, lsize = 4096; iloop < lsize; ++iloop) {
		hash_t hash = random64();
		timestamp = time_system();
		string_const_t rndstr = string_const(buffer, random32_range(0, sizeof(buffer)));
		resource_source_set(&source, timestamp, hash, 0, STRING_ARGS(rndstr));

		resource_source_map(&source, 0, map);
		change = hashmap_lookup(map, hash);
		EXPECT_PTRNE(change, nullptr);
		EXPECT_TICKEQ(change->timestamp, timestamp);
		EXPECT_HASHEQ(change->hash, hash);
	}

	size_t allocated = 0;
	size_t used = 0;
	resource_change_block_t* block = &source.first;
	while (block) {
		resource_change_data_t* data = block->current_data;
		while (data) {
			allocated += data->size;
			used += data->used;
			data = data->next;
		}
		block = block->next;
	}

	//log_infof(HASH_TEST, STRING_CONST("Used %" PRIsize "/%" PRIsize " (%.0" PRIREAL "%%)"),
	//          used, allocated, REAL_C(100.0) * ((real)used / (real)allocated));
	EXPECT_REALGT((real)used / (real)allocated, REAL_C(0.75));

#endif

	resource_source_finalize(&source);
	hashmap_finalize(map);

	return 0;
}

DECLARE_TEST(source, unset) {
	hashmap_fixed_t fixedmap;
	hashmap_t* map;
	resource_source_t source;

	map = (hashmap_t*)&fixedmap;

	hashmap_initialize(map, sizeof(fixedmap.bucket) / sizeof(fixedmap.bucket[0]), 8);
	resource_source_initialize(&source);

#if RESOURCE_ENABLE_LOCAL_SOURCE

	//Verify platform handling and set/unset logic
	//Define four platforms, A-D
	//A-C are specializations in increasing order
	//D is a separate unrelated platform
	//In reverse order:
	//  Set single key values for default platform and A-D
	//  then unset for platform B and C
	//  Set new key value for default platform and C
	//  Set new key value for platform D
	//Verify map result for default platform and A-D
	resource_source_initialize(&source);

	uint64_t platformA = 0x0001;
	uint64_t platformB = 0x0011;
	uint64_t platformC = 0x0101;
	uint64_t platformD = 0x1000;

	hash_t keyOne = HASH_TEST;
	hash_t keyTwo = HASH_RESOURCE;
	hash_t keyThree = 0;

	tick_t tick = time_system();

	resource_source_set(&source, tick + 4, keyThree, platformD, STRING_CONST("dThree"));

	resource_source_set(&source, tick + 3, keyTwo, platformC, STRING_CONST("cTwo"));
	resource_source_set(&source, tick + 3, keyTwo, 0, STRING_CONST("defaultTwo"));

	resource_source_unset(&source, tick + 2, keyOne, platformB);
	resource_source_unset(&source, tick + 1, keyOne, platformC);

	resource_source_set(&source, tick, keyOne, platformD, STRING_CONST("d"));
	resource_source_set(&source, tick, keyOne, platformC, STRING_CONST("c"));
	resource_source_set(&source, tick, keyOne, platformB, STRING_CONST("b"));
	resource_source_set(&source, tick, keyOne, platformA, STRING_CONST("a"));
	resource_source_set(&source, tick, keyOne, 0, STRING_CONST("default"));

	resource_change_t* change;
	resource_source_map(&source, 0, map);
	change = hashmap_lookup(map, keyOne);
	EXPECT_PTRNE(change, nullptr);
	EXPECT_TICKEQ(change->timestamp, tick);
	EXPECT_HASHEQ(change->hash, keyOne);
	EXPECT_CONSTSTRINGEQ(change->value, string_const(STRING_CONST("default")));
	change = hashmap_lookup(map, keyTwo);
	EXPECT_PTRNE(change, nullptr);
	EXPECT_TICKEQ(change->timestamp, tick + 3);
	EXPECT_EQ(change->hash, keyTwo);
	EXPECT_CONSTSTRINGEQ(change->value, string_const(STRING_CONST("defaultTwo")));
	change = hashmap_lookup(map, keyThree);
	EXPECT_PTREQ(change, nullptr);

	resource_source_map(&source, platformA, map);
	change = hashmap_lookup(map, keyOne);
	EXPECT_PTRNE(change, nullptr);
	EXPECT_TICKEQ(change->timestamp, tick);
	EXPECT_HASHEQ(change->hash, keyOne);
	EXPECT_CONSTSTRINGEQ(change->value, string_const(STRING_CONST("a")));
	change = hashmap_lookup(map, keyTwo);
	EXPECT_PTRNE(change, nullptr);
	EXPECT_TICKEQ(change->timestamp, tick + 3);
	EXPECT_HASHEQ(change->hash, keyTwo);
	EXPECT_CONSTSTRINGEQ(change->value, string_const(STRING_CONST("defaultTwo")));
	change = hashmap_lookup(map, keyThree);
	EXPECT_PTREQ(change, nullptr);

	resource_source_map(&source, platformB, map);
	change = hashmap_lookup(map, keyOne);
	EXPECT_PTRNE(change, nullptr);
	EXPECT_TICKEQ(change->timestamp, tick);
	EXPECT_HASHEQ(change->hash, keyOne);
	EXPECT_CONSTSTRINGEQ(change->value, string_const(STRING_CONST("a")));
	change = hashmap_lookup(map, keyTwo);
	EXPECT_PTRNE(change, nullptr);
	EXPECT_TICKEQ(change->timestamp, tick + 3);
	EXPECT_HASHEQ(change->hash, keyTwo);
	EXPECT_CONSTSTRINGEQ(change->value, string_const(STRING_CONST("defaultTwo")));
	change = hashmap_lookup(map, keyThree);
	EXPECT_PTREQ(change, nullptr);

	resource_source_map(&source, platformC, map);
	change = hashmap_lookup(map, keyOne);
	EXPECT_PTRNE(change, nullptr);
	EXPECT_TICKEQ(change->timestamp, tick);
	EXPECT_HASHEQ(change->hash, keyOne);
	EXPECT_CONSTSTRINGEQ(change->value, string_const(STRING_CONST("a")));
	change = hashmap_lookup(map, keyTwo);
	EXPECT_PTRNE(change, nullptr);
	EXPECT_TICKEQ(change->timestamp, tick + 3);
	EXPECT_HASHEQ(change->hash, keyTwo);
	EXPECT_CONSTSTRINGEQ(change->value, string_const(STRING_CONST("cTwo")));
	change = hashmap_lookup(map, keyThree);
	EXPECT_PTREQ(change, nullptr);

	resource_source_map(&source, platformD, map);
	change = hashmap_lookup(map, keyOne);
	EXPECT_PTRNE(change, nullptr);
	EXPECT_TICKEQ(change->timestamp, tick);
	EXPECT_HASHEQ(change->hash, keyOne);
	EXPECT_CONSTSTRINGEQ(change->value, string_const(STRING_CONST("d")));
	change = hashmap_lookup(map, keyTwo);
	EXPECT_PTRNE(change, nullptr);
	EXPECT_TICKEQ(change->timestamp, tick + 3);
	EXPECT_HASHEQ(change->hash, keyTwo);
	EXPECT_CONSTSTRINGEQ(change->value, string_const(STRING_CONST("defaultTwo")));
	change = hashmap_lookup(map, keyThree);
	EXPECT_PTRNE(change, nullptr);
	EXPECT_TICKEQ(change->timestamp, tick + 4);
	EXPECT_HASHEQ(change->hash, keyThree);
	EXPECT_CONSTSTRINGEQ(change->value, string_const(STRING_CONST("dThree")));

#endif

	resource_source_finalize(&source);
	hashmap_finalize(map);

	return 0;
}

DECLARE_TEST(source, collapse) {
	hashmap_fixed_t fixedmap;
	hashmap_t* map;
	resource_source_t source;

	map = (hashmap_t*)&fixedmap;

	hashmap_initialize(map, sizeof(fixedmap.bucket) / sizeof(fixedmap.bucket[0]), 8);
	resource_source_initialize(&source);

#if RESOURCE_ENABLE_LOCAL_SOURCE

	const hash_t keys[8] = {
		0, HASH_TEST, HASH_RESOURCE, HASH_DEBUG,
		HASH_NONE, HASH_TRUE, HASH_FALSE, HASH_SYSTEM
	};
	const uint64_t platforms[4] = {
		0, 1, 3, 7
	};

	resource_change_t expected[4][8];
	resource_change_t* change;
	size_t iloop, lsize;
	char buffer[1024];
	for (iloop = 0, lsize = 4096; iloop < lsize; ++iloop) {
		size_t ichg, chgsize;
		for (ichg = 0, chgsize = 1024; ichg < chgsize; ++ichg) {
			hash_t hash = keys[random32_range(0, 8)];
			hash_t platform = platforms[random32_range(0, 4)];
			tick_t timestamp = time_system();
			if (random32_range(0, 100) > 75) {
				resource_source_unset(&source, timestamp, hash, platform);
			}
			else {
				string_const_t rndstr = string_const(buffer, random32_range(0, sizeof(buffer)));
				resource_source_set(&source, timestamp, hash, platform, STRING_ARGS(rndstr));
			}
		}

		memset(expected, 0, sizeof(expected));

		size_t iplat, platsize;
		size_t ihash, hashsize;
		for (iplat = 0, platsize = 4; iplat < platsize; ++iplat) {
			resource_source_map(&source, platforms[iplat], map);
			for (ihash = 0, hashsize = 8; ihash < hashsize; ++ihash) {
				change = hashmap_lookup(map, keys[ihash]);
				if (change) {
					EXPECT_HASHEQ(change->hash, keys[ihash]);
					if (change->value.str) {
						expected[iplat][ihash] = *change;
						expected[iplat][ihash].value = string_to_const(string_clone(STRING_ARGS(change->value)));
					}
				}
			}
		}

		resource_source_collapse_history(&source);

		for (iplat = 0, platsize = 4; iplat < platsize; ++iplat) {
			resource_source_map(&source, platforms[iplat], map);
			for (ihash = 0, hashsize = 8; ihash < hashsize; ++ihash) {
				change = hashmap_lookup(map, keys[ihash]);
				if (expected[iplat][ihash].value.str) {
					EXPECT_PTRNE(change, nullptr);
					EXPECT_TICKEQ(change->timestamp, expected[iplat][ihash].timestamp);
					EXPECT_HASHEQ(change->hash, expected[iplat][ihash].hash);
					EXPECT_TRUE(resource_platform_is_more_specific(platforms[iplat], change->platform));
					EXPECT_CONSTSTRINGEQ(change->value, expected[iplat][ihash].value);
					string_deallocate((char*)expected[iplat][ihash].value.str);
				}
				else {
					EXPECT_PTREQ(change, nullptr);
				}
			}
		}
	}

#endif

	resource_source_finalize(&source);
	hashmap_finalize(map);

	return 0;
}

static void
test_source_declare(void) {
	ADD_TEST(source, set);
	ADD_TEST(source, unset);
	ADD_TEST(source, collapse);
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
