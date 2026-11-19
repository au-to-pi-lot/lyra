// Tests for the string intrinsics (src/intrinsics_string.c), plus the string
// parts of `count`, `<`/`>` ordering, `=` and hashing.
//
// Multi-byte text is written as C hex escapes, split into separate literals so
// a following hex digit isn't swallowed: "\xc3\xa9" is U+00E9 (2 bytes) and
// "\xe2\x82\xac" is U+20AC (3 bytes).
#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "../src/eval.h"
#include "../src/parser.h"
#include "../src/prelude.h"
#include "../src/closure.h"
#include "../src/gc.h"
#include "../src/intrinsics.h"
#include "../src/repr.h"
#include "../src/panic.h"
#include "../src/types/value.h"
#include "../src/types/list.h"
#include "../src/types/int.h"
#include "../src/types/string.h"

typedef struct Interp {
    GC gc;
    Closure *global;
} Interp;

static void interp_init(Interp *in) {
    gc_init(&in->gc);
    in->global = make_closure(&in->gc, NULL);
    prelude(&in->gc, in->global);
}

static void interp_free(Interp *in) {
    gc_free_all(&in->gc);
}

static Value *run(Interp *in, const char *src) {
    Value *parsed = parse(&in->gc, src);
    ck_assert_msg(parsed != NULL, "parse failed: %s", src);
    Value *result = eval_s_expr(&in->gc, in->global, parsed);
    ck_assert_msg(result != NULL, "evaluation failed: %s", src);
    return result;
}

static bool run_true(Interp *in, const char *src) {
    Value *v = run(in, src);
    ck_assert_msg(v->type == BOOLEAN, "expected a boolean from %s", src);
    return v->data.as_boolean;
}

static int run_int(Interp *in, const char *src) {
    Value *v = run(in, src);
    ck_assert_msg(v->type == INT, "expected an int from %s", src);
    return v->data.as_int;
}

static bool run_missing(Interp *in, const char *src) {
    return run(in, src) == MISSING;
}

// Compares the result's full bytes, so embedded NULs count.
static void assert_string(Interp *in, const char *src, const char *expected, size_t len) {
    Value *v = run(in, src);
    ck_assert_msg(v->type == STRING, "expected a string from %s", src);
    UT_string *s = v->data.as_string;
    ck_assert_msg(utstring_len(s) == len, "length %zu, expected %zu, from %s", (size_t)utstring_len(s), len, src);
    ck_assert_msg(memcmp(utstring_body(s), expected, len) == 0, "wrong bytes from %s", src);
}

#define ASSERT_STR(in, src, lit) assert_string((in), (src), (lit), sizeof(lit) - 1)

static bool eval_panics(Interp *in, const char *src) {
    Value *parsed = parse(&in->gc, src);
    ck_assert_msg(parsed != NULL, "parse failed: %s", src);
    PanicHandler handler;
    panic_push(&handler);
    if (setjmp(handler.env) == 0) {
        eval_s_expr(&in->gc, in->global, parsed);
        panic_pop(&handler);
        return false;
    }
    return true;
}

#define ck_assert_panics(in, src, expected)                                    \
    do {                                                                       \
        bool panicked_ = eval_panics((in), (src));                             \
        ck_assert_msg(panicked_, "expected a panic from: %s", (src));          \
        ck_assert_str_eq(panic_message(), (expected));                         \
    } while (0)

// ---- count -------------------------------------------------------------------

START_TEST(test_count)
{
    Interp in; interp_init(&in);
    ck_assert_int_eq(run_int(&in, "(count \"\")"), 0);
    ck_assert_int_eq(run_int(&in, "(count \"hello\")"), 5);
    // Byte length, not code points
    ck_assert_int_eq(run_int(&in, "(count \"h\xc3\xa9llo\")"), 6);
    ck_assert_int_eq(run_int(&in, "(count \"\xe2\x82\xac\")"), 3);
    // Embedded NUL counts
    ck_assert_int_eq(run_int(&in, "(count (from-bytes '(97 0 98)))"), 3);
    ck_assert_int_eq(run_int(&in, "(count '(1 2 3))"), 3);
    ck_assert_int_eq(run_int(&in, "(count '())"), 0);
    ck_assert_int_eq(run_int(&in, "(count (dict ('a 1)))"), 1);
    ck_assert_panics(&in, "(count 5)", "count expects a dict, set, list or string, but got 5");
    interp_free(&in);
}
END_TEST

// ---- byte --------------------------------------------------------------------

START_TEST(test_byte)
{
    Interp in; interp_init(&in);
    ck_assert_int_eq(run_int(&in, "(byte \"abc\" 0)"), 97);
    ck_assert_int_eq(run_int(&in, "(byte \"abc\" 2)"), 99);
    ck_assert(run_missing(&in, "(byte \"abc\" 3)"));
    ck_assert(run_missing(&in, "(byte \"abc\" -1)"));
    ck_assert(run_missing(&in, "(byte \"\" 0)"));
    // Bytes above 127 are 128-255, never negative
    ck_assert_int_eq(run_int(&in, "(byte \"\xc3\xa9\" 0)"), 0xc3);
    ck_assert_int_eq(run_int(&in, "(byte \"\xc3\xa9\" 1)"), 0xa9);
    ck_assert_int_eq(run_int(&in, "(byte (from-bytes '(255)) 0)"), 255);
    ck_assert_int_eq(run_int(&in, "(byte (from-bytes '(97 0 98)) 1)"), 0);
    ck_assert_int_eq(run_int(&in, "(byte (from-bytes '(97 0 98)) 2)"), 98);
    ck_assert_panics(&in, "(byte 5 0)", "byte expects a string, but got 5");
    ck_assert_panics(&in, "(byte \"a\" \"0\")", "byte expects an int, but got \"0\"");
    ck_assert_panics(&in, "(byte \"a\")", "byte expects exactly 2 arguments, but got 1");
    interp_free(&in);
}
END_TEST

// ---- slice -------------------------------------------------------------------

START_TEST(test_slice)
{
    Interp in; interp_init(&in);
    ASSERT_STR(&in, "(slice \"hello\" 1 3)", "el");
    ASSERT_STR(&in, "(slice \"hello\" 0 5)", "hello");
    ASSERT_STR(&in, "(slice \"hello\" 2 2)", "");
    ASSERT_STR(&in, "(slice \"hello\" 5 5)", "");
    ASSERT_STR(&in, "(slice \"\" 0 0)", "");
    ck_assert(run_missing(&in, "(slice \"hello\" 3 2)"));
    ck_assert(run_missing(&in, "(slice \"hello\" -1 2)"));
    ck_assert(run_missing(&in, "(slice \"hello\" 0 6)"));
    ck_assert(run_missing(&in, "(slice \"hello\" 6 6)"));
    ck_assert(run_missing(&in, "(slice \"\" 0 1)"));
    // Embedded NUL survives a slice
    assert_string(&in, "(slice (from-bytes '(97 0 98 99)) 1 3)", "\0" "b", 2);
    ck_assert_panics(&in, "(slice \"a\" 0)", "slice expects exactly 3 arguments, but got 2");
    ck_assert_panics(&in, "(slice 'a 0 1)", "slice expects a string, but got a");
    interp_free(&in);
}
END_TEST

START_TEST(test_slice_utf8_boundaries)
{
    Interp in; interp_init(&in);
    // "a" U+00E9 "b" U+20AC "c": bytes a | c3 a9 | b | e2 82 ac | c  (8 bytes)
    const char *s = "\"a\xc3\xa9" "b\xe2\x82\xac" "c\"";
    char buf[128];
#define SLICE(a, b) (snprintf(buf, sizeof buf, "(slice %s %d %d)", s, (a), (b)), buf)
    ASSERT_STR(&in, SLICE(1, 3), "\xc3\xa9");          // whole 2-byte char
    ASSERT_STR(&in, SLICE(4, 7), "\xe2\x82\xac");      // whole 3-byte char
    ASSERT_STR(&in, SLICE(0, 8), "a\xc3\xa9" "b\xe2\x82\xac" "c");
    // Start or end inside a sequence
    ck_assert(run_missing(&in, SLICE(2, 3)));  // start on continuation
    ck_assert(run_missing(&in, SLICE(1, 2)));  // end on continuation
    ck_assert(run_missing(&in, SLICE(5, 7)));
    ck_assert(run_missing(&in, SLICE(4, 6)));
    ck_assert(run_missing(&in, SLICE(0, 2)));
    // An empty slice inside a sequence is still a split
    ck_assert(run_missing(&in, SLICE(2, 2)));
    // Boundary at the very end is fine
    ASSERT_STR(&in, SLICE(8, 8), "");
#undef SLICE
    interp_free(&in);
}
END_TEST

// ---- find --------------------------------------------------------------------

START_TEST(test_find)
{
    Interp in; interp_init(&in);
    ck_assert_int_eq(run_int(&in, "(find \"hello world\" \"o\")"), 4);
    ck_assert_int_eq(run_int(&in, "(find \"hello world\" \"world\")"), 6);
    ck_assert_int_eq(run_int(&in, "(find \"hello\" \"hello\")"), 0);
    ck_assert(run_missing(&in, "(find \"hello\" \"help\")"));
    ck_assert(run_missing(&in, "(find \"hello\" \"hello!\")"));
    ck_assert(run_missing(&in, "(find \"\" \"a\")"));
    // Start offset
    ck_assert_int_eq(run_int(&in, "(find \"hello world\" \"o\" 5)"), 7);
    ck_assert_int_eq(run_int(&in, "(find \"hello world\" \"o\" 4)"), 4);
    ck_assert(run_missing(&in, "(find \"hello world\" \"o\" 8)"));
    ck_assert(run_missing(&in, "(find \"abc\" \"a\" 1)"));
    // Empty needle matches at the start offset, including at the very end
    ck_assert_int_eq(run_int(&in, "(find \"abc\" \"\")"), 0);
    ck_assert_int_eq(run_int(&in, "(find \"abc\" \"\" 2)"), 2);
    ck_assert_int_eq(run_int(&in, "(find \"abc\" \"\" 3)"), 3);
    ck_assert_int_eq(run_int(&in, "(find \"\" \"\")"), 0);
    // Start out of range
    ck_assert(run_missing(&in, "(find \"abc\" \"a\" 4)"));
    ck_assert(run_missing(&in, "(find \"abc\" \"\" 4)"));
    ck_assert(run_missing(&in, "(find \"abc\" \"a\" -1)"));
    // Overlapping prefixes
    ck_assert_int_eq(run_int(&in, "(find \"aaab\" \"aab\")"), 1);
    // Multi-byte: offsets are bytes
    ck_assert_int_eq(run_int(&in, "(find \"h\xc3\xa9llo\" \"llo\")"), 3);
    ck_assert_int_eq(run_int(&in, "(find \"h\xc3\xa9llo\" \"\xc3\xa9\")"), 1);
    // A start offset inside a sequence is refused
    ck_assert(run_missing(&in, "(find \"h\xc3\xa9llo\" \"l\" 2)"));
    // Embedded NULs are ordinary bytes
    ck_assert_int_eq(run_int(&in, "(find (from-bytes '(97 0 98 0 99)) (from-bytes '(0 99)))"), 3);
    ck_assert_int_eq(run_int(&in, "(find (from-bytes '(97 0 98)) (from-bytes '(0)))"), 1);
    ck_assert(run_missing(&in, "(find (from-bytes '(97 0 98)) (from-bytes '(98 0)))"));
    ck_assert_panics(&in, "(find 'a \"a\")", "find expects a string, but got a");
    ck_assert_panics(&in, "(find \"a\")", "find expects 2 or 3 arguments, but got 1");
    interp_free(&in);
}
END_TEST

// ---- starts-with? / ends-with? -------------------------------------------------

START_TEST(test_starts_ends_with)
{
    Interp in; interp_init(&in);
    ck_assert(run_true(&in, "(starts-with? \"hello\" \"he\")"));
    ck_assert(run_true(&in, "(starts-with? \"hello\" \"hello\")"));
    ck_assert(run_true(&in, "(starts-with? \"hello\" \"\")"));
    ck_assert(run_true(&in, "(starts-with? \"\" \"\")"));
    ck_assert(!run_true(&in, "(starts-with? \"hello\" \"lo\")"));
    ck_assert(!run_true(&in, "(starts-with? \"he\" \"hello\")"));
    ck_assert(!run_true(&in, "(starts-with? \"\" \"a\")"));
    ck_assert(run_true(&in, "(ends-with? \"hello\" \"lo\")"));
    ck_assert(run_true(&in, "(ends-with? \"hello\" \"hello\")"));
    ck_assert(run_true(&in, "(ends-with? \"hello\" \"\")"));
    ck_assert(run_true(&in, "(ends-with? \"\" \"\")"));
    ck_assert(!run_true(&in, "(ends-with? \"hello\" \"he\")"));
    ck_assert(!run_true(&in, "(ends-with? \"lo\" \"hello\")"));
    ck_assert(!run_true(&in, "(ends-with? \"\" \"a\")"));
    // Multi-byte
    ck_assert(run_true(&in, "(starts-with? \"\xc3\xa9t\xc3\xa9\" \"\xc3\xa9\")"));
    ck_assert(run_true(&in, "(ends-with? \"\xc3\xa9t\xc3\xa9\" \"t\xc3\xa9\")"));
    // Embedded NUL: "a\0b" vs "a\0c"
    ck_assert(run_true(&in, "(starts-with? (from-bytes '(97 0 98)) (from-bytes '(97 0)))"));
    ck_assert(!run_true(&in, "(starts-with? (from-bytes '(97 0 98)) (from-bytes '(97 0 99)))"));
    ck_assert(run_true(&in, "(ends-with? (from-bytes '(97 0 98)) (from-bytes '(0 98)))"));
    ck_assert(!run_true(&in, "(ends-with? (from-bytes '(97 0 98)) (from-bytes '(0 99)))"));
    ck_assert_panics(&in, "(starts-with? 1 \"a\")", "starts-with? expects a string, but got 1");
    ck_assert_panics(&in, "(ends-with? \"a\" 1)", "ends-with? expects a string, but got 1");
    interp_free(&in);
}
END_TEST

// ---- parse-int ---------------------------------------------------------------

START_TEST(test_parse_int)
{
    Interp in; interp_init(&in);
    ck_assert_int_eq(run_int(&in, "(parse-int \"0\")"), 0);
    ck_assert_int_eq(run_int(&in, "(parse-int \"42\")"), 42);
    ck_assert_int_eq(run_int(&in, "(parse-int \"-42\")"), -42);
    ck_assert_int_eq(run_int(&in, "(parse-int \"+7\")"), 7);
    ck_assert_int_eq(run_int(&in, "(parse-int \"007\")"), 7);
    // Range: INT_MIN..INT_MAX, same as integer literals
    ck_assert_int_eq(run_int(&in, "(parse-int \"2147483647\")"), 2147483647);
    ck_assert_int_eq(run_int(&in, "(parse-int \"-2147483648\")"), -2147483647 - 1);
    ck_assert(run_missing(&in, "(parse-int \"2147483648\")"));
    ck_assert(run_missing(&in, "(parse-int \"-2147483649\")"));
    ck_assert(run_missing(&in, "(parse-int \"99999999999999999999999\")"));
    // Not a number
    ck_assert(run_missing(&in, "(parse-int \"\")"));
    ck_assert(run_missing(&in, "(parse-int \"-\")"));
    ck_assert(run_missing(&in, "(parse-int \"+\")"));
    ck_assert(run_missing(&in, "(parse-int \"abc\")"));
    ck_assert(run_missing(&in, "(parse-int \"12abc\")"));
    ck_assert(run_missing(&in, "(parse-int \"1.5\")"));
    ck_assert(run_missing(&in, "(parse-int \" 1\")"));
    ck_assert(run_missing(&in, "(parse-int \"1 \")"));
    ck_assert(run_missing(&in, "(parse-int \"0x10\")"));
    ck_assert(run_missing(&in, "(parse-int \"--1\")"));
    // An embedded NUL must not truncate the number: "12\0" is not 12
    ck_assert(run_missing(&in, "(parse-int (from-bytes '(49 50 0 51)))"));
    ck_assert(run_missing(&in, "(parse-int (from-bytes '(49 50 0)))"));
    ck_assert(run_missing(&in, "(parse-int (from-bytes '(0)))"));
    ck_assert_panics(&in, "(parse-int 5)", "parse-int expects a string, but got 5");
    interp_free(&in);
}
END_TEST

// ---- bytes / from-bytes ------------------------------------------------------

START_TEST(test_bytes_roundtrip)
{
    Interp in; interp_init(&in);
    ck_assert_str_eq(utstring_body(repr(&in.gc, run(&in, "(bytes \"abc\")"))), "(97 98 99)");
    ck_assert(is_nil(run(&in, "(bytes \"\")")));
    ck_assert_str_eq(utstring_body(repr(&in.gc, run(&in, "(bytes \"\xc3\xa9\xe2\x82\xac\")"))),
                     "(195 169 226 130 172)");
    ck_assert_str_eq(utstring_body(repr(&in.gc, run(&in, "(bytes (from-bytes '(97 0 255 128)))"))),
                     "(97 0 255 128)");
    ASSERT_STR(&in, "(from-bytes '(104 105))", "hi");
    ASSERT_STR(&in, "(from-bytes '())", "");
    ASSERT_STR(&in, "(from-bytes '(195 169))", "\xc3\xa9");
    assert_string(&in, "(from-bytes '(97 0 98))", "a\0b", 3);
    assert_string(&in, "(from-bytes '(0))", "\0", 1);
    ck_assert(run_true(&in, "(= (from-bytes (bytes \"h\xc3\xa9llo\")) \"h\xc3\xa9llo\")"));
    // Every byte value round-trips
    ck_assert(run_true(&in, "(= (bytes (from-bytes (bytes (from-bytes '(0 1 127 128 254 255))))) '(0 1 127 128 254 255))"));
    interp_free(&in);
}
END_TEST

START_TEST(test_from_bytes_errors)
{
    Interp in; interp_init(&in);
    ck_assert_panics(&in, "(from-bytes '(256))", "from-bytes expects bytes in 0-255, but got 256");
    ck_assert_panics(&in, "(from-bytes '(97 -1))", "from-bytes expects bytes in 0-255, but got -1");
    ck_assert_panics(&in, "(from-bytes '(\"a\"))", "from-bytes expects a list of ints, but got element \"a\"");
    ck_assert_panics(&in, "(from-bytes '(1.5))", "from-bytes expects a list of ints, but got element 1.5");
    ck_assert_panics(&in, "(from-bytes 5)", "from-bytes expects a list, but got 5");
    ck_assert_panics(&in, "(bytes 5)", "bytes expects a string, but got 5");
    interp_free(&in);
}
END_TEST

// ---- join --------------------------------------------------------------------

START_TEST(test_join)
{
    Interp in; interp_init(&in);
    ASSERT_STR(&in, "(join '(\"a\" \"b\" \"c\") \", \")", "a, b, c");
    ASSERT_STR(&in, "(join '(\"a\" \"b\" \"c\") \"\")", "abc");
    ASSERT_STR(&in, "(join '(\"only\") \",\")", "only");
    ASSERT_STR(&in, "(join '() \",\")", "");
    ASSERT_STR(&in, "(join '(\"\") \",\")", "");
    ASSERT_STR(&in, "(join '(\"\" \"\") \",\")", ",");
    ASSERT_STR(&in, "(join '(\"\xc3\xa9\" \"\xe2\x82\xac\") \"\xc3\xa9\")", "\xc3\xa9\xc3\xa9\xe2\x82\xac");
    // NULs in pieces and in the separator
    assert_string(&in, "(join (cons (from-bytes '(97 0)) (cons (from-bytes '(0 98)) nil)) (from-bytes '(0)))", "a\0\0\0" "b", 5);
    ck_assert_panics(&in, "(join '(\"a\" 1) \",\")", "join expects a string, but got 1");
    ck_assert_panics(&in, "(join '(\"a\") 1)", "join expects a string, but got 1");
    ck_assert_panics(&in, "(join \"a\" \",\")", "join expects a list, but got \"a\"");
    ck_assert_panics(&in, "(join '(\"a\"))", "join expects exactly 2 arguments, but got 1");
    interp_free(&in);
}
END_TEST

START_TEST(test_join_large)
{
    Interp in; interp_init(&in);
    // 16 pieces joined into a 256-byte string, then one 16-byte piece per byte of it
    run(&in, "(define piece \"0123456789abcdef\")");
    run(&in, "(define big (join (map (lambda (b) piece) (bytes piece)) \"\"))");
    run(&in, "(define parts (map (lambda (b) piece) (bytes big)))");
    ck_assert_int_eq(run_int(&in, "(count parts)"), 256);
    ck_assert_int_eq(run_int(&in, "(count (join parts \",\"))"), 256 * 16 + 255);
    interp_free(&in);
}
END_TEST

// ---- ordering and equality ---------------------------------------------------

START_TEST(test_string_ordering)
{
    Interp in; interp_init(&in);
    ck_assert(run_true(&in, "(< \"a\" \"b\")"));
    ck_assert(!run_true(&in, "(< \"b\" \"a\")"));
    ck_assert(!run_true(&in, "(< \"a\" \"a\")"));
    ck_assert(run_true(&in, "(<= \"a\" \"a\")"));
    ck_assert(run_true(&in, "(<= \"a\" \"b\")"));
    ck_assert(!run_true(&in, "(<= \"b\" \"a\")"));
    ck_assert(run_true(&in, "(> \"b\" \"a\")"));
    ck_assert(!run_true(&in, "(> \"a\" \"a\")"));
    ck_assert(run_true(&in, "(>= \"a\" \"a\")"));
    ck_assert(!run_true(&in, "(>= \"a\" \"b\")"));
    // Prefix sorts first; empty sorts before everything
    ck_assert(run_true(&in, "(< \"ab\" \"abc\")"));
    ck_assert(run_true(&in, "(< \"\" \"a\")"));
    ck_assert(!run_true(&in, "(< \"\" \"\")"));
    ck_assert(run_true(&in, "(<= \"\" \"\")"));
    // Uppercase sorts before lowercase (ASCII order)
    ck_assert(run_true(&in, "(< \"Z\" \"a\")"));
    // Bytes compare as unsigned: 0xc3 > 0x7f
    ck_assert(run_true(&in, "(< \"z\" \"\xc3\xa9\")"));
    ck_assert(run_true(&in, "(< \"\xc3\xa9\" \"\xe2\x82\xac\")"));
    // Embedded NUL is an ordinary smallest byte
    ck_assert(run_true(&in, "(< (from-bytes '(97 0 98)) (from-bytes '(97 0 99)))"));
    ck_assert(run_true(&in, "(< (from-bytes '(97 0)) (from-bytes '(97 1)))"));
    ck_assert(run_true(&in, "(< \"a\" (from-bytes '(97 0)))"));
    ck_assert(!run_true(&in, "(< (from-bytes '(97 0)) \"a\")"));
    // Numbers still work; mixing is still an error
    ck_assert(run_true(&in, "(< 1 2)"));
    ck_assert(eval_panics(&in, "(< \"a\" 1)"));
    interp_free(&in);
}
END_TEST

START_TEST(test_string_equality_with_nul)
{
    Interp in; interp_init(&in);
    ck_assert(run_true(&in, "(= (from-bytes '(97 0 98)) (from-bytes '(97 0 98)))"));
    ck_assert(!run_true(&in, "(= (from-bytes '(97 0 98)) (from-bytes '(97 0 99)))"));
    ck_assert(!run_true(&in, "(= (from-bytes '(97 0)) \"a\")"));
    ck_assert(!run_true(&in, "(= (from-bytes '(0)) \"\")"));
    // Hashing sees the whole string too: these are distinct dict keys
    ck_assert_int_eq(run_int(&in,
        "(count (dict ((from-bytes '(97 0 98)) 1) ((from-bytes '(97 0 99)) 2) (\"a\" 3)))"), 3);
    ck_assert_int_eq(run_int(&in, "(get (dict ((from-bytes '(97 0 98)) 7)) (from-bytes '(97 0 98)))"), 7);
    interp_free(&in);
}
END_TEST

// ---- NUL safety in string, write, repr, the reader and symbol ------------------

START_TEST(test_nul_in_concat_and_conversion)
{
    Interp in; interp_init(&in);
    assert_string(&in, "(string \"a\" (from-bytes '(0)) \"b\")", "a\0b", 3);
    assert_string(&in, "(string (from-bytes '(97 0 98)))", "a\0b", 3);
    assert_string(&in, "(string (from-bytes '(0 0)) (from-bytes '(0)))", "\0\0\0", 3);
    interp_free(&in);
}
END_TEST

START_TEST(test_nul_in_write)
{
    Interp in; interp_init(&in);
    FILE *f = tmpfile();
    ck_assert_ptr_nonnull(f);
    intrinsic_set_print_stream(f);
    run(&in, "(write \"a\" (from-bytes '(0 98)) \"c\")");
    run(&in, "(print (from-bytes '(0)))");
    intrinsic_set_print_stream(NULL);
    rewind(f);
    char buf[32];
    size_t n = fread(buf, 1, sizeof buf, f);
    fclose(f);
    ck_assert_uint_eq(n, 6);
    ck_assert(memcmp(buf, "a\0bc\0\n", 6) == 0);
    interp_free(&in);
}
END_TEST

START_TEST(test_nul_repr_roundtrip)
{
    Interp in; interp_init(&in);
    Value *s = run(&in, "(from-bytes '(97 0 98 34 92 10 9 13 0 49))");
    UT_string *text = repr(&in.gc, s);
    ck_assert_str_eq(utstring_body(text), "\"a\\0b\\\"\\\\\\n\\t\\r\\01\"");
    Value *back = eval_s_expr(&in.gc, in.global, parse(&in.gc, utstring_body(text)));
    ck_assert(values_equal(s, back));
    // Non-ASCII bytes pass through unchanged
    Value *u = run(&in, "\"h\xc3\xa9\"");
    ck_assert(values_equal(u, eval_s_expr(&in.gc, in.global, parse(&in.gc, utstring_body(repr(&in.gc, u))))));
    // The reader accepts \0 in source
    ck_assert_int_eq(run_int(&in, "(count \"a\\0b\")"), 3);
    ck_assert_int_eq(run_int(&in, "(byte \"a\\0b\" 1)"), 0);
    // Unknown escapes keep both characters
    assert_string(&in, "\"\\q\"", "\\q", 2);
    interp_free(&in);
}
END_TEST

START_TEST(test_symbol_rejects_nul)
{
    Interp in; interp_init(&in);
    ck_assert_panics(&in, "(symbol (from-bytes '(97 0 98)))", "symbol: a symbol name cannot contain a NUL byte");
    ck_assert(run_true(&in, "(= (symbol \"ab\") 'ab)"));
    interp_free(&in);
}
END_TEST

// ---- GC ----------------------------------------------------------------------

START_TEST(test_strings_survive_gc)
{
    Interp in; interp_init(&in);
    run(&in, "(define s (join (cons \"ab\" (cons (from-bytes '(0)) (cons \"cd\" nil))) \"-\"))");
    for (int i = 0; i < 5; i++) gc_collect(&in.gc, in.global);
    assert_string(&in, "s", "ab-\0-cd", 7);
    interp_free(&in);
}
END_TEST

// ---- Suite -------------------------------------------------------------------

static Suite *string_suite(void) {
    Suite *s = suite_create("Strings");
    TCase *tc = tcase_create("Intrinsics");
    tcase_set_timeout(tc, 300);
    tcase_add_test(tc, test_count);
    tcase_add_test(tc, test_byte);
    tcase_add_test(tc, test_slice);
    tcase_add_test(tc, test_slice_utf8_boundaries);
    tcase_add_test(tc, test_find);
    tcase_add_test(tc, test_starts_ends_with);
    tcase_add_test(tc, test_parse_int);
    tcase_add_test(tc, test_bytes_roundtrip);
    tcase_add_test(tc, test_from_bytes_errors);
    tcase_add_test(tc, test_join);
    tcase_add_test(tc, test_join_large);
    tcase_add_test(tc, test_string_ordering);
    tcase_add_test(tc, test_string_equality_with_nul);
    tcase_add_test(tc, test_nul_in_concat_and_conversion);
    tcase_add_test(tc, test_nul_in_write);
    tcase_add_test(tc, test_nul_repr_roundtrip);
    tcase_add_test(tc, test_symbol_rejects_nul);
    tcase_add_test(tc, test_strings_survive_gc);
    suite_add_tcase(s, tc);
    return s;
}

int main(void) {
    SRunner *sr = srunner_create(string_suite());
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return number_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
