#!/usr/bin/env python

from __future__ import print_function, absolute_import
import os
import sys
import argparse

from spec import read_kernel_specification, Kernel, Parameter, Argument


parser = argparse.ArgumentParser(description="WebRunner Kernel Specification Compiler")
parser.add_argument(
    "--code", dest="code", required=True,
    help="Output C source file")
parser.add_argument(
    "--header", dest="header", required=True,
    help="Output C header file")
parser.add_argument(
    "input", nargs=1,
    help="Input XML specification file")


def generate_source(source_filename, kernel):
    with open(source_filename, "w") as source:
        print("""\
#include <string.h>

#include <webserver/parse.h>
#include <webserver/logs.h>
#include <runner/benchmark.h>
#include <{kernel_header}>

struct {kernel_prefix}_parameters {kernel_prefix}_parameters_default = {{""".format(
                kernel_prefix=kernel.prefix, kernel_header=kernel.header),
            file=source)
        for parameter in kernel.parameters:
            print(" " * 4 + ".{name} = {value},".format(
                    name=parameter.name, value=parameter.c_default),
                file=source)
        print("""\
}};

void {kernel_prefix}_parse_parameter(
    struct {kernel_prefix}_parameters parameters[restrict static 1],
    size_t name_size,
    const char name[restrict static name_size],
    size_t value_size,
    const char value[restrict static value_size])
{{
    switch (name_size) {{""".format(kernel_prefix=kernel.prefix), file=source)

        for name_length in sorted(set(len(parameter.name) for parameter in kernel.parameters)):
            print(" " * 8 + "case {len}:".format(len=name_length), file=source)
            for i, parameter in enumerate(filter(lambda param: len(param.name) == name_length, kernel.parameters)):
                print("""\
            {if_keyword} (memcmp(name, \"{name}\", {name_len}) == 0) {{
                if (!parse_{type}(value_size, value, &parameters->{name})) {{
                    log_fatal("failed to parse value %.*s for parameter {name} in {kernel_full_name}\\n", (int) value_size, value);
                }}""".format(
                        if_keyword="if" if i == 0 else "} else if",
                        name=parameter.name, type=parameter.type, name_len=name_length, kernel_full_name=kernel.full_name),
                    file=source)
                if parameter.min is not None:
                    print("""\
                if (parameters->{name} < {min}) {{
                    log_fatal("invalid value %.*s for parameter {name} in kernel {kernel_full_name}\\n", (int) value_size, value);
                }}""".format(name=parameter.name, min=parameter.c_min, kernel_full_name=kernel.full_name),
                        file=source)
                if parameter.max is not None:
                    print("""\
                if (parameters->{name} > {max}) {{
                    log_fatal("invalid value %.*s for parameter {name} in {kernel_full_name}\\n", (int) value_size, value);
                }}""".format(name=parameter.name, max=parameter.c_max, kernel_full_name=kernel.full_name),
                        file=source)
                print(" " * 16 + "return;", file=source)
            print("""\
            }
            break;""", file=source)

        print("""\
    }}
    log_fatal("invalid parameter %.*s for {kernel_full_name}\\n", (int) name_size, name);
}}

DEFINE_PROFILE_FUNCTION({kernel_prefix})""".format(
                kernel_full_name=kernel.full_name, kernel_prefix=kernel.prefix),
            file=source)


def generate_header(header_filename, kernel):
    with open(header_filename, "w") as header:
        print("""\
#pragma once

#include <stddef.h>
#include <stdint.h>

struct {kernel_prefix}_parameters {{""".format(kernel_prefix=kernel.prefix), file=header)
        for parameter in kernel.parameters:
            print(" " * 4 + "{type} {name};".format(name=parameter.name, type=parameter.c_type), file=header)
        print("""\
}};

extern struct {kernel_prefix}_parameters {kernel_prefix}_parameters_default;

struct {kernel_prefix}_arguments {{""".format(kernel_prefix=kernel.prefix), file=header)
        for argument in kernel.arguments:
            print(" " * 4 + "{type} {name};".format(name=argument.name, type=argument.c_type), file=header)
        print("""\
}};

static inline void {kernel_prefix}_call(void* function,
    const struct {kernel_prefix}_arguments arguments[restrict static 1])
{{
    typedef float (*{kernel_name}_function)({kernel_argtypes});
    {kernel_name}_function {kernel_name} = ({kernel_name}_function) function;
    {kernel_name}({kernel_args});
}}

unsigned long long {kernel_prefix}_profile(void* function,
    const struct {kernel_prefix}_arguments arguments[restrict static 1],
    int perf_counter_fd, size_t max_iterations);

void {kernel_prefix}_parse_parameter(
    struct {kernel_prefix}_parameters parameters[restrict static 1],
    size_t name_size, const char name[restrict static name_size],
    size_t value_size, const char value[restrict static value_size]);
void {kernel_prefix}_create_arguments(
    struct {kernel_prefix}_arguments[restrict static 1],
    const struct {kernel_prefix}_parameters parameters[restrict static 1]);
void {kernel_prefix}_free_arguments(
    struct {kernel_prefix}_arguments[restrict static 1],
    const struct {kernel_prefix}_parameters parameters[restrict static 1]);

""".format(kernel_name=kernel.name, kernel_prefix=kernel.prefix,
                kernel_argtypes=", ".join(argument.c_type for argument in kernel.arguments),
                kernel_args=", ".join("arguments->" + argument.name for argument in kernel.arguments)),
            file=header)



def main():
    options = parser.parse_args()

    kernel = read_kernel_specification(options.input[0])
    generate_header(options.header, kernel)
    generate_source(options.code, kernel)


if __name__ == "__main__":
    if __package__ is None:
        __package__ = "webrunner"
    sys.exit(main())
