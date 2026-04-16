/* File: object-squelch.h */

#ifndef INCLUDED_OBJECT_SQUELCH_H
#define INCLUDED_OBJECT_SQUELCH_H

#include "../h-basic.h"

typedef struct object_type object_type;

extern byte squelch_level[SQUELCH_BYTES];

int do_cmd_autoinscribe_item(s16b k_idx);
int get_autoinscription_index(s16b k_idx);
int add_autoinscription(s16b kind, cptr inscription);
int remove_autoinscription(s16b kind);
int apply_autoinscription(object_type* o_ptr);
char* squelch_to_label(int squelch);
void do_squelch_pile(int y, int x);

#endif /* INCLUDED_OBJECT_SQUELCH_H */
