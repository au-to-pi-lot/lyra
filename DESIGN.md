# Lyra Language Design

## Vision

Lyra is a high-performance, garbage-collected, functional-first programming language with pragmatic concessions to imperativity. It synthesizes the best ideas from multiple language traditions while avoiding their pitfalls.

## Core Philosophy

- **Functional-first, not functional-pure**: Immutability by default, but pragmatic escape hatches
- **Performance matters**: Inspired by SBCL and LuaJIT
- **Ergonomics matter**: Learning from decades of language design
- **Native concurrency**: Multi-threaded and concurrent from the ground up
- **Tasteful syntax**: Convenience features done right

## Language Influences

### Scheme
- Hygienic macros
- Single namespace for functions and values
- Core intrinsic functions and macros
- Starting implementation base

### Clojure
- Spelling: `true`, `false`, `nil`
- No explicit user-level cons cells
- Additional bracket syntax for data structures `[]` `{}`

### Python
- Splat syntax (`*args`, `**kwargs`)
- F-string interpolation
- `range()` and other standard library naming
- **Not**: Module system (execution-based imports cause circular dependency issues)

### Ruby
- Naming conventions: `hash` over `dict`
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
- All spaces OR all tabs - no mixing within a file
- Lexer tracks which style is used, errors on mixed indentation
- Both camps can use their preference
- Blank lines and comments ignored for indentation

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
- Users can add custom syntax transformations
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

# {} - Sets and maps (distinguished by : syntax)
{1, 2, 3}           # Set (homogeneous elements)
{a: 1, b: 2}        # Map (key-value pairs with :)
{a: 1, 2}           # Syntax error (can't mix)

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
(has-next? iter) -> bool
(next iter) -> value
```

Or simpler: return `nil` when exhausted (Lua-style).

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
- Retroactive implementation (add traits to existing types)
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

**Compilation Targets:**
- C code generation (initial target - portable, optimizable by C compiler)
- LLVM IR (eventual - better optimization, easier debugging)
- JIT compilation (eventual - for REPL and dynamic code)

**Self-Hosting Goal:**

Write the Lyra compiler in Lyra itself. Self-hosting is the ultimate test that the language is expressive and ergonomic enough for complex programs.

**Path to Self-Hosting:**
1. C interpreter (current) - define semantics
2. Mark & Sweep GC - memory management
3. Complete core language - closures, macros, pattern matching
4. Add Lyra features - syntax sugar, types, modules, traits
5. Write compiler in Lyra - targets C or LLVM IR
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

**Result Types over Exceptions:**

Exceptions are procedural and hide control flow. Lyra uses Result types for recoverable errors:

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

**Panics for Unrecoverable Errors:**
```scheme
panic("Unreachable code reached!")  ; Unwinds, prints stack trace, kills thread/program
```

**Guidelines:**
- **Result**: Recoverable errors caller should handle (file not found, parse error, validation)
- **Panic**: Programmer errors that shouldn't happen (assertion failure, unreachable code, index out of bounds)

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

**Strings:**
- UTF-8 encoded immutable byte arrays
- Indexing returns code points (not bytes)
- Separate mutable byte buffers for performance-critical code

```scheme
let s := "hello 世界"
s[0]        ; -> "h" (code point)
s[6]        ; -> "世" (code point)
(bytes s)   ; -> byte array if you need bytes
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

## Standard Library

**Philosophy: Batteries Included (Python-inspired)**

Core functionality should be in stdlib, not external packages:
- **Collections**: vectors, hash maps, sets, queues, etc.
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
