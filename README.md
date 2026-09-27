# Lyra

A functional-first programming language with modern ergonomics, aiming for high performance and native concurrency.

**Status**: Early development - currently a basic Scheme interpreter in C with a mark-and-sweep garbage collector. See [DESIGN.md](DESIGN.md) for long-term goals and influences.

## Building from source

Clone the repository with submodules (vcpkg is checked out at `external/vcpkg/`):

```shell
git clone --recurse-submodules <url>

# If you forgot to recurse on initial clone
git submodule sync --recursive
```

Libraries (`uthash`, `check`, `replxx`) are installed via vcpkg, which is compiled locally as part of the build, so no system-level libraries are needed beyond a compiler toolchain.

### Linux / macOS (Makefile)

Prerequisites: `gcc`, `make`, `automake`, `autoconf`, `cmake`, `libtool`.

```shell
make install   # bootstraps vcpkg and installs dependencies
make           # build ./lyra
make run       # run the REPL
make test      # run the test suite
```

### Windows (MSVC + CMake)

Prerequisites: [Visual Studio Build Tools 2022](https://visualstudio.microsoft.com/downloads/) with the "Desktop development with C++" workload, and [CMake](https://cmake.org/download/) (3.21+).

From a **Developer PowerShell/Command Prompt for VS 2022** (or after running `vcvars64.bat`), in the repo root:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

The first configure step bootstraps vcpkg and installs dependencies automatically (no separate `vcpkg install` step needed). Resulting binaries land in `build\Debug\`:

```powershell
.\build\Debug\lyra.exe
ctest --test-dir build -C Debug --output-on-failure   # run the test suite
```

CMake also works on Linux/macOS (`-G "Unix Makefiles"` or `Ninja`) if you'd rather not use the Makefile.
