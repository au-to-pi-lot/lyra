# Notes for Claude

**IMPORTANT**: Proactively update this file whenever you:
- Create new modules or significant components
- Learn important patterns or conventions in the codebase
- Identify TODOs or design decisions that should be remembered
- Discover quirks, gotchas, or non-obvious behavior
- Make architectural decisions

Think of this as your persistent memory across conversations!

## Project Setup

### vcpkg
The vcpkg package manager is located at `./external/vcpkg/vcpkg`.

To search for packages:
```bash
./external/vcpkg/vcpkg search <package-name>
```

### Build System
The makefile automatically picks up **all** `.c` files in `src/` using `find`. You don't need to manually add new source files to the makefile - just create them in `src/` and they'll be included in the build.

## Architecture Notes

### Memory Management
- **GC Implementation**: Basic mark-and-sweep garbage collector in `src/gc.c`
  - All allocations tracked in a linked list via `GCObject` headers
  - Mark phase starts from root closure and recursively marks reachable objects
  - Sweep phase frees unmarked objects
  - Threshold auto-adjusts: starts at 1000 objects, doubles based on live set size
  - **TODO**: Need to replace `malloc()` calls throughout codebase with `gc_alloc_*()` functions
  - **TODO**: Need to integrate GC triggers (call `gc_collect()` when threshold reached)

### Value Types
- Core types defined in `src/types/value.h`: CONS, INT, FLOAT, BOOLEAN, STRING, SYMBOL, FUNCTION, MACRO
- Closures use uthash for variable storage
- Callables can be intrinsic (C functions) or derived (user-defined Lisp functions/macros)
