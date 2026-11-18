#pragma once

#include <stdbool.h>
#include <stddef.h>

// Fills `buf` with `len` bytes from the operating system's CSPRNG
// (BCryptGenRandom on Windows, /dev/urandom elsewhere). Returns false if no
// random source was available; `buf` is then left in an unspecified state.
//
// Isolated in its own translation unit for the same reason as platform_dir.h:
// windows.h must never share a translation unit with types/value.h.
bool platform_random_bytes(void *buf, size_t len);
