#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <runner/spec.h>

enum webrunner_command {
	webrunner_command_invalid = 0,
	webrunner_command_monitor,
	webrunner_command_run,
};

enum webrunner_command parse_webrunner_command(size_t command_size, const char command[restrict static command_size]);

/**
 * @brief Loads ELF image into memory, and finds the specified function by name.
 * @bug The function always returns pointer to the start of the executable segment.
 * @bug The function doesn't load data segment.
 * @bug The function doesn't apply relocations.
 * @param[in] elf_image     Pointer to the ELF image.
 * @param[in] image_size    Size of the ELF image.
 * @param[in] function_name Name of the function to locate after loading the ELF image.
 * @return Pointer to the function with name @a function_name in executable segment.
 */
generic_function load_kernel(const void* elf_image, size_t image_size, const char* function_name);

void process_request(int connection_socket);

void enable_sandbox(int connection_socket);
