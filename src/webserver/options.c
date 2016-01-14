#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <limits.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "options.h"

struct options parse_options(int argc, char** argv) {
	struct options options = {
		.access_log = STDOUT_FILENO,
		.error_log = STDERR_FILENO,
		.port = 8081,
		.queue_size = 10,
	};
	for (int argi = 1; argi < argc; argi += 1) {
		if (strcmp(argv[argi], "--access-log") == 0) {
			if (argi + 1 == argc) {
				fprintf(stderr, "Error: expected 'access-log' argument\n");
				print_options_help(argv[0]);
				exit(EXIT_FAILURE);
			}
			const char* access_log_filename = argv[argi + 1];
			options.access_log = open(access_log_filename, O_WRONLY | O_APPEND);
			if (options.access_log == -1) {
				fprintf(stderr, "Error: failed to open '%s': %s\n", access_log_filename, strerror(errno));
				exit(EXIT_FAILURE);
			}
			argi += 1;
		} else if (strcmp(argv[argi], "--error-log") == 0) {
			if (argi + 1 == argc) {
				fprintf(stderr, "Error: expected 'error-log' argument\n");
				print_options_help(argv[0]);
				exit(EXIT_FAILURE);
			}
			const char* error_log_filename = argv[argi + 1];
			options.error_log = open(error_log_filename, O_WRONLY | O_APPEND);
			if (options.error_log == -1) {
				fprintf(stderr, "Error: failed to open '%s': %s\n", error_log_filename, strerror(errno));
				exit(EXIT_FAILURE);
			}
			argi += 1;
		} else if ((strcmp(argv[argi], "-q") == 0) || (strcmp(argv[argi], "--queue-size") == 0)) {
			if (argi + 1 == argc) {
				fprintf(stderr, "Error: expected 'queue-size' argument\n");
				print_options_help(argv[0]);
				exit(EXIT_FAILURE);
			}
			if (sscanf(argv[argi + 1], "%"SCNu32, &options.queue_size) != 1) {
				fprintf(stderr, "Error: failed to parse %s as unsigned decimal number\n", argv[argi + 1]);
				exit(EXIT_FAILURE);
			}
			if (options.queue_size > INT_MAX) {
				fprintf(stderr, "Error: specified queue size %"PRIu32" is above API limit %d\n", options.queue_size, INT_MAX);
				exit(EXIT_FAILURE);
			}
			argi += 1;
		} else if ((strcmp(argv[argi], "-p") == 0) || (strcmp(argv[argi], "--port") == 0)) {
			if (argi + 1 == argc) {
				fprintf(stderr, "Error: expected 'port' argument\n");
				print_options_help(argv[0]);
				exit(EXIT_FAILURE);
			}
			if (sscanf(argv[argi + 1], "%"SCNu16, &options.port) != 1) {
				fprintf(stderr, "Error: failed to parse %s as 16-bit unsigned integer\n", argv[argi + 1]);
				exit(EXIT_FAILURE);
			}
			argi += 1;
		} else if ((strcmp(argv[argi], "--help") == 0) || (strcmp(argv[argi], "-h") == 0)) {
			print_options_help(argv[0]);
			exit(EXIT_SUCCESS);
		} else {
			fprintf(stderr, "Error: unknown argument '%s'\n", argv[argi]);
			print_options_help(argv[0]);
			exit(EXIT_FAILURE);
		}
	}
	return options;
}

void print_options_help(const char* program_name) {
	printf("%s [options]\n", program_name);
	printf("Options:\n");
	printf("      --access-log  The filename for the access log (default: stdout)\n");
	printf("      --error-log   The filename for the error log (default: stderr)\n");
	printf("  -p  --port        The TCP/IP port to listen on (default: 8081)\n");
	printf("  -q  --queue-size  The size of queue for the listening socket (default: 10)\n");
}
