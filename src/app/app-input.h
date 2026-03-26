#ifndef INCLUDED_APP_INPUT_H
#define INCLUDED_APP_INPUT_H

#include "h-basic.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum app_input_layer {
    APP_INPUT_LAYER_LEGACY = 0,
    APP_INPUT_LAYER_INTENT = 1
} app_input_layer;

typedef enum app_input_device {
    APP_INPUT_DEVICE_NONE = 0,
    APP_INPUT_DEVICE_KEYBOARD = 1,
    APP_INPUT_DEVICE_POINTER = 2,
    APP_INPUT_DEVICE_TOUCH = 3,
    APP_INPUT_DEVICE_GAMEPAD = 4,
    APP_INPUT_DEVICE_SYSTEM = 5
} app_input_device;

typedef enum app_input_type {
    APP_INPUT_TYPE_NONE = 0,
    APP_INPUT_TYPE_KEY = 1,
    APP_INPUT_TYPE_TEXT = 2,
    APP_INPUT_TYPE_POINTER_MOTION = 3,
    APP_INPUT_TYPE_POINTER_BUTTON = 4,
    APP_INPUT_TYPE_POINTER_WHEEL = 5,
    APP_INPUT_TYPE_GAMEPAD_BUTTON = 6,
    APP_INPUT_TYPE_GAMEPAD_AXIS = 7,
    APP_INPUT_TYPE_SYSTEM = 8
} app_input_type;

typedef enum app_input_modifier {
    APP_INPUT_MODIFIER_SHIFT = 0x0001u,
    APP_INPUT_MODIFIER_CTRL = 0x0002u,
    APP_INPUT_MODIFIER_ALT = 0x0004u,
    APP_INPUT_MODIFIER_META = 0x0008u,
    APP_INPUT_MODIFIER_CAPS_LOCK = 0x0010u,
    APP_INPUT_MODIFIER_NUM_LOCK = 0x0020u
} app_input_modifier;

typedef enum app_input_flag {
    APP_INPUT_FLAG_PRESS = 0x0001u,
    APP_INPUT_FLAG_RELEASE = 0x0002u,
    APP_INPUT_FLAG_REPEAT = 0x0004u,
    APP_INPUT_FLAG_SYNTHETIC = 0x0008u
} app_input_flag;

typedef enum app_intent_kind {
    APP_INTENT_KIND_NONE = 0,
    APP_INTENT_KIND_NAVIGATE = 1,
    APP_INTENT_KIND_CONFIRM = 2,
    APP_INTENT_KIND_CANCEL = 3,
    APP_INTENT_KIND_MENU = 4,
    APP_INTENT_KIND_ACTIVATE_SLOT = 5,
    APP_INTENT_KIND_TARGET_DELTA = 6,
    APP_INTENT_KIND_COMMAND = 7,
    APP_INTENT_KIND_TEXT = 8,
    APP_INTENT_KIND_SYSTEM = 9
} app_intent_kind;

typedef enum app_intent_flag {
    APP_INTENT_FLAG_ANALOG = 0x0001u,
    APP_INTENT_FLAG_REPEAT = 0x0002u,
    APP_INTENT_FLAG_LONG_PRESS = 0x0004u
} app_intent_flag;

typedef struct app_input_key_event {
    u32b logical_key;
    u32b physical_key;
    u16b repeat_count;
    u16b reserved;
} app_input_key_event;

typedef struct app_input_text_event {
    u32b codepoint;
    char utf8[8];
} app_input_text_event;

typedef struct app_input_pointer_event {
    s32b x;
    s32b y;
    s32b dx;
    s32b dy;
    u16b button;
    u16b clicks;
} app_input_pointer_event;

typedef struct app_input_wheel_event {
    s32b x;
    s32b y;
} app_input_wheel_event;

typedef struct app_input_gamepad_event {
    u16b control;
    s16b value;
    u16b secondary_control;
    u16b reserved;
} app_input_gamepad_event;

typedef struct app_input_system_event {
    u16b code;
    u16b value;
    u32b data;
} app_input_system_event;

typedef struct app_input {
    u16b layer;
    u16b type;
    u16b device;
    u16b modifiers;
    u16b flags;
    u16b source_id;
    u32b reserved;
    u64b sequence;
    u64b timestamp_usec;
    union {
        app_input_key_event key;
        app_input_text_event text;
        app_input_pointer_event pointer;
        app_input_wheel_event wheel;
        app_input_gamepad_event gamepad;
        app_input_system_event system;
    } payload;
} app_input;

typedef struct app_intent_vector {
    s16b dy;
    s16b dx;
} app_intent_vector;

typedef struct app_intent_slot {
    s16b group;
    s16b index;
} app_intent_slot;

typedef struct app_intent_command {
    u32b command_id;
    char token[24];
} app_intent_command;

typedef struct app_intent {
    u16b kind;
    u16b flags;
    u16b repeat_count;
    u16b reserved;
    u64b sequence;
    u64b timestamp_usec;
    union {
        app_intent_vector vector;
        app_intent_slot slot;
        app_intent_command command;
        app_input_text_event text;
        app_input_system_event system;
    } payload;
} app_intent;

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_APP_INPUT_H */
