#ifndef INCLUDED_SUPPORT_TEXT_OUTPUT_H
#define INCLUDED_SUPPORT_TEXT_OUTPUT_H

#include "h-basic.h"
#include "platform-io.h"

extern ang_file* text_out_file;
extern void (*text_out_hook)(byte a, cptr str);
extern int text_out_wrap;
extern int text_out_indent;
void text_out_to_file(byte attr, cptr str);

#endif /* INCLUDED_SUPPORT_TEXT_OUTPUT_H */
