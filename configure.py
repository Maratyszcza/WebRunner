#!/usr/bin/env python

from __future__ import print_function

import os
import sys
import glob
import argparse
import ninja_syntax


class Configuration:
    def __init__(self, options):
        self.root_dir = os.path.dirname(os.path.abspath(__file__))
        self.source_dir = os.path.join(self.root_dir, "src")
        self.build_dir = os.path.join(self.root_dir, "build")
        self.artifact_dir = self.root_dir
        self.writer = ninja_syntax.Writer(open(os.path.join(self.root_dir, "build.ninja"), "w"))
        self.object_ext = ".o"

        # Variables
        cflags = ["-std=gnu11", "-g", "-Wall", "-Werror", "-Wno-error=unused-variable", "-fcolor-diagnostics"]
        ldflags = ["-g", "-Wl,-fuse-ld=gold"]
        self.writer.variable("cc", "clang")
        self.writer.variable("cflags", " ".join(cflags))
        self.writer.variable("ldflags", " ".join(ldflags))
        self.writer.variable("optflags", "-O2")
        self.writer.variable("includes", "-I" + self.source_dir)
        self.writer.variable("python", "python")
        self.writer.variable("spec_compiler", "$python " + os.path.join(self.root_dir, "tools", "spec-compiler.py"))
        self.writer.variable("spec_collector", "$python " + os.path.join(self.root_dir, "tools", "spec-collector.py"))
        if not sys.platform.startswith("linux"):
            print("Unsupported platform: %s" % sys.platform, file=sys.stdout)
            sys.exit(1)

        # Rules
        self.writer.rule("cc", "$cc -o $out -c $in -MMD -MF $out.d $optflags $cflags $includes",
            deps="gcc", depfile="$out.d",
            description="CC $descpath")
        self.writer.rule("ccld", "$cc $ldflags $libdirs -o $out $in $libs",
            description="CCLD $descpath")
        self.writer.rule("spec-compile", "$spec_compiler $in --code $code --header $header",
            description="COMPILE $descpath")
        self.writer.rule("spec-collect", "$spec_collector $in --code $code --header $header",
            description="COLLECT $descpath")
        self.writer.rule("install", "install -m $mode $in $out",
            description="INSTALL $descpath")
        self.writer.rule("enable", "systemctl enable $service",
            description="ENABLE $service")
        self.writer.rule("start", "systemctl start $service",
            description="START $service")
        self.writer.rule("stop", "systemctl stop $service",
            description="STOP $service")

    def check(self):
        """Checks system requirements for WebRunner"""
        import sys
        if not sys.platform.startswith("linux"):
            print("Unsupported platform %s: WebRunner is a Linux-only software" % sys.platform, file=sys.stdout)
            sys.exit(1)

        import platform
        import re
        kernel_version_match = re.match(r"\d+\.\d+\.\d+", platform.release())
        if kernel_version_match:
            kernel_release = tuple(map(int, kernel_version_match.group(0).split(".")))
            if kernel_release < (3,17,):
                print("Unsupported Linux kernel %s: WebRunner requires Linux 3.17+" % kernel_version_match.group(0))
                sys.exit(1)

    def cc(self, source_file, object_file=None):
        if not os.path.isabs(source_file):
            source_file = os.path.join(self.source_dir, source_file)
        if object_file is None:
            object_file = os.path.join(self.build_dir, os.path.relpath(source_file, self.source_dir)) + self.object_ext
        elif not os.path.isabs(object_file):
            object_file = os.path.join(self.build_dir, object_file)
        variables = {
            "descpath": os.path.relpath(source_file, self.source_dir)
        }
        self.writer.build(object_file, "cc", source_file, variables=variables)
        return object_file

    def ccld(self, object_files, executable_file):
        if not os.path.isabs(executable_file):
            executable_file = os.path.join(self.artifact_dir, executable_file)
        variables = {
            "descpath": os.path.relpath(executable_file, self.artifact_dir)
        }
        self.writer.build(executable_file, "ccld", object_files, variables=variables)
        return executable_file

    def spec_compile(self, spec_file, code_file=None, header_file=None):
        if not os.path.isabs(spec_file):
            spec_file = os.path.join(self.source_dir, spec_file)
        if code_file is None:
            code_file = os.path.splitext(spec_file)[0] + "-gen.c"
        elif not os.path.isabs(code_file):
            code_file = os.path.join(self.source_dir, code_file)
        if header_file is None:
            header_file = os.path.splitext(code_file)[0] + ".h"
        elif not os.path.isabs(header_file):
            header_file = os.path.join(self.source_dir, header_file)
        variables = {
            "descpath": os.path.relpath(spec_file, self.source_dir),
            "code": code_file,
            "header": header_file
        }
        self.writer.build([code_file, header_file], "spec-compile", spec_file, variables=variables)
        return code_file, header_file

    def spec_collect(self, spec_files, code_file, header_file):
        spec_files = [f if os.path.isabs(f) else os.path.join(self.source_dir, f) for f in spec_files]
        if not os.path.isabs(code_file):
            code_file = os.path.join(self.source_dir, code_file)
        if not os.path.isabs(header_file):
            header_file = os.path.join(self.source_dir, header_file)
        variables = {
            "descpath": os.path.relpath(code_file, self.source_dir),
            "code": code_file,
            "header": header_file
        }
        self.writer.build([code_file, header_file], "spec-collect", spec_files, variables=variables)
        return code_file, header_file

    def install(self, local_path, system_path, mode="644"):
        if not os.path.isabs(local_path):
            local_path = os.path.join(self.root_dir, local_path)
        variables = {
            "mode": mode,
            "descpath": os.path.relpath(local_path, self.root_dir)
        }
        self.writer.build(system_path, "install", local_path, variables=variables)
        return system_path

    def enable(self, deps, service, target_name="enable"):
        self.writer.build(target_name, "enable", deps, variables=dict(service=service))

    def start(self, deps, service, target_name="start"):
        self.writer.build(target_name, "start", deps, variables=dict(service=service))

    def stop(self, deps, service, target_name="stop"):
        self.writer.build(target_name, "stop", deps, variables=dict(service=service))

    def phony(self, name, targets):
        if isinstance(targets, str):
            targets = [targets]
        self.writer.build(name, "phony", " & ".join(targets))

    def default(self, target):
        self.writer.default(target)


parser = argparse.ArgumentParser(description="WebRunner configuration script")


def main():
    options = parser.parse_args()

    config = Configuration(options)

    kernel_specifications = []
    kernel_objects = []
    for source_dir, _, filenames in os.walk(os.path.join(config.source_dir, "kernels")):
        for filename in filenames:
            if os.path.basename(filename).startswith("."):
                continue

            fileext = os.path.splitext(filename)[1]
            if fileext == ".xml":
                specification_file = os.path.join(source_dir, filename)
                gen_c_file, gen_h_file = config.spec_compile(specification_file)
                gen_object = config.cc(gen_c_file)

                kernel_specifications.append(specification_file)
                kernel_objects.append(gen_object)
            elif fileext == ".c" and not filename.endswith("-gen.c"):
                kernel_objects.append(config.cc(os.path.join(source_dir, filename)))

    config.spec_collect(kernel_specifications, "runner/spec.c", "runner/spec.h")

    webserver_objects = [
        config.cc("webserver/server.c"),
        config.cc("webserver/request.c"),
        config.cc("webserver/options.c"),
        config.cc("webserver/logs.c"),
        config.cc("webserver/http.c"),
        config.cc("webserver/parse.c"),
    ]
    runner_objects = [
        config.cc("runner/perfctr.c"),
        config.cc("runner/median.c"),
        config.cc("runner/sandbox.c"),
        config.cc("runner/loader.c"),
        config.cc("runner/spec.c"),
    ]

    webrunner = config.ccld(webserver_objects + runner_objects + kernel_objects, "webrunner")
    config.default(webrunner)

    webrunner_program = config.install(webrunner, "/usr/sbin/webrunner", mode="755")
    webrunner_service = config.install("webrunner.service", "/etc/systemd/system/webrunner.service")
    config.enable([webrunner_program, webrunner_service], "webrunner.service")
    config.start([], "webrunner.service")
    config.stop([], "webrunner.service")
    config.phony("install", ["enable"])


if __name__ == "__main__":
    sys.exit(main())
