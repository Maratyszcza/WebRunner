#include <stdio.h>
#include <string.h>

#include <webrunner.h>
#include <webserver/logs.h>
#include <webserver/http.h>

enum http_method parse_http_method(size_t method_size, const char method[restrict static method_size]) {
	switch (method_size) {
		case sizeof("POST") - 1:
			if (memcmp(method, "POST", method_size) == 0) {
				return http_method_post;
			}
			break;
	}
	return http_method_unknown;
}

enum http_header_name parse_http_header_name(size_t name_size, const char name[restrict static name_size]) {
	switch (name_size) {
		case sizeof("Content-Length") - 1:
			if (memcmp(name, "Content-Length", name_size) == 0) {
				return http_header_name_content_length;
			}
			break;
		case sizeof("Content-Type") - 1:
			if (memcmp(name, "Content-Type", name_size) == 0) {
				return http_header_name_content_type;
			}
			break;
	}
	return http_header_name_unknown;
}

enum http_content_type parse_http_content_type(size_t value_size, const char value[restrict static value_size]) {
	switch (value_size) {
		case sizeof("application/octet-stream") - 1:
			if (memcmp(value, "application/octet-stream", value_size) == 0) {
				return http_content_type_application_octet_stream;
			}
			break;
	}
	return http_content_type_unknown;
}

struct http_parameter parse_http_parameter(size_t query_size, const char query[restrict static query_size]) {
	if (query_size == 0) {
		return (struct http_parameter) { 0 };
	}

	if ((query[0] == '?') || (query[0] == '&')) {
		query++;
		query_size--;
	}

	const char *const name_start = query;
	const char *const name_end = memchr(name_start, '=', query_size);
	if (name_end == NULL) {
		return (struct http_parameter) {
			.name = name_start,
			.name_size = query_size
		};
	}

	const char *const query_end = &query[query_size];
	const char *const value_start = name_end + 1;
	const char *value_end = memchr(value_start, '&', query_end - value_start);
	if (value_end == NULL) {
		return (struct http_parameter) {
			.name = name_start,
			.name_size = name_end - name_start,
			.value = value_start,
			.value_size = query_end - value_start
		};
	} else {
		return (struct http_parameter) {
			.name = name_start,
			.name_size = name_end - name_start,
			.value = value_start,
			.value_size = value_end - value_start,
			.next = value_end + 1
		};
	}
}

void http_respond_status(int socket, enum http_status status, const char reason[restrict static 1]) {
	if (dprintf(socket, "HTTP/1.1 %d %s\r\n\r\n", status, reason) < 0) {
		log_fatal("failed to write HTTP status\n");
	}
}
