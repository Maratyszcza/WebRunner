#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <alloca.h>
#include <limits.h>

#include <errno.h>
#include <sys/socket.h>

#include <webserver/http.h>
#include <webserver/logs.h>
#include <webserver/parse.h>
#include <webrunner.h>
#include <runner/spec.h>
#include <runner/perfctr.h>

#define MAX_HEADERS_SIZE 65536

struct webrunner_request {
	size_t headers_length;
	size_t content_length;
	enum webrunner_command command;
	enum webrunner_kernel kernel;
	const char* kernel_parameters_query;
	size_t kernel_parameters_query_size;
};

/**
 * @brief Parse a single HTTP request header and update information in the request structure.
 * @param[in]      header  Pointer to the first byte of the HTTP request header.
 * @param[in]      size    Size of the HTTP request header.
 * @param[in, out] request Pointer to an @a webrunner_request structure, which might be updated by the function.
 */
static void parse_request_header(size_t header_size, const char header[restrict static header_size], struct webrunner_request request[restrict static 1]) {
	const char *const separator_pos = (const char*) memchr(header, ':', header_size);
	if (separator_pos == NULL) {
		return;
	}
	const size_t header_name_size = separator_pos - header;
	const enum http_header_name header_name = parse_http_header_name(header_name_size, header);
	if (header_name == http_header_name_unknown) {
		return;
	}

	/**
	 * HTTP 1.1 specification:
	 *     header-field   = field-name ":" OWS field-value OWS
	 */
	const char* header_value_start = separator_pos + 1;
	const char* header_value_end = &header[header_size];
	/* Skip optional whitespace before header value */
	while (header_value_start != header_value_end && isspace(*header_value_start)) {
		header_value_start++;
	}
	/* Skip optional whitespace after header value */
	while (header_value_end != header_value_start && isspace(header_value_end[-1])) {
		header_value_end--;
	}
	const size_t header_value_size = header_value_end - header_value_start;

	switch (header_name) {
		case http_header_name_content_length:
		{
			/* strtoul works with C strings, so copy header value into a null-terminated buffer */
			char header_value[header_value_size + 1];
			memcpy(header_value, header_value_start, header_value_size);
			header_value[header_value_size] = '\0';

			/* Set errno to 0 to detect errors after strtoul: strtoul sets errno only when fails */
			errno = 0;
			unsigned long content_length = strtoul(header_value_start, NULL, 10);
			if (errno != 0) {
				log_fatal("failed to parse content length value: %s\n", strerror(errno));
			}
			request->content_length = (size_t) content_length;
			break;
		}
		case http_header_name_content_type:
		{
			const enum http_content_type content_type = parse_http_content_type(header_value_size, header_value_start);
			if (content_type != http_content_type_application_octet_stream) {
				log_fatal("invalid content type: %.*s", (int) header_value_size, header_value_start);
			}
			break;
		}
		case http_header_name_unknown:
			break;
	}
}

static struct webrunner_request parse_target(size_t target_size, const char target[restrict static target_size]) {
	const char* target_end = &target[target_size];
	struct webrunner_request request = { 0 };

	/* Structure of a resource:
	 * /<command>/<uarch>?kernel=<kernel>&<param>=<value>&...
	 */

	/* Parse location */
	const char* command = target;
	if (*command == '/') {
		command++;
	}
	const char* command_end = (const char*) memchr(command, '/', target_end - command);
	if (command_end == NULL) {
		log_fatal("failed to parse command: invalid target %.*s\n", (int) target_size, target);
	}
	request.command = parse_webrunner_command(command_end - command, command);
	if (request.command != webrunner_command_run) {
		log_fatal("invalid command: %.*s\n", (int) (command_end - command), command);
	}

	const char* uarch = command_end + 1;
	const char* uarch_end = (const char*) memchr(uarch, '?', target_end - uarch);
	if (uarch_end == NULL) {
		log_fatal("failed to parse uarch: invalid target %.*s\n", (int) target_size, target);
	}

	/* Pre-parse query parameters */
	const char* query = uarch_end;
	const struct http_parameter kernel_parameter = parse_http_parameter(target_end - query, query);
	if (kernel_parameter.name == NULL) {
		log_fatal("required parameter kernel not specified\n");
	}
	const enum webrunner_parameter kernel_parameter_name = parse_webrunner_parameter(kernel_parameter.name_size, kernel_parameter.name);
	if (kernel_parameter_name != webrunner_parameter_kernel) {
		log_fatal("unexpected parameter %.*s in place of required parameter kernel\n",
			(int) kernel_parameter.name_size, kernel_parameter.name);
	}
	if (kernel_parameter.value == NULL) {
		log_fatal("required parameter kernel specified without value\n");
	}
	request.kernel = parse_kernel_name(kernel_parameter.value_size, kernel_parameter.value);
	if (request.kernel == webrunner_kernel_invalid) {
		log_fatal("invalid kernel value: %.*s\n",
			(int) kernel_parameter.value_size, kernel_parameter.value);
	}

	if (kernel_parameter.next) {
		request.kernel_parameters_query = kernel_parameter.next;
		request.kernel_parameters_query_size = target_end - kernel_parameter.next;
	}
	return request;
}

static struct webrunner_request parse_request_line(size_t line_size, const char* line) {
	const char* line_end = &line[line_size];

	/* Expected: "POST /target HTTP/1.1" */

	/* Split request line into method, target, and protocol */
	const char* method = line;
	const char* space_pos = (const char*) memchr(method, ' ', line_size);
	if (space_pos == NULL) {
		log_fatal("invalid HTTP request line: %.*s\n", (int) line_size, line);
	}
	const size_t method_size = space_pos - method;

	const char* target = space_pos + 1;
	space_pos = (const char*) memchr(target, ' ', line_end - target);
	if (space_pos == NULL) {
		log_fatal("invalid HTTP request line: %.*s\n", (int) line_size, line);
	}
	const size_t target_size = space_pos - target;

	const char* protocol = space_pos + 1;
	const size_t protocol_size = line_end - protocol;


	/* Validate HTTP method */
	const enum http_method http_method = parse_http_method(method_size, method);
	if (http_method != http_method_post) {
		log_fatal("invalid HTTP method: %.*s\n", (int) method_size, method);
	}

	/* Validate HTTP protocol */
	if ((protocol_size < 8) || protocol[0] != 'H' || protocol[1] != 'T' || protocol[2] != 'T' || protocol[3] != 'P' || protocol[4] != '/') {
		log_fatal("invalid HTTP protocol: %.*s\n", (int) protocol_size, protocol);
	}

	/* Validate and pre-parse target */
	return parse_target(target_size, target);
}

static struct webrunner_request parse_request_headers(
	size_t buffer_size,
	const char buffer[restrict static buffer_size])
{
	const char *const buffer_end = &buffer[buffer_size];

	struct end_of_line end_of_line = find_end_of_line(buffer_size, buffer);
	if (end_of_line.start == end_of_line.end) {
		log_fatal("HTTP request line exceeds WebRunner limit (%d bytes)\n", MAX_HEADERS_SIZE);
	}
	struct webrunner_request request = parse_request_line(end_of_line.start - buffer, buffer);

	const char* line;
	do {
		line = end_of_line.end;
		end_of_line = find_end_of_line(buffer_end - line, line);
		if (end_of_line.start == end_of_line.end) {
			log_fatal("HTTP headers exceed WebRunner limit (%d bytes)\n", MAX_HEADERS_SIZE);
		}
		if (line != end_of_line.start) {
			parse_request_header(end_of_line.start - line, line, &request);
		}
	} while (line != end_of_line.start);

	request.headers_length = end_of_line.end - buffer;
	return request;
}

void process_request(int connection_socket) {
	char request_buffer[MAX_HEADERS_SIZE];
	int bytes_received = recv(connection_socket, request_buffer, MAX_HEADERS_SIZE, 0);
	if (bytes_received == -1) {
		log_fatal("could not read request socket: %s\n", strerror(errno));
	}
	const struct webrunner_request request = parse_request_headers(bytes_received, request_buffer);
	const enum webrunner_kernel kernel = request.kernel;

	void* parameters = alloca(kernel_specifications[kernel].parameters_size);
	memcpy(parameters, kernel_specifications[kernel].parameters_default, kernel_specifications[kernel].parameters_size);
	if (request.kernel_parameters_query_size != 0) {
		const char* query = request.kernel_parameters_query;
		const char *const query_end = &query[request.kernel_parameters_query_size];
		do {
			const struct http_parameter parameter = parse_http_parameter(query_end - query, query);
			if (parameter.name == NULL) {
				log_error("empty parameter in the query %.*s\n",
					(int) request.kernel_parameters_query_size, request.kernel_parameters_query);
			} else if (parameter.value == NULL) {
				log_error("parameter %.*s specified without value\n",
					(int) parameter.name_size, parameter.name);
			} else {
				kernel_specifications[kernel].parse_parameter(parameters,
					parameter.name_size, parameter.name, parameter.value_size, parameter.value);
			}
			query = parameter.next;
		} while (query);
	}

	const void *const request_end = &request_buffer[bytes_received];
	const void* request_body = &request_buffer[request.headers_length];
	generic_function function = load_kernel(request_body, request_end - request_body, kernel_specifications[kernel].name);

	switch (request.command) {
		case webrunner_command_run:
		{
			const struct performance_counters performance_counters = init_performance_counters();

			void* arguments = alloca(kernel_specifications[kernel].arguments_size);
			kernel_specifications[kernel].create_arguments(arguments, parameters);

			enable_sandbox(connection_socket);

			http_respond_status(connection_socket, http_status_ok, "OK");

			for (size_t i = 0; i < performance_counters.count; i++) {
				ioctl(performance_counters.counters[i].file_descriptor, PERF_EVENT_IOC_ENABLE, 0);
				unsigned long long count = kernel_specifications[kernel].profile(function, arguments,
					performance_counters.counters[i].file_descriptor, 100);
				ioctl(performance_counters.counters[i].file_descriptor, PERF_EVENT_IOC_DISABLE, 0);
				if (count != ULLONG_MAX) {
					dprintf(connection_socket, "%s: %llu\n", performance_counters.counters[i].name, count);
				}
			}

			kernel_specifications[kernel].free_arguments(arguments, parameters);
			break;
		}
		case webrunner_command_invalid:
			__builtin_unreachable();
	}
}
