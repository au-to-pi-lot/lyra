// Tests for the hasher (src/hash.c), value hashing (src/value_hash.c), the
// CHAMP tries behind dict and set (src/types/champ.c) and their intrinsics
// (src/intrinsics_dict.c).
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
#include "../src/hash.h"
#include "../src/value_hash.h"
#include "../src/types/value.h"
#include "../src/types/list.h"
#include "../src/types/int.h"
#include "../src/types/symbol.h"
#include "../src/types/champ.h"

// ---- Helpers ----------------------------------------------------------------

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

static const char *run_repr(Interp *in, const char *src) {
    return utstring_body(repr(&in->gc, run(in, src)));
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

// See tests/test_eval.c: keep ck_assert out of the protected region.
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

static void check_valid(ChampNode *root) {
    const char *why = champ_validate(root);
    ck_assert_msg(why == NULL, "invalid trie: %s", why ? why : "");
}

// ---- SipHash-1-3 --------------------------------------------------------------

// Reference values from CPython 3.14, whose bytes hash is SipHash-1-3:
// `hash(bytes(range(n)))` under PYTHONHASHSEED=0 (all-zero key) and
// PYTHONHASHSEED=12345 (key derived by CPython's LCG, giving the k0/k1 below).
typedef struct SipVector {
    size_t len;
    uint64_t zero_key;
    uint64_t seeded_key;
} SipVector;

static const SipVector SIP_VECTORS[] = {
    {1, 0x68a914128e01e473ULL, 0xddb5fc492fbdf63aULL},
    {7, 0x2f098ab0c751325aULL, 0x831edfe12fee6ffdULL},
    {8, 0xead411e67ebe2eeaULL, 0x354edb093928c942ULL},
    {9, 0x75927f9d95124362ULL, 0x09a5e47bf18abeccULL},
    {15, 0xf30eb725bb91c9eaULL, 0xbe8dc664d017b99eULL},
    {16, 0x8972188433a5c5b7ULL, 0x2e932605ea370595ULL},
    {17, 0x4883c49a2c009c1dULL, 0x76887087110a4b41ULL},
    {63, 0x385d3e39e5f37359ULL, 0x171afa1ac779cd10ULL},
};
#define SEEDED_K0 0x25556dc46dc3dca0ULL
#define SEEDED_K1 0xfc3ee4dbd06f6c90ULL

START_TEST(test_siphash13_reference_vectors)
{
    unsigned char msg[64];
    for (int i = 0; i < 64; i++) msg[i] = (unsigned char)i;
    for (size_t i = 0; i < sizeof SIP_VECTORS / sizeof SIP_VECTORS[0]; i++) {
        const SipVector *v = &SIP_VECTORS[i];
        ck_assert_msg(siphash13(0, 0, msg, v->len) == v->zero_key, "zero key, length %zu", v->len);
        ck_assert_msg(siphash13(SEEDED_K0, SEEDED_K1, msg, v->len) == v->seeded_key,
                      "seeded key, length %zu", v->len);
    }
}
END_TEST

START_TEST(test_hasher_streaming_matches_one_shot)
{
    unsigned char msg[64];
    for (int i = 0; i < 64; i++) msg[i] = (unsigned char)(i * 7 + 3);
    uint64_t whole = siphash13(SEEDED_K0, SEEDED_K1, msg, sizeof msg);
    // Every way of cutting the message in three must agree
    for (size_t a = 0; a <= sizeof msg; a += 3) {
        for (size_t b = a; b <= sizeof msg; b += 5) {
            Hasher h;
            hasher_init(&h, SEEDED_K0, SEEDED_K1);
            hasher_write(&h, msg, a);
            hasher_write(&h, msg + a, b - a);
            hasher_write(&h, msg + b, sizeof msg - b);
            ck_assert(hasher_finish(&h) == whole);
        }
    }
    // write_u64 is 8 little-endian bytes
    unsigned char le[8] = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01};
    Hasher h;
    hasher_init(&h, 1, 2);
    hasher_write_u64(&h, 0x0102030405060708ULL);
    ck_assert(hasher_finish(&h) == siphash13(1, 2, le, 8));
}
END_TEST

// ---- value_hash -----------------------------------------------------------------

static uint64_t hash_of(Interp *in, const char *src) {
    return value_hash(run(in, src));
}

START_TEST(test_equal_values_hash_equal)
{
    Interp in;
    interp_init(&in);
    const char *pairs[][2] = {
        {"42", "(+ 40 2)"},
        {"-7", "(- 7)"},
        {"true", "(= 1 1)"},
        {"\"abc\"", "(string \"a\" \"bc\")"},
        {"'foo", "(symbol \"foo\")"},
        {"'(1 (2 \"x\") sym)", "(cons 1 (cons (cons 2 (cons \"x\" nil)) (cons 'sym nil)))"},
        {"nil", "'()"},
        {"(dict ('a 1) ('b '(2 3)))", "(dict ('b '(2 3)) ('a 1))"},
        {"(set 1 2 3 4 5)", "(set 5 4 3 2 1)"},
        {"(set (dict ('k (set 1))))", "(set (assoc (dict) 'k (set-add (set) 1)))"},
    };
    for (size_t i = 0; i < sizeof pairs / sizeof pairs[0]; i++) {
        Value *a = run(&in, pairs[i][0]);
        Value *b = run(&in, pairs[i][1]);
        ck_assert_msg(values_equal(a, b), "not equal: %s vs %s", pairs[i][0], pairs[i][1]);
        ck_assert_msg(value_hash(a) == value_hash(b), "hashes differ: %s vs %s", pairs[i][0], pairs[i][1]);
    }
    interp_free(&in);
}
END_TEST

START_TEST(test_distinct_values_hash_apart)
{
    Interp in;
    interp_init(&in);
    // Each pair is a near miss the hash format is designed to separate
    ck_assert(hash_of(&in, "1") != hash_of(&in, "\"1\""));
    ck_assert(hash_of(&in, "'(\"ab\" \"c\")") != hash_of(&in, "'(\"a\" \"bc\")"));
    ck_assert(hash_of(&in, "'((1 2) 3)") != hash_of(&in, "'(1 (2 3))"));
    ck_assert(hash_of(&in, "'a") != hash_of(&in, "\"a\""));
    ck_assert(hash_of(&in, "true") != hash_of(&in, "1"));
    ck_assert(hash_of(&in, "nil") != hash_of(&in, "'(nil)"));
    ck_assert(hash_of(&in, "(set 1)") != hash_of(&in, "'(1)"));
    ck_assert(hash_of(&in, "(set)") != hash_of(&in, "(dict)"));
    ck_assert(hash_of(&in, "(dict (1 2))") != hash_of(&in, "(dict (2 1))"));
    // Uninterned symbols hash by identity, not name
    ck_assert(hash_of(&in, "(unique 'x)") != hash_of(&in, "(unique 'x)"));
    ck_assert(hash_of(&in, "(unique 'x)") != hash_of(&in, "'x"));
    interp_free(&in);
}
END_TEST

START_TEST(test_uninterned_symbol_hash_is_stable)
{
    Interp in;
    interp_init(&in);
    run(&in, "(define u (unique 'k))");
    ck_assert(hash_of(&in, "u") == hash_of(&in, "u"));
    ck_assert(hash_of(&in, "(cons u nil)") == hash_of(&in, "(cons u nil)"));
    interp_free(&in);
}
END_TEST

// ---- CHAMP, driven from C ----------------------------------------------------------

static Value **lookup_int(ChampNode *root, Value *key) {
    return champ_lookup(root, key, champ_hash(key));
}

// Small fixed pool of int keys, so tests can compare against a plain array.
#define POOL 6000
static Value *pool[POOL];

static void make_pool(GC *gc) {
    for (int i = 0; i < POOL; i++) pool[i] = make_int(gc, i);
}

static ChampNode *insert_int(GC *gc, ChampNode *root, int k, int v) {
    return champ_insert(gc, root, pool[k], make_int(gc, v), champ_hash(pool[k]));
}

static ChampNode *remove_int(GC *gc, ChampNode *root, int k) {
    return champ_remove(gc, root, pool[k], champ_hash(pool[k]));
}

static int get_int(ChampNode *root, int k) {
    Value **e = lookup_int(root, pool[k]);
    return e ? e[1]->data.as_int : -1;
}

// Hashers for forcing trie shapes. Each agrees with values_equal on ints.
static uint64_t hash_constant(Value *v) { (void)v; return 0x5eedULL; }
static uint64_t hash_mod7(Value *v) { return (uint64_t)(v->data.as_int % 7) * 0x9e3779b97f4a7c15ULL; }
// Keys differ only in the top 4 bits, which the trie reaches last: every key
// shares a 12-node-deep path.
static uint64_t hash_top_bits(Value *v) { return ((uint64_t)(v->data.as_int & 15) << 60) | 0x123456789ULL; }
// Distinct in the first level only for multiples of 32 apart; exercises
// chains of single-child nodes of varying depth.
static uint64_t hash_identity(Value *v) { return (uint64_t)(uint32_t)v->data.as_int; }

START_TEST(test_champ_persistence)
{
    GC gc;
    gc_init(&gc);
    make_pool(&gc);

    ChampNode *v0 = champ_empty(CHAMP_DICT_STRIDE);
    ChampNode *versions[101];
    versions[0] = v0;
    for (int i = 1; i <= 100; i++) {
        versions[i] = insert_int(&gc, versions[i - 1], i, i * 10);
    }
    // Overwrite and remove on top of version 100
    ChampNode *changed = insert_int(&gc, versions[100], 50, -5);
    ChampNode *removed = remove_int(&gc, changed, 7);

    // Every older version still has exactly what it had
    for (int i = 0; i <= 100; i++) {
        ck_assert_uint_eq(versions[i]->size, (unsigned)i);
        check_valid(versions[i]);
        for (int k = 1; k <= 100; k++) {
            ck_assert_int_eq(get_int(versions[i], k), k <= i ? k * 10 : -1);
        }
    }
    ck_assert_int_eq(get_int(changed, 50), -5);
    ck_assert_int_eq(get_int(changed, 7), 70);
    ck_assert_int_eq(get_int(removed, 7), -1);
    ck_assert_int_eq(get_int(removed, 50), -5);
    ck_assert_uint_eq(removed->size, 99);

    // No-op updates return the same trie
    ck_assert_ptr_eq(remove_int(&gc, versions[10], 99), versions[10]);
    Value **e = lookup_int(versions[10], pool[3]);
    ck_assert_ptr_eq(champ_insert(&gc, versions[10], pool[3], e[1], champ_hash(pool[3])), versions[10]);

    gc_free_all(&gc);
}
END_TEST

// Random insert/remove workload under `hash`, checked against an array, with
// the invariants validated as it goes.
static void random_workload(uint64_t (*hash)(Value *), int keyspace, int steps, unsigned seed) {
    champ_set_hash_function(hash);
    GC gc;
    gc_init(&gc);
    make_pool(&gc);

    int *expected = malloc(sizeof(int) * keyspace);
    for (int i = 0; i < keyspace; i++) expected[i] = -1;
    size_t count = 0;

    ChampNode *root = champ_empty(CHAMP_DICT_STRIDE);
    srand(seed);
    for (int step = 0; step < steps; step++) {
        int k = rand() % keyspace;
        if (rand() % 3 == 0) {
            root = remove_int(&gc, root, k);
            if (expected[k] != -1) count--;
            expected[k] = -1;
        } else {
            int v = rand() % 1000;
            root = insert_int(&gc, root, k, v);
            if (expected[k] == -1) count++;
            expected[k] = v;
        }
        ck_assert_uint_eq(root->size, count);
        if (step % 500 == 0) check_valid(root);
    }
    check_valid(root);
    for (int k = 0; k < keyspace; k++) {
        ck_assert_int_eq(get_int(root, k), expected[k]);
    }

    // Remove everything: must collapse back to the empty trie
    for (int k = 0; k < keyspace; k++) {
        root = remove_int(&gc, root, k);
        if (k % 97 == 0) check_valid(root);
    }
    ck_assert_uint_eq(root->size, 0);
    ck_assert_ptr_eq(root, champ_empty(CHAMP_DICT_STRIDE));

    free(expected);
    gc_free_all(&gc);
    champ_set_hash_function(NULL);
}

START_TEST(test_champ_random_workload)
{
    random_workload(NULL, 3000, 20000, 1);
}
END_TEST

START_TEST(test_champ_collision_nodes)
{
    // Every key has the same hash: one collision node under a 13-level path
    random_workload(hash_constant, 60, 2000, 2);
    // Seven full-hash buckets: several collision nodes
    random_workload(hash_mod7, 200, 4000, 3);
}
END_TEST

START_TEST(test_champ_deep_paths)
{
    random_workload(hash_top_bits, 16, 2000, 4);
    random_workload(hash_identity, 5000, 20000, 5);
}
END_TEST

START_TEST(test_champ_collision_node_contents)
{
    champ_set_hash_function(hash_constant);
    GC gc;
    gc_init(&gc);
    make_pool(&gc);

    ChampNode *root = champ_empty(CHAMP_DICT_STRIDE);
    for (int i = 0; i < 5; i++) root = insert_int(&gc, root, i, i);
    // Walk down to the bottom: 13 levels of single-child nodes, then a
    // collision node holding all five keys
    ChampNode *n = root;
    int depth = 0;
    while (!n->collision) {
        ck_assert_uint_eq(champ_payload_arity(n), 0);
        ck_assert_uint_eq(champ_node_arity(n), 1);
        n = n->slots[0];
        depth++;
    }
    ck_assert_int_eq(depth, 13);
    ck_assert_uint_eq(n->size, 5);

    // Down to one key: the whole path collapses into the root
    for (int i = 0; i < 4; i++) root = remove_int(&gc, root, i);
    ck_assert(!root->collision);
    ck_assert_uint_eq(champ_payload_arity(root), 1);
    ck_assert_uint_eq(champ_node_arity(root), 0);
    ck_assert_int_eq(get_int(root, 4), 4);
    check_valid(root);

    gc_free_all(&gc);
    champ_set_hash_function(NULL);
}
END_TEST

START_TEST(test_champ_canonical_equality)
{
    uint64_t (*hashes[])(Value *) = {NULL, hash_constant, hash_mod7, hash_top_bits, hash_identity};
    for (size_t h = 0; h < sizeof hashes / sizeof hashes[0]; h++) {
        champ_set_hash_function(hashes[h]);
        GC gc;
        gc_init(&gc);
        make_pool(&gc);
        int n = hashes[h] == hash_top_bits ? 16 : 300;

        // Forward insertion vs. backward insertion vs. inserting extra keys
        // and removing them again
        ChampNode *a = champ_empty(CHAMP_SET_STRIDE), *b = a, *c = a;
        for (int i = 0; i < n; i++) a = champ_insert(&gc, a, pool[i], NULL, champ_hash(pool[i]));
        for (int i = n - 1; i >= 0; i--) b = champ_insert(&gc, b, pool[i], NULL, champ_hash(pool[i]));
        for (int i = 0; i < n + 200; i++) c = champ_insert(&gc, c, pool[i], NULL, champ_hash(pool[i]));
        for (int i = n + 199; i >= n; i--) c = champ_remove(&gc, c, pool[i], champ_hash(pool[i]));

        ck_assert_msg(champ_equal(a, b), "hash function %zu: forward != backward", h);
        ck_assert_msg(champ_equal(a, c), "hash function %zu: forward != insert+remove", h);
        ChampNode *d = champ_remove(&gc, a, pool[n / 2], champ_hash(pool[n / 2]));
        ck_assert(!champ_equal(a, d));
        check_valid(a);
        check_valid(b);
        check_valid(c);

        gc_free_all(&gc);
    }
    champ_set_hash_function(NULL);
}
END_TEST

// ---- Lyra-level behaviour -----------------------------------------------------------

START_TEST(test_dict_basics)
{
    Interp in;
    interp_init(&in);
    run(&in, "(define d (dict ('a 1) ('b (+ 1 1)) (\"c\" '(3))))");
    ck_assert_int_eq(run_int(&in, "(get d 'a)"), 1);
    ck_assert_int_eq(run_int(&in, "(get d 'b)"), 2);
    ck_assert_str_eq(run_repr(&in, "(get d \"c\")"), "(3)");
    ck_assert_int_eq(run_int(&in, "(count d)"), 3);
    ck_assert_int_eq(run_int(&in, "(count (dict))"), 0);
    ck_assert(run_true(&in, "(contains? d 'a)"));
    ck_assert(!run_true(&in, "(contains? d 'z)"));
    ck_assert(run_true(&in, "(is-dict d)"));
    ck_assert(!run_true(&in, "(is-dict (set))"));
    ck_assert(!run_true(&in, "(is-dict '(a))"));
    // Keys and values are evaluated; a repeated key keeps the last value
    run(&in, "(define k 'kk)");
    ck_assert_int_eq(run_int(&in, "(get (dict (k 5)) 'kk)"), 5);
    ck_assert_int_eq(run_int(&in, "(get (dict ('a 1) ('a 2)) 'a)"), 2);
    ck_assert_int_eq(run_int(&in, "(count (dict ('a 1) ('a 2)))"), 1);
    // Dicts, sets and uninterned symbols as keys
    ck_assert_str_eq(run_repr(&in, "(get (dict ((dict ('a 1)) 'x)) (dict ('a 1)))"), "x");
    ck_assert_str_eq(run_repr(&in, "(get (dict ((set 1 2) 'y)) (set 2 1))"), "y");
    run(&in, "(define u1 (unique 'k))");
    run(&in, "(define u2 (unique 'k))");
    run(&in, "(define du (dict (u1 1) (u2 2) ('k 3)))");
    ck_assert_int_eq(run_int(&in, "(count du)"), 3);
    ck_assert_int_eq(run_int(&in, "(get du u2)"), 2);
    ck_assert_int_eq(run_int(&in, "(get du 'k)"), 3);
    interp_free(&in);
}
END_TEST

START_TEST(test_missing_vs_nil)
{
    Interp in;
    interp_init(&in);
    run(&in, "(define d (dict ('present nil)))");
    ck_assert(run_true(&in, "(= (get d 'absent) missing)"));
    ck_assert(!run_true(&in, "(= (get d 'present) missing)"));
    ck_assert(run_true(&in, "(is-nil (get d 'present))"));
    ck_assert(run_true(&in, "(contains? d 'present)"));
    ck_assert_int_eq(run_int(&in, "(get d 'absent 7)"), 7);
    ck_assert(run_true(&in, "(is-nil (get d 'present 7))"));
    // `missing` is an uninterned symbol: not the interned 'missing, not a
    // fresh (unique 'missing), and printed as #:missing
    ck_assert(run_true(&in, "(is-symbol missing)"));
    ck_assert(!run_true(&in, "(= missing 'missing)"));
    ck_assert(!run_true(&in, "(= missing (unique 'missing))"));
    ck_assert_str_eq(run_repr(&in, "missing"), "#:missing");
    ck_assert_str_eq(run_repr(&in, "(string missing)"), "\"missing\"");
    interp_free(&in);
}
END_TEST

START_TEST(test_dict_persistence)
{
    Interp in;
    interp_init(&in);
    run(&in, "(define d1 (dict ('a 1) ('b 2)))");
    run(&in, "(define d2 (assoc d1 'c 3))");
    run(&in, "(define d3 (assoc d2 'a 100))");
    run(&in, "(define d4 (dissoc d3 'b))");
    // Every version keeps exactly what it had
    ck_assert(run_true(&in, "(= d1 (dict ('a 1) ('b 2)))"));
    ck_assert(run_true(&in, "(= d2 (dict ('a 1) ('b 2) ('c 3)))"));
    ck_assert(run_true(&in, "(= d3 (dict ('a 100) ('b 2) ('c 3)))"));
    ck_assert(run_true(&in, "(= d4 (dict ('a 100) ('c 3)))"));
    ck_assert_int_eq(run_int(&in, "(count d1)"), 2);
    ck_assert(run_true(&in, "(= (get d1 'c) missing)"));
    ck_assert_int_eq(run_int(&in, "(get d2 'a)"), 1);
    ck_assert_int_eq(run_int(&in, "(get d3 'b)"), 2);
    // Sets too
    run(&in, "(define s1 (set 'x))");
    run(&in, "(define s2 (set-add s1 'y))");
    run(&in, "(define s3 (set-remove s2 'x))");
    ck_assert(run_true(&in, "(= s1 (set 'x))"));
    ck_assert(run_true(&in, "(= s2 (set 'x 'y))"));
    ck_assert(run_true(&in, "(= s3 (set 'y))"));
    interp_free(&in);
}
END_TEST

START_TEST(test_dict_equality)
{
    Interp in;
    interp_init(&in);
    ck_assert(run_true(&in, "(= (dict ('a 1) ('b 2) ('c 3)) (dict ('c 3) ('a 1) ('b 2)))"));
    ck_assert(run_true(&in, "(= (dict ('a 1)) (dissoc (dict ('a 1) ('b 2)) 'b))"));
    ck_assert(run_true(&in, "(= (dict) (dissoc (dict ('a 1)) 'a))"));
    ck_assert(!run_true(&in, "(= (dict ('a 1)) (dict ('a 2)))"));
    ck_assert(!run_true(&in, "(= (dict ('a 1)) (dict ('b 1)))"));
    ck_assert(!run_true(&in, "(= (dict ('a 1)) (dict ('a 1) ('b 2)))"));
    ck_assert(run_true(&in, "(= (dict ('a '(1 (2)))) (dict ('a '(1 (2)))))"));
    ck_assert(run_true(&in, "(= (set 1 2 3) (set 3 2 1))"));
    ck_assert(run_true(&in, "(= (set 1 2 3) (set-remove (set 1 2 3 4) 4))"));
    ck_assert(!run_true(&in, "(= (set 1 2) (set 1 2 3))"));
    ck_assert(!run_true(&in, "(= (set) (dict))"));
    ck_assert(!run_true(&in, "(= (set 'a) '(a))"));
    // Float values are fine (only keys need total equality)
    ck_assert(run_true(&in, "(= (dict ('x 1.5)) (dict ('x 1.5)))"));
    // Nested
    ck_assert(run_true(&in, "(= (set (dict ('a (set 1 2)))) (set (dict ('a (set 2 1)))))"));
    // match uses the same equality
    ck_assert_int_eq(run_int(&in, "(match (dict ('a 1) ('b 2)) (,(dict ('b 2) ('a 1)) 1) (x 0))"), 1);
    interp_free(&in);
}
END_TEST

START_TEST(test_unhashable_keys_rejected)
{
    Interp in;
    interp_init(&in);
    ck_assert_panics(&in, "(dict (1.5 'x))",
                     "dict: a dict key must have total equality, but 1.5 is a float");
    ck_assert_panics(&in, "(set 1 2.0)",
                     "set: a set member must have total equality, but 2.0 is a float");
    ck_assert_panics(&in, "(assoc (dict) '(1 2.5) 'x)",
                     "assoc: a dict key must have total equality, but (1 2.5) contains a float, 2.5");
    ck_assert_panics(&in, "(get (dict ('a 1)) 0.5)",
                     "get: a dict key must have total equality, but 0.5 is a float");
    ck_assert_panics(&in, "(contains? (set 1) 0.5)",
                     "contains?: a set member must have total equality, but 0.5 is a float");
    ck_assert_panics(&in, "(set-add (set) 0.5)",
                     "set-add: a set member must have total equality, but 0.5 is a float");
    // Functions and macros
    ck_assert(eval_panics(&in, "(set car)"));
    ck_assert(strstr(panic_message(), "is a function") != NULL);
    ck_assert(eval_panics(&in, "(dict (if 1))"));
    ck_assert(strstr(panic_message(), "is a macro") != NULL);
    // A dict whose *values* aren't hashable can't be a key either
    ck_assert(eval_panics(&in, "(set (dict ('a 1.5)))"));
    ck_assert(strstr(panic_message(), "contains a float, 1.5") != NULL);
    // ... but a float as a value is fine
    ck_assert_str_eq(run_repr(&in, "(get (dict ('a 1.5)) 'a)"), "1.5");
    interp_free(&in);
}
END_TEST

START_TEST(test_dict_bad_arguments)
{
    Interp in;
    interp_init(&in);
    ck_assert_panics(&in, "(dict 5)", "dict expects (key value) pairs, but got 5");
    ck_assert_panics(&in, "(dict ('a 1 2))", "dict expects (key value) pairs, but got ((quote a) 1 2)");
    ck_assert_panics(&in, "(get '(a) 'a)", "get expects a dict, but got (a)");
    ck_assert_panics(&in, "(get (set 'a) 'a)", "get expects a dict, but got (set 'a)");
    ck_assert_panics(&in, "(get (dict))", "get expects a dict, a key and an optional default, but got 1 arguments");
    ck_assert_panics(&in, "(assoc (dict) 'a)", "assoc expects exactly 3 arguments, but got 2");
    ck_assert_panics(&in, "(count '(1 2))", "count expects a dict or a set, but got (1 2)");
    ck_assert_panics(&in, "(set-add (dict) 1)", "set-add expects a set, but got (dict)");
    ck_assert_panics(&in, "(union (set) 1)", "union expects a set, but got 1");
    ck_assert_panics(&in, "(union)", "union expects at least 1 argument, but got 0");
    interp_free(&in);
}
END_TEST

START_TEST(test_dict_printing)
{
    Interp in;
    interp_init(&in);
    ck_assert_str_eq(run_repr(&in, "(dict)"), "(dict)");
    ck_assert_str_eq(run_repr(&in, "(set)"), "(set)");
    ck_assert_str_eq(run_repr(&in, "(dict ('a 1))"), "(dict ('a 1))");
    ck_assert_str_eq(run_repr(&in, "(set 'a)"), "(set 'a)");
    ck_assert_str_eq(run_repr(&in, "(dict (\"k\" '(1 b)))"), "(dict (\"k\" '(1 b)))");
    ck_assert_str_eq(run_repr(&in, "(dict (1 nil))"), "(dict (1 ()))");
    ck_assert_str_eq(run_repr(&in, "(set (dict (true (set))))"), "(set (dict (true (set))))");

    // The printed form reads back as an equal value
    const char *exprs[] = {
        "(dict ('a 1) ('b \"two\") (3 '(x y)) ('(1 2) 'sym))",
        "(set 1 2 3 'four \"five\" '(6 7))",
        "(dict ('inner (dict ('x (set 1 2)))))",
    };
    for (size_t i = 0; i < sizeof exprs / sizeof exprs[0]; i++) {
        Value *v = run(&in, exprs[i]);
        const char *printed = utstring_body(repr(&in.gc, v));
        Value *back = run(&in, printed);
        ck_assert_msg(values_equal(v, back), "%s printed as %s, which reads back differently",
                      exprs[i], printed);
    }
    interp_free(&in);
}
END_TEST

START_TEST(test_dict_listing)
{
    Interp in;
    interp_init(&in);
    run(&in, "(define d (dict ('a 1) ('b 2) ('c 3)))");
    // Order is unspecified, so check contents via sets
    ck_assert(run_true(&in, "(= (apply set (keys d)) (set 'a 'b 'c))"));
    ck_assert(run_true(&in, "(= (apply set (values d)) (set 1 2 3))"));
    ck_assert(run_true(&in, "(= (apply set (entries d)) (set '(a 1) '(b 2) '(c 3)))"));
    ck_assert(run_true(&in, "(= (apply set (members (set 1 2 2 3))) (set 1 2 3))"));
    ck_assert_str_eq(run_repr(&in, "(keys (dict))"), "()");
    ck_assert_str_eq(run_repr(&in, "(members (set))"), "()");
    // Rebuilding from entries gives an equal dict
    ck_assert(run_true(&in,
        "(= d (foldl (lambda (acc e) (assoc acc (car e) (car (cdr e)))) (dict) (entries d)))"));
    interp_free(&in);
}
END_TEST

START_TEST(test_set_operations)
{
    Interp in;
    interp_init(&in);
    run(&in, "(define a (set 1 2 3 4))");
    run(&in, "(define b (set 3 4 5))");
    ck_assert(run_true(&in, "(= (union a b) (set 1 2 3 4 5))"));
    ck_assert(run_true(&in, "(= (union b a) (set 1 2 3 4 5))"));
    ck_assert(run_true(&in, "(= (union a b (set 9)) (set 1 2 3 4 5 9))"));
    ck_assert(run_true(&in, "(= (union a) a)"));
    ck_assert(run_true(&in, "(= (intersection a b) (set 3 4))"));
    ck_assert(run_true(&in, "(= (intersection a b (set 4)) (set 4))"));
    ck_assert(run_true(&in, "(= (intersection a (set)) (set))"));
    ck_assert(run_true(&in, "(= (difference a b) (set 1 2))"));
    ck_assert(run_true(&in, "(= (difference b a) (set 5))"));
    ck_assert(run_true(&in, "(= (difference a b (set 1)) (set 2))"));
    ck_assert(run_true(&in, "(= (set-add a 9) (set 1 2 3 4 9))"));
    ck_assert(run_true(&in, "(= (set-add a 1) a)"));
    ck_assert(run_true(&in, "(= (set-remove a 1) (set 2 3 4))"));
    ck_assert(run_true(&in, "(= (set-remove a 99) a)"));
    ck_assert(run_true(&in, "(contains? a 3)"));
    ck_assert(!run_true(&in, "(contains? a 5)"));
    ck_assert(run_true(&in, "(is-set a)"));
    // None of that changed a or b
    ck_assert(run_true(&in, "(= a (set 1 2 3 4))"));
    ck_assert(run_true(&in, "(= b (set 3 4 5))"));
    ck_assert_int_eq(run_int(&in, "(count a)"), 4);
    interp_free(&in);
}
END_TEST

START_TEST(test_deep_dict_from_lyra)
{
    Interp in;
    interp_init(&in);
    char buf[128];
    run(&in, "(define d (dict))");
    run(&in, "(define s (set))");
    for (int i = 0; i < 3000; i++) {
        snprintf(buf, sizeof buf, "(define d (assoc d '(k %d) %d))", i, i * 2);
        run(&in, buf);
        snprintf(buf, sizeof buf, "(define s (set-add s \"m%d\"))", i);
        run(&in, buf);
    }
    ck_assert_int_eq(run_int(&in, "(count d)"), 3000);
    ck_assert_int_eq(run_int(&in, "(count s)"), 3000);
    check_valid(run(&in, "d")->data.as_champ);
    check_valid(run(&in, "s")->data.as_champ);
    for (int i = 0; i < 3000; i += 7) {
        snprintf(buf, sizeof buf, "(get d '(k %d))", i);
        ck_assert_int_eq(run_int(&in, buf), i * 2);
        snprintf(buf, sizeof buf, "(contains? s \"m%d\")", i);
        ck_assert(run_true(&in, buf));
    }
    ck_assert(run_true(&in, "(= (get d '(k 3000)) missing)"));
    interp_free(&in);
}
END_TEST

START_TEST(test_dicts_survive_gc)
{
    Interp in;
    interp_init(&in);
    char buf[160];
    run(&in, "(define d (dict))");
    run(&in, "(define old nil)");
    for (int i = 0; i < 3000; i++) {
        // Values are freshly allocated lists and nested sets, so a missed
        // trace would free something still reachable from the trie
        snprintf(buf, sizeof buf,
                 "(define d (assoc d (string \"key\" (string %d)) (cons %d (cons (set %d) nil))))", i, i, i);
        run(&in, buf);
        if (i == 150) run(&in, "(define old d)");
        if (i % 500 == 0) gc_collect(&in.gc, in.global);
    }
    // Garbage: intermediate tries that nothing references
    for (int i = 0; i < 50; i++) {
        run(&in, "(dissoc (assoc d 'temp (dict ('x '(1 2 3)))) \"key5\")");
    }
    size_t before = in.gc.num_objects;
    gc_collect(&in.gc, in.global);
    ck_assert_uint_lt(in.gc.num_objects, before);  // garbage was swept
    gc_collect(&in.gc, in.global);

    // Allocate over the freed memory, then check everything is intact
    for (int i = 0; i < 1000; i++) run(&in, "(cons 1 (cons \"filler\" nil))");
    ck_assert_int_eq(run_int(&in, "(count d)"), 3000);
    ck_assert_int_eq(run_int(&in, "(count old)"), 151);
    check_valid(run(&in, "d")->data.as_champ);
    check_valid(run(&in, "old")->data.as_champ);
    for (int i = 0; i < 3000; i += 7) {
        snprintf(buf, sizeof buf, "(= (get d \"key%d\") (cons %d (cons (set %d) nil)))", i, i, i);
        ck_assert_msg(run_true(&in, buf), "lost entry %d", i);
    }
    ck_assert(run_true(&in, "(= (get old \"key3000\") missing)"));
    ck_assert(run_true(&in, "(= (get old \"key150\") (cons 150 (cons (set 150) nil)))"));
    interp_free(&in);
}
END_TEST

START_TEST(test_collision_nodes_survive_gc)
{
    // Every key collides, so the whole dict lives in one collision node at
    // the bottom of a 13-node path
    champ_set_hash_function(hash_constant);
    Interp in;
    interp_init(&in);
    char buf[128];
    run(&in, "(define d (dict))");
    for (int i = 0; i < 60; i++) {
        snprintf(buf, sizeof buf, "(define d (assoc d \"k%d\" (cons %d nil)))", i, i);
        run(&in, buf);
        if (i % 20 == 0) gc_collect(&in.gc, in.global);
    }
    run(&in, "(define d (dissoc d \"k10\"))");
    gc_collect(&in.gc, in.global);
    for (int i = 0; i < 500; i++) run(&in, "(cons 2 (cons \"filler\" nil))");
    check_valid(run(&in, "d")->data.as_champ);
    ck_assert_int_eq(run_int(&in, "(count d)"), 59);
    for (int i = 0; i < 60; i++) {
        if (i == 10) {
            snprintf(buf, sizeof buf, "(= (get d \"k%d\") missing)", i);
        } else {
            snprintf(buf, sizeof buf, "(= (get d \"k%d\") '(%d))", i, i);
        }
        ck_assert_msg(run_true(&in, buf), "entry %d", i);
    }
    interp_free(&in);
    champ_set_hash_function(NULL);
}
END_TEST

Suite *dict_suite(void) {
    Suite *s = suite_create("Dict");

    TCase *tc_hash = tcase_create("Hash");
    tcase_add_test(tc_hash, test_siphash13_reference_vectors);
    tcase_add_test(tc_hash, test_hasher_streaming_matches_one_shot);
    tcase_add_test(tc_hash, test_equal_values_hash_equal);
    tcase_add_test(tc_hash, test_distinct_values_hash_apart);
    tcase_add_test(tc_hash, test_uninterned_symbol_hash_is_stable);
    suite_add_tcase(s, tc_hash);

    TCase *tc_champ = tcase_create("Champ");
    tcase_set_timeout(tc_champ, 120);
    tcase_add_test(tc_champ, test_champ_persistence);
    tcase_add_test(tc_champ, test_champ_random_workload);
    tcase_add_test(tc_champ, test_champ_collision_nodes);
    tcase_add_test(tc_champ, test_champ_deep_paths);
    tcase_add_test(tc_champ, test_champ_collision_node_contents);
    tcase_add_test(tc_champ, test_champ_canonical_equality);
    suite_add_tcase(s, tc_champ);

    TCase *tc_lyra = tcase_create("Lyra");
    tcase_set_timeout(tc_lyra, 120);
    tcase_add_test(tc_lyra, test_dict_basics);
    tcase_add_test(tc_lyra, test_missing_vs_nil);
    tcase_add_test(tc_lyra, test_dict_persistence);
    tcase_add_test(tc_lyra, test_dict_equality);
    tcase_add_test(tc_lyra, test_unhashable_keys_rejected);
    tcase_add_test(tc_lyra, test_dict_bad_arguments);
    tcase_add_test(tc_lyra, test_dict_printing);
    tcase_add_test(tc_lyra, test_dict_listing);
    tcase_add_test(tc_lyra, test_set_operations);
    suite_add_tcase(s, tc_lyra);

    TCase *tc_big = tcase_create("Big");
    tcase_set_timeout(tc_big, 300);
    tcase_add_test(tc_big, test_deep_dict_from_lyra);
    suite_add_tcase(s, tc_big);

    TCase *tc_gc = tcase_create("GC");
    tcase_set_timeout(tc_gc, 300);
    tcase_add_test(tc_gc, test_dicts_survive_gc);
    tcase_add_test(tc_gc, test_collision_nodes_survive_gc);
    suite_add_tcase(s, tc_gc);

    return s;
}

int main(void) {
    SRunner *sr = srunner_create(dict_suite());
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return number_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
