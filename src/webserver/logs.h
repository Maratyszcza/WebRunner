#pragma once

void setup_logs(int error_log, int access_log);
void log_access(const char* format, ...) __attribute__((__format__(__printf__, 1, 2)));;
void log_error(const char* format, ...) __attribute__((__format__(__printf__, 1, 2)));;
void log_fatal(const char* format, ...) __attribute__((__noreturn__, __format__(__printf__, 1, 2)));;
