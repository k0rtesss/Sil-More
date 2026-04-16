#ifndef INCLUDED_SUPPORT_UTIL_H
#define INCLUDED_SUPPORT_UTIL_H

#include "h-basic.h"

s16b quark_add(cptr str);
cptr quark_str(s16b i);
errr quarks_init(void);
errr quarks_free(void);

void bell(cptr reason);
void sound(int val);
void msg_print(cptr msg);
void msg_format(cptr fmt, ...);

int count_wrapped_lines(cptr str, int wrap_width, int indent);
void text_out(cptr str);
void text_out_c(byte a, cptr str);

bool is_a_vowel(int ch);
int damroll(int num, int sides);

#endif /* INCLUDED_SUPPORT_UTIL_H */
