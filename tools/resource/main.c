/* main.c  -  Resource library  -  Public Domain  -  2014 Mattias Jansson / Rampant Pixels
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

#include <foundation/foundation.h>
#include <resource/resource.h>

#include "errorcodes.h"

typedef struct {
	unsigned int      flag;
	string_const_t    key;
	string_const_t    value;
} resource_op_t;

typedef struct {
	bool              display_help;
	int               binary;
	string_const_t    source_path;
	uuid_t            uuid;
	uint64_t          platform;
	resource_op_t*    op;
	bool              collapse;
	bool              clearblobs;
} resource_input_t;

static resource_input_t
resource_parse_command_line(const string_const_t* cmdline);

static void
resource_print_usage(void);

static void*
resource_read_file(const char* path, size_t length, resource_blob_t* blob) {
	stream_t* stream = stream_open(path, length, STREAM_IN | STREAM_BINARY);
	if (!stream)
		return 0;
	size_t size = stream_size(stream);
	void* data = memory_allocate(HASH_RESOURCE, size, 0, MEMORY_PERSISTENT);
	if (stream_read(stream, data, size) != size) {
		memory_deallocate(data);
		data = 0;
	}
	stream_deallocate(stream);
	blob->size = size;
	blob->checksum = hash(data, size);
	return data;
}

int
main_initialize(void) {
	int ret = 0;
	application_t application;
	foundation_config_t foundation_config;
	resource_config_t resource_config;

	memset(&foundation_config, 0, sizeof(foundation_config));
	memset(&resource_config, 0, sizeof(resource_config));

	resource_config.enable_local_source = true;
	resource_config.enable_local_cache = true;
	resource_config.enable_remote_source = true;

	memset(&application, 0, sizeof(application));
	application.name = string_const(STRING_CONST("resource"));
	application.short_name = string_const(STRING_CONST("resource"));
	application.config_dir = string_const(STRING_CONST("resource"));
	application.flags = APPLICATION_UTILITY;

	log_enable_prefix(false);
	log_set_suppress(0, ERRORLEVEL_WARNING);

	if ((ret = foundation_initialize(memory_system_malloc(), application, foundation_config)) < 0)
		return ret;
	if ((ret = resource_module_initialize(resource_config)) < 0)
		return ret;

	log_set_suppress(HASH_RESOURCE, ERRORLEVEL_INFO);

	return 0;
}

int
main_run(void* main_arg) {
	int result = RESOURCE_RESULT_OK;
	size_t iop, opsize;
	resource_source_t source;
	resource_blob_t blob;
	tick_t tick;
	void* blobdata;
	resource_input_t input = resource_parse_command_line(environment_command_line());

	FOUNDATION_UNUSED(main_arg);

	resource_source_initialize(&source);

	if (input.display_help) {
		resource_print_usage();
		goto exit;
	}

	resource_source_set_path(STRING_ARGS(input.source_path));

	resource_source_read(&source, input.uuid);
	tick = time_system();
	for (iop = 0, opsize = array_size(input.op); iop < opsize; ++iop) {
		resource_op_t op = input.op[iop];
		switch (op.flag) {
		case RESOURCE_SOURCEFLAG_VALUE:
			resource_source_set(&source, tick++, hash(STRING_ARGS(op.key)), input.platform,
			                    STRING_ARGS(op.value));
			break;

		case RESOURCE_SOURCEFLAG_UNSET:
			resource_source_unset(&source, tick++, hash(STRING_ARGS(op.key)), input.platform);
			break;

		case RESOURCE_SOURCEFLAG_BLOB:
			blobdata = resource_read_file(STRING_ARGS(op.value), &blob);
			if (blobdata) {
				if (resource_source_write_blob(input.uuid, tick, hash(STRING_ARGS(op.key)),
				                               input.platform, blob.checksum, blobdata, blob.size))
					resource_source_set_blob(&source, tick++, hash(STRING_ARGS(op.key)), input.platform,
					                         blob.checksum, blob.size);
				else
					log_warnf(HASH_RESOURCE, WARNING_RESOURCE, STRING_CONST("Failed to write blob data for %.*s"),
					          STRING_FORMAT(op.key));
			}
			else {
				log_warnf(HASH_RESOURCE, WARNING_RESOURCE,
				          STRING_CONST("Failed to read blob data for %.*s from %.*s"),
				          STRING_FORMAT(op.key), STRING_FORMAT(op.value));
			}
			memory_deallocate(blobdata);
			break;
		}
	}
	if (input.collapse)
		resource_source_collapse_history(&source);
	if (input.clearblobs)
		resource_source_clear_blob_history(&source, input.uuid);
	if (!resource_source_write(&source, input.uuid, input.binary)) {
		log_warn(HASH_RESOURCE, WARNING_INVALID_VALUE, STRING_CONST("Unable to write output file"));
		result = RESOURCE_RESULT_UNABLE_TO_OPEN_OUTPUT_FILE;
	}

exit:

	resource_source_finalize(&source);

	return result;
}

void
main_finalize(void) {
	resource_module_finalize();
	foundation_finalize();
}

resource_input_t
resource_parse_command_line(const string_const_t* cmdline) {
	resource_input_t input;
	size_t arg, asize;

	memset(&input, 0, sizeof(input));

	for (arg = 1, asize = array_size(cmdline); arg < asize; ++arg) {
		if (string_equal(STRING_ARGS(cmdline[arg]), STRING_CONST("--help")))
			input.display_help = true;
		else if (string_equal(STRING_ARGS(cmdline[arg]), STRING_CONST("--source"))) {
			if (arg < asize - 1)
				input.source_path = cmdline[++arg];
		}
		else if (string_equal(STRING_ARGS(cmdline[arg]), STRING_CONST("--uuid"))) {
			if (arg < asize - 1) {
				++arg;
				input.uuid = string_to_uuid(STRING_ARGS(cmdline[arg]));
				if (uuid_is_null(input.uuid))
					log_warnf(HASH_RESOURCE, WARNING_INVALID_VALUE, STRING_CONST("Invalid UUID: %.*s"),
					          STRING_FORMAT(cmdline[arg]));
			}
		}
		else if (string_equal(STRING_ARGS(cmdline[arg]), STRING_CONST("--platform"))) {
			if (arg < asize - 1) {
				bool hex = false;
				string_const_t value = cmdline[++arg];
				if ((value.length > 2) && string_equal(value.str, 2, STRING_CONST("0x"))) {
					value.str += 2;
					value.length -= 2;
					hex = true;
				}
				else if (string_find_first_not_of(STRING_ARGS(cmdline[arg]), STRING_CONST("0123456789"),
				                                  0) != STRING_NPOS) {
					hex = true;
				}
				input.platform = string_to_uint64(STRING_ARGS(value), hex);
			}
		}
		else if (string_equal(STRING_ARGS(cmdline[arg]), STRING_CONST("--set"))) {
			if (arg < asize - 2) {
				resource_op_t op;
				op.flag = RESOURCE_SOURCEFLAG_VALUE;
				op.key = cmdline[++arg];
				op.value = cmdline[++arg];
				array_push(input.op, op);
			}
		}
		else if (string_equal(STRING_ARGS(cmdline[arg]), STRING_CONST("--unset"))) {
			if (arg < asize - 1) {
				resource_op_t op;
				op.flag = RESOURCE_SOURCEFLAG_UNSET;
				op.key = cmdline[++arg];
				array_push(input.op, op);
			}
		}
		else if (string_equal(STRING_ARGS(cmdline[arg]), STRING_CONST("--blob"))) {
			if (arg < asize - 1) {
				resource_op_t op;
				op.flag = RESOURCE_SOURCEFLAG_BLOB;
				op.key = cmdline[++arg];
				op.value = cmdline[++arg];
				array_push(input.op, op);
			}
		}
		else if (string_equal(STRING_ARGS(cmdline[arg]), STRING_CONST("--collapse"))) {
			input.collapse = true;
		}
		else if (string_equal(STRING_ARGS(cmdline[arg]), STRING_CONST("--clearblobs"))) {
			input.clearblobs = true;
		}
		else if (string_equal(STRING_ARGS(cmdline[arg]), STRING_CONST("--binary"))) {
			input.binary = 1;
		}
		else if (string_equal(STRING_ARGS(cmdline[arg]), STRING_CONST("--ascii"))) {
			input.binary = 0;
		}
		else if (string_equal(STRING_ARGS(cmdline[arg]), STRING_CONST("--debug"))) {
			log_set_suppress(0, ERRORLEVEL_NONE);
			log_set_suppress(HASH_RESOURCE, ERRORLEVEL_NONE);
		}
		else if (string_equal(STRING_ARGS(cmdline[arg]), STRING_CONST("--")))
			break; //Stop parsing cmdline options
		else {
			//Unknown argument, display help
			input.display_help = true;
		}
	}
	error_context_pop();

	bool already_help = input.display_help;
	if (!already_help && !input.source_path.length) {
		log_errorf(HASH_RESOURCE, ERROR_INVALID_VALUE, STRING_CONST("No source path given"));
		input.display_help = true;
	}
	if (!already_help && uuid_is_null(input.uuid)) {
		log_errorf(HASH_RESOURCE, ERROR_INVALID_VALUE, STRING_CONST("No UUID given"));
		input.display_help = true;
	}

	return input;
}

static void
resource_print_usage(void) {
	const error_level_t saved_level = log_suppress(0);
	log_set_suppress(0, ERRORLEVEL_DEBUG);
	log_info(0, STRING_CONST(
	             "resource usage:\n"
	             "  resource --source <path> --uuid <uuid> [--platform <id>]\n"
	             "           [--set <key> <value>] [--blob <key> <file>] [--unset <key>]\n"
	             "           [--collapse] [--clearblobs]\n"
	             "           [--binary] [--ascii] [--debug] [--help] [--]\n"
	             "    Required arguments:\n"
	             "      --source <path>              Set resource file repository to <path>\n"
	             "      --uuid <uuid>                Resource UUID\n"
	             "    Repeatable arguments:\n"
	             "      --set <key> <value>          Set <key> to <value> in resource\n"
	             "      --blob <key> <value>         Set <key> to blob read from <file> in resource\n"
	             "      --unset <key>                Unset <key> in resource\n"
	             "    Optional arguments:\n"
	             "      --platform <id>              Platform specifier\n"
	             "      --collapse                   Collapse history\n"
	             "      --clearblobs                 Clear unreferenced blobs\n"
	             "      --binary                     Write binary file\n"
	             "      --ascii                      Write ASCII file (default)\n"
	             "      --debug                      Enable debug output\n"
	             "      --help                       Display this help message\n"
	             "      --                           Stop processing command line arguments"
	         ));
	log_set_suppress(0, saved_level);
}
