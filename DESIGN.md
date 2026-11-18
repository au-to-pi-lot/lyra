# Lyra Language Design

## Vision

Lyra is a high-performance, garbage-collected, functional-first programming language with pragmatic concessions to imperativity. It synthesizes the best ideas from multiple language traditions while avoiding their pitfalls.

## Core Philosophy

- **Functional-first, not functional-pure**: Immutability by default, but pragmatic escape hatches
- **Performance matters**: Inspired by SBCL and LuaJIT
- **Ergonomics matter**: Learning from decades of language design
- **Native concurrency**: Multi-threaded and concurrent from the ground up
- **Tasteful syntax**: Convenience features done right
- **What you reach for first doesn't explode**: The short, obvious name is the total, safe operation. A variant that panics or skips a check gets the longer, explicit name (Rust gets this backwards: `v[i]` panics while `v.get(i)` is the safe one).
- **Cleanup doesn't depend on the GC**: Files, sockets and locks are released by scope, deterministically, never by finalizers or collection timing (CPython refcounting vs PyPy, Java finalizers)

## Language Influences

### Scheme
- Hygienic macros
- Single namespace for functions and values
- Core intrinsic functions and macros
- Starting implementation base

### Clojure
- Spelling: `true`, `false`, `nil` (`nil` is provisional; see "Absence: `Option`, not `nil`" under Error Handling)
- No explicit user-level cons cells
- Additional bracket syntax for data structures `[]` `{}`

### Python
- Splat syntax (`*args`, `**kwargs`)
- F-string interpolation
- `range()` and other standard library naming
- `dict` (dictionary) as the name for the everyday hash map
- **Not**: Module system (execution-based imports cause circular dependency issues)

### Ruby
- Object model influences

### Rust
- Static type inference (eventual)
- Modern parallelizability and concurrency primitives
- Memory safety concepts (where applicable with GC)

### Haskell
- Functional programming patterns
- Type system ideas (eventual)
- **Not**: Haskell-style purity enforcement

### Common Lisp
- Standard library design patterns
- Performance-oriented compilation

### JavaScript/TypeScript
- Lessons in what not to do
- **Not**: Async/await (see Concurrency section)

### Lua
- Simplicity and performance balance
- Clean C interop
- **Not**: 1-based indexing (confusing, doesn't match any other language or math conventions)
- **Not**: Tables as only data structure (need specialized types for different performance characteristics)

### APL
- Terseness as a virtue (where appropriate)
- Array-oriented thinking

### C#
- Declarative namespaces (avoid Python's circular import problems)
- Two-pass compilation model (declarations before usage)

## Planned Features

### Syntax

**Indentation-Based Blocks:**

Python-style significant whitespace, implemented via lexer INDENT/DEDENT tokens:

```
let fac := (n) ->
    let result := 1
    for i in 1..n:
        result *= i
    result
```

Lexer emits INDENT/DEDENT tokens that parser treats as block delimiters. This keeps parser simple - whitespace handling is purely lexer concern.

**Whitespace Rules:**
- Spaces are the preferred style, but tabs are allowed (no Haskell-style tab ban)
- All spaces OR all tabs - mixing within a file is an error
- Lexer tracks which style is used, errors on mixed indentation
- Blank lines and comments ignored for indentation
- Indentation at the REPL (pasting blocks with blank lines, knowing when an entry is done) is open: #29

**Sugar-First Philosophy:**

Lyra is **sugar-first**: modern readable syntax is primary, s-expressions are the power-user escape hatch.

```scheme
; 95% of code - modern sugared syntax
fn factorial(n):
    if n <= 1:
        1
    else:
        n * factorial(n - 1)

; 5% of code - s-expressions for macros and metaprogramming
(defmacro unless [condition & body]
  `(if (not ,condition) (do ,@body)))

; Mix freely when needed
fn process(data):
    (filter positive? data)   ; Can use s-expr forms anywhere
    .map(x -> x * 2)
    .collect()
```

**Why this design:**
- Beginners learn sugared syntax only
- S-expressions are "infinite cosmic power, itty bitty living space"
- Macro authors have full AST manipulation (s-exprs are explicit AST)
- Error messages show sugared form via span tracking
- No Template Haskell complexity - s-exprs are simpler than typed AST manipulation

**Precedence and Grouping:**
```scheme
let x = (3 + 4) * (5 + 6)    ; Grouping parens in infix expressions
let y = (* (+ 3 4) (+ 5 6))  ; S-expression form

; Context determines meaning - infix expressions allow grouping parens
; S-expressions follow traditional Lisp rules
```

**Self-Hosted Syntax Transformations:**

Sugar is defined in Lyra itself via `defsyntax`, not hardcoded in C:

```scheme
; Define syntax transformation (pattern -> template)
(defsyntax fn
  [(fn ,name (,params ...) -> ,body)
   `(defn ,name [,@params] ,body)]

  [(fn ,name (,params ...) -> INDENT ,@body DEDENT)
   `(defn ,name [,@params] (do ,@body))])

(defsyntax infix
  [(,a + ,b) `(+ ,a ,b)]
  [(,a * ,b) `(* ,a ,b)]
  [(,a <= ,b) `(<= ,a ,b)])

(defsyntax let-binding
  [(let ,name := ,value)
   `(def ,name ,value)])
```

Benefits:
- Sugar defined in the language, not the implementation
- Users can add custom syntax transformations (*under reconsideration*: per-library dialects make code hard to read and hard for tooling to parse without running it; `defsyntax` may become internal to the prelude/stdlib. #30)
- Introspectable and modifiable
- Self-documenting transformations

Implementation approach:
1. Bootstrap minimal s-expression core in C (defn, def, if, lambda, defmacro)
2. Prelude defines syntax transformations via `defsyntax`
3. Parser produces surface syntax AST with span information (AST + Spans, not full CST)
4. Syntax transformer (written in Lyra) applies `defsyntax` rules
5. Result: pure s-expression AST with span mapping for error messages
6. Spans allow error messages to reference original sugared source

Two-tier macro system:
- **Syntax macros** (`defsyntax`): transform surface syntax to core forms
- **Semantic macros** (`defmacro`): transform core s-expressions

Inspired by Racket's approach: minimal core, everything else self-hosted.

**Comments:**
```scheme
# Single-line comment
fn foo(x):  # End-of-line comment
    x * 2

#| Multi-line comment
   Can span multiple lines
   Useful for docstrings
|#

#|#
 | C comment style decoration optionally for e.g. docstring?
 |
#|#
```

**Multiline Strings:**

The `//` syntax for multiline strings (distinct from `#` comments):

```scheme
let s =
    //hello
    //world

s == "hello\nworld"  # true

# Leading/trailing whitespace preserved
let s =
    // leading spaces retained
    //trailing spaces too

# Empty lines via empty //
let s =
    //line 1
    //
    //line 3

s == "line 1\n\nline 3"

# All // must align (same column)
let s =
    //correct
    //alignment
        //misaligned  # Syntax error!

# Terminates at dedent or non-// line
let s =
    //string content
let x = 5  # String ended, new statement

# Works in expressions
print(
    //multiline string
    //as function argument
)

# Expression form (REPL)
//standalone
//string

# Interpolation (with prefix, like Python f-strings)
let name = "Alice"
let msg = f//
    //Hello, {name}!
    //Welcome.
```

**Function Call Syntax:**

Arguments on separate lines don't need commas:

```scheme
# Multi-line: no commas
f(
    //first string
    //continued

    42
    true
)

# Single-line: commas required
f("short", 42, true)

# Mixed: commas only within lines
f(
    "first"
    42, 43, 44  # Multiple on one line
    //last
)
```

This eliminates trailing comma issues with multiline strings.

**Bracket Types and Data Structures:**

```scheme
# () - S-expressions and tuples
(foo bar)           # S-expression (function call)
(1, 2, 3)           # Tuple (commas distinguish from s-expr)
(Int, String)       # Tuple type

# [] - Lists (immutable linked lists)
[1, 2, 3]           # List literal - primary sequence type
[]                  # Empty list
match lst:
  [] -> base
  [x & xs] -> recur(x, xs)  # O(1) head/tail for recursion

# {} - Sets and dicts (inline syntax is provisional and open: #32)
{1, 2, 3}           # Set (homogeneous elements)
{a: 1, b: 2}        # Dict (key-value pairs with :)
{a: 1, 2}           # Syntax error (can't mix)
(set 'a 'b 'c)      # S-expression forms; symbols are the idiomatic keys
(dict ('a 1) ('b 2) ('c 3))
                    # Keys and set members need total equality: no floats (#17).
                    # Iteration order is not guaranteed (hash seeding against
                    # flooding, speed); ordered dicts are separate. #31

# Explicit constructors
(vector 1 2 3)      # Vector - random access, O(1) indexing
(list 1 2 3)        # List (also available if needed)
```

**Why lists as `[]`:**
- Primary sequence type for functional programming
- Natural for recursion with pattern matching
- Immutable by default, O(1) cons/head/tail
- No `(list 1 2 3)` ugliness in common code
- Vectors explicit when you need different performance characteristics

**Operators:**

Lyra uses familiar operators with clear, consistent semantics:

```scheme
# Arithmetic (numeric only)
+       # Addition: 3 + 4 = 7
-       # Subtraction: 5 - 3 = 2
*       # Multiplication: 3 * 4 = 12
/       # Float division (always): 10 / 3 = 3.333...

# Collection operations
+       # Concatenation/union:
        # "a" + "b" = "ab"
        # [1, 2] + [3, 4] = [1, 2, 3, 4]
        # {1, 2} + {3, 4} = {1, 2, 3, 4} (set union)

-       # Set difference: {1, 2, 3} - {2} = {1, 3}

# Comparison
==      # Equality
!=      # Inequality
<, >, <=, >=  # Ordering

# Logical
||      # Boolean or (not +)
&&      # Boolean and (not *)
!       # Boolean not

# Type-based via Add trait
# No implicit conversions - type error for mixed types
"Count: " + 42       # ERROR: must use str(42) or f"Count: {42}"
```

**Named functions for ambiguous operations:**
```scheme
divmod(10, 3)              # (3, 1) - integer division + remainder
intersection(set1, set2)   # Set intersection (not *)
cartesian(set1, set2)      # Cartesian product
union(set1, set2)          # Set union (+ also works)
repeat(list, n)            # Repeat list/string
```

**Rationals:**
```scheme
let r = rational(2, 3)    # Explicit constructor (not 2/3 literal)
rational(2, 3) / 2        # 1/3 (stays rational)
rational(2, 3) / 2.0      # 0.333... (float contaminates)
```

**Other Syntax Features:**
- **`=` operator**: Binding/assignment, `==` for comparison
- **Assignment never creates a binding**: assigning to a name that isn't in scope is an error, never an implicit new variable (Lua's globals-by-typo)
- **`->` arrow**: Function definitions (single or multi-line)
- **Range syntax**: `1..n` (Rust/Ruby style)
- **Destructuring assignment**: Pattern matching in bindings
- **String interpolation**: F-string style (prefix with `f//` for multiline)
- **Infix arithmetic**: Natural mathematical expressions
- **Multiple bracket types**: Different semantics for `()`, `[]`, `{}`
- **Splat/spread**: `*args` unpacking for iterables
- **0-based indexing**: Standard (not Lua's 1-based)
- **Parens opt out of indentation**: Inside s-exprs `(...)`, whitespace is style only

### Modules

**Design: Declarative, Two-Pass Loading**

Inspired by C# namespaces to avoid Python's circular import problems.

**Module Declaration:**
```scheme
(module mylib
  (export foo bar)
  (import otherlib [baz quux])

  (fn foo [x] ...)
  (fn bar [y] ...))
```

**Two-Pass Evaluation:**
1. First pass: collect all definitions (functions, types, macros) - build symbol table
2. Second pass: evaluate bodies with full symbol table available
3. No "execution order" issues - declarations are hoisted

**Why This Works:**
- Lisp naturally separates reading structure from evaluation
- Names are symbolic until evaluation time
- Circular references between modules work fine
- Side effects happen explicitly (not during import)

**Implementation Needs:**
- File I/O for loading `.lyra` files
- Module registry/cache (load once)
- Search paths for module resolution
- Special form in evaluator (not just a macro)
- Eventually: module precompilation/caching

### Iteration and Streaming

**Iterator Protocol (low-level):**
```scheme
(next iter) -> Option[value]   ; Some(value), or None when exhausted
```

Not a `nil` sentinel (Lua-style): a sentinel can't be told apart from a real element, so iterating a list of lists would stop at the first `[]` once `nil` means the empty list.

**High-Level Abstractions:**
- `(for [x iterable] ...)` - loop over any iterable
- `(let [a b *rest] iterable)` - splat destructuring
- `(map f iter)`, `(filter pred iter)` - lazy operations
- `(lines file)` - lazy file reading

**Example:**
```scheme
; Lazy file processing
(with-file [f "data.txt"]
  (for [line (lines f)]
    (process line)))
```

**Iterator vs Stream:**
- Iterators: synchronous pull-based (for loops, lazy sequences)
- Streams: asynchronous push-based (eventual, for I/O events)
- Start with iterators, add async streams later with concurrency primitives

### Type System (Eventual)

**Static type inference** (Rust/Haskell inspired):
- Gradual typing support (untyped code is slower, typed code enables optimizations)
- No explicit type annotations required for simple cases (inference does the work)
- Type checking happens after syntax desugaring

**Algebraic Data Types (ADTs):**

Sum types (tagged unions / enums with data):
```scheme
; Result type
(deftype Result [T E]
  (Ok T)
  (Err E))

; Option type
(deftype Option [T]
  (Some T)
  (None))

; Recursive types
(deftype Tree [T]
  (Leaf T)
  (Branch (Tree T) (Tree T)))
```

Pattern matching:
```scheme
(match result
  [(Ok value) (process value)]
  [(Err e) (handle-error e)])

(match tree
  [(Leaf x) x]
  [(Branch left right) (combine (sum left) (sum right))])
```

**Parametric Polymorphism (Generics):**

Functions work over any type:
```scheme
; Implicit generics
(fn identity [x] x)
(fn first [[x *rest]] x)

; Explicit type parameters (when needed)
(fn make-pair [T U] [a : T b : U] -> (Pair T U)
  (Pair a b))
```

**Not including (initially):**
- Higher-kinded types (HKTs) - adds significant complexity
- Can revisit if concrete need arises
- Regular generics + traits cover 95% of use cases

**Traits over Classes:**

Immutable values don't need OOP. Traits (Rust) / Type Classes (Haskell) / Protocols (Clojure) provide polymorphism without identity or mutation.

**Why Traits?**
- Objects assume identity and mutation
- Traits: behavior for values without state
- Retroactive implementation (add traits to existing types), within Rust's coherence rules: an implementation must live with the trait or the type, and orphan instances are not allowed. Named implementations are out, since use sites shouldn't have to pick one. #24
- Clean separation: data (structs) vs behavior (traits)

**Syntax:**
```scheme
; Define trait
(deftrait Show
  (show [x] -> String))

; Implement for type
(impl Show for Point
  (defn show [p]
    (str "Point(" (:x p) ", " (:y p) ")")))

; Use polymorphically
(defn debug [x]
  (println (show x)))  ; Works on any Show-able type
```

**Method Call Syntax:**

Uniform call syntax for ergonomics:
```scheme
(import graphics.point [Point show])

(show my-point)        ; Function style
(my-point.show)        ; Method style (syntactic sugar)
```

The `.method` syntax is sugar for function lookup with type-based dispatch:
- Not methods attached to objects
- Searches for function `method` accepting first arg of that type
- Enables IDE autocomplete (`.` triggers list of available trait methods)
- OOP ergonomics without OOP complexity

**Discoverability:**
- Traits are defined in modules alongside types
- Import trait to use its methods
- IDE shows all trait methods available for a type via `.` autocomplete
- LSP "find implementations" shows all types implementing a trait

### Performance

**Compilation:**

Lyra aims for ahead-of-time compilation and eventual self-hosting.

**Compilation Targets: C → LLVM → native**

Performance of both the generated code and the compiler itself is a long-term priority. Backends arrive in this order, each earning its place:

1. **C** (first). Gets a self-hosted compiler working cheaply. The C compiler does optimization and register allocation, and generated C is readable and debuggable (`#line` maps back to Lyra source). It also provides the **bootstrap seed**: the compiler's own C output is committed, so building Lyra from scratch needs only a C compiler, and the C interpreter can retire. (LLVM IR changes between LLVM versions, which makes it a poor long-lived seed.)
2. **LLVM IR** (when performance work begins). Better code, guaranteed tail calls (`tailcc` + `musttail`), and a route to JIT compilation for the REPL via ORC. Emitted as textual IR and handed to `clang`, rather than linked against LLVM's C++ API.
3. **Native assembly** (eventually). Owning code generation means owning the stack frame layout. That is what makes Lyra's runtime design fully achievable: precise GC stack maps, growable green-thread stacks that are moved when they grow (Go-style, "millions of threads"), tail calls without workarounds, and fast compiles that LLVM can't offer. SBCL, Chez Scheme and Go all have their own backends for these reasons. Targets: x86-64 (System V and Windows) and ARM64.

**Constraints the C backend must respect,** so it doesn't box in the GC or concurrency plans:
- **Tail calls**: C guarantees none. Use `musttail` where available (clang, including `clang-cl` on Windows; recent GCC). Fall back to trampolines elsewhere. MSVC has no equivalent.
- **GC roots**: C can't report where pointers live on its stack, and conservative stack scanning would pin objects and rule out the compacting GC (Stage 2 below). Generated code registers roots in an explicit **shadow stack** instead.
- **Green threads**: stacks can't be moved without precise stack maps, so the C (and LLVM) backends use fixed-size stacks with guard pages and a small assembly stub for context switches.
- **Plain, standard C only.** Never post-process the C compiler's output. GHC's via-C backend ended up needing the "evil mangler", a Perl script that rewrote gcc's assembly, and was eventually retired.

**Architecture implications:**
- **The IR is the real decision, not the target.** Lower to a mid-level IR that makes closures, allocation, GC safepoints and tail calls explicit, so each backend is a comparatively thin pass and adding one doesn't mean rewriting the compiler.
- **Backends check each other.** With several backends, the same program compiled each way should behave identically. Combined with randomly generated test programs (in the style of Csmith), this is the main defence against miscompiles, and it is the precondition for the native backend.
- **Keep the LLVM GC integration cheap.** LLVM's own GC stack maps (statepoints) are complex and likely to be superseded by the native backend's stack maps, so the LLVM backend may keep using the shadow stack.
- **The compiler needs an evaluator anyway.** `defmacro`, `defsyntax` and compile-time evaluation run Lyra code during compilation, so the compiler contains an interpreter (or compiles and loads macros in-process). With the C backend, an interpreter is the natural choice, and it can also serve the REPL until a JIT exists.
- **Compiler speed is mostly not about the backend.** LLVM is often the slowest single step, but the largest wins come from the front end: compiling modules in parallel, caching per module, and recompiling incrementally. Design for these from the start.

**Purity tracking:** The interpreter once carried an `is_pure` flag on every callable and an `is-pure` intrinsic, but nothing consumed it, so it was removed. It should come back alongside an optimizer that uses it (memoization, reordering, dead-code elimination). Open question from the panic work: can a function that may panic count as pure?

**Self-Hosting Goal:**

Write the Lyra compiler in Lyra itself. Self-hosting is the ultimate test that the language is expressive and ergonomic enough for complex programs.

**Path to Self-Hosting:**
1. C interpreter (current) - define semantics
2. Mark & Sweep GC - memory management
3. Complete core language - closures, macros, pattern matching
4. Add Lyra features - syntax sugar, types, modules, traits
5. Write compiler in Lyra - targets C first (see Compilation Targets)
6. Bootstrap - use C interpreter to compile the Lyra compiler
7. Self-hosted - Lyra compiler compiles itself

**Bootstrap Cycle:**
```
lyra_interpreter.c (C)
  ↓ interprets
lyra_compiler.lyra
  ↓ compiles to C
lyra_compiler.c
  ↓ C compiler produces
lyra_compiler (native)
  ↓ verifies by compiling
lyra_compiler.lyra (again)
```

**Benefits:**
- Dogfooding exposes design flaws
- Proves language expressiveness
- Compiler improvements feed back into itself
- Performance targets: SBCL and LuaJIT tier

**Garbage Collection (Staged Approach):**

Lyra's immutability-first design influences GC strategy:

**Stage 1: Mark and Sweep (Current Goal)**
- Simple tracing GC (~500 lines of C)
- Stop-the-world collection
- Handles cycles naturally (important for closures, recursive data)
- Good enough for language development
- Priority: Get off "alloc and pray"

**Stage 2: Generational GC (Pre-Self-Hosting)**
- Divide heap into young/old generations
- Perfect for functional style (most objects die young)
- Immutable data = generational hypothesis works great
- Fast bump-pointer allocation
- Memory compaction
- Implement once allocation patterns are understood

**Stage 3: Concurrent GC (Production)**
- Low-latency collection
- Necessary for green threads + parallelism
- Stop-the-world pauses kill multi-threading benefits
- Immutable data simplifies implementation (no write barriers for immutable objects)
- Post-self-hosting concern

**Why Immutability Helps GC:**
- Most allocations are short-lived (perfect for generational)
- No write barriers needed for immutable objects
- Safe to share between threads without synchronization
- Allocation patterns are predictable

### Concurrency

**Model: Green Threads + Immutability**

Lyra uses lightweight green threads (Go-style goroutines) for concurrency, not async/await. This provides:
- True parallelism across CPU cores
- Simple synchronous-looking code (no function coloring)
- Cheap thread spawning (thousands/millions of threads)
- M:N scheduling (many green threads on N OS threads)

**Structured concurrency:** with green threads at the core, leaked tasks are a real risk. In Go, a goroutine can outlive everything that cares about it, so errors vanish and leaks are found in production. A spawned task's lifetime should be bounded by a scope that waits for it, with cancellation propagating down and failures up (Trio nurseries, Kotlin coroutine scopes). What exactly `spawn` means under this is open: #18. Settle it before the scheduler is built.

**Primitives:**
- `(spawn expr)` or `(go expr)` - spawn green thread
- Channels (CSP-style) - thread-safe message passing
- `(parallel-for [x xs] ...)` - data parallelism
- `(pmap f collection)` - parallel map

**Thread Safety through Immutability:**
- Immutable data shared freely between threads (zero-cost)
- No locks needed for reads
- Race-free by default

**Escape Hatches for Mutation:**
When immutability is impractical (e.g., large tensors, buffers):
- `(atom value)` - thread-safe mutable reference (atomic operations)
- `(mutex value)` - explicit locks for complex critical sections
- Thread-local mutation - mutable during construction, immutable when shared

Note: Linear/affine types considered but deferred - redundant with atoms/mutexes and doesn't mesh well with green threads on shared heap.

**Use Case: Data Parallelism**
```scheme
; Divide tensor work across threads
(defn parallel-matmul [a b]
  (let [result (make-tensor rows cols)]
    (parallel-for [i (range rows)]
      (compute-row! result i a b))  ; Each thread writes disjoint region
    result))
```

**Why Not Async/Await:**
- Async on single thread: no parallelism
- Async on multiple threads: function coloring + race conditions anyway
- Green threads provide parallelism without the downsides

**Implementation Path:**
1. Start with OS threads + immutable data
2. Add channels for message passing
3. Implement green thread scheduler (M:N threading)
4. Optional: Work-stealing scheduler for load balancing

## Error Handling

**Result Types, No Exceptions:**

Lyra has no exceptions. They hide control flow, and checked exceptions in particular don't compose with higher-order functions (Java). Errors that belong in a function's signature are return values. Lyra uses Result types for recoverable errors (propagation sugar and the rest of the design: #23):

```scheme
(deftype Result [T E]
  (Ok T)
  (Err E))

fn divide(a, b) -> Result[Float, Error]
    if b == 0:
        Err("Division by zero")
    else:
        Ok(a / b)

; Pattern match to handle
match divide(10, 0):
    Ok(val) -> println(val)
    Err(e) -> println("Error: " e)
```

**Absence: `Option`, not `nil`:**

`nil` exists today because the interpreter is dynamically typed, and it is provisional. Once the static type system lands, `nil` is at most a synonym for the empty list, and "no value" is spelled `Option`. Partial functions return `Option` (or similar) instead of failing or returning a sentinel: `(head [])` is `None`, not a crash. (Haskell's `head`, decision in #19.)

**Panics for Unrecoverable Errors:**
```scheme
panic("Unreachable code reached!")  ; Unwinds, prints stack trace, kills thread/program
```

**Guidelines:**
- **Result**: Recoverable errors caller should handle (file not found, parse error, validation)
- **Panic**: Programmer errors that shouldn't happen (assertion failure, unreachable code, an explicitly unchecked access that fails)
- The default spelling of an operation that can fail is the total one, returning `Option`/`Result`. A panicking shortcut, if one exists, gets the longer name (see Core Philosophy).

**Resource Cleanup:**

Resources are released by scope, deterministically, on every exit path including panics. No finalizers. GC timing is never observable through resource behavior.

**Rich Diagnostics (Rust-inspired):**

Error messages should be worth reading, with context and suggestions:

```
Error: Type mismatch in function call
  ┌─ example.lyra:15:12
  │
15│     foo(42, "hello")
  │             ^^^^^^^ expected Int, found String
  │
  = help: function signature is `foo(Int, Int) -> String`
  = note: did you mean to call `foo(42, parse("hello"))`?
```

Requirements:
- Span information (source location for every AST node)
- Type information at error site
- Suggestion engine for common mistakes
- Stack traces for panics and debugging

## Reflection and Metaprogramming

**Compile-Time Evaluation:**
```scheme
; Heavy computation at compile time
const LOOKUP_TABLE := (generate-table 1000)

; Zero runtime cost - table is baked into binary
```

**Runtime Reflection (for tooling, serialization, debugging):**
```scheme
; Type introspection
(type-of val)           ; -> Point
(has-trait? val Show)   ; -> true/false
(fields Point)          ; -> [x y]

; Useful for:
; - JSON/serialization (automatic derivation)
; - REPL inspection and autocomplete
; - Generic debugging/logging
```

Reflection is primarily for tooling, not core language features.

## Strings and Numeric Types

**Strings (Rust's model):**
- Immutable, always valid UTF-8. Text and raw bytes are distinct types, and converting between them names the encoding explicitly (Python's lesson). Invalid input is an error at the conversion, never a silently corrupted string.
- **No integer indexing by code point.** On UTF-8 that would be O(n) per access, and code points aren't user-perceived characters anyway ("é" may be one or two; a flag emoji is two).
- Slicing uses **byte offsets**, which must fall on character boundaries. Searching returns byte offsets, so search-then-slice is O(1).
- Code points and grapheme clusters are reached by **iteration** (`chars`, `graphemes`), not indexing.
- **Byte access is first-class**: viewing a string's bytes is cheap and needs no copy, because it matters even for Unicode text (protocols, hashing, parsing).
- No Python-style variable-width storage (PEP 393) to buy O(1) code-point indexing.
- Separate mutable byte buffers for performance-critical code.
- **File paths and OS strings are not text**: they get their own type (Rust's `OsString`, not Python's `surrogateescape`). #25
- **No ambient locale**: text operations, number parsing and formatting use a fixed default (US English) unless the programmer passes a locale explicitly. #26
- Slices sharing their parent's buffer can pin a large buffer alive (Java before 7u6); how to handle that is open: #27

```scheme
let s := "hello 世界"
(slice s 0 5)     ; -> "hello" (byte offsets)
(slice s 6 9)     ; -> "世" (3 bytes)
(slice s 6 7)     ; -> error: not a character boundary
(chars s)         ; -> iterator of code points
(graphemes s)     ; -> iterator of grapheme clusters
(bytes s)         ; -> byte view, no copy
```

**Numeric Types:**

Rust-style explicit sizing when performance matters:
- `i8`, `i16`, `i32`, `i64` - signed integers
- `u8`, `u16`, `u32`, `u64` - unsigned integers
- `f32`, `f64` - floats
- `Integer` - arbitrary precision (bigint behind the scenes)
- `Rational` - exact rational numbers
- `Complex` - complex numbers

Clear types, no implicit conversions, hardware-close when needed.

**Overflow** behaves the same in every build, never undefined (C) or build-dependent (Rust debug vs release). Wraparound is the preferred definition. Exact semantics, and how they relate to `Integer`: #28

**Equality and ordering (Rust's model):** partial and total are separate traits. Floats implement only the partial versions, since IEEE 754's NaN ≠ NaN breaks reflexivity. Anything that needs lawful equality or ordering requires the total trait: set members, map keys, sorting, deduplication. Making partial equality ergonomic, so that floats don't become a constant papercut, is open: #17.

## Standard Library

**Philosophy: Batteries Included (Python-inspired)**

Core functionality should be in stdlib, not external packages:
- **Collections**: vectors, dicts, sets, queues, etc.
- **I/O**: files, streams, buffering
- **Networking**: TCP, UDP, HTTP client
- **Datetime**: parsing, formatting, timezones
- **Regex**: pattern matching
- **Concurrency**: channels, threads, synchronization
- **Math**: trigonometry, statistics, random, matrices, tensors
- **Text**: string manipulation, Unicode support

## Tooling and Development Experience

**Package Manager (Cargo-inspired):**
- Integrated build system and package manager
- Dependency resolution that actually works
- Avoid npm/pip/cabal/rubygems nightmares
- Lock files for reproducible builds
- Central registry with version management

**Unified CLI:**

The `lyra` command is a shim that dispatches to specialized tools (like `cargo` or `go`):

```bash
lyra new myproject      # Create new project
lyra build              # Compile code (lyra-compiler)
lyra test               # Run tests (lyra-test)
lyra run                # Execute code (lyra-interpreter)
lyra fmt                # Format code (lyra-fmt)
lyra check              # Type check without building
lyra add package        # Add dependency (lyra-pkg)
```

Single entry point, consistent interface. Tools can be separate binaries or linked into one.

**Formatter:**
- Opinionated, automatic formatting (like `gofmt`)
- One true style, no bikeshedding
- Integrated into build tooling
- Runs on save in IDE

**LSP (Language Server Protocol):**
- Autocomplete with type information
- Go-to-definition
- Hover documentation
- Inline error diagnostics
- Refactoring support
- Essential for IDE integration

**Debugger:**
- Interactive debugging is essential
- Breakpoints, step-through, inspect values
- REPL-integrated and standalone modes
- Stack traces with source locations
- Variable inspection and modification

**Testing Framework:**
```scheme
; Built-in test framework
(deftest test-factorial
  (assert-eq (fac 5) 120)
  (assert-eq (fac 0) 1)
  (assert-throws (fac -1)))

; Property-based testing
(defproperty prop-reverse
  (forall [xs (gen-list gen-int)]
    (= xs (reverse (reverse xs)))))
```

Run with:
```bash
lyra test              # Run all tests
lyra test --watch      # Run on file changes
lyra test test-factorial  # Run specific test
```

Property-based testing may be external library initially, but core should provide primitives.

**REPL Features:**

```scheme
lyra> fn foo(x) -> x * 2
lyra> foo(5)
10

; Hot code reloading
lyra> ; Edit foo.lyra in editor
lyra> :reload foo
; foo redefined, existing code updated

; Interactive debugging
lyra> :break foo
lyra> foo(5)
; Breaks at foo
debug> inspect x
x = 5
debug> step
debug> continue

; Pretty printing (via Show trait)
lyra> my-point
Point { x: 10, y: 20 }
```

Hot reloading + interactive debugging + pretty printing = excellent development experience.

**FFI (Foreign Function Interface):**

Goals (defer detailed design):
- C interop (call C from Lyra, expose Lyra to C)
- Safe by default (unsafe blocks for raw pointers)
- Don't warp the language around FFI (unlike C++)
- Inspirations: Python ctypes, Ruby FFI, Haskell FFI

## Implementation Status

Currently: Basic Scheme interpreter in C with alloc-and-pray memory management

Next steps:
1. Implement mark-and-sweep GC
2. Complete basic Scheme functionality (closures, macros)
3. Add Lyra-specific syntax features (indentation, infix, sugar)
4. Implement type inference and checking
5. Build compiler (targets C initially)
6. Self-hosting (compiler written in Lyra)

## Non-Goals

- **Not a Scheme**: Will diverge significantly
- **Not pure functional**: Pragmatism over dogma
- **Not minimal**: Syntax sugar is good, actually
- **Not slow**: Performance is a feature
- **No first-class continuations**: no `call/cc`, and no user-facing delimited continuations. Rank-and-file programmers don't need them, and they cost performance everywhere

## Anti-Patterns to Avoid

Learning from others' mistakes:

**Python 2→3: Breaking Changes Are Catastrophic**
- Never break syntax in production
- Deprecation path must be years long
- Maintain backward compatibility aggressively
- Version fragmentation kills ecosystems

**Python GIL: Parallelism Must Be Real**
- No global interpreter lock
- True multi-threading from day one
- Don't paint yourself into a single-threaded corner
- (See Concurrency section: green threads + immutability)

**Haskell Strings: Performance-Critical Types Matter**
- Strings as linked lists of chars is insane
- Default string type must be efficient (UTF-8 byte arrays)
- Don't sacrifice performance for theoretical elegance
- Linked lists are fine for algorithms, not for text

**Python: Bytes Are Not Text**
- An encoded string and raw bytes are different things, and the type system should say so
- Every encoding/decoding step names its encoding. Nothing depends on the ambient locale
- (See Strings: Rust's model, no code-point indexing, first-class byte access)

**Haskell: Partial Functions in the Standard Library**
- `head []` shouldn't explode
- Functions that can't produce a result for some inputs return `Option`/`Result`
- (See Error Handling: Absence)

**IEEE 754: NaN Breaks Equality**
- NaN ≠ NaN makes float equality non-reflexive, which silently breaks sets, maps and sorting
- Separate partial from total equality/ordering, as Rust does, and make the partial case ergonomic (#17)

**Go: Leaked Goroutines**
- Unstructured `go` lets tasks outlive their purpose. Errors vanish, leaks pile up
- Structured concurrency: task lifetimes bounded by scope (#18)

**Lua: Assignment Creates Globals**
- A typo in an assignment silently creates a new global
- Assignment never creates a binding (see Syntax)

**Java: Checked Exceptions and Finalizers**
- Checked exceptions don't compose with `map`, folds or any higher-order function
- Finalizers make cleanup depend on GC timing
- No exceptions, errors are values. Cleanup is scoped (see Error Handling)

**JavaScript: Standard Library and Naming Consistency**
- Comprehensive standard library from the start
- Consistent naming conventions (not `parseInt` vs `parseFloat` vs `Number.parse...`)
- No weird legacy quirks (`typeof null === "object"`)
- Think through naming before shipping

**TypeScript: Don't Bolt Types Onto Untyped Language**
- TypeScript is amazing but it's making the best of a bad situation
- Type system should be sound from the start
- No `any` escape hatches everywhere
- Don't design a type system that can't handle trivial cases
- If you're adding types, make them actually work

**Common Lisp: Features Need Good Syntax**
- CL has every feature under the sun, all with awful ugly syntax
- `(loop for x from 0 to 10 collecting (* x x))` - trying to be English, failing
- `(defmethod foo ((x (eql :bar))) ...)` - parentheses hell
- Features are great, but syntax matters
- Convenience features should be convenient to use

**APL: Ergonomics and Expressiveness**
- Array-oriented thinking is brilliant
- Operator→function→array hierarchy forbids higher-order user functions (gross)
- Can't pass user-defined functions around freely (huge functional limitation)
- Symbols you can't type on regular keyboard = DOA for adoption
- Terseness is good, but not at the cost of accessibility
- Take the ideas (array operations), skip the problems (weird symbols, rigid hierarchy)

**General Principle:**
Get the fundamentals right early. Concurrency model, memory model, core types, and naming are nearly impossible to fix later without breaking the world.

## Philosophy on Design Decisions

Every feature must be:
1. **Tasteful**: Better than existing implementations
2. **Justified**: Solves a real problem
3. **Composable**: Works well with other features
4. **Performant**: Doesn't compromise speed goals

Learning from the mistakes and successes of Python, Ruby, Lua, Common Lisp, Scheme, Haskell, JavaScript/TypeScript, Rust, and APL.
