#include "platform_file.h"

#ifdef _WIN32
#include <windows.h>
#include <stdlib.h>

FILE *platform_fopen_utf8(const char *path, const char *mode) {
    int wpath_len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    if (wpath_len <= 0) {
        return NULL;
    }
    wchar_t *wpath = malloc((size_t)wpath_len * sizeof(wchar_t));
    if (!wpath) {
        return NULL;
    }
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wpath, wpath_len);

    wchar_t wmode[16];
    size_t i = 0;
    for (; mode[i] != '\0' && i < 15; i++) {
        wmode[i] = (wchar_t)(unsigned char)mode[i];
    }
    wmode[i] = L'\0';

    FILE *file = _wfopen(wpath, wmode);
    free(wpath);
    return file;
}

#else

FILE *platform_fopen_utf8(const char *path, const char *mode) {
    return fopen(path, mode);
}

#endif
