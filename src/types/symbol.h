#include "value.h"

/// Symbols come in two flavours, both of type SYMBOL:
///
/// - Interned symbols (everything the reader or `symbol` produces) are
///   identified by their name: two interned symbols named `x` are equal.
/// - Uninterned symbols (from `unique` and `gensym`) are equal only to
///   themselves, and can never be equal to an interned symbol.
///
/// Representation: a symbol's `as_symbol` UT_string is its *identity key*,
/// which is also the key it binds under in a Closure. For an interned symbol
/// the key is just its name. For an uninterned symbol it is
/// `name '\0' <serial>`, where the serial is drawn from a process-wide
/// counter. No interned symbol can contain a NUL byte (the reader and
/// `symbol` only build names from C strings), so an uninterned key can never
/// collide with an interned one, nor with another uninterned symbol.
/// `utstring_body` still yields the print name, since it stops at the NUL.

Value *make_symbol(GC *gc, UT_string *name);

Value *symbol_from_char(GC *gc, char *name);

/// @brief Allocate a fresh uninterned symbol with the given print name.
Value *make_uninterned_symbol(GC *gc, const char *name);

/// @brief True unless `sym` is an uninterned symbol.
bool symbol_is_interned(Value *sym);

/// @brief Symbol equality: interned by name, uninterned by identity.
bool symbol_eq(Value *a, Value *b);

/// @brief True iff `val` is the *interned* symbol named `name`.
/// Use this, not strcmp on the body, to recognise special forms and keywords.
bool symbol_is(Value *val, const char *name);

/// @brief Copy of `key` without its first `skip` bytes, keeping the hidden
/// serial of an uninterned key (used to strip the `*` of variadic names).
UT_string *symbol_key_suffix(GC *gc, UT_string *key, size_t skip);

Value *make_variadic_marker(GC *gc, UT_string *name);
