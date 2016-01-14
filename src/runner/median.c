#include <stddef.h>
#include <stdlib.h>

static int compare_ulonglong(const void *a_ptr, const void *b_ptr) {
	const unsigned long long a = *((unsigned long long*) a_ptr);
	const unsigned long long b = *((unsigned long long*) b_ptr);
	if (a < b) {
		return -1;
	} else if (a > b) {
		return 1;
	} else {
		return 0;
	}
}

static inline unsigned long long average(unsigned long long a, unsigned long long b) {
	return (a / 2) + (b / 2) + (a & b & 1ull);
}

unsigned long long median(unsigned long long array[], size_t length) {
	qsort(array, length, sizeof(unsigned long long), &compare_ulonglong);
	if (length % 2 == 0) {
		const unsigned long long median_lo = array[length / 2 - 1];
		const unsigned long long median_hi = array[length / 2];
		return average(median_lo, median_hi);
	} else {
		return array[length / 2];
	}
}
