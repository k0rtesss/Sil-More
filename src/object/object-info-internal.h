#ifndef INCLUDED_OBJECT_INFO_INTERNAL_H
#define INCLUDED_OBJECT_INFO_INTERNAL_H

#include "../h-basic.h"

typedef struct object_type object_type;

#define OBJECT_INFO_CAPTURE_WRAP_COLS 72
#define OBJECT_INFO_NAME_BUF_COLS 128

void p_text_out(cptr str);
void p_text_out_c(byte attr, cptr str);
bool object_info_screen_out_head(const object_type* o_ptr);

#endif /* INCLUDED_OBJECT_INFO_INTERNAL_H */
