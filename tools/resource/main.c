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
	bool              display_help;
	int               binary;
	string_const_t    source_path;
	uuid_t            uuid;
	string_const_t    key;
	string_const_t    value;
} resource_input_t;

static resource_input_t
resource_parse_command_line(const string_const_t* cmdline);

static void
resource_print_usage(void);

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
	resource_source_t source;
	resource_input_t input = resource_parse_command_line(environment_command_line());

	FOUNDATION_UNUSED(main_arg);

	resource_source_initialize(&source);

	if (input.display_help) {
		resource_print_usage();
		goto exit;
	}

	resource_source_set_path(STRING_ARGS(input.source_path));

	resource_source_read(&source, input.uuid);
	resource_source_set(&source, time_system(), hash(STRING_ARGS(input.key)), STRING_ARGS(input.value));
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
		else if (string_equal(STRING_ARGS(cmdline[arg]), STRING_CONST("--set"))) {
			if (arg < asize - 2) {
				input.key = cmdline[++arg];
				input.value = cmdline[++arg];
			}
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
	             "  resource --source <path> --uuid <uuid> [--set <key> <value>] [--binary] [--ascii] [--debug] [--help] [--]\n"
	             "    Arguments:\n"
	             "      --source <path>              Operate on resource file given by <path>\n"
	             "      --uuid <uuid>                Resource UUID\n"
	             "    Optional arguments:\n"
	             "      --set <key> <value>          Set <key> to <value> in resource\n"
	             "      --binary                     Write binary file\n"
	             "      --ascii                      Write ASCII file (default)\n"
	             "      --debug                      Enable debug output\n"
	             "      --help                       Display this help message\n"
	             "      --                           Stop processing command line arguments"
	         ));
	log_set_suppress(0, saved_level);
}
