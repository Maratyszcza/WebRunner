# WebRunner (PeachPy.IO backend)

WebRunner is a service to execute user-supplied untrusted machine code on your server without compromising its security.

Key features:

- REST API (i.e. you communicate with the service through stateless HTTP requests)
- Built-in loader for ELF object files
- Sandboxing of untrusted code through [seccomp-bpf](https://lwn.net/Articles/475043/) mechanism
- Benchmarking and analyzing the code with hardware event counters.
- Self-check command to support automation of service downtime 
- Extendable set of supported kernels

# WebRunner dependencies

## Required dependecies
- Linux kernel >= 3.17
- Python 2.7
- [Ninja](https://ninja-build.org) build system (`sudo apt-get install ninja-build`)
- [ninja-syntax](https://pypi.python.org/pypi/ninja_syntax/) module (`sudo pip install ninja-syntax`)

## Recommended dependecies
- systemd (WebRunner includes service configuration only for systemd)
- Ubuntu 15.10 (WebRunner was tested only on this distribution)

## Optional dependecies
- [PeachPy](https://github.com/Maratyszcza/PeachPy) (required to run [the example](https://github.com/Maratyszcza/WebRunner/tree/master/example))

# Building WebRunner

Configure and compile:

```bash
./configure.py
ninja
```

**Recommended:** install WebRunner to `/usr/sbin/webrunner` and register as a **systemd** service:

```bash
sudo ninja install
```

After installation you can start the service with `sudo ninja start` and terminate it with `sudo ninja stop`

**Alternative:** run WebRunner without installation:

```bash
./webrunner # webrunner -h to list options
```
