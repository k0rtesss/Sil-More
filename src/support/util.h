#ifndef INCLUDED_SUPPORT_UTIL_H
#define INCLUDED_SUPPORT_UTIL_H

#include "h-basic.h"

typedef struct app_ui_scene app_ui_scene;

s16b quark_add(cptr str);
cptr quark_str(s16b i);
errr quarks_init(void);
errr quarks_free(void);
errr messages_init(void);
void messages_free(void);

void bell(cptr reason);
void sound(int val);
void msg_print(cptr msg);
void msg_format(cptr fmt, ...);
void message_format(u16b message_type, s16b extra, cptr fmt, ...);
void message(u16b message_type, s16b extra, cptr message);
void message_flush(void);
bool prompt_text_input(cptr prompt, cptr detail, char* buf, size_t len,
    bool allow_randomize);
int get_check_other(cptr prompt, char other);
bool get_check(cptr prompt);
bool get_check_oath_multiline(cptr prompt);
s16b get_quantity(cptr prompt, int max);
bool get_com(cptr prompt, char* command);
errr input_byte_unshift(int key);
void input_byte_queue_clear(void);
bool input_byte_queue_pending(void);
void inkey_set_cursor_hidden(bool hidden);
bool inkey_cursor_hidden(void);
void input_clear_pending(void);
void repeat_push(int what);
bool repeat_pull(int* what);
void repeat_clear(void);
s16b message_num(void);
cptr message_str(s16b age);
byte message_color(s16b age);
void message_topline_override(byte color, cptr text);
void message_topline_clear_override(void);
bool build_message_subwindow_ui_scene(app_ui_scene* scene);
bool askfor_aux(char* buf, size_t len);
bool askfor_name(char* buf, size_t len);

int count_wrapped_lines(cptr str, int wrap_width, int indent);
void text_out(cptr str);
void text_out_c(byte a, cptr str);

bool is_a_vowel(int ch);
int damroll(int num, int sides);
bool parse_u64b_hex(const char* text, u64b* out);

#endif /* INCLUDED_SUPPORT_UTIL_H */
