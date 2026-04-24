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

#ifndef INCLUDED_SUPPORT_UTF8_H
#define INCLUDED_SUPPORT_UTF8_H

#include "h-basic.h"

size_t utf8_char_len(cptr s);
size_t utf8_clip_bytes(cptr s, size_t max_bytes);
size_t utf8_clip_cells(cptr s, size_t max_cells, size_t max_bytes);
size_t utf8_strlcpy(char* dst, cptr src, size_t dst_size);
size_t utf8_strlen_cells(cptr s);
size_t utf8_strnlen_cells(cptr s, size_t max_bytes);
bool utf8_next_codepoint(cptr* cursor, u32b* out_codepoint);
int utf8_latin1_fold_codepoint(u32b codepoint);

#endif /* INCLUDED_SUPPORT_UTF8_H */
