#include <sys/mman.h>
#include <string.h>
#include <errno.h>

#include <webserver/logs.h>
#include <kernels/blis/sgemm-gen.h>

void blis_sgemm_create_arguments(struct blis_sgemm_arguments arguments[restrict static 1], const struct blis_sgemm_parameters parameters[restrict static 1]) {
	void* a_array = mmap(NULL, parameters->k * parameters->mr * sizeof(float), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
	if (a_array == MAP_FAILED) {
		log_fatal("failed to allocate memory for A array: %s\n", strerror(errno));
	}

	void* b_array = mmap(NULL, parameters->k * parameters->nr * sizeof(float), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
	if (b_array == MAP_FAILED) {
		log_fatal("failed to allocate memory for B array: %s\n", strerror(errno));
	}

	void* c_array = mmap(NULL, (parameters->mr * parameters->rs_c) * (parameters->nr * parameters->cs_c) * sizeof(float), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
	if (c_array == MAP_FAILED) {
		log_fatal("failed to allocate memory for C array: %s\n", strerror(errno));
	}

	static float alpha = 1.0f;
	static float beta = 0.0f;

	arguments->k     = parameters->k;
	arguments->alpha = &alpha;
	arguments->a     = a_array;
	arguments->b     = b_array;
	arguments->beta  = &beta;
	arguments->c     = c_array;
	arguments->rs_c  = parameters->rs_c;
	arguments->cs_c  = parameters->cs_c;
}

void blis_sgemm_free_arguments(struct blis_sgemm_arguments arguments[restrict static 1], const struct blis_sgemm_parameters parameters[restrict static 1]) {
	if (arguments->a != NULL) {
		munmap((void*) arguments->a, parameters->k * parameters->mr * sizeof(float));
		arguments->a = NULL;
	}
	if (arguments->b != NULL) {
		munmap((void*) arguments->b, parameters->k * parameters->nr * sizeof(float));
		arguments->b = NULL;
	}
	if (arguments->c != NULL) {
		munmap((void*) arguments->c, (parameters->mr * parameters->rs_c) * (parameters->nr * parameters->cs_c) * sizeof(float));
		arguments->c = NULL;
	}
}
