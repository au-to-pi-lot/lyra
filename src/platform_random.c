#include "platform_random.h"

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>

bool platform_random_bytes(void *buf, size_t len) {
    // BCRYPT_USE_SYSTEM_PREFERRED_RNG: no algorithm handle needed.
    return BCryptGenRandom(NULL, (PUCHAR)buf, (ULONG)len, BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
}

#else
#include <stdio.h>

bool platform_random_bytes(void *buf, size_t len) {
    FILE *f = fopen("/dev/urandom", "rb");
    if (f == NULL) {
        return false;
    }
    size_t got = fread(buf, 1, len, f);
    fclose(f);
    return got == len;
}

#endif
