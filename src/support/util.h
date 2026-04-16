#ifndef INCLUDED_SUPPORT_UTIL_H
#define INCLUDED_SUPPORT_UTIL_H

#include "h-basic.h"

typedef struct app_ui_scene app_ui_scene;
struct editing_buffer;

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
u16b message_type(s16b age);
void message_add(cptr str, u16b type);
bool message_topline_snapshot(char* out_text, size_t out_text_size,
    byte* out_color, u16b* out_type, bool* out_more_pending);
bool prompt_text_input(cptr prompt, cptr detail, char* buf, size_t len,
    bool allow_randomize);
bool get_check(cptr prompt);
bool get_check_oath_multiline(cptr prompt);
s16b get_quantity(cptr prompt, int max);
bool get_com(cptr prompt, char* command);
errr input_byte_unshift(int key);
errr input_byte_enqueue(int key);
void input_byte_queue_clear(void);
bool input_byte_queue_pending(void);
void inkey_set_cursor_hidden(bool hidden);
bool inkey_cursor_hidden(void);
void input_clear_pending(void);
bool input_submit_movement_command(const app_movement_command* command);
void input_clear_movement_commands(void);
bool input_wait_for_movement_or_legacy(u16b context, u16b wait_reason,
    app_movement_command* out_command, char* out_ch);
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
void msg_debug(cptr fmt, ...);

bool is_a_vowel(int ch);
int damroll(int num, int sides);
int int_exp(int base, int power);
bool parse_u64b_hex(const char* text, u64b* out);
cptr get_ext_color_name(byte ext_color);
#ifdef SET_UID
void user_name(char* buf, size_t len, int id);
#endif
void editing_buffer_init(
    struct editing_buffer* eb_ptr, const char* buf, size_t max_size);
void editing_buffer_destroy(struct editing_buffer* eb_ptr);
int editing_buffer_put_chr(struct editing_buffer* eb_ptr, char ch);
int editing_buffer_set_position(
    struct editing_buffer* eb_ptr, size_t new_pos);
int editing_buffer_delete(struct editing_buffer* eb_ptr);
void editing_buffer_clear(struct editing_buffer* eb_ptr);
void editing_buffer_get_all(
    struct editing_buffer* eb_ptr, char buf[], size_t max_size);
int editing_buffer_put_str(
    struct editing_buffer* eb_ptr, const char* str, int n);
#ifdef ALLOW_REPEAT
void repeat_check(void);
#endif

#endif /* INCLUDED_SUPPORT_UTIL_H */
