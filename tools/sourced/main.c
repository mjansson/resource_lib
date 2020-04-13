/* main.c  -  Resource library  -  Public Domain  -  2016 Mattias Jansson
 *
 * This library provides a cross-platform resource I/O library in C11 providing
 * basic resource loading, saving and streaming functionality for projects based
 * on our foundation library.
 *
 * The latest source code maintained by Mattias Jansson is always available at
 *
 * https://github.com/mjansson/resource_lib
 *
 * The foundation library source code maintained by Mattias Jansson is always available at
 *
 * https://github.com/mjansson/foundation_lib
 *
 * This library is put in the public domain; you can redistribute it and/or modify it without any restrictions.
 *
 */

#include <foundation/foundation.h>
#include <resource/resource.h>
#include <network/network.h>

#include "server.h"

typedef struct {
	bool display_help;
	string_const_t source_path;
	string_const_t* config_files;
	unsigned int port;
} sourced_input_t;

static sourced_input_t
sourced_parse_command_line(const string_const_t* cmdline);

static void
sourced_parse_config(const char* path, size_t path_size, const char* buffer, size_t size, const json_token_t* tokens,
                     size_t numtokens);

static void
sourced_print_usage(void);

int
main_initialize(void) {
	int ret = 0;
	application_t application;
	foundation_config_t foundation_config;
	network_config_t network_config;
	resource_config_t resource_config;

	memset(&foundation_config, 0, sizeof(foundation_config));
	memset(&network_config, 0, sizeof(network_config));
	memset(&resource_config, 0, sizeof(resource_config));

	resource_config.enable_local_source = true;
	resource_config.enable_local_cache = true;
	resource_config.enable_local_autoimport = true;

	memset(&application, 0, sizeof(application));
	application.name = string_const(STRING_CONST("sourced"));
	application.short_name = string_const(STRING_CONST("sourced"));
	application.company = string_const(STRING_CONST(""));
	application.flags = APPLICATION_DAEMON;

	log_enable_prefix(true);
	log_set_suppress(0, ERRORLEVEL_DEBUG);

	if ((ret = foundation_initialize(memory_system_malloc(), application, foundation_config)) < 0)
		return ret;

	if ((ret = network_module_initialize(network_config)) < 0)
		return ret;

	if ((ret = resource_module_initialize(resource_config)) < 0)
		return ret;

	log_set_suppress(HASH_NETWORK, ERRORLEVEL_INFO);
	log_set_suppress(HASH_RESOURCE, ERRORLEVEL_DEBUG);

	return 0;
}

int
main_run(void* main_arg) {
	FOUNDATION_UNUSED(main_arg);

	sourced_input_t input = sourced_parse_command_line(environment_command_line());

	for (size_t cfgfile = 0, fsize = array_size(input.config_files); cfgfile < fsize; ++cfgfile)
		sjson_parse_path(STRING_ARGS(input.config_files[cfgfile]), sourced_parse_config);

	if (input.source_path.length)
		resource_source_set_path(STRING_ARGS(input.source_path));

	if (!resource_source_path().length) {
		log_errorf(HASH_RESOURCE, ERROR_INVALID_VALUE, STRING_CONST("No source path given"));
		input.display_help = true;
	}

	if (input.display_help) {
		sourced_print_usage();
		goto exit;
	}

	// TODO: Find all import maps in autoimport paths and load into memory DB
	// TODO:   if no import maps, create default maps

	// TODO: Run as daemon

	server_run(input.port);

exit:

	array_deallocate(input.config_files);

	return 0;
}

void
main_finalize(void) {
	resource_module_finalize();
	network_module_finalize();
	foundation_finalize();
}

static void
sourced_parse_config(const char* path, size_t path_size, const char* buffer, size_t size, const json_token_t* tokens,
                     size_t numtokens) {
	resource_module_parse_config(path, path_size, buffer, size, tokens, numtokens);
}

static sourced_input_t
sourced_parse_command_line(const string_const_t* cmdline) {
	sourced_input_t input;
	size_t arg, asize;

	error_context_push(STRING_CONST("parse command line"), STRING_CONST(""));
	memset(&input, 0, sizeof(input));

	for (arg = 1, asize = array_size(cmdline); arg < asize; ++arg) {
		if (string_equal(STRING_ARGS(cmdline[arg]), STRING_CONST("--help")))
			input.display_help = true;
		else if (string_equal(STRING_ARGS(cmdline[arg]), STRING_CONST("--source"))) {
			if (arg < asize - 1)
				input.source_path = cmdline[++arg];
		} else if (string_equal(STRING_ARGS(cmdline[arg]), STRING_CONST("--config"))) {
			if (arg < asize - 1)
				array_push(input.config_files, cmdline[++arg]);
		} else if (string_equal(STRING_ARGS(cmdline[arg]), STRING_CONST("--port"))) {
			if (arg < asize - 1) {
				string_const_t portstr = cmdline[++arg];
				input.port = string_to_uint(STRING_ARGS(portstr), false);
			}
		} else if (string_equal(STRING_ARGS(cmdline[arg]), STRING_CONST("--debug"))) {
			log_set_suppress(0, ERRORLEVEL_NONE);
			log_set_suppress(HASH_NETWORK, ERRORLEVEL_NONE);
			log_set_suppress(HASH_RESOURCE, ERRORLEVEL_NONE);
		} else if (string_equal(STRING_ARGS(cmdline[arg]), STRING_CONST("--")))
			break;  // Stop parsing cmdline options
	}
	error_context_pop();

	return input;
}

static void
sourced_print_usage(void) {
	const error_level_t saved_level = log_suppress(0);
	log_set_suppress(0, ERRORLEVEL_DEBUG);
	log_enable_prefix(false);
	log_info(0, STRING_CONST(
	                "sourced usage:\n"
	                "  sourced [--source <path>] [--config <path>] [--port <port>]\n"
	                "          [--debug] [--help] ... [--]\n"
	                "    Optional arguments:\n"
	                "      --source <path>              Operate on resource file source structure given by <path>\n"
	                "      --config <path>              Read and parse config file given by <path>\n"
	                "                                   Loads all .json/.sjson files in <path> if it is a directory\n"
	                "      --port <port>                Network port to use\n"
	                "      --debug                      Enable debug output\n"
	                "      --help                       Display this help message\n"
	                "      --                           Stop processing command line arguments"));
	log_set_suppress(0, saved_level);
	log_enable_prefix(true);
}
