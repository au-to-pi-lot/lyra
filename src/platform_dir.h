#pragma once

// Lists the entries of a directory (excluding "." and ".."), isolated from
// windows.h's INT/FLOAT/BOOLEAN typedefs which collide with types/value.h's
// ValueType enum constants of the same names.
//
// Returns a NULL-terminated, heap-allocated array of heap-allocated filename
// strings, or NULL if the directory could not be opened. Free with
// platform_free_directory_listing().
char **platform_list_directory(const char *path);
void platform_free_directory_listing(char **entries);
