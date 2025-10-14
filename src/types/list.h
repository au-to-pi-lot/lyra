#pragma once

#include "value.h"
#include "../gc.h"
#include <stdbool.h>

extern Value NIL_VALUE;
extern Value *NIL;

typedef struct ListWithEnd {
    Value *list;
    Value *end;
} ListWithEnd;

/// @brief Count the number of items in the list.
/// @param list The list to count
/// @return Number of items
int list_length(Value *list);

/// @brief Determine whether val is nil
/// @param val 
/// @return 
bool is_nil(Value *list);

/// @brief Add a new item to the front of the list
/// @param item 
/// @param list 
/// @return The new list including the new item
Value *list_append(GC *gc, Value *item, Value *list);

/// @brief Create a new list with all integers [0, stop) in order
/// @param stop 
/// @return 
Value *list_range(GC *gc, int stop);

/// @brief Create a new list of specified length filled with nil values
/// @param length 
/// @return 
Value *make_list(GC *gc, int length);

/// @brief Get the first element of the list; if the list is nil, returns nil
/// @param list 
/// @return 
Value *list_head(Value *list);

/// @brief Get the list with the first element removed; if the list is nil, returns nil
/// @param list 
/// @return 
Value *list_tail(Value *list);

/// @brief Allocate a shallow copy of the list
/// @param list 
/// @return 
Value *list_copy(GC *gc, Value *list);

/// @brief Allocate a shallow copy of the list; return both a pointer to the head of the list and a pointer to the last value for optimization purposes
/// @param list 
/// @return 
ListWithEnd list_copy_with_end(GC *gc, Value *list);

/// @brief Get the item in the list at the specified index
/// @param list 
/// @param index 
/// @return The item at the specified index, or nil if the list is too short
Value *list_index(Value *list, int index);

/// @brief Concatenate two lists
/// @param left 
/// @param right 
/// @return A new list which contains all of the items in the left followed by all of the items in the right
Value *list_concat(GC *gc, Value *left, Value *right);

/// @brief Remove the first n items of the list
/// @param list 
/// @param n 
/// @return List with first n items removed; if list is too short, returns nil
Value *list_drop(Value *list, int n);

/// @brief Remove all items except the first n
/// @param list 
/// @param n 
/// @return List with all items removed; if list is shorter than n, returns whole list
Value *list_keep(GC *gc, Value *list, int n);

/// @brief Returns the input list in reverse order
/// @param list 
/// @return 
Value *list_reverse(GC *gc, Value *list);

Value *list_map(GC *gc, Value *(*func)(Value *item), Value *list);

Value *list_fold(GC *gc, Value *(*func)(GC *gc, Value *accumulator, Value *item), Value *list, Value *start);

Value *list_filter(GC *gc, bool (*func)(Value *item), Value *list);
