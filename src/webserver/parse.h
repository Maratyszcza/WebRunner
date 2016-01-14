#pragma once

#include <stdint.h>
#include <stdbool.h>

enum webrunner_parameter {
	webrunner_parameter_invalid = 0,
	webrunner_parameter_kernel,
	webrunner_parameter
};

enum webrunner_parameter parse_webrunner_parameter(size_t parameter_size, const char parameter[restrict static parameter_size]);

struct end_of_line {
	const char* start;
	const char* end;
};

/**
 * @brief Find end of line sequence ('\r\n') in a string
 * @param[in] buffer_size Length of the string in bytes.
 * @param[in] buffer      Pointer to the start of the string.
 * @return A structure describing where the end-of-line sequence starts and ends.
 *         If no end-of-line sequence found, the structure would have .start and .end at the end of the buffer. 
 */
struct end_of_line find_end_of_line(size_t buffer_size, const char buffer[restrict static buffer_size]);

bool parse_uint32(size_t string_size, const char string[restrict static string_size], uint32_t value[restrict static 1]);
bool parse_uint64(size_t string_size, const char string[restrict static string_size], uint64_t value[restrict static 1]);
