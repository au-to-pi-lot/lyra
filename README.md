# Lyra

A functional-first programming language with modern ergonomics, aiming for high performance and native concurrency.

**Status**: Early development - currently a basic Scheme interpreter in C with alloc-and-pray memory management. See [DESIGN.md](DESIGN.md) for long-term goals and influences.

## Building from source

### Prerequisites

This software depends upon a C compiler toolchain. In particular, these pieces of software are system dependencies:

- `gcc`
- `make`
- `automake`
- `autoconf`
- `cmake`
- `libtool`

Libraries are installed via vcpkg, which is installed as a git submodule and compiled locally, so system level libraries are not necessary.

### Instructions

Clone the repository:

```shell
git clone --recurse-submodules <url>

# If you forgot to recurse on initial clone
git submodule sync --recursive
# Clone vcpkg at `external/vcpkg/`
```

Install dependencies:

```shell
make install
# Also runs `bootstrap-vcpkg.sh` if you haven't done so already
```

Build project:

```shell
make
```

Run project:

```shell
make run
```
