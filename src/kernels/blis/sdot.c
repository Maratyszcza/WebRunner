#include <sys/mman.h>
#include <string.h>
#include <errno.h>

#include <webserver/logs.h>
#include <kernels/blis/sdot-gen.h>

#define BIT_AND_PTR(ptr, n) ((void*) (((intptr_t) (ptr)) & ((intptr_t) (n))))

void blis_sdot_create_arguments(struct blis_sdot_arguments arguments[restrict static 1], const struct blis_sdot_parameters parameters[restrict static 1]) {
	void* x_array = mmap(NULL, parameters->n * parameters->incx * sizeof(float) + 64, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
	if (x_array == MAP_FAILED) {
		log_fatal("failed to allocate memory for x array: %s\n", strerror(errno));
	}

	void* y_array = mmap(NULL, parameters->n * parameters->incy * sizeof(float) + 64, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
	if (y_array == MAP_FAILED) {
		log_fatal("failed to allocate memory for y array: %s\n", strerror(errno));
	}

	arguments->n    = parameters->n;
	arguments->x    = x_array + parameters->offx;
	arguments->incx = parameters->incx;
	arguments->y    = y_array + parameters->offy;
	arguments->incy = parameters->incy;
}

void blis_sdot_free_arguments(struct blis_sdot_arguments arguments[restrict static 1], const struct blis_sdot_parameters parameters[restrict static 1]) {
	if (arguments->x != NULL) {
		munmap(BIT_AND_PTR(arguments->x, -4096), parameters->n * parameters->incx * sizeof(float) + 64);
		arguments->x = NULL;
	}
	if (arguments->y != NULL) {
		munmap(BIT_AND_PTR(arguments->y, -4096), parameters->n * parameters->incy * sizeof(float) + 64);
		arguments->y = NULL;
	}
}
