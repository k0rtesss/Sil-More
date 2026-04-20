/*
 * Copyright (C) 2025-2026 Sil-More contributors
 *
 * This file is part of Sil-More.
 *
 * Sil-More is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Sil-More is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See LICENSE.md
 * for more details.
 */

/* mem/alloc.h - Modern memory allocation interface */

#ifndef INCLUDED_MEM_ALLOC_H
#define INCLUDED_MEM_ALLOC_H

#include "../h-basic.h"
#include <stdlib.h>
#include <string.h>

/*
 * Modern type-safe memory allocation wrappers.
 * These replace the legacy z-virt.h macro-based system with:
 * - Better type safety (compiler checks types)
 * - Clearer semantics (explicit return values)
 * - Standard behavior (uses calloc/free)
 * - Better debugging (inline functions appear in stack traces)
 */

/**
 * Allocate and zero-initialize an array of elements.
 * 
 * @param count Number of elements
 * @param type Element type
 * @return Pointer to zeroed memory, or NULL on failure
 * 
 * Example:
 *   int* array = mem_alloc_array(100, int);
 *   if (!array) { handle_error(); }
 */
#define mem_alloc_array(count, type) \
    ((type*)calloc((count), sizeof(type)))

/**
 * Allocate and zero-initialize a single object.
 * 
 * @param type Object type
 * @return Pointer to zeroed memory, or NULL on failure
 * 
 * Example:
 *   monster_type* mon = mem_alloc(monster_type);
 *   if (!mon) { handle_error(); }
 */
#define mem_alloc(type) \
    ((type*)calloc(1, sizeof(type)))

/**
 * Free memory and return NULL.
 * Safe to call on NULL pointers.
 * 
 * @param ptr Pointer to free
 * @return Always returns NULL
 * 
 * Example:
 *   ptr = mem_free(ptr);  // Frees and NULLs in one step
 */
static inline void* mem_free(void* ptr)
{
    if (ptr) free(ptr);
    return NULL;
}

/**
 * Macro helper for freeing and NULLing a pointer.
 * Equivalent to: ptr = mem_free(ptr);
 * 
 * Example:
 *   mem_free_null(array);  // array is now NULL
 */
#define mem_free_null(ptr) ((ptr) = mem_free(ptr))

#endif /* INCLUDED_MEM_ALLOC_H */
