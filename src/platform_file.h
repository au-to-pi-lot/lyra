#pragma once

#include <stdio.h>

// Opens a file whose path is UTF-8, isolated from windows.h (see
// platform_dir.h for why). On Windows, fopen would read the path in the ANSI
// code page, so this converts to UTF-16 and calls _wfopen; elsewhere it is
// plain fopen. Returns NULL on failure (including an invalid UTF-8 path on
// Windows). `mode` must be ASCII.
FILE *platform_fopen_utf8(const char *path, const char *mode);
