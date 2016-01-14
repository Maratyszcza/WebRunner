#pragma once

#include <limits.h>
#include <cpuid.h>

#include <unistd.h>
#include <linux/perf_event.h>

#include <runner/perfctr.h>

#define DECLARE_PROFILE_FUNCTION(name) \
	unsigned long long name##_profile(void* name, \
		const struct name##_arguments arguments[restrict static 1], \
		int perf_counter_descriptor, size_t max_iterations);

#define DEFINE_PROFILE_FUNCTION(name) \
	unsigned long long name##_profile(void* name, \
		const struct name##_arguments arguments[restrict static 1], \
		int perf_counter_descriptor, size_t max_iterations) \
	{ \
		unsigned long long overhead_count[max_iterations]; \
		size_t overhead_samples = 0; \
		for (size_t iteration = 0; iteration < max_iterations; iteration++) { \
			unsigned long long start_count = 0, end_count = 0; \
			if (read(perf_counter_descriptor, &start_count, sizeof(start_count)) != sizeof(start_count)) \
				continue; \
	\
			uint32_t eax, ebx, ecx, edx; \
			__cpuid(0, eax, ebx, ecx, edx); \
			__cpuid(0, eax, ebx, ecx, edx); \
	\
			if (read(perf_counter_descriptor, &end_count, sizeof(end_count)) != sizeof(end_count)) \
				continue; \
	\
			overhead_count[overhead_samples++] = end_count - start_count; \
		} \
	\
		/* Performance counters aren't working */ \
		if (overhead_samples == 0) \
			return ULLONG_MAX; \
	\
		unsigned long long computation_count[max_iterations]; \
		size_t computation_samples = 0; \
		for (size_t iteration = 0; iteration < max_iterations; iteration++) { \
			unsigned long long start_count = 0, end_count = 0; \
			if (read(perf_counter_descriptor, &start_count, sizeof(start_count)) != sizeof(start_count)) \
				continue; \
	\
			uint32_t eax, ebx, ecx, edx; \
			__cpuid(0, eax, ebx, ecx, edx); \
			name##_call(name, arguments); \
			__cpuid(0, eax, ebx, ecx, edx); \
	\
			if (read(perf_counter_descriptor, &end_count, sizeof(end_count)) != sizeof(end_count)) \
				continue; \
	\
			computation_count[computation_samples++] = end_count - start_count; \
		} \
	\
		const unsigned long long median_overhead_count = median(overhead_count, overhead_samples); \
		const unsigned long long median_computation_count = median(computation_count, computation_samples); \
	\
		if (median_computation_count > median_overhead_count) \
			return median_computation_count - median_overhead_count; \
		else \
			return 0; \
	}
