#pragma once

#include <stddef.h>
#include <stdbool.h>

enum http_status {
	http_status_ok = 200,
	http_status_bad_request = 400,
	http_status_method_not_allowed = 405,

};

enum http_method {
	http_method_unknown = 0,
	http_method_head,
	http_method_post,
};

enum http_header_name {
	http_header_name_unknown = 0,
	http_header_name_content_length,
	http_header_name_content_type,	
};

enum http_content_type {
	http_content_type_unknown = 0,
	http_content_type_application_octet_stream,
};

struct http_parameter {
	const char* name;
	size_t name_size;
	const char* value;
	size_t value_size;
	const char* next;
};

enum http_method parse_http_method(size_t method_size, const char method[restrict static method_size]);
enum http_header_name parse_http_header_name(size_t name_size, const char name[restrict static name_size]);
enum http_content_type parse_http_content_type(size_t value_size, const char value[restrict static value_size]);
struct http_parameter parse_http_parameter(size_t query_size, const char query[restrict static query_size]);

void http_respond_status(int socket, enum http_status status, const char reason[restrict static 1]);
