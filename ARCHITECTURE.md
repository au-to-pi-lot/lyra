# Lyra: Implementation Architecture

This is the deep-dive reference for the current C interpreter — how the GC works, what every intrinsic does, bugs that got fixed and why, the module system, platform quirks. It's meant to be **read in sections on demand**, not loaded wholesale. If you're about to touch a subsystem, read that subsystem's section here first; otherwise you probably don't need this file open.

See [CLAUDE.md](CLAUDE.md) for the short list of patterns to follow, and [DESIGN.md](DESIGN.md) for where the language is ultimately headed.

## Project Setup

### vcpkg
The vcpkg package manager is located at `./external/vcpkg/vcpkg` (or `vcpkg.exe` on Windows).

To search for packages:
```bash
./external/vcpkg/vcpkg search <package-name>
```

## Build System
The makefile automatically picks up **all** `.c` files in `src/` using `find`. `CMakeLists.txt` does the same with `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)`. Neither needs a manual source list updated when you add a file.

### Debug Flags
You can enable debug output for specific subsystems by adding `-D` flags to `CFLAGS` in the makefile:
- `-DDEBUG_LAMBDA`: Shows debug output for lambda creation (parameter processing, variadic args)
- `-DDEBUG_MACRO`: Shows debug output for macro expansion (parameters, bindings, expansion result)
- `-DDEBUG_QUASIQUOTE`: Shows debug output for quasiquote/unquote processing

Example:
```bash
# Add to makefile CFLAGS line:
CFLAGS = -Wall -Wextra -std=c11 -g -DDEBUG_MACRO -DDEBUG_QUASIQUOTE
```

### Windows / MSVC support
- REPL line editing uses **replxx** (`vcpkg.json`), not libedit/ncurses - libedit has no real MSVC port. `src/main.c`'s `repl()` uses `replxx_init`/`replxx_input`/`replxx_history_add`/`replxx_end`.
- `isatty`/`STDIN_FILENO` are aliased to `_isatty`/`_fileno(stdin)` under `_WIN32` in `src/main.c`.
- **Gotcha**: `types/value.h`'s `ValueType` enum has constants literally named `INT`, `FLOAT`, `BOOLEAN` - these collide with `windows.h`'s typedefs of the same names (`minwindef.h`/`winnt.h`). Any file that includes `windows.h` must NOT also see `value.h` in the same translation unit, or MSVC fails with a cascade of redefinition errors starting at `minwindef.h`. (See [CLAUDE.md](CLAUDE.md)'s "Cross-platform code" pattern.)
  - This is why directory listing (used by the module system's `list-directory` intrinsic) is isolated in `src/platform_dir.c`/`.h`: a tiny platform-neutral C API (`platform_list_directory`/`platform_free_directory_listing`) that hides `windows.h` (Win32 `FindFirstFile`/`FindNextFile`) vs `dirent.h` (POSIX `opendir`/`readdir`) behind a plain `char **` interface. `src/intrinsics.c` only sees the neutral header.
- `find_path(UTHASH_INCLUDE_DIR ...)` is required in `CMakeLists.txt` because uthash's vcpkg port is header-only and ships no CMake config.
- `check`'s vcpkg CMake config exports `Check::check` (static) or `Check::checkShared` (shared) - not lowercase `check::check`. `CMakeLists.txt` uses the generator-expression form the port itself recommends: `$<IF:$<TARGET_EXISTS:Check::check>,Check::check,Check::checkShared>`.
- `replxx::replxx`'s exported target links `Threads::Threads`, so `find_package(Threads REQUIRED)` must run before `find_package(replxx CONFIG REQUIRED)`.
- Verified working: `cmake -S . -B build -G "Visual Studio 17 2022" -A x64 && cmake --build build --config Debug` - all 90 tests pass via `ctest`, and the CLI/REPL/module-import/match all work correctly.

### Building in a git worktree
Implementer subagents work in git worktrees (under `.claude/worktrees/`), and `git worktree add` doesn't check out submodules, so `external/vcpkg/` is empty there. `CMakeLists.txt` defaults `CMAKE_TOOLCHAIN_FILE` to that path, so configuring fails. Point it at the main checkout's vcpkg instead:
```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<main-checkout>/external/vcpkg/scripts/buildsystems/vcpkg.cmake
```
(`git submodule update --init` inside the worktree also works, but it re-clones vcpkg, which is slow.) The Makefile's `install` target expects `./external/vcpkg` too, so on Linux/macOS the same issue applies there.

**Concurrent MSVC builds**: when two worktrees built at the same time, one `cl.exe` compile hung for about 10 minutes, sitting idle. Killing it and rebuilding worked fine. The cause is unconfirmed (possibly contention over the shared PDB server, `mspdbsrv.exe`). If a build stalls with an idle compiler, kill it and retry before you debug anything else.

## Memory Management
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
  - `parse_with_pos()` returns consumed character count, enabling multi-expression file parsing

- **File Evaluation**: `run_file()` now evaluates all expressions in a file
  - Uses `parse_with_pos()` to track consumed characters
  - Advances through file sequentially, parsing and evaluating each expression
  - Only prints the result of the final expression
  - Skips trailing whitespace/comments gracefully

## Panics (Unrecoverable Errors)
Lyra's error model (DESIGN.md "Error Handling"): **panic** for programmer errors, **ok/err result values** for recoverable ones. Results don't exist yet; panics are implemented in [src/panic.h](src/panic.h)/[src/panic.c](src/panic.c).

- **Mechanism**: `lyra_panic(fmt, ...)` formats a message into a static buffer, pops the innermost `PanicHandler`, and `longjmp`s to it. Handlers form a stack (`panic_push`/`panic_pop`); the caller does `setjmp(handler.env)` itself in its own frame (see the usage block in `panic.h`). With no handler installed, the message is printed and the process exits 1
- **No catching from Lyra code**: only C installs handlers. A panic always unwinds to the top level
- **Top-level handlers** ([src/main.c](src/main.c)):
  - REPL (`repl_eval`): prints `panic: <message>` to **stderr**, returns to the prompt. The session survives; definitions completed before the panic are kept (nothing is rolled back)
  - File and `-c` mode (`run_source`): prints `panic: <message>` to stderr and exits with `panic_exit_status()`: 1 (`PANIC_DEFAULT_STATUS`) unless the panic came from `lyra_panic_with_status` / `panic-with-status`
  - `panic_report()` flushes stdout before writing to stderr so output ordering is sane
- **What panics**: undefined variables (`undefined variable: \`x\``), non-callable call heads (`not callable: 5`), arity/type errors in intrinsics, malformed `lambda` parameter lists, `define-macro` on a non-function, `match` with no matching clause or with `,@` in a pattern, `eval-string` parse errors, and explicit `(panic ...)`
- **What doesn't (yet)**: I/O failures (`load-file` can't open a file, `list-directory` can't open a directory) still print and return `nil`; they're marked `// TODO: result` and will become err results. `get-var` on an unbound symbol returns `nil` by design (the module system uses it as a "defined?" check).
- **Bad arguments** (message style `<name> expects ..., but got <repr>`): wrong arity almost everywhere (`car`, `cons`, `=`, `<`, `divmod`, `apply`, the `is-*` predicates, `if`, `define`, `quote`, ...); non-numbers to `+ - * / < <= > >=`; non-lists to `car`/`cdr`/`cons`/`apply`; non-integers to `divmod`; `division by zero` from `/` and `divmod`; calling a derived function or macro with the wrong number of arguments (`wrong number of arguments: expected 2, but got 1`, or `expected at least N` with a variadic) and destructuring a non-list
- **Deliberately not panics**: `(car nil)`/`(cdr nil)` are `nil`; `=` across different types (including `(= 1 1.0)`) is `false`. `()` evaluates to `nil` (self-evaluating, so `(())` panics with `not callable: ()`). `<`/`<=`/`>`/`>=` panic on mixed int/float even though `+ - * /` promote
- **GC safety**: collection runs only at top-level safe points (after a REPL expression / file / `-c` run), never mid-evaluation, so a panic can't interrupt a collection. Whatever the aborted evaluation allocated stays registered and is swept by the next `gc_collect` if unreachable. `gc_alloc_cons` initializes `car`/`cdr` to `NIL`, so a half-built list never holds garbage pointers
- **Parser memory**: the parser never panics; it reports failure by returning `NULL`. So its manually managed tokens can't leak through a `longjmp`. Callers that turn a parse failure into a panic (`eval-string`) do so after `parse_with_pos` has returned
- **setjmp hygiene**: keep functions that call `setjmp` small (`repl_eval`, `run_source`, the test helpers), so no locals change between `setjmp` and a possible `longjmp`. Locals that must be read in the panic branch after being changed need `volatile`
- **Testing**: `eval_panics()` / `ck_assert_panics()` in [tests/test_eval.c](tests/test_eval.c) evaluate with a handler installed and expose the message. Keep `ck_assert` calls outside the protected region: in Check's no-fork mode (Windows) a failed assertion `longjmp`s too and would skip `panic_pop`

## Value Types
- Core types defined in `src/types/value.h`: CONS, INT, FLOAT, BOOLEAN, STRING, SYMBOL, FUNCTION, MACRO
- Closures use uthash for variable storage, keyed by the symbol's `as_symbol` UT_string bytes (with explicit length, so embedded NULs count)
- **Interned vs. uninterned symbols** (`src/types/symbol.h`): both are SYMBOL (`is-symbol` is true for both).
  - Interned symbols (from the reader and `symbol`) are equal when their names are equal.
  - Uninterned symbols (from `unique` and `gensym`) are equal only to themselves, never to an interned symbol of the same name.
  - Representation: `as_symbol` holds the symbol's *identity key*. An interned key is just the name; an uninterned key is `name '\0' <serial>`, the serial coming from `gc->uninterned_counter`. Interned names can't contain NUL (they're only ever built from C strings), so no interned key can equal an uninterned one, and serials never repeat within an interpreter. `utstring_body` still gives the print name.
  - Because closures key on the full key, an uninterned symbol used as a binding name (`(lambda (,g) ...)`) is its own variable. It can't collide with a user variable of the same name or with another uninterned symbol.
  - Use the helpers: `symbol_eq` (equality, used by `=`, `match` and the module registry), `symbol_is(v, "quote")` (recognizing special forms and keywords: interned only, so a `(unique 'quote)` is not `quote`), `symbol_is_interned`, and `symbol_key_suffix` (strips a `*` variadic prefix while keeping the hidden serial). Don't `strcmp` symbol bodies; that compares only print names.
  - Printing: an uninterned symbol prints as `#:name`. The reader can't read that back as the same symbol. `(string s)` returns just the name (`"name"`), and `(symbol (string s))` gives back the *interned* symbol: round-tripping through text always loses identity.
- Callables can be intrinsic (C functions) or derived (user-defined Lisp functions/macros)

## Purity Tracking
- **Design**: Functions track whether they have side effects via `is_pure` flag in `Callable` struct
  - Pure functions: can be memoized, reordered, constant-folded by compiler
  - Impure functions: have side effects (I/O, mutation, etc.)
- **Intrinsics declare purity**: Arithmetic/list operations are pure, I/O operations are impure
- **Purity is viral**: If a derived function calls any impure function, it becomes impure (future work)
- **Query at runtime**: `(is-pure fn)` returns true/false
- **Use case**: Compiler can use this for optimizations (memoization, dead code elimination, etc.)

**Pure intrinsics**: `+`, `-`, `*`, `/`, `divmod`, `=`, `car`, `cdr`, `cons`, `get-var`, `is-pure`, `string`, `symbol`
**Impure intrinsics**: `panic`/`panic-with-status` (abort evaluation), `gensym` (mutates counter), `load-file` (file I/O), `eval-string` (dynamic eval), `print`/`write` (stdout), `expand-macro`/`expand-macro-1` (run macro bodies), `apply` (purity-polymorphic, conservatively impure)

## Recursion & Macros
- **Z Combinator**: The Z combinator (call-by-value Y combinator) now works correctly for recursive functions
  - Fixed GC bugs that were incorrectly freeing live values in closures
  - The Z combinator creates complex closure references, making it a good stress test for the GC
  - Updated to support variadic arguments: `(lambda (*args) (apply (x x) args))`
  - Example: `(define factorial (Z (lambda (self) (lambda (n) (if (= n 0) 1 (* n (self (- n 1))))))))` works correctly

- **Variadic Arguments**: Functions and macros support variadic parameters with `*name` syntax
  - `(lambda (a b *rest) body)` - collects remaining args into `rest` list
  - **Suffix-only**: Variadic parameter must be the last parameter (no infix/prefix variadics)
  - Internally converted to `VARIADIC_MARKER` type (cannot be forged from Lisp code - hygienic!)
  - Works for both derived functions and derived macros
  - Example: `(define sum (lambda (*nums) ...))`
  - Invalid: `(lambda (first *middle last) ...)` - error: variadic must be last

- **`fn` Macro**: Convenient syntax for defining recursive functions
  - Syntax: `(fn name (params...) body)`
  - Expands to: `(define name (Z (lambda (name) (lambda (params...) body))))`
  - Automatically wraps function in Z combinator for recursion
  - Example: `(fn factorial (n) (if (= n 0) 1 (* n (factorial (- n 1)))))`

- **Quasiquote/Unquote**: Template mechanism for macros
  - Backtick `` ` `` creates a template, comma `,` evaluates within template
  - Comma-at `,@` splices a list into the surrounding list
  - Example: `` `(define ,name (lambda ,params ,body))``
  - Works from files; **don't use in REPL** (EditLine waits for closing backtick)

## Intrinsic Functions & Special Forms
Available built-in functions and special forms defined in [src/prelude.c](src/prelude.c):

**Special Forms** (don't evaluate arguments):
- `lambda` - Create anonymous function: `(lambda (x y) (+ x y))`
- `if` - Conditional: `(if condition then-expr else-expr)`
- `define` - Define variable: `(define x 10)`
- `define-macro` - Define macro: `(define-macro name (lambda ...))`
- `quote` - Return unevaluated: `(quote (1 2 3))` or `'(1 2 3)`
- `quasiquote` - Template with selective evaluation: `` `(a ,b c)``
- `begin` - Sequence expressions without creating scope: `(begin expr1 expr2 expr3)` → returns value of expr3
  - **Critical for macros**: Unlike `(lambda () ...)`, `begin` doesn't create a new closure
  - Allows `define` in macro expansions to affect caller's scope
- `module` - Declare module with exports: `(module name (export sym1 sym2 ...) body...)`
  - Evaluates body and registers specified symbols in global `*module-exports*` registry
  - Multiple files can declare the same module - exports are merged
  - See Module System section for details

**Arithmetic**:
- `+`, `-`, `*`, `/` - Basic arithmetic (variadic, auto-promote int→float)
- `divmod` - Integer division with remainder: `(divmod 17 5)` → `(3 2)`
- `=` - Equality test: `(= a b)`. Values of different types are never equal (`(= 1 1.0)` → `false`)
  - Atoms: numbers by value, strings by contents, booleans by value, interned symbols by name, uninterned symbols (`unique`/`gensym`) by identity
  - Lists are compared **structurally**, recursively, applying the atom rules at every level: `(= '(1 (2 "x")) '(1 (2 "x")))` → `true`. A list of uniques equals another only if it holds the same uniques
  - Functions and macros are equal only to themselves: `(= car car)` → `true`, but two separately written `(lambda (x) x)` are not equal
  - Implemented by `values_equal` in `src/intrinsics.c`. `match` uses the same function for literal and `,expr` patterns, so the three can't drift apart

**List Operations**:
- `car` - Get first element: `(car '(1 2 3))` → `1`
- `cdr` - Get rest of list: `(cdr '(1 2 3))` → `(2 3)`
- `cons` - Prepend element: `(cons 1 '(2 3))` → `(1 2 3)`
- `apply` - Call function with list as args: `(apply + '(1 2 3))` → `6`

**Type Constructors** (polymorphic):
- `string` - Convert to string or concatenate
  - `(string 'foo)` → `"foo"` (symbol to string)
  - `(string 42)` → `"42"` (int to string)
  - `(string "hello" " " "world")` → `"hello world"` (concatenation)
- `symbol` - Convert to symbol
  - `(symbol "foo")` → `'foo` (string to symbol)
  - `(symbol 'foo)` → `'foo` (identity)

**Type Predicates** (all pure):
- `is-cons` - Check if value is a cons cell/list: `(is-cons '(1 2 3))` → `true`
- `is-int` - Check if value is an integer: `(is-int 42)` → `true`
- `is-float` - Check if value is a float: `(is-float 3.14)` → `true`
- `is-boolean` - Check if value is a boolean: `(is-boolean true)` → `true`
- `is-string` - Check if value is a string: `(is-string "foo")` → `true`
- `is-symbol` - Check if value is a symbol: `(is-symbol 'foo)` → `true`
- `is-function` - Check if value is a function: `(is-function +)` → `true`
- `is-macro` - Check if value is a macro: `(is-macro lambda)` → `true`

**Macro Utilities**:
- `gensym` - Generate a fresh **uninterned** symbol: `(gensym)` → `#:G__0`, then `#:G__1`, ...
  - Its identity makes it unique, not its name. Typing `'G__0` gives an interned symbol that is *not* equal to it, so macro-generated bindings can't be forged or captured
  - The numbered print name only keeps `expand-macro` output readable
  - Use in macros to avoid variable name conflicts
  - Essential for writing hygienic macros
- `unique` - Make a fresh uninterned symbol: `(unique 'file-not-found)` or `(unique "file-not-found")` → `#:file-not-found`; `(unique)` → `#:unique`
  - Equal to nothing but itself: `(= (unique 'x) (unique 'x))` → `false`, and `(= (unique 'x) 'x)` → `false`
  - The optional name may be a symbol or a string (both are intended, not an implicit conversion), and it is only for printing. Anything else, or more than one argument, panics. Meant for values like error identities that no other code can recreate
  - Match one held in a variable with a value pattern: `(match e (,file-not-found ...) ...)`. Writing `'file-not-found` in a pattern gives the interned symbol, which won't match
- `get-var` - Get value of variable by symbol: `(get-var 'x)` → value of x
  - Used internally by module system
- `is-pure` - Check if function is pure: `(is-pure +)` → `true`
- `expand-macro-1` - Expand a quoted form **one step** without evaluating it: `(expand-macro-1 '(let ((x 1)) x))` → `((lambda (x) x) 1)`
- `expand-macro` - Expand repeatedly **at the head** until the head is no longer a macro (subforms are not expanded): `(expand-macro '(fn id (x) x))` → `(define id (Z (lambda (id) (lambda (x) x))))`
  - Both are ordinary functions, so the form must be quoted (it may also be computed); the head is resolved in the caller's scope, but the macro body runs in its definition-site scope
  - Only derived (user/prelude) macros expand. Special forms (`if`, `lambda`, `define`, ...), function calls, unbound heads and atoms come back unchanged
  - No `expand-macro-all`: a tree walker (e.g. a compiler written in Lyra) is expected to call these itself
  - Impure: expansion runs the macro body (which may call `gensym`, etc.). Strictly, purity depends on the macro being expanded

**File I/O** (impure):
- `load-file` - Read file contents as string: `(load-file "module.lyra")` → file contents as a string
- `eval-string` - Evaluate string as code: `(eval-string "(+ 1 2)")` → `3`
  - Evaluates in **current closure** - definitions affect caller's scope

**Output** (impure):
- `print` - Write strings to stdout followed by a newline: `(print "a" "b")` writes `ab` + newline, returns `nil`
  - Any number of arguments, all of which must be strings (no implicit conversion); otherwise panics (`print requires string arguments`) before writing anything. `(print)` emits just a newline
- `write` - Same as `print` but with no trailing newline: `(write "a" "b")` writes `ab`
  - Both share `write_strings` in `intrinsics.c` and flush after every call
  - Named `write` so a later task can add a file-handle first argument
  - Output goes to `stdout` unless redirected with `intrinsic_set_print_stream(FILE*)` (test hook)

**Errors** (impure):
- `panic` - Abort evaluation with a message: `(panic "bad value: " (string x))`
  - Same argument rules as `print`: any number of strings, concatenated with no separator; a non-string argument is itself a panic (`panic requires string arguments`). `(panic)` has an empty message
  - Not catchable from Lyra code; see the Panics section
- `panic-with-status` - Same, with an explicit process exit status first: `(panic-with-status 2 "bad config: " path)`
  - Status must be an int in 1..255 (0 would report success; POSIX truncates beyond 255); otherwise it's an ordinary status-1 panic
  - In file / `-c` mode the process exits with that status. In the REPL it behaves exactly like `panic` (message printed, back to the prompt, status ignored)

**Higher-Level Forms** (defined in Lisp):
- `Z` - Z combinator for recursion
- `fn` - Convenient recursive function definition: `(fn factorial (n) ...)`
- `foldl` - Left fold over list: `(foldl + 0 '(1 2 3))` → `6`
  - Signature: `(foldl f acc lst)`
  - Reduces list from left using accumulator
  - Tail-recursive (can be TCO-optimized)
  - Processes as: `(f (f (f acc x1) x2) x3)` (left-associative)
- `foldr` - Right fold over list: `(foldr cons '(1 2 3) '())` → `(1 2 3)`
  - Signature: `(foldr f lst acc)`
  - Reduces list from right using accumulator
  - NOT tail-recursive (requires stack space)
  - Processes as: `(f x1 (f x2 (f x3 acc)))` (right-associative)
  - Use for operations where associativity matters (cons, append)
- `reverse` - Reverse a list: `(reverse '(1 2 3))` → `(3 2 1)`
  - Implemented as: `(foldl (lambda (acc item) (cons item acc)) nil lst)`
- `map` - Apply function to each element: `(map (lambda (x) (* x 2)) '(1 2 3))` → `(2 4 6)`
  - Signature: `(map f lst)`
  - Implemented using `foldl` and `reverse` to preserve order
- `filter` - Keep elements that satisfy predicate: `(filter even? '(1 2 3 4))` → `(2 4)`
  - Signature: `(filter pred lst)`
  - Implemented using `foldl` and `reverse` to preserve order

**Macros** (defined in Lisp):
- `let` - Local variable bindings: `(let ((x 10) (y 20)) (+ x y) (* x y))` → `200`
  - Expands to: `((lambda (x y) (+ x y) (* x y)) 10 20)`
  - Supports implicit progn (multiple body expressions)
  - Uses `map` to extract variable names and values from bindings
- `import` - Module system: `(import module-name (symbol1 symbol2 ...))`
  - Example: `(import math (square add-twice))`
  - Loads `"module-name.lyra"` if not already loaded
  - Makes all symbols from module available in current scope
  - Track loading via `*module-loaded-<name>*` marker variables
  - **Implementation**: Fully derived macro (not an intrinsic!)
  - Uses `begin`, `string`, `symbol`, `eval-string`, and `load-file`
- `match` - Pattern matching (intrinsic special form, [src/intrinsics.c:1069](src/intrinsics.c#L1069))
  - Syntax: `(match expr ((pattern1) body1) ((pattern2) body2) ...)`
  - Supports: literal values, quoted symbols, variable binding, list patterns, variadic patterns (`*args`), value patterns (`,expr`)
  - **Value patterns** `,expr` (read as `(unquote expr)`), mirroring quasiquote: `expr` is evaluated and the pattern matches a value equal to the result. Equality is exactly `=` (shared with literal patterns), so uninterned symbols match only themselves and lists match structurally. Value patterns bind nothing and may be nested anywhere in a list pattern: `((,tag x) x)`
    - `expr` is evaluated in the scope *around* the `match`, never seeing bindings from the pattern it sits in: in `((x ,x) ...)` the `,x` is the outer `x`. Evaluation happens while matching, so only for clauses that are actually tried, and possibly more than once if it appears in several clauses
    - `,@expr` in a pattern panics: `match: ,@ is not allowed in a pattern, but got (unquote-splicing ...)`
    - Inside a macro's quasiquote template, `,` is consumed by the template. To emit a value pattern, build it: `,(cons 'unquote (cons 'u nil))`, or splice the value itself as a quoted literal: `(quote ,u)`
  - Example: `(match '(lambda (x) x) (('lambda params body) params) ((x) 'no-match))`
  - See tests in `tests/test_eval.c` for full spec

## Module System

### Current Implementation (V2 - Multi-File Modules) ✅
The module system supports **multi-file modules** with explicit exports! Multiple files can contribute to the same module, enabling mutual recursion across files (like C# partial classes).

**Architecture**:
- `module` is an **intrinsic special form** in [src/intrinsics.c](src/intrinsics.c#L622)
- `import` is a **derived macro** in [src/prelude.c](src/prelude.c#L149) (written in Lisp!)
- Global registry `*module-exports*` stores exports: `((module-name ((sym1 . val1) ...)) ...)`
- `module` intrinsic walks to root closure to ensure registry is always global

**Declaring a module**:
```lisp
; In math/core.lyra
(module math
  (export square double)

  (define square (lambda (x) (* x x)))
  (define double (lambda (x) (+ x x))))

; In math/extra.lyra
(module math
  (export cube half)

  (define cube (lambda (x) (* x x x)))
  (define half (lambda (x) (/ x 2))))
```

**Importing a module**:
```lisp
; Loads all .lyra files in math/ directory
(import math (square double cube half))

(square 5)   ; → 25
(cube 3)     ; → 27
(half 10)    ; → 5.0
(double 7)   ; → 14
```

**How it works**:
1. **Module declaration**: `(module name (export sym1 sym2 ...) body...)`
   - Evaluates body in current closure (definitions happen there)
   - Looks up exported symbols and their values
   - Registers exports in global `*module-exports*` registry (always in root closure)
   - Multiple files declaring same module **merge** their exports (later files don't overwrite)

2. **Import**: `(import module-name (sym1 sym2 ...))`
   - Calls `list-directory` to find all `.lyra` files in `module-name/` directory
   - Loads each file with `eval-string (load-file ...)` (which calls `module` to register exports)
   - Looks up requested symbols from `*module-exports*` registry
   - Defines each imported symbol in caller's scope
   - Load-once semantics via `*module-loaded-<name>*` marker

**Key implementation details**:
- **Root closure access**: `module` intrinsic walks parent chain to find root closure, ensuring `*module-exports*` is always global (even when called from nested closures)
- **Export merging**: When a module is declared multiple times, exports are merged (new exports added, existing kept unless redefined)
- **Selective imports**: Only requested symbols are imported into caller's scope
- **Directory scanning**: `list-directory` intrinsic uses `platform_list_directory()` (see Windows/MSVC support above) to find module files
- **Load helpers**: `load-files` recursively loads all files from a list using the Z combinator

**Features**:
- ✅ Multi-file modules (C#-style partial modules)
- ✅ Explicit exports via `(export ...)` list
- ✅ Selective imports (only import requested symbols)
- ✅ Hierarchical modules via `/` separator: `web/http/client`
- ✅ Module merging (multiple files contribute to same module)
- ✅ Load-once semantics (tracked per module name)

**Module naming conventions**:
- Pattern: `[a-zA-Z][a-zA-Z0-9_]*(/[a-zA-Z][a-zA-Z0-9_]*)*`
- Use **underscores** (not hyphens) to avoid ambiguity with infix minus in future sugared syntax
- Use **`/`** for hierarchy (not `.` - reserved for future record access)
- Examples: `math`, `web/http`, `web/http/client`

**Example with mutual recursion**:
```lisp
; In math/even.lyra
(module math
  (export is-even)
  ; is-even calls is-odd from math/odd.lyra
  (define is-even (lambda (n)
    (if (= n 0) true (is-odd (- n 1))))))

; In math/odd.lyra
(module math
  (export is-odd)
  ; is-odd calls is-even from math/even.lyra
  (define is-odd (lambda (n)
    (if (= n 0) false (is-even (- n 1))))))

; Usage
(import math (is-even is-odd))
(is-even 4)  ; → true
(is-odd 5)   ; → true
```

**Limitations**:
- No namespace isolation (imports pollute caller's scope)
- No re-exports yet (can't re-export symbols from other modules)
- No module-qualified access (can't do `math.square`, only `square`)
- Directory must exist (no fallback to single file)

## CLI Usage

The Lyra interpreter supports multiple modes:

```bash
./lyra                    # Start REPL
./lyra file.lyra          # Execute a file
./lyra -c 'code'          # Execute code directly
./lyra -h                 # Show help
./lyra --help             # Show help
```

**Examples**:
```bash
./lyra -c '(+ 1 2 3)'                                # → 6
./lyra -c '(is-pure +)'                              # → true
./lyra -c '(import math (square)) (square 7)'        # → 49
./lyra -c '(define x 10) (define y 20) (+ x y)'      # → 30
```

## Important Fixes & Gotchas

### Fixed Bugs
- **Macro argument binding bug** ([src/call.c:87](src/call.c#L87)): Was using `list_head(args)` instead of `list_head(args_iter)` when binding macro parameters, causing incorrect argument binding when iterating through parameter list. This caused the `fn` macro to fail because parameters were bound to wrong values.
- **GC freed variadic parameter names** ([src/gc.c](src/gc.c), `gc_mark_from_value`): `VARIADIC_MARKER` values (the processed form of a `*args` parameter) hold their name in `data.as_symbol`, but the mark phase only marked that string for `SYMBOL`. The first `gc_collect` freed the name of every variadic parameter still in use, e.g. the ones inside the prelude's `Z` combinator, so in the REPL any second expression that called an `fn`-defined function (`map`, `foldl`, ...) crashed. Found while testing that the REPL survives a panic.

### Code Patterns
- **Macro expansion**: Macros receive unevaluated arguments, run the macro body (the transformer) in a fresh child of the closure **captured where the macro was defined** (lexical scope; params are bound there, and expansion-time `define`s stay there), then evaluate the resulting expansion in the **caller's** closure ([src/call.c](src/call.c), `call_derived_macro`)
  - Expansion and evaluation are separate steps: `expand_derived_macro` produces the expansion only, and `call_derived_macro` = expand + `eval_s_expr`
  - `macroexpand_1` / `macroexpand` ([src/call.h](src/call.h)) are the C-level "expand without evaluating" helpers behind the `expand-macro-1` / `expand-macro` intrinsics: they resolve the head symbol in the given closure like `eval_s_expr` does, expand only **derived** macros (special forms / intrinsic macros, functions, unbound or non-symbol heads, and atoms come back unchanged, by pointer), and never descend into subforms. `macroexpand` repeats at the head until the form stops being a derived-macro call
- **Function application**: Functions receive evaluated arguments and execute in a new closure derived from their definition environment
- **`begin` special form**: Critical for macros - evaluates expressions sequentially in the **current** closure without creating a new scope
  - This allows `define` in macro expansions to affect the caller's scope
  - Unlike `(lambda () ...)` which creates a new closure

### Macro Hygiene Patterns
**Problem**: Macros can accidentally capture or conflict with user variable names **in the generated code**.

**Solution**: Use `gensym` to generate guaranteed-unique symbols for variables that appear in the macro's template. Gensyms are uninterned (see "Value Types"), so their uniqueness comes from identity, not from the name: no user code can write a symbol that equals or binds the same variable as a gensym.

**Important Distinction**:
- Variables used **during macro expansion** (in macro body): Safe - they resolve in the macro's definition-site scope, never the caller's
- Variables used **in generated code** (in the template): Need gensym if not user-provided

**Example**:
```lisp
; BAD: Unhygienic macro (tmp appears in generated code)
(define-macro bad-double (lambda (x)
  `(let ((tmp ,x))         ; 'tmp' in template - could conflict!
     (+ tmp tmp))))

; GOOD: Hygienic macro using gensym
(define-macro safe-double (lambda (x)
  (define tmp-sym (gensym))   ; Expansion-time variable - safe
  `(let ((,tmp-sym ,x))        ; Unquoted gensym in template
     (+ ,tmp-sym ,tmp-sym))))

; ALSO GOOD: Variables only during expansion
(define-macro let (lambda (bindings body)
  (define vars (map car bindings))   ; Used during expansion only
  `((lambda ,vars ,body) ,@vals)))   ; vars/vals don't appear as symbols
```

**Usage**:
```lisp
(safe-double 21)  ; → 42

; Even if user has 'tmp' defined, safe-double uses a fresh #:G__N
(define tmp 100)
(safe-double 21)  ; → 42 (not affected by tmp)

; ...and even a user variable literally named G__0 is a different variable
(define G__0 100)
```

**Key Points**:
- Use `gensym` for any **symbol literals** that appear in the quasiquoted template
- Variables used only in the macro body (before the template) are safe
- Each `gensym()` call returns a new uninterned symbol (printed `#:G__0`, `#:G__1`, ...)
- Never build a gensym'd name as text (`string`/`symbol`/`eval-string`). That yields an interned symbol and throws away the hygiene. No prelude macro does this. `import`'s `*module-loaded-...*` markers are deliberately interned, because every `import` of a module must find the same marker

## Testing
- **Test Coverage**: 125 tests across 3 suites, all passing ✅ (including 15 `match` pattern-matching tests)
  - `test_eval.c`: 86 tests covering evaluation, special forms, macros, recursion, folds, list functions, gensym, match patterns, print/write, panics
  - `test_list.c`: 25 tests covering list operations
  - `test_parser.c`: 14 tests covering lexing and parsing
- Run tests with `make test` (Linux/macOS) or `ctest --test-dir build -C Debug` (Windows)
- Tests verify:
  - Basic evaluation (integers, floats, strings, symbols)
  - Arithmetic operations (+, -, *, /, divmod)
  - List operations (car, cdr, cons, apply)
  - Control flow (if, quote)
  - Functions (lambda, closures, variadic args)
  - Macros (define-macro, fn, quasiquote/unquote)
  - Recursion (Z combinator, fn macro)
  - Macro hygiene (gensym usage)
  - **Pattern matching (match macro)** - 15 tests covering:
    - Literal int/symbol matching, variable binding
    - Fixed and variadic list patterns (`*args`)
    - Nested patterns, guard conditions
    - Empty list, first-match-wins semantics
    - Realistic compiler example (`compile-expr`)

**Note**: Module system and purity tracking are tested manually via CLI but not yet in the test suites.

## Known Issues & TODOs
- **Macro hygiene**: ✅ **Solved!** - Use `gensym` to generate unique symbols in macros
  - `gensym` generates uninterned symbols (printed `#:G__0`, `#:G__1`, ...) that can't be forged by typing their name
  - See "Macro Hygiene Patterns" section for usage examples
- **Error handling**: Programmer errors panic (see Panics section), but there are no stack traces or source locations yet, many intrinsics still return `nil` silently on bad input, and recoverable errors have no result values yet
- **Tail-call optimization (TCO)**: Not implemented - recursive functions will overflow stack on large inputs
  - `foldl`, `reverse`, `map`, `filter` are all tail-recursive (would benefit from TCO)
  - These use `foldl` internally, so they're stack-efficient once TCO is added
  - `foldr` is inherently not tail-recursive (needs stack proportional to list length)
