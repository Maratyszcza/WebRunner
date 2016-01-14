#pragma once

#include <stddef.h>

#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>

struct performance_counter {
	const char* name;
	int file_descriptor;
};

struct performance_counters {
	struct performance_counter* counters;
	size_t count;
};

struct performance_counters init_performance_counters(void);

unsigned long long median(unsigned long long array[], size_t length);
