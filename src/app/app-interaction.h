#ifndef INCLUDED_APP_INTERACTION_H
#define INCLUDED_APP_INTERACTION_H

#include "h-basic.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_INTERACTION_FORMAT_VERSION 3u
#define APP_INTERACTION_TEXT_MAX 192u
#define APP_INTERACTION_VALUE_MAX 96u
#define APP_INTERACTION_META_MAX 40u
#define APP_INTERACTION_OPTION_MAX 64u
#define APP_INTERACTION_LABEL_MAX 96u
#define APP_INTERACTION_KEY_MAX 4u

typedef enum app_interaction_kind {
    APP_INTERACTION_KIND_NONE = 0,
    APP_INTERACTION_KIND_PROMPT = 1,
    APP_INTERACTION_KIND_TEXT_ENTRY = 2,
    APP_INTERACTION_KIND_LIST = 3,
    APP_INTERACTION_KIND_TARGETING = 4,
    APP_INTERACTION_KIND_LOOK = 5
} app_interaction_kind;

typedef enum app_interaction_flag {
    APP_INTERACTION_FLAG_CAN_CONFIRM = 0x0001u,
    APP_INTERACTION_FLAG_CAN_CANCEL = 0x0002u,
    APP_INTERACTION_FLAG_SHOW_OPTIONS = 0x0004u,
    APP_INTERACTION_FLAG_SHOW_VALUE = 0x0008u,
    APP_INTERACTION_FLAG_SHOW_CURSOR = 0x0010u,
    APP_INTERACTION_FLAG_PLAIN_LIST = 0x0020u
} app_interaction_flag;

typedef enum app_interaction_entry_flag {
    APP_INTERACTION_ENTRY_FLAG_NONE = 0x00u,
    APP_INTERACTION_ENTRY_FLAG_DISABLED = 0x01u,
    APP_INTERACTION_ENTRY_FLAG_SELECTED = 0x02u
} app_interaction_entry_flag;

typedef struct app_interaction_option {
    byte attr;
    byte tag;
    byte enabled;
    byte selected;
    byte flags;
    char key[APP_INTERACTION_KEY_MAX];
    char label[APP_INTERACTION_LABEL_MAX];
    char meta[APP_INTERACTION_META_MAX];
} app_interaction_option;

typedef struct app_raw_cell_snapshot {
    byte attr;
    byte story;
    char ch;
    char reserved;
} app_raw_cell_snapshot;

typedef struct app_interaction_state {
    u16b format_version;
    u16b kind;
    u16b reason;
    u16b flags;
    byte prompt_attr;
    byte detail_attr;
    byte value_attr;
    byte reserved;
    s16b selected_index;
    s16b cursor_index;
    u16b option_count;
    u16b reserved2;
    char prompt[APP_INTERACTION_TEXT_MAX];
    char detail[APP_INTERACTION_TEXT_MAX];
    char value[APP_INTERACTION_VALUE_MAX];
    app_interaction_option options[APP_INTERACTION_OPTION_MAX];
} app_interaction_state;

void app_interaction_clear(app_interaction_state* interaction);
void app_interaction_begin(app_interaction_state* interaction, u16b kind,
    u16b wait_reason);
app_interaction_option* app_interaction_append_entry(
    app_interaction_state* interaction);

/* Backward-compatible aliases for earlier naming. */
#define APP_INTERACTION_KIND_TEXT_INPUT APP_INTERACTION_KIND_TEXT_ENTRY
#define APP_INTERACTION_KIND_QUANTITY APP_INTERACTION_KIND_TEXT_ENTRY
#define APP_INTERACTION_KIND_TARGET APP_INTERACTION_KIND_TARGETING

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_APP_INTERACTION_H */
