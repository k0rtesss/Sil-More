#ifndef INCLUDED_OBJECT_INFO_H
#define INCLUDED_OBJECT_INFO_H

#include "h-basic.h"

typedef struct object_type object_type;

bool object_info_out(const object_type* o_ptr);
void note_info_screen(const object_type* o_ptr);
void object_info_screen(const object_type* o_ptr);
void object_info_screen_multi(const object_type** objects,
    const char** headings, int count);

#endif /* INCLUDED_OBJECT_INFO_H */
