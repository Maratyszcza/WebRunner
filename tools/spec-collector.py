#!/usr/bin/env python

from __future__ import print_function, absolute_import
import os
import sys
import argparse

from spec import read_kernel_specification, Kernel, Parameter, Argument


parser = argparse.ArgumentParser(description="WebRunner Kernel Specification Collector")
parser.add_argument(
    "--code", dest="code", required=True,
    help="Output C source file")
parser.add_argument(
    "--header", dest="header", required=True,
    help="Output C header file")
parser.add_argument(
    "input", nargs="+",
    help="Input XML specification file")


def generate_source(source_filename, kernels):
    with open(source_filename, "w") as source:
        print("""\
#include <string.h>
#include <inttypes.h>

#include <runner/spec.h>""", file=source)
        for kernel in kernels:
            print("#include <{header}>".format(header=kernel.header), file=source)

        print("""
enum webrunner_kernel parse_kernel_name(size_t name_size, const char name[restrict static name_size]) {
    switch (name_size) {""", file=source)

        for name_length in sorted(set(len(kernel.name) for kernel in kernels)):
            print(" " * 8 + "case {len}:".format(len=name_length), file=source)
            for i, kernel in enumerate(filter(lambda kernel: len(kernel.name) == name_length, kernels)):
                print("""\
            {if_keyword} (memcmp(name, \"{name}\", {name_len}) == 0) {{
                return webrunner_kernel_{prefix};""".format(
                        if_keyword="if" if i == 0 else "} else if",
                        name=kernel.name, prefix=kernel.prefix, name_len=name_length),
                    file=source)
            print("""\
            }
            break;""", file=source)

        print("""\
    }
    return webrunner_kernel_invalid;
}

const struct kernel_specification kernel_specifications[] = {""", file=source)

        for kernel in kernels:
            print("""\
    [webrunner_kernel_{prefix}] = {{
        .name = "{name}",
        .parameters_size = sizeof(struct {prefix}_parameters),
        .arguments_size = sizeof(struct {prefix}_arguments),
        .parameters_default = &{prefix}_parameters_default,
        .parse_parameter = (generic_parse_parameter_function) {prefix}_parse_parameter,
        .create_arguments = (generic_create_arguments_function) {prefix}_create_arguments,
        .free_arguments = (generic_free_arguments_function) {prefix}_free_arguments,
        .profile = (generic_profile_function) {prefix}_profile,
    }},""".format(name=kernel.name, prefix=kernel.prefix), file=source)

        print("};", file=source)

def generate_header(header_filename, kernels):
    with open(header_filename, "w") as header:
        print("""\
#pragma once

#include <stddef.h>

enum webrunner_kernel {
    webrunner_kernel_invalid = 0,""", file=header)
        for kernel in kernels:
            print("""\
    webrunner_kernel_{kernel_prefix},""".format(kernel_prefix=kernel.prefix), file=header)
        print("""\
};

enum webrunner_kernel parse_kernel_name(size_t name_size, const char name[restrict static name_size]);

typedef void (*generic_function)(void);
typedef void (*generic_parse_parameter_function)(void*, size_t, const char*, size_t, const char*);
typedef void (*generic_create_arguments_function)(void*, const void*);
typedef void (*generic_free_arguments_function)(void*, const void*);
typedef unsigned long long (*generic_profile_function)(generic_function, const void*, int, size_t);

struct kernel_specification {
    const char* name;
    size_t parameters_size;
    size_t arguments_size;
    void* parameters_default;
    generic_parse_parameter_function parse_parameter;
    generic_create_arguments_function create_arguments;
    generic_free_arguments_function free_arguments;
    generic_profile_function profile;
};

extern const struct kernel_specification kernel_specifications[];""", file=header)



def main():
    options = parser.parse_args()

    kernels = []
    for xml in options.input:
        kernel = read_kernel_specification(xml)
        kernels.append(kernel)

    generate_header(options.header, kernels)
    generate_source(options.code, kernels)


if __name__ == "__main__":
    if __package__ is None:
        __package__ = "webrunner"
    sys.exit(main())
