// String intrinsics. A string is UTF-8 text addressed by byte offset (see
// DESIGN.md "Strings"); there's no char type, so a "character" is an int in
// 0-255. Strings are length-tracked UT_strings and may contain NUL bytes, so
// nothing here uses strlen or strcmp on contents.
//
// Failure convention: out-of-range access, a bad slice, a failed search and an
// unparseable number return `missing` rather than panicking. Wrong argument
// types and out-of-range byte values in `from-bytes` still panic.

#include "intrinsics.h"
#include "panic.h"
#include "repr.h"
#include "types/int.h"
#include "types/list.h"
#include "types/boolean.h"
#include "types/string.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

// ---- Helpers ---------------------------------------------------------------

static void expect_n_args(const char *name, Value *args, int n) {
    int got = list_length(args);
    if (got != n) {
        lyra_panic("%s expects exactly %d %s, but got %d", name, n, n == 1 ? "argument" : "arguments", got);
    }
}

static const char *describe(GC *gc, Value *v) {
    return utstring_body(repr(gc, v));
}

static UT_string *expect_str(GC *gc, const char *name, Value *v) {
    if (v->type != STRING) lyra_panic("%s expects a string, but got %s", name, describe(gc, v));
    return v->data.as_string;
}

static int expect_int_arg(GC *gc, const char *name, Value *v) {
    if (v->type != INT) lyra_panic("%s expects an int, but got %s", name, describe(gc, v));
    return v->data.as_int;
}

static Value *expect_list_arg(GC *gc, const char *name, Value *v) {
    if (v->type != CONS && !is_nil(v)) lyra_panic("%s expects a list, but got %s", name, describe(gc, v));
    return v;
}

static Value *new_string(GC *gc, const char *bytes, size_t len) {
    UT_string *s = gc_alloc_string(gc);
    utstring_bincpy(s, bytes, len);
    return make_string(gc, s);
}

// A UTF-8 continuation byte (10xxxxxx) can't start a character, so an offset
// that lands on one splits a multi-byte sequence. `off` is in [0, len].
static bool at_boundary(const char *s, size_t len, size_t off) {
    return off >= len || ((unsigned char)s[off] & 0xC0) != 0x80;
}

// First occurrence of needle in hay at or after `from`, or -1. Byte-wise, so
// NULs are ordinary bytes.
static long find_bytes(const char *hay, size_t hay_len, const char *needle, size_t needle_len, size_t from) {
    if (needle_len == 0) return (long)from;
    if (needle_len > hay_len) return -1;
    for (size_t i = from; i + needle_len <= hay_len; i++) {
        if (hay[i] == needle[0] && memcmp(hay + i, needle, needle_len) == 0) return (long)i;
    }
    return -1;
}

// ---- Ordering --------------------------------------------------------------

// Bytewise: memcmp over the common prefix, then the shorter string is smaller.
int string_compare(Value *a, Value *b) {
    size_t la = utstring_len(a->data.as_string), lb = utstring_len(b->data.as_string);
    int c = memcmp(utstring_body(a->data.as_string), utstring_body(b->data.as_string), la < lb ? la : lb);
    if (c != 0) return c < 0 ? -1 : 1;
    return la < lb ? -1 : la > lb ? 1 : 0;
}

// ---- Access ----------------------------------------------------------------

Value *intrinsic_byte(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // (byte s i) => int 0-255, or missing if i is out of range
    expect_n_args("byte", args, 2);
    UT_string *s = expect_str(gc, "byte", list_index(args, 0));
    int i = expect_int_arg(gc, "byte", list_index(args, 1));
    if (i < 0 || (size_t)i >= utstring_len(s)) return MISSING;
    return make_int(gc, (unsigned char)utstring_body(s)[i]);
}

Value *intrinsic_slice(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // (slice s start end) => the bytes [start, end), or missing
    expect_n_args("slice", args, 3);
    UT_string *s = expect_str(gc, "slice", list_index(args, 0));
    int start = expect_int_arg(gc, "slice", list_index(args, 1));
    int end = expect_int_arg(gc, "slice", list_index(args, 2));
    size_t len = utstring_len(s);
    if (start < 0 || end < start || (size_t)end > len) return MISSING;
    const char *body = utstring_body(s);
    if (!at_boundary(body, len, (size_t)start) || !at_boundary(body, len, (size_t)end)) return MISSING;
    return new_string(gc, body + start, (size_t)(end - start));
}

// ---- Searching -------------------------------------------------------------

Value *intrinsic_find(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // (find s sub) / (find s sub start) => byte offset of the first match at or
    // after start, or missing. An empty `sub` matches at `start`.
    int n = list_length(args);
    if (n != 2 && n != 3) lyra_panic("find expects 2 or 3 arguments, but got %d", n);
    UT_string *s = expect_str(gc, "find", list_index(args, 0));
    UT_string *sub = expect_str(gc, "find", list_index(args, 1));
    int start = n == 3 ? expect_int_arg(gc, "find", list_index(args, 2)) : 0;
    size_t len = utstring_len(s);
    if (start < 0 || (size_t)start > len) return MISSING;
    if (!at_boundary(utstring_body(s), len, (size_t)start)) return MISSING;
    long at = find_bytes(utstring_body(s), len, utstring_body(sub), utstring_len(sub), (size_t)start);
    return at < 0 ? MISSING : make_int(gc, (int)at);
}

Value *intrinsic_starts_with(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // (starts-with? s prefix)
    expect_n_args("starts-with?", args, 2);
    UT_string *s = expect_str(gc, "starts-with?", list_index(args, 0));
    UT_string *p = expect_str(gc, "starts-with?", list_index(args, 1));
    size_t lp = utstring_len(p);
    return lp <= utstring_len(s) && memcmp(utstring_body(s), utstring_body(p), lp) == 0 ? TRUE : FALSE;
}

Value *intrinsic_ends_with(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // (ends-with? s suffix)
    expect_n_args("ends-with?", args, 2);
    UT_string *s = expect_str(gc, "ends-with?", list_index(args, 0));
    UT_string *p = expect_str(gc, "ends-with?", list_index(args, 1));
    size_t ls = utstring_len(s), lp = utstring_len(p);
    return lp <= ls && memcmp(utstring_body(s) + (ls - lp), utstring_body(p), lp) == 0 ? TRUE : FALSE;
}

// ---- Conversion ------------------------------------------------------------

Value *intrinsic_parse_int(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // (parse-int s) => int, or missing. Same range rules as an integer
    // literal (strtoll, then INT_MIN..INT_MAX), plus an optional sign. The
    // whole string must be the number: no whitespace, no trailing text.
    expect_n_args("parse-int", args, 1);
    UT_string *s = expect_str(gc, "parse-int", list_index(args, 0));
    const char *body = utstring_body(s);
    size_t len = utstring_len(s);
    size_t digits = len > 0 && (body[0] == '-' || body[0] == '+') ? 1 : 0;
    if (digits == len) return MISSING;
    for (size_t i = digits; i < len; i++) {
        if (body[i] < '0' || body[i] > '9') return MISSING;  // also rejects NULs
    }
    errno = 0;
    long long n = strtoll(body, NULL, 10);  // UT_string bodies are NUL-terminated
    if (errno == ERANGE || n < INT_MIN || n > INT_MAX) return MISSING;
    return make_int(gc, (int)n);
}

Value *intrinsic_bytes(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // (bytes s) => list of ints 0-255
    expect_n_args("bytes", args, 1);
    UT_string *s = expect_str(gc, "bytes", list_index(args, 0));
    const char *body = utstring_body(s);
    Value *result = NIL;
    for (size_t i = utstring_len(s); i > 0; i--) {
        result = list_append(gc, make_int(gc, (unsigned char)body[i - 1]), result);
    }
    return result;
}

Value *intrinsic_from_bytes(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // (from-bytes list) => string. Every element must be an int in 0-255.
    expect_n_args("from-bytes", args, 1);
    Value *list = expect_list_arg(gc, "from-bytes", list_index(args, 0));
    UT_string *out = gc_alloc_string(gc);
    for (Value *r = list; !is_nil(r); r = list_tail(r)) {
        Value *b = list_head(r);
        if (b->type != INT) lyra_panic("from-bytes expects a list of ints, but got element %s", describe(gc, b));
        if (b->data.as_int < 0 || b->data.as_int > 255) {
            lyra_panic("from-bytes expects bytes in 0-255, but got %d", b->data.as_int);
        }
        char c = (char)b->data.as_int;
        utstring_bincpy(out, &c, 1);
    }
    return make_string(gc, out);
}

Value *intrinsic_join(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // (join list-of-strings separator) => one string, built in a single pass
    expect_n_args("join", args, 2);
    Value *list = expect_list_arg(gc, "join", list_index(args, 0));
    UT_string *sep = expect_str(gc, "join", list_index(args, 1));
    size_t total = 0;
    int count = 0;
    for (Value *r = list; !is_nil(r); r = list_tail(r), count++) {
        total += utstring_len(expect_str(gc, "join", list_head(r)));
    }
    if (count > 1) total += (size_t)(count - 1) * utstring_len(sep);
    UT_string *out = gc_alloc_string(gc);
    utstring_reserve(out, total);
    bool first = true;
    for (Value *r = list; !is_nil(r); r = list_tail(r)) {
        if (!first) utstring_bincpy(out, utstring_body(sep), utstring_len(sep));
        first = false;
        UT_string *piece = list_head(r)->data.as_string;
        utstring_bincpy(out, utstring_body(piece), utstring_len(piece));
    }
    return make_string(gc, out);
}
