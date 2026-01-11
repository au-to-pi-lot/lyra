# Notes for Claude

**IMPORTANT**: Proactively update this file whenever you:
- Create new modules or significant components
- Learn important patterns or conventions in the codebase
- Identify TODOs or design decisions that should be remembered
- Discover quirks, gotchas, or non-obvious behavior
- Make architectural decisions

Think of this as your persistent memory across conversations!
You can update this file at any time in any way you deem appropriate, including to change these header instructions.

**MAINTENANCE GUIDELINES** (Target: ~250 lines):

*What to KEEP:*
- Architecture decisions (GC design, module system, closure semantics)
- Non-obvious patterns (macro hygiene, root closure walking)
- Bugs that wasted hours (with file:line references)
- Critical gotchas (variadic args must be suffix-only, begin vs lambda for macros)
- Current TODOs and known issues

*What to REMOVE:*
- API documentation that duplicates code comments (full intrinsic function lists)
- Temporary TODOs that got resolved
- Info that's obvious from reading the code structure
- Outdated notes (check for contradictions like "not yet implemented" when it is)

*Decision rule:* If it would take >5 minutes to re-discover from the code, keep it. Otherwise, consider moving to code comments or removing.

## Project Setup

### vcpkg
The vcpkg package manager is located at `./external/vcpkg/vcpkg`.

To search for packages:
```bash
./external/vcpkg/vcpkg search <package-name>
```

### Build System
The makefile automatically picks up **all** `.c` files in `src/` using `find`. You don't need to manually add new source files to the makefile - just create them in `src/` and they'll be included in the build.

#### Debug Flags
You can enable debug output for specific subsystems by adding `-D` flags to `CFLAGS` in the makefile:
- `-DDEBUG_LAMBDA`: Shows debug output for lambda creation (parameter processing, variadic args)
- `-DDEBUG_MACRO`: Shows debug output for macro expansion (parameters, bindings, expansion result)
- `-DDEBUG_QUASIQUOTE`: Shows debug output for quasiquote/unquote processing

Example:
```bash
# Add to makefile CFLAGS line:
CFLAGS = -Wall -Wextra -std=c11 -g -DDEBUG_MACRO -DDEBUG_QUASIQUOTE
```

## Architecture Notes

### Memory Management
- **GC Implementation**: Basic mark-and-sweep garbage collector in `src/gc.c`
  - All allocations tracked in a linked list via `GCObject` headers
  - Mark phase starts from root closure and recursively marks reachable objects
    - **Cycle detection**: Checks if objects already marked before recursing to handle cycles (e.g., closures containing functions that reference the same closure)
  - Sweep phase frees unmarked objects with proper destructors (e.g., `utstring_free()` for `GC_UTSTRING` types)
  - Threshold auto-adjusts: starts at 1000 objects, doubles based on live set size
  - GC runs at safe points: after each REPL expression and after file evaluation
  - **Cleanup at exit**: `gc_free_all()` performs two-pass cleanup:
    1. First pass clears uthash hash tables in closures (frees uthash internal structures)
    2. Second pass frees all GC objects with appropriate destructors
  - ✅ **Verified leak-free** with valgrind on trivial commands

- **Parser Memory Management**: Parser uses manual memory management (not GC)
  - `make_token()` takes ownership of `UT_string` buffers (transfer semantics, not copy)
  - All tokens freed via `free_token()` after use
  - `parse_with_pos()` returns consumed character count, enabling multi-expression file parsing

### Purity Tracking
- **Design**: Functions track whether they have side effects via `is_pure` flag in `Callable` struct
  - Pure functions: can be memoized, reordered, constant-folded by compiler
  - Impure functions: have side effects (I/O, mutation, etc.)
- **Intrinsics declare purity**: Arithmetic/list operations are pure, I/O operations are impure
- **Purity is viral**: If a derived function calls any impure function, it becomes impure (future work)
- **Query at runtime**: `(is-pure fn)` returns true/false

**Notable impure intrinsics**: `gensym` (mutates counter), `load-file` (file I/O), `eval-string` (dynamic eval), `apply` (purity-polymorphic, conservatively impure)

### Recursion & Macros

- **Z Combinator**: The Z combinator (call-by-value Y combinator) now works correctly for recursive functions
  - Fixed GC bugs that were incorrectly freeing live values in closures
  - The Z combinator creates complex closure references, making it a good stress test for the GC
  - Updated to support variadic arguments: `(lambda (*args) (apply (x x) args))`

- **Variadic Arguments**: Functions and macros support variadic parameters with `*name` syntax
  - `(lambda (a b *rest) body)` - collects remaining args into `rest` list
  - **Suffix-only**: Variadic parameter must be the last parameter (no infix/prefix variadics)
  - Internally converted to `VARIADIC_MARKER` type (cannot be forged from Lisp code - hygienic!)
  - Invalid: `(lambda (first *middle last) ...)` - error: variadic must be last

- **`fn` Macro**: Convenient syntax for defining recursive functions
  - Syntax: `(fn name (params...) body)`
  - Expands to: `(define name (Z (lambda (name) (lambda (params...) body))))`
  - Automatically wraps function in Z combinator for recursion

- **Quasiquote/Unquote**: Template mechanism for macros
  - Backtick `` ` `` creates a template, comma `,` evaluates within template
  - Comma-at `,@` splices a list into the surrounding list
  - Works from files; **don't use in REPL** (EditLine waits for closing backtick)

### Special Forms - Key Gotchas

- **`begin` special form**: Critical for macros - evaluates expressions sequentially in the **current** closure without creating a new scope
  - This allows `define` in macro expansions to affect the caller's scope
  - Unlike `(lambda () ...)` which creates a new closure
  - Essential for `let`, `import`, and other macros that need to introduce bindings

- **`module` intrinsic**: Declares module with exports
  - Walks to root closure to ensure `*module-exports*` registry is always global
  - Multiple files declaring same module **merge** their exports

- **`import` macro**: Fully derived macro (not intrinsic!) in `src/prelude.c`
  - Uses `begin`, `string`, `symbol`, `eval-string`, and `load-file` primitives
  - Loads all `.lyra` files in `module-name/` directory
  - Implements load-once semantics via `*module-loaded-<name>*` markers

## Module System

### Architecture
Multi-file module support with explicit exports (like C# partial classes):

- `module` is an **intrinsic special form** in [src/intrinsics.c](src/intrinsics.c#L622)
- `import` is a **derived macro** in [src/prelude.c](src/prelude.c#L149) (written in Lisp!)
- Global registry `*module-exports*` stores exports: `((module-name ((sym1 . val1) ...)) ...)`
- `module` intrinsic walks to root closure to ensure registry is always global

### Key Implementation Details
- **Root closure access**: `module` intrinsic walks parent chain to find root closure, ensuring `*module-exports*` is always global (even when called from nested closures)
- **Export merging**: When a module is declared multiple times, exports are merged (new exports added, existing kept unless redefined)
- **Selective imports**: Only requested symbols are imported into caller's scope
- **Directory scanning**: `list-directory` intrinsic uses POSIX `opendir`/`readdir` to find module files

### Module Naming Conventions
- Pattern: `[a-zA-Z][a-zA-Z0-9_]*(/[a-zA-Z][a-zA-Z0-9_]*)*`
- Use **underscores** (not hyphens) to avoid ambiguity with infix minus in future sugared syntax
- Use **`/`** for hierarchy (not `.` - reserved for future record access)
- Examples: `math`, `web/http`, `web/http/client`

### Limitations
- No namespace isolation (imports pollute caller's scope)
- No re-exports yet (can't re-export symbols from other modules)
- No module-qualified access (can't do `math.square`, only `square`)
- Directory must exist (no fallback to single file)

## Important Fixes & Gotchas

### Fixed Bugs
- **Macro argument binding bug** ([src/call.c:87](src/call.c#L87)): Was using `list_head(args)` instead of `list_head(args_iter)` when binding macro parameters, causing incorrect argument binding when iterating through parameter list. This caused the `fn` macro to fail because parameters were bound to wrong values.

### Code Patterns
- **Macro expansion**: Macros receive unevaluated arguments, expand them in macro's closure, then evaluate the expansion in the caller's closure ([src/call.c:133-134](src/call.c#L133-L134))
- **Function application**: Functions receive evaluated arguments and execute in a new closure derived from their definition environment

### Macro Hygiene Patterns
**Problem**: Macros can accidentally capture or conflict with user variable names **in the generated code**.

**Solution**: Use `gensym` to generate guaranteed-unique symbols for variables that appear in the macro's template.

**Important Distinction**:
- Variables used **during macro expansion** (in macro body): Safe - they're in macro's closure
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

**Key Points**:
- Use `gensym` for any **symbol literals** that appear in the quasiquoted template
- Variables used only in the macro body (before the template) are safe
- Each `gensym()` call returns a unique symbol (G__0, G__1, ...)

## Testing
- Run tests with `make test`
- 106 tests across 3 suites: `test_eval.c`, `test_list.c`, `test_parser.c`
- All currently passing ✅
- Module system and purity tracking tested manually via CLI (not in test suites yet)

## Known Issues & TODOs
- **Error handling**: Limited error messages, no stack traces
- **Tail-call optimization (TCO)**: Not implemented - recursive functions will overflow stack on large inputs
  - `foldl`, `reverse`, `map`, `filter` are all tail-recursive (would benefit from TCO)
  - `foldr` is inherently not tail-recursive (needs stack proportional to list length)
- **Generational GC**: Current mark-and-sweep works but could be more efficient
