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

### Value Types
- Core types defined in `src/types/value.h`: CONS, INT, FLOAT, BOOLEAN, STRING, SYMBOL, FUNCTION, MACRO
- Closures use uthash for variable storage
- Callables can be intrinsic (C functions) or derived (user-defined Lisp functions/macros)

### Purity Tracking
- **Design**: Functions track whether they have side effects via `is_pure` flag in `Callable` struct
  - Pure functions: can be memoized, reordered, constant-folded by compiler
  - Impure functions: have side effects (I/O, mutation, etc.)
- **Intrinsics declare purity**: Arithmetic/list operations are pure, I/O operations are impure
- **Purity is viral**: If a derived function calls any impure function, it becomes impure (future work)
- **Query at runtime**: `(is-pure fn)` returns true/false
- **Use case**: Compiler can use this for optimizations (memoization, dead code elimination, etc.)

**Pure intrinsics**: `+`, `-`, `*`, `/`, `divmod`, `=`, `car`, `cdr`, `cons`, `get-var`, `is-pure`, `string`, `symbol`
**Impure intrinsics**: `gensym` (mutates counter), `load-file` (file I/O), `eval-string` (dynamic eval), `apply` (purity-polymorphic, conservatively impure)

### Recursion & Macros
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

### Intrinsic Functions & Special Forms
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
- `=` - Equality test

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
- `gensym` - Generate unique symbol: `(gensym)` → `G__0` (increments each call)
  - Use in macros to avoid variable name conflicts
  - Essential for writing hygienic macros
- `get-var` - Get value of variable by symbol: `(get-var 'x)` → value of x
  - Used internally by module system
- `is-pure` - Check if function is pure: `(is-pure +)` → `true`

**File I/O** (impure):
- `load-file` - Read file contents as string: `(load-file "module.lyra")` → `"(define ...)\n..."`
- `eval-string` - Evaluate string as code: `(eval-string "(+ 1 2)")` → `3`
  - Evaluates in **current closure** - definitions affect caller's scope

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
- `match` - Pattern matching (TODO - not yet implemented, tests written)
  - Syntax: `(match expr ((pattern1) body1) ((pattern2) body2) ...)`
  - Supports: literal values, quoted symbols, variable binding, list patterns, variadic patterns (`*args`)
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
- **Directory scanning**: `list-directory` intrinsic uses POSIX `opendir`/`readdir` to find module files
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
./lyra file.lyra          # Run a file
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

### Code Patterns
- **Macro expansion**: Macros receive unevaluated arguments, expand them in macro's closure, then evaluate the expansion in the caller's closure ([src/call.c:133-134](src/call.c#L133-L134))
- **Function application**: Functions receive evaluated arguments and execute in a new closure derived from their definition environment
- **`begin` special form**: Critical for macros - evaluates expressions sequentially in the **current** closure without creating a new scope
  - This allows `define` in macro expansions to affect the caller's scope
  - Unlike `(lambda () ...)` which creates a new closure

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

**Usage**:
```lisp
(safe-double 21)  ; → 42

; Even if user has 'tmp' defined, safe-double uses unique G__N
(define tmp 100)
(safe-double 21)  ; → 42 (not affected by tmp)
```

**Key Points**:
- Use `gensym` for any **symbol literals** that appear in the quasiquoted template
- Variables used only in the macro body (before the template) are safe
- Each `gensym()` call returns a unique symbol (G__0, G__1, ...)

## Testing
- **Test Coverage**: 90 tests across 3 suites (40 existing passing ✅, 11 match tests waiting for implementation ⏳)
  - `test_eval.c`: 51 tests covering evaluation, special forms, macros, recursion, folds, list functions, gensym, match patterns
  - `test_list.c`: 25 tests covering list operations
  - `test_parser.c`: 14 tests covering lexing and parsing
- Run tests with `make test`
- Tests verify:
  - Basic evaluation (integers, floats, strings, symbols)
  - Arithmetic operations (+, -, *, /, divmod)
  - List operations (car, cdr, cons, apply)
  - Control flow (if, quote)
  - Functions (lambda, closures, variadic args)
  - Macros (define-macro, fn, quasiquote/unquote)
  - Recursion (Z combinator, fn macro)
  - Macro hygiene (gensym usage)
  - **Pattern matching (match macro)** - 11 tests covering:
    - Literal int/symbol matching, variable binding
    - Fixed and variadic list patterns (`*args`)
    - Nested patterns, guard conditions
    - Empty list, first-match-wins semantics
    - Realistic compiler example (`compile-expr`)

**Note**: Module system and purity tracking are tested manually via CLI but not yet in the test suites.

## Known Issues & TODOs
- **Macro hygiene**: ✅ **Solved!** - Use `gensym` to generate unique symbols in macros
  - `gensym` generates symbols like `G__0`, `G__1`, etc.
  - See "Macro Hygiene Patterns" section for usage examples
- **Error handling**: Limited error messages, no stack traces
- **Tail-call optimization (TCO)**: Not implemented - recursive functions will overflow stack on large inputs
  - `foldl`, `reverse`, `map`, `filter` are all tail-recursive (would benefit from TCO)
  - These use `foldl` internally, so they're stack-efficient once TCO is added
  - `foldr` is inherently not tail-recursive (needs stack proportional to list length)
