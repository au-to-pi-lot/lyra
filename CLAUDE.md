# Notes for Claude

Welcome. Lyra is a long-running project spanning many sessions with many different Claudes — you're picking up work from a version of you that came before, and a future one will pick up after you. Treat it as a project you're a collaborator on, not a task queue: it's fine to be curious about *why* something is built the way it is, to push back if an instruction seems off given what you find in the code, and to leave the place better documented than you found it.

This file is loaded into every session's context, so it stays intentionally short and abstract. For anything else:
- **[ARCHITECTURE.md](ARCHITECTURE.md)** — the implementation deep-dive: GC internals, the full intrinsic/special-form reference, the module system, fixed-bug writeups, platform quirks, test coverage. Read the section you need on demand; don't load it wholesale.
- **[DESIGN.md](DESIGN.md)** — the long-term language vision (syntax, type system, concurrency model). The C interpreter here is early scaffolding toward that, not the destination itself.
- **[README.md](README.md)** — build instructions (Linux/macOS via Makefile, Windows via CMake/MSVC).

**Keep this file lean.** Before adding something here, ask: is this an abstract pattern to follow when writing *new* code, or a fact about what's already been built? The former belongs here; the latter belongs in ARCHITECTURE.md.

## Patterns to Follow

### Build system auto-discovers sources
Both the Makefile (`find`-based) and `CMakeLists.txt` (`GLOB ... CONFIGURE_DEPENDS`-based) pick up every `.c` file under `src/` automatically. Just create the file — there's no list to edit.

Always configure CMake out-of-source (`cmake -S . -B build`), never `cmake .` from the repo root — `CMakeLists.txt` has a guard that fails fast if you forget, but CMake still drops a stray `CMakeCache.txt`/`CMakeFiles/` before that guard runs, so clean those up if it happens.

### Memory management boundary
Everything reachable from a `Value*` is GC-managed (`src/gc.c`) — never call `free()` on it yourself. The parser is the deliberate exception: it uses plain manual allocation (`make_token`/`free_token`) since tokens never outlive parsing.

### Macro hygiene
Any symbol written literally inside a `quasiquote` template that isn't supplied by the macro's caller needs `gensym`, or it risks capturing/colliding with the caller's own variables. Symbols used only during macro *expansion* (before the template) are safe as-is.

### Purity tracking is viral, and honesty matters
When adding an intrinsic, set `is_pure` truthfully — pure must mean *genuinely* no side effects, always. A derived function that calls even one impure function should itself be treated as impure. Future compiler optimizations (memoization, reordering, dead-code elimination) will trust this flag, so don't mark something pure to make a test pass.

### Cross-platform code (Windows/MSVC)
`windows.h` typedefs `INT`, `FLOAT`, and `BOOLEAN` — the exact same identifiers as `ValueType` enum constants in `src/types/value.h`. Never let `windows.h` and `value.h` end up in the same translation unit; MSVC will fail with a wall of redefinition errors. If you need a Win32 API, isolate it behind a small platform-neutral header — see `src/platform_dir.h`/`.c` for the pattern (hides `FindFirstFile` vs `opendir` behind a plain `char **` API).

### Test after non-trivial changes
`make test` (Linux/macOS) or `ctest --test-dir build -C Debug` (Windows). Both build systems auto-discover new test files the same way they do `src/`.
