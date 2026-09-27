#define _POSIX_C_SOURCE 200809L
#include "platform_dir.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>

char **platform_list_directory(const char *path) {
    char pattern[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s/*", path);

    WIN32_FIND_DATAA entry;
    HANDLE dir = FindFirstFileA(pattern, &entry);
    if (dir == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    size_t capacity = 16;
    size_t count = 0;
    char **entries = malloc(capacity * sizeof(char *));

    do {
        if (strcmp(entry.cFileName, ".") == 0 || strcmp(entry.cFileName, "..") == 0) {
            continue;
        }

        if (count + 1 >= capacity) {
            capacity *= 2;
            entries = realloc(entries, capacity * sizeof(char *));
        }

        entries[count++] = _strdup(entry.cFileName);
    } while (FindNextFileA(dir, &entry) != 0);

    FindClose(dir);
    entries[count] = NULL;
    return entries;
}

#else
#include <dirent.h>

char **platform_list_directory(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) {
        return NULL;
    }

    size_t capacity = 16;
    size_t count = 0;
    char **entries = malloc(capacity * sizeof(char *));

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (count + 1 >= capacity) {
            capacity *= 2;
            entries = realloc(entries, capacity * sizeof(char *));
        }

        entries[count++] = strdup(entry->d_name);
    }

    closedir(dir);
    entries[count] = NULL;
    return entries;
}

#endif

void platform_free_directory_listing(char **entries) {
    if (!entries) {
        return;
    }
    for (char **p = entries; *p != NULL; p++) {
        free(*p);
    }
    free(entries);
}
