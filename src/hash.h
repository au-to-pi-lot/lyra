#pragma once

// The "how" half of value hashing: a streaming SipHash-1-3 hasher.
//
// Hashing is split the way Rust splits Hash/Hasher. Each value type decides
// *what* to feed (value_hash.c: a type tag, then words and bytes), and only
// this file knows the algorithm. Swapping SipHash for something else later
// means changing this file alone.
//
// Byte order: words are fed as 8 little-endian bytes, so a hash depends only
// on the byte stream, never on the host's endianness.
//
// Keying: hasher_init_process() keys the hasher with a per-process key, drawn
// from the OS random source the first time it's needed. Setting the
// environment variable LYRA_HASH_SEED to a decimal integer (0 included)
// derives the key from that number instead, so runs are reproducible (dict
// and set iteration order follows the hash). A malformed LYRA_HASH_SEED is
// reported on stderr and exits with status 1.
//
// This header deliberately doesn't include types/value.h.

#include <stddef.h>
#include <stdint.h>

typedef struct Hasher {
    uint64_t v0, v1, v2, v3;
    uint64_t tail;    // bytes not yet compressed, little-endian, low bytes first
    size_t ntail;     // how many bytes are in `tail` (0..7)
    uint64_t length;  // total bytes fed so far (only the low byte matters)
} Hasher;

// Start a hasher keyed with (k0, k1).
void hasher_init(Hasher *h, uint64_t k0, uint64_t k1);

// Start a hasher keyed with the process-wide key (see above).
void hasher_init_process(Hasher *h);

void hasher_write(Hasher *h, const void *data, size_t len);
void hasher_write_u8(Hasher *h, uint8_t byte);
void hasher_write_u64(Hasher *h, uint64_t word);

// The hash of everything written so far. Doesn't modify the hasher.
uint64_t hasher_finish(const Hasher *h);

// One-shot SipHash-1-3 of a byte buffer.
uint64_t siphash13(uint64_t k0, uint64_t k1, const void *data, size_t len);

// The process-wide key (initializing it on first use).
void hash_process_key(uint64_t *k0, uint64_t *k1);
