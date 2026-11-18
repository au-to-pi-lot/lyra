#include "hash.h"
#include "platform_random.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

// SipHash (Aumasson & Bernstein, 2012) with 1 compression round and 3
// finalization rounds: the variant Rust's HashMap and CPython (3.11+) use.
#define SIP_C_ROUNDS 1
#define SIP_D_ROUNDS 3

#define ROTL64(x, b) (uint64_t)(((x) << (b)) | ((x) >> (64 - (b))))

static void sip_round(Hasher *h) {
    h->v0 += h->v1; h->v1 = ROTL64(h->v1, 13); h->v1 ^= h->v0; h->v0 = ROTL64(h->v0, 32);
    h->v2 += h->v3; h->v3 = ROTL64(h->v3, 16); h->v3 ^= h->v2;
    h->v0 += h->v3; h->v3 = ROTL64(h->v3, 21); h->v3 ^= h->v0;
    h->v2 += h->v1; h->v1 = ROTL64(h->v1, 17); h->v1 ^= h->v2; h->v2 = ROTL64(h->v2, 32);
}

static void sip_compress(Hasher *h, uint64_t m) {
    h->v3 ^= m;
    for (int i = 0; i < SIP_C_ROUNDS; i++) sip_round(h);
    h->v0 ^= m;
}

void hasher_init(Hasher *h, uint64_t k0, uint64_t k1) {
    h->v0 = k0 ^ 0x736f6d6570736575ULL;
    h->v1 = k1 ^ 0x646f72616e646f6dULL;
    h->v2 = k0 ^ 0x6c7967656e657261ULL;
    h->v3 = k1 ^ 0x7465646279746573ULL;
    h->tail = 0;
    h->ntail = 0;
    h->length = 0;
}

void hasher_write(Hasher *h, const void *data, size_t len) {
    const unsigned char *p = data;
    h->length += len;

    // Top up a partial word left by an earlier write
    while (len > 0 && h->ntail > 0) {
        h->tail |= (uint64_t)*p << (8 * h->ntail);
        p++;
        len--;
        if (++h->ntail == 8) {
            sip_compress(h, h->tail);
            h->tail = 0;
            h->ntail = 0;
        }
    }
    if (len == 0) return;  // still inside a partial word

    // Whole words
    while (len >= 8) {
        uint64_t m = 0;
        for (int i = 0; i < 8; i++) m |= (uint64_t)p[i] << (8 * i);
        sip_compress(h, m);
        p += 8;
        len -= 8;
    }

    // Leftover bytes start a new partial word
    for (size_t i = 0; i < len; i++) {
        h->tail |= (uint64_t)p[i] << (8 * i);
    }
    h->ntail = len;
}

void hasher_write_u8(Hasher *h, uint8_t byte) {
    hasher_write(h, &byte, 1);
}

void hasher_write_u64(Hasher *h, uint64_t word) {
    unsigned char bytes[8];
    for (int i = 0; i < 8; i++) bytes[i] = (unsigned char)(word >> (8 * i));
    hasher_write(h, bytes, 8);
}

uint64_t hasher_finish(const Hasher *h) {
    Hasher s = *h;
    uint64_t b = (s.length << 56) | s.tail;
    sip_compress(&s, b);
    s.v2 ^= 0xff;
    for (int i = 0; i < SIP_D_ROUNDS; i++) sip_round(&s);
    return s.v0 ^ s.v1 ^ s.v2 ^ s.v3;
}

uint64_t siphash13(uint64_t k0, uint64_t k1, const void *data, size_t len) {
    Hasher h;
    hasher_init(&h, k0, k1);
    hasher_write(&h, data, len);
    return hasher_finish(&h);
}

// ---- Process key ---------------------------------------------------------

static bool key_ready = false;
static uint64_t process_k0, process_k1;

// splitmix64: spreads a small seed over a full key's worth of bits.
static uint64_t splitmix64(uint64_t *state) {
    uint64_t z = (*state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

static void init_process_key(void) {
    const char *seed_text = getenv("LYRA_HASH_SEED");
    if (seed_text != NULL && seed_text[0] != '\0') {
        char *end = NULL;
        errno = 0;
        unsigned long long seed = strtoull(seed_text, &end, 10);
        if (errno != 0 || *end != '\0' || seed_text[0] == '-' || seed_text[0] == '+' ||
            seed_text[0] == ' ') {
            fprintf(stderr, "LYRA_HASH_SEED must be a non-negative decimal integer, but got \"%s\"\n",
                    seed_text);
            exit(1);
        }
        uint64_t state = (uint64_t)seed;
        process_k0 = splitmix64(&state);
        process_k1 = splitmix64(&state);
    } else {
        uint64_t key[2];
        if (!platform_random_bytes(key, sizeof key)) {
            // No OS random source: fall back to something that at least
            // differs between runs. Not secure, but hashes stay correct.
            uint64_t state = (uint64_t)time(NULL) ^ ((uint64_t)clock() << 32) ^
                             (uint64_t)(uintptr_t)&key;
            key[0] = splitmix64(&state);
            key[1] = splitmix64(&state);
        }
        process_k0 = key[0];
        process_k1 = key[1];
    }
    key_ready = true;
}

void hash_process_key(uint64_t *k0, uint64_t *k1) {
    if (!key_ready) init_process_key();
    *k0 = process_k0;
    *k1 = process_k1;
}

void hasher_init_process(Hasher *h) {
    uint64_t k0, k1;
    hash_process_key(&k0, &k1);
    hasher_init(h, k0, k1);
}
