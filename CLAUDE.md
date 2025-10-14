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
    - **Cycle detection**: Checks if objects already marked before recursing to handle cycles (e.g., closures containing functions that reference the same closure)
  - Sweep phase frees unmarked objects with proper destructors (e.g., `utstring_free()` for `GC_UTSTRING` types)
  - Threshold auto-adjusts: starts at 1000 objects, doubles based on live set size
  - GC runs at safe points: after each REPL expression and after file evaluation
  - Heap can grow during evaluation but gets cleaned up between expressions
  - **Cleanup at exit**: `gc_free_all()` performs two-pass cleanup:
    1. First pass clears uthash hash tables in closures (frees uthash internal structures)
    2. Second pass frees all GC objects with appropriate destructors
  - ✅ **Verified leak-free** with valgrind on trivial commands

- **Parser Memory Management**: Parser uses manual memory management (not GC)
  - `make_token()` takes ownership of `UT_string` buffers (transfer semantics, not copy)
  - All tokens freed via `free_token()` after use

### Value Types
- Core types defined in `src/types/value.h`: CONS, INT, FLOAT, BOOLEAN, STRING, SYMBOL, FUNCTION, MACRO
- Closures use uthash for variable storage
- Callables can be intrinsic (C functions) or derived (user-defined Lisp functions/macros)

### Recursion
- **Z Combinator**: The Z combinator (call-by-value Y combinator) now works correctly for recursive functions
  - Fixed GC bugs that were incorrectly freeing live values in closures
  - The Z combinator creates complex closure references, making it a good stress test for the GC
  - Example: `(define factorial (Z (lambda (self) (lambda (n) (if (= n 0) 1 (* n (self (- n 1))))))))` works correctly
