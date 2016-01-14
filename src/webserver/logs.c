#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <time.h>
#include <unistd.h>

#include <webrunner.h>

int error_log_fd = STDERR_FILENO;
int access_log_fd = STDOUT_FILENO;

void setup_logs(int error_log, int access_log) {
	error_log_fd = error_log;
	access_log_fd = access_log;
}

void log_message(int fd, const char* format, va_list args) {
	/* Query local date & time */
	time_t time_secs = time(NULL);
	if (time_secs != ((time_t) -1)) {
		struct tm local_time;
		if (localtime_r(&time_secs, &local_time) != NULL) {
			/* Format date & time */
			char date_buffer[256];
			if (strftime(date_buffer, sizeof(date_buffer), "%a, %d %b %y %T", &local_time) != 0) {
				dprintf(fd, "[%s] ", date_buffer);
			}
		}
	}

	vdprintf(fd, format, args);
}

void log_access(const char* format, ...) {
	va_list args;
	va_start(args, format);
	log_message(error_log_fd, format, args);
	va_end(args);
}

void log_error(const char* format, ...) {
	va_list args;
	va_start(args, format);
	log_message(error_log_fd, format, args);
	va_end(args);
}

void log_fatal(const char* format, ...) {
	va_list args;
	va_start(args, format);
	log_message(error_log_fd, format, args);
	va_end(args);

	exit(EXIT_FAILURE);
}
