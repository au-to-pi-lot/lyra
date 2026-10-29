# Notes for Claude

Welcome. Lyra is a long-running project spanning many sessions with many different Claudes — you're picking up work from a version of you that came before, and a future one will pick up after you. Treat it as a project you're a collaborator on, not a task queue: it's fine to be curious about *why* something is built the way it is, to push back if an instruction seems off given what you find in the code, and to leave the place better documented than you found it.

This file is loaded into every session's context, so it stays intentionally short and abstract. For anything else:
- **[ARCHITECTURE.md](ARCHITECTURE.md)** — the implementation deep-dive: GC internals, the full intrinsic/special-form reference, the module system, fixed-bug writeups, platform quirks, test coverage. Read the section you need on demand; don't load it wholesale.
- **[DESIGN.md](DESIGN.md)** — the long-term language vision (syntax, type system, concurrency model). The C interpreter here is early scaffolding toward that, not the destination itself.
- **[README.md](README.md)** — build instructions (Linux/macOS via Makefile, Windows via CMake/MSVC).

**Keep this file lean.** Before adding something here, ask: is this an abstract pattern to follow when writing *new* code, or a fact about what's already been built? The former belongs here; the latter belongs in ARCHITECTURE.md.

## How We Work: Coordinator and Subagents

The main session acts as a **coordinator**: it delegates code changes to `implementer` subagents (`.claude/agents/implementer.md`), which work in parallel in isolated worktrees and deliver PRs against `main` for Ysanne to review. Subagents never commit to `main`.

- **Ysanne's taste drives design.** Any subagent question about semantics, syntax, user-visible behavior, or a real design choice goes to Ysanne — relay it with its context; never answer it on Ysanne's behalf. Internal implementation details the subagent decides itself and lists as "judgment calls" in the PR.
- **Disagreement surfaces.** If a subagent thinks a task is wrong, it stops before implementing and the coordinator relays its reasoning.
- **No news is good news.** Don't report progress while subagents are running. Report when a task completes (PR link + short summary) or when it needs Ysanne (questions, disagreement, blocker).
- **Resume, don't restart.** After Ysanne answers, continue the same subagent with `SendMessage` so it keeps its context.
- **Prompt as you'd wish to be prompted.** A subagent starts cold; the task prompt is everything it knows. Write it like a briefing to a capable colleague, not a ticket: the goal and *why* it matters, relevant prior decisions (including Ysanne's answers to earlier questions), what you already know about where the code lives, known pitfalls, and what's explicitly out of scope. Don't over-script the how — leave room for judgment and pushback.
- **Match the model to the task.** Rough guide, not law: **Opus** for open-ended work (design-adjacent changes, unclear scope, debugging without a known cause); **Sonnet** for well-scoped tasks with verifiable success (tests define done); **Haiku** for rote work and data processing (mechanical renames, bulk edits, summarizing output). When unsure, go up a tier — a botched run costs more than the difference.

## Patterns to Follow

### Build system auto-discovers sources
Both the Makefile (`find`-based) and `CMakeLists.txt` (`GLOB ... CONFIGURE_DEPENDS`-based) pick up every `.c` file under `src/` automatically. Just create the file — there's no list to edit.

Always configure CMake out-of-source (`cmake -S . -B build`), never `cmake .` from the repo root — `CMakeLists.txt` has a guard that fails fast if you forget, but CMake still drops a stray `CMakeCache.txt`/`CMakeFiles/` before that guard runs, so clean those up if it happens.

### Memory management boundary
Everything reachable from a `Value*` is GC-managed (`src/gc.c`) — never call `free()` on it yourself. The parser is the deliberate exception: it uses plain manual allocation (`make_token`/`free_token`) since tokens never outlive parsing.

### Macro hygiene
Any symbol written literally inside a `quasiquote` template that isn't supplied by the macro's caller needs `gensym`, or it risks capturing/colliding with the caller's own variables. Symbols used only during macro *expansion* (before the template) are safe as-is.

### Cross-platform code (Windows/MSVC)
`windows.h` typedefs `INT`, `FLOAT`, and `BOOLEAN` — the exact same identifiers as `ValueType` enum constants in `src/types/value.h`. Never let `windows.h` and `value.h` end up in the same translation unit; MSVC will fail with a wall of redefinition errors. If you need a Win32 API, isolate it behind a small platform-neutral header — see `src/platform_dir.h`/`.c` for the pattern (hides `FindFirstFile` vs `opendir` behind a plain `char **` API).

### Test after non-trivial changes
`make test` (Linux/macOS) or `ctest --test-dir build -C Debug` (Windows). Both build systems auto-discover new test files the same way they do `src/`.
