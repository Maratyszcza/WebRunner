#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include <webrunner.h>
#include <webserver/parse.h>

enum webrunner_command parse_webrunner_command(size_t command_size, const char command[restrict static command_size]) {
	switch (command_size) {
		case sizeof("run") - 1:
			if (memcmp(command, "run", command_size) == 0) {
				return webrunner_command_run;
			}
			break;
		case sizeof("monitor") - 1:
			if (memcmp(command, "monitor", command_size) == 0) {
				return webrunner_command_monitor;
			}
			break;
	}
	return webrunner_command_invalid;
}

enum webrunner_parameter parse_webrunner_parameter(size_t parameter_size, const char parameter[restrict static parameter_size]) {
	switch (parameter_size) {
		case sizeof("kernel") - 1:
			if (memcmp(parameter, "kernel", parameter_size) == 0) {
				return webrunner_parameter_kernel;
			}
			break;
	}
	return webrunner_parameter_invalid;
}

struct end_of_line find_end_of_line(size_t buffer_size, const char buffer[restrict static buffer_size]) {
	const char* current = buffer;
	const char* const last = &buffer[buffer_size - 1];
	while (current < last) {
		if ((current[0] == '\r') && (current[1] == '\n')) {
			return (struct end_of_line) { .start = current, .end = current + 2 };
		} else {
			current += 1;
		}
	}
	const char* const end = &buffer[buffer_size];
	return (struct end_of_line) { .start = end, .end = end };
}

bool parse_uint32(size_t string_size, const char string[restrict static string_size], uint32_t value[restrict static 1]) {
	char cstring[string_size + 1];
	memcpy(cstring, string, string_size);
	cstring[string_size] = '\0';
	return sscanf(cstring, "%"SCNu32, value) == 1;
}

bool parse_uint64(size_t string_size, const char string[restrict static string_size], uint64_t value[restrict static 1]) {
	char cstring[string_size + 1];
	memcpy(cstring, string, string_size);
	cstring[string_size] = '\0';
	return sscanf(cstring, "%"SCNu64, value) == 1;
}
