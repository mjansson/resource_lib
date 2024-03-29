/* main.c  -  Resource source test  -  Public Domain  -  2013 Mattias Jansson
 *
 * This library provides a cross-platform foundation library in C11 providing basic support
 * data types and functions to write applications and games in a platform-independent fashion.
 * The latest source code is always available at
 *
 * https://github.com/mjansson/foundation_lib
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
	app.company = string_const(STRING_CONST(""));
	app.flags = APPLICATION_UTILITY;
	app.exception_handler = test_exception_handler;
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
	config.enable_local_cache = true;
	return resource_module_initialize(config);
}

static void
test_source_finalize(void) {
	resource_module_finalize();
}

static void
test_source_event(event_t* event) {
	resource_event_handle(event);
}

DECLARE_TEST(source, set) {
	hashmap_fixed_t fixedmap;
	hashmap_t* map;
	resource_source_t source;

	map = (hashmap_t*)&fixedmap;

	hashmap_initialize(map, sizeof(fixedmap.bucket) / sizeof(fixedmap.bucket[0]), 8);
	resource_source_initialize(&source);

	EXPECT_PTREQ(source.first.next, nullptr);

	resource_change_t* change;
	resource_source_map(&source, 0, map);
	change = hashmap_lookup(map, HASH_TEST);
	EXPECT_PTREQ(change, nullptr);

	tick_t timestamp = time_system();
	resource_source_set(&source, timestamp, HASH_TEST, 0, STRING_CONST("test"));

	resource_source_map(&source, 0, map);
	change = hashmap_lookup(map, HASH_TEST);
#if RESOURCE_ENABLE_LOCAL_SOURCE
	EXPECT_PTRNE(change, nullptr);
	EXPECT_TICKEQ(change->timestamp, timestamp);
	EXPECT_HASHEQ(change->hash, HASH_TEST);
	EXPECT_CONSTSTRINGEQ(change->value.value, string_const(STRING_CONST("test")));
#endif

	thread_sleep(100);

	resource_source_set(&source, time_system(), HASH_TEST, 0, STRING_CONST("foobar"));

	resource_source_map(&source, 0, map);
	change = hashmap_lookup(map, HASH_TEST);
#if RESOURCE_ENABLE_LOCAL_SOURCE
	EXPECT_PTRNE(change, nullptr);
	EXPECT_TICKGT(change->timestamp, timestamp);
	EXPECT_HASHEQ(change->hash, HASH_TEST);
	EXPECT_CONSTSTRINGEQ(change->value.value, string_const(STRING_CONST("foobar")));
#endif

	size_t iloop, lsize;
	char buffer[1024];
	for (iloop = 0; iloop < sizeof(buffer); ++iloop)
		buffer[iloop] = (char)random32_range('a', 'z' + 1);
	timestamp = time_system();
	for (iloop = 0, lsize = 4096; iloop < lsize; ++iloop) {
		hash_t hash = random64();
		string_const_t rndstr = string_const(buffer, random32_range(0, sizeof(buffer)));
		resource_source_set(&source, ++timestamp, hash, 0, STRING_ARGS(rndstr));

		resource_source_map(&source, 0, map);
		change = hashmap_lookup(map, hash);
#if RESOURCE_ENABLE_LOCAL_SOURCE
		EXPECT_PTRNE(change, nullptr);
		EXPECT_TICKEQ(change->timestamp, timestamp);
		EXPECT_HASHEQ(change->hash, hash);
		EXPECT_UINTEQ(change->flags, RESOURCE_SOURCEFLAG_VALUE);
		EXPECT_CONSTSTRINGEQ(change->value.value, rndstr);
#else
		FOUNDATION_UNUSED(rndstr);
#endif
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

#if RESOURCE_ENABLE_LOCAL_SOURCE
	// log_infof(HASH_TEST, STRING_CONST("Used %" PRIsize "/%" PRIsize " (%.0" PRIREAL "%%)"),
	//          used, allocated, REAL_C(100.0) * ((real)used / (real)allocated));
	EXPECT_REALGT((real)used / (real)allocated, REAL_C(0.7));
#endif

	resource_source_finalize(&source);
	hashmap_finalize(map);

	return 0;
}

DECLARE_TEST(source, unset) {
	hashmap_fixed_t fixedmap;
	hashmap_t* map;
	resource_source_t source;
	resource_change_t* change;

	map = (hashmap_t*)&fixedmap;

	hashmap_initialize(map, sizeof(fixedmap.bucket) / sizeof(fixedmap.bucket[0]), 8);
	resource_source_initialize(&source);

	// Verify platform handling and set/unset logic
	// Define four platforms, A-D
	// A-C are specializations in increasing order
	// D is a separate unrelated platform
	// In reverse order:
	//  Set single key values for default platform and A-D
	//  then unset for platform B and C
	//  Set new key value for default platform and C
	//  Set new key value for platform D
	// Verify map result for default platform and A-D
	resource_source_initialize(&source);

	uint64_t platformA = resource_platform((resource_platform_t){1, -1, -1, -1, -1, -1});
	uint64_t platformB = resource_platform((resource_platform_t){1, 2, -1, -1, -1, -1});
	uint64_t platformC = resource_platform((resource_platform_t){1, 2, 3, -1, -1, -1});
	uint64_t platformD = resource_platform((resource_platform_t){1, -1, -1, -1, 4, -1});

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

	resource_source_set(&source, tick - 1, keyTwo, 0, STRING_CONST("oldTwo"));

	resource_source_map(&source, 0, map);
#if RESOURCE_ENABLE_LOCAL_SOURCE
	change = hashmap_lookup(map, keyOne);
	EXPECT_PTRNE(change, nullptr);
	EXPECT_TICKEQ(change->timestamp, tick);
	EXPECT_HASHEQ(change->hash, keyOne);
	EXPECT_CONSTSTRINGEQ(change->value.value, string_const(STRING_CONST("default")));
	change = hashmap_lookup(map, keyTwo);
	EXPECT_PTRNE(change, nullptr);
	EXPECT_TICKEQ(change->timestamp, tick + 3);
	EXPECT_EQ(change->hash, keyTwo);
	EXPECT_CONSTSTRINGEQ(change->value.value, string_const(STRING_CONST("defaultTwo")));
#endif
	change = hashmap_lookup(map, keyThree);
	EXPECT_PTREQ(change, nullptr);

	resource_source_map(&source, platformA, map);
#if RESOURCE_ENABLE_LOCAL_SOURCE
	change = hashmap_lookup(map, keyOne);
	EXPECT_PTRNE(change, nullptr);
	EXPECT_TICKEQ(change->timestamp, tick);
	EXPECT_HASHEQ(change->hash, keyOne);
	EXPECT_CONSTSTRINGEQ(change->value.value, string_const(STRING_CONST("a")));
	change = hashmap_lookup(map, keyTwo);
	EXPECT_PTRNE(change, nullptr);
	EXPECT_TICKEQ(change->timestamp, tick + 3);
	EXPECT_HASHEQ(change->hash, keyTwo);
	EXPECT_CONSTSTRINGEQ(change->value.value, string_const(STRING_CONST("defaultTwo")));
#endif
	change = hashmap_lookup(map, keyThree);
	EXPECT_PTREQ(change, nullptr);

	resource_source_map(&source, platformB, map);
#if RESOURCE_ENABLE_LOCAL_SOURCE
	change = hashmap_lookup(map, keyOne);
	EXPECT_PTRNE(change, nullptr);
	EXPECT_TICKEQ(change->timestamp, tick);
	EXPECT_HASHEQ(change->hash, keyOne);
	EXPECT_CONSTSTRINGEQ(change->value.value, string_const(STRING_CONST("a")));
	change = hashmap_lookup(map, keyTwo);
	EXPECT_PTRNE(change, nullptr);
	EXPECT_TICKEQ(change->timestamp, tick + 3);
	EXPECT_HASHEQ(change->hash, keyTwo);
	EXPECT_CONSTSTRINGEQ(change->value.value, string_const(STRING_CONST("defaultTwo")));
#endif
	change = hashmap_lookup(map, keyThree);
	EXPECT_PTREQ(change, nullptr);

	resource_source_map(&source, platformC, map);
#if RESOURCE_ENABLE_LOCAL_SOURCE
	change = hashmap_lookup(map, keyOne);
	EXPECT_PTRNE(change, nullptr);
	EXPECT_TICKEQ(change->timestamp, tick);
	EXPECT_HASHEQ(change->hash, keyOne);
	EXPECT_CONSTSTRINGEQ(change->value.value, string_const(STRING_CONST("a")));
	change = hashmap_lookup(map, keyTwo);
	EXPECT_PTRNE(change, nullptr);
	EXPECT_TICKEQ(change->timestamp, tick + 3);
	EXPECT_HASHEQ(change->hash, keyTwo);
	EXPECT_CONSTSTRINGEQ(change->value.value, string_const(STRING_CONST("cTwo")));
#endif
	change = hashmap_lookup(map, keyThree);
	EXPECT_PTREQ(change, nullptr);

	resource_source_map(&source, platformD, map);
#if RESOURCE_ENABLE_LOCAL_SOURCE
	change = hashmap_lookup(map, keyOne);
	EXPECT_PTRNE(change, nullptr);
	EXPECT_TICKEQ(change->timestamp, tick);
	EXPECT_HASHEQ(change->hash, keyOne);
	EXPECT_CONSTSTRINGEQ(change->value.value, string_const(STRING_CONST("d")));
	change = hashmap_lookup(map, keyTwo);
	EXPECT_PTRNE(change, nullptr);
	EXPECT_TICKEQ(change->timestamp, tick + 3);
	EXPECT_HASHEQ(change->hash, keyTwo);
	EXPECT_CONSTSTRINGEQ(change->value.value, string_const(STRING_CONST("defaultTwo")));
	change = hashmap_lookup(map, keyThree);
	EXPECT_PTRNE(change, nullptr);
	EXPECT_TICKEQ(change->timestamp, tick + 4);
	EXPECT_HASHEQ(change->hash, keyThree);
	EXPECT_CONSTSTRINGEQ(change->value.value, string_const(STRING_CONST("dThree")));
#endif

	resource_source_finalize(&source);
	hashmap_finalize(map);

	return 0;
}

static resource_change_t*
resource_unique_set_per_platform(resource_change_t* change, resource_change_t* best, void* data) {
	int* result = (int*)data;
	if (change && best) {
		*result = (change->platform == best->platform) ? -1 : 0;
		if (*result < 0) {
			log_infof(HASH_TEST, STRING_CONST("%" PRIfixPTR " : %" PRIfixPTR), (uintptr_t)change, (uintptr_t)best);
			log_infof(HASH_TEST, STRING_CONST("%" PRIhash " : %" PRIhash), change->hash, best->hash);
			log_infof(HASH_TEST, STRING_CONST("%" PRItick " : %" PRItick), change->timestamp, best->timestamp);
			log_infof(HASH_TEST, STRING_CONST("%u : %u"), change->flags, best->flags);
			log_infof(HASH_TEST, STRING_CONST("%" PRIsize " : %" PRIsize), change->value.value.length,
			          best->value.value.length);
		}
		EXPECT_TYPENE(change->platform, best->platform, uint64_t, PRIx64);
	}
	return change;
}

DECLARE_TEST(source, collapse) {
	hashmap_fixed_t fixedmap;
	hashmap_t* map;
	resource_source_t source;

	map = (hashmap_t*)&fixedmap;

	hashmap_initialize(map, sizeof(fixedmap.bucket) / sizeof(fixedmap.bucket[0]), 8);
	resource_source_initialize(&source);

	const hash_t keys[8] = {0, HASH_TEST, HASH_RESOURCE, HASH_DEBUG, HASH_NONE, HASH_TRUE, HASH_FALSE, HASH_SYSTEM};
	const uint64_t platforms[4] = {resource_platform((resource_platform_t){-1, -1, -1, -1, -1, -1}),
	                               resource_platform((resource_platform_t){1, -1, -1, -1, -1, -1}),
	                               resource_platform((resource_platform_t){1, 2, -1, -1, -1, -1}),
	                               resource_platform((resource_platform_t){1, 2, 3, 4, -1, -1})};

	struct resource_expect_change_t {
		resource_change_t change;
		string_t value;
	};

	struct resource_expect_change_t expected[4][8];
	resource_change_t* change;
	size_t iloop, lsize;
	char buffer[1024];
	tick_t timestamp = time_system();
	for (iloop = 0; iloop < sizeof(buffer); ++iloop)
		buffer[iloop] = (char)random32_range('a', 'z' + 1);
	for (iloop = 0, lsize = 4096; iloop < lsize; ++iloop) {
		size_t ichg, chgsize;
		for (ichg = 0, chgsize = 1024; ichg < chgsize; ++ichg) {
			hash_t hash = keys[random32_range(0, 8)];
			hash_t platform = platforms[random32_range(0, 4)];
			if (random32_range(0, 100) > 75) {
				resource_source_unset(&source, timestamp++, hash, platform);
			} else {
				resource_source_set(&source, timestamp++, hash, platform, buffer, random32_range(10, sizeof(buffer)));
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
					if (change->flags == RESOURCE_SOURCEFLAG_VALUE) {
						EXPECT_CONSTSTRINGEQ(string_const(change->value.value.str, 10), string_const(buffer, 10));
						string_t clonestr = string_clone(STRING_ARGS(change->value.value));
						expected[iplat][ihash].change = *change;
						expected[iplat][ihash].value = clonestr;
						expected[iplat][ihash].change.value.value = string_to_const(clonestr);
					}
				}
			}
		}

		// When mapping without all timestamps there should only be one change for each platform for each key
		// even before history collapse (it is a local map collapse)
		int result = 0;
		resource_source_map_all(&source, map, false);
		resource_source_map_reduce(&source, map, &result, resource_unique_set_per_platform);
		EXPECT_EQ(result, 0);

		resource_source_collapse_history(&source);

		// After a history collapse a map of all timestamps should only be one change for each platform for each key
		resource_source_map_all(&source, map, true);
		resource_source_map_reduce(&source, map, &result, resource_unique_set_per_platform);
		EXPECT_EQ(result, 0);

		for (iplat = 0, platsize = 4; iplat < platsize; ++iplat) {
			resource_source_map(&source, platforms[iplat], map);
			for (ihash = 0, hashsize = 8; ihash < hashsize; ++ihash) {
				change = hashmap_lookup(map, keys[ihash]);
				if (expected[iplat][ihash].change.flags == RESOURCE_SOURCEFLAG_VALUE) {
#if RESOURCE_ENABLE_LOCAL_SOURCE
					EXPECT_PTRNE(change, nullptr);
					EXPECT_TICKEQ(change->timestamp, expected[iplat][ihash].change.timestamp);
					EXPECT_HASHEQ(change->hash, expected[iplat][ihash].change.hash);
					EXPECT_TRUE(resource_platform_is_equal_or_more_specific(platforms[iplat], change->platform));
					EXPECT_CONSTSTRINGEQ(change->value.value, expected[iplat][ihash].change.value.value);
#endif
					string_deallocate((char*)expected[iplat][ihash].value.str);
				} else {
					EXPECT_PTREQ(change, nullptr);
				}
			}
		}
	}

	resource_source_finalize(&source);
	hashmap_finalize(map);

	return 0;
}

DECLARE_TEST(source, blob) {
	hashmap_fixed_t fixedmap;
	hashmap_t* map;
	resource_source_t source;
	uuid_t uuid;
	hash_t checksum;
	size_t size;
	char data[1024];
	tick_t timestamp;
	uint64_t platform;
	size_t iidx, isize;
	string_const_t path;
	string_t* files;
	resource_change_t* change;

	map = (hashmap_t*)&fixedmap;

	hashmap_initialize(map, sizeof(fixedmap.bucket) / sizeof(fixedmap.bucket[0]), 8);
	resource_source_initialize(&source);

	path = environment_temporary_directory();
	resource_source_set_path(STRING_ARGS(path));

	uuid = uuid_generate_random();
	size = sizeof(data);
	timestamp = time_system();
	platform = 0x1234;
	for (iidx = 0, isize = size; iidx < isize; ++iidx)
		data[iidx] = (char)(random32() & 0xFF);
	checksum = hash(data, sizeof(data));
	resource_source_write_blob(uuid, timestamp, HASH_TEST, platform, checksum, data, size);
	resource_source_set_blob(&source, timestamp, HASH_TEST, platform, checksum, size);

	for (iidx = 0, isize = size; iidx < isize; ++iidx)
		data[iidx] = (char)(random32() & 0xFF);
	checksum = hash(data, sizeof(data));
	++timestamp;
	resource_source_write_blob(uuid, timestamp, HASH_TEST, platform, checksum, data, size);
	resource_source_set_blob(&source, timestamp, HASH_TEST, platform, checksum, size);

	++timestamp;
	resource_source_unset(&source, timestamp, HASH_TEST, platform);

	for (iidx = 0, isize = size; iidx < isize; ++iidx)
		data[iidx] = (char)(random32() & 0xFF);
	checksum = hash(data, sizeof(data));
	++timestamp;
	resource_source_write_blob(uuid, timestamp, HASH_TEST, platform, checksum, data, size);
	resource_source_set_blob(&source, timestamp, HASH_TEST, platform, checksum, size);

	// Verify three blob files exist
	files = fs_matching_files(STRING_ARGS(path), STRING_CONST("^.*\\.blob$"), true);
#if RESOURCE_ENABLE_LOCAL_SOURCE
	EXPECT_INTEQ(array_size(files), 3);
#endif
	string_array_deallocate(files);

	resource_source_clear_blob_history(&source, uuid);

	// Verify three blob files exist
	files = fs_matching_files(STRING_ARGS(path), STRING_CONST("^.*\\.blob$"), true);
#if RESOURCE_ENABLE_LOCAL_SOURCE
	EXPECT_INTEQ(array_size(files), 3);
#endif
	string_array_deallocate(files);

	resource_source_collapse_history(&source);

	files = fs_matching_files(STRING_ARGS(path), STRING_CONST("^.*\\.blob$"), true);
#if RESOURCE_ENABLE_LOCAL_SOURCE
	EXPECT_INTEQ(array_size(files), 3);
#endif
	string_array_deallocate(files);

	resource_source_clear_blob_history(&source, uuid);

	// Verify only one blob file exist, and that it's the correct one
	resource_source_map(&source, platform, map);
	change = hashmap_lookup(map, HASH_TEST);
	files = fs_matching_files(STRING_ARGS(path), STRING_CONST("^.*\\.blob$"), true);
#if RESOURCE_ENABLE_LOCAL_SOURCE
	EXPECT_TYPEEQ(change->platform, platform, uint64_t, PRIx64);
	EXPECT_TICKEQ(change->timestamp, timestamp);
	EXPECT_HASHEQ(change->hash, HASH_TEST);
	EXPECT_TYPEEQ(change->flags, RESOURCE_SOURCEFLAG_BLOB, unsigned int, "u");
	EXPECT_INTEQ(array_size(files), 1);

	string_t refpath = resource_stream_make_path(data, sizeof(data), STRING_CONST(""), uuid);
	refpath.str++;
	refpath.length--;  // Make relative
	char filename[64];
	string_t reffile =
	    string_format(filename, sizeof(filename), STRING_CONST(".%" PRIhash ".%" PRIx64 ".%" PRIhash ".blob"),
	                  HASH_TEST, platform, checksum);
	reffile = string_append(STRING_ARGS(refpath), sizeof(data), STRING_ARGS(reffile));
	EXPECT_STRINGEQ(files[0], string_to_const(reffile));
#endif
	string_array_deallocate(files);

	resource_source_finalize(&source);
	hashmap_finalize(map);

	fs_remove_directory(STRING_ARGS(path));

	return 0;
}

DECLARE_TEST(source, io) {
	return 0;
}

static void
test_source_declare(void) {
	ADD_TEST(source, set);
	ADD_TEST(source, unset);
	ADD_TEST(source, collapse);
	ADD_TEST(source, blob);
	ADD_TEST(source, io);
}

static test_suite_t test_source_suite = {test_source_application, test_source_memory_system, test_source_config,
                                         test_source_declare,     test_source_initialize,    test_source_finalize,
                                         test_source_event};

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
