#pragma once

#include <stdint.h>
#include <stdio.h>

struct options {
	int access_log;
	int error_log;
	uint16_t port;
	uint32_t queue_size;
};

struct options parse_options(int argc, char** argv);
void print_options_help(const char* program_name);
