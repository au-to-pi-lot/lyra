#pragma once

#include "types/value.h"

Value *call(GC *gc, Closure *closure, Callable *callable, Value *arguments);

// Run a derived macro's body against (unevaluated) args in a child of
// `closure` and return the expansion, without evaluating it.
Value *expand_derived_macro(GC *gc, Derived callable, Value *args);

// Expand `form` one step if its head is a symbol bound (in `closure`) to a
// derived macro. Anything else -- atoms, nil, non-macro heads, intrinsic
// macros/special forms -- is returned unchanged. `*expanded` (may be NULL)
// reports whether an expansion happened. Returns NULL on expansion error.
Value *macroexpand_1(GC *gc, Closure *closure, Value *form, bool *expanded);

// Repeatedly apply macroexpand_1 at the head until the form is no longer a
// derived-macro call. Subforms are not expanded.
Value *macroexpand(GC *gc, Closure *closure, Value *form, bool *expanded);
