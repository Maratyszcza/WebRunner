#include <sys/mman.h>
#include <string.h>
#include <errno.h>

#include <webserver/logs.h>
#include <kernels/playground-gen.h>

void playground_create_arguments(struct playground_arguments arguments[restrict static 1], const struct playground_parameters parameters[restrict static 1]) {
	arguments->iterations = parameters->iterations;
}

void playground_free_arguments(struct playground_arguments arguments[restrict static 1], const struct playground_parameters parameters[restrict static 1]) {
}
