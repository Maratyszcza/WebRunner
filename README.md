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

# REST API

WebRunner commands must follow the pattern `http://server[:port]/machine-id/command[?query]`

- `machine-id` is an arbitrary string. It is parsed, but ignored by the WebRunner.
- `command` is one of the supported commands (**monitor** or **run**).
- `query` is an optional query string with command parameters.

## **monitor** command

The **monitor** command is used to check server status.

##### HTTP request

- Method: `HEAD`

- URL: `http://server[:port]/machine-id/monitor`

##### HTTP response

A server would respond HTTP status ok 200 (OK) to this command.

##### Example

```bash
curl --head "http://localhost:8081/local/monitor"
```

## **run** command

The **run** command is used to benchmark and analyze a function in an ELF object. The ELF object must be sent in the request body.

##### HTTP request

- Method: `POST`

- Content-Type: `application/octet-stream`

- URL: `http://server[:port]/machine-id/run?kernel=kernel-name&[param1=value1&param2=value2&...]`

The `kernel` parameter specifies kernel type. Query parameters after it depend on the kernel type and specify parameters of the kernel run. Look at XML specifications in the [`/src/kernels`](https://github.com/Maratyszcza/WebRunner/tree/master/src/kernels) directory for permitted kernel types and their parameters.

##### HTTP response

The server would respond with a line of names of hardware performance counters and their values (one per line)

##### Example

```bash
wget --header="Content-Type:application/octet-stream" --post-file=sdot.o \
  "http://localhost:8081/local/run?kernel=sdot&n=10000&incx=1&incy=2"
```
