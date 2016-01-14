# WebRunner (PeachPy.IO backend)
WebRunner is a primitive HTTP server for secure execution of PeachPy kernels.

# Dependencies

## Required
- Linux kernel >= 3.17
- Python 2.7
- [Ninja](https://ninja-build.org) build system (`sudo apt-get install ninja-build`)
- [ninja-syntax](https://pypi.python.org/pypi/ninja_syntax/) module (`sudo pip install ninja-syntax`)

## Recommended
- systemd (WebRunner includes service configuration only for systemd)
- Ubuntu 15.10 (WebRunner was tested only on this distribution)

## Optional
- [PeachPy](https://github.com/Maratyszcza/PeachPy) (required to run [the example](https://github.com/Maratyszcza/WebRunner/tree/master/example))

# Build and Installation

To build WebRunner:

```bash
./configure.py
ninja
```

To install WebRunner to `/usr/sbin/webrunner` and register as a **systemd** service:

```bash
sudo ninja install
```

Then you can start the service with `sudo ninja start` and terminate it with `sudo ninja stop`

For development, you can run WebRunner without installation:

```bash
./webrunner # webrunner -h to list options
```
