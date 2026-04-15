#include "angband.h"

#include "app/app-command.h"
#include "externs.h"

static char app_command_movement_legacy_command_char(
    const app_movement_command* command)
{
    if (!app_movement_command_is_valid(command))
        return '\0';

    switch (command->action)
    {
    case APP_MOVEMENT_ACTION_MOVE_DIR:
        return ';';

    case APP_MOVEMENT_ACTION_RUN_DIR:
        return '.';

    case APP_MOVEMENT_ACTION_INTERACT_DIR:
        return '/';

    case APP_MOVEMENT_ACTION_WAIT:
        return 'z';

    case APP_MOVEMENT_ACTION_REST:
        return 'Z';

    default:
        return '\0';
    }
}

static bool app_command_requires_inscription_confirmation(char command_char)
{
    int i;

    if (!command_char)
        return false;

    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        cptr s;
        object_type* o_ptr = &inventory[i];

        if (!o_ptr->k_idx || !o_ptr->obj_note)
            continue;

        s = strchr(quark_str(o_ptr->obj_note), '^');
        while (s)
        {
            if ((s[1] == command_char) || (s[1] == '*'))
            {
                if (!get_check("Are you sure? "))
                    return true;
            }

            s = strchr(s + 1, '^');
        }
    }

    return false;
}

static bool app_command_prompt_repeat_count(int* out_repeat_count)
{
    char buf[16] = "";

    if (!out_repeat_count)
        return false;

    while (true)
    {
        int count = 0;
        size_t i;
        bool invalid = false;

        message_flush();
        if (!prompt_text_input("Repeat how many times:",
                "Enter digits. Blank or 0 means 99.", buf, sizeof(buf), false))
        {
            return false;
        }

        if (!buf[0])
        {
            *out_repeat_count = 99;
            return true;
        }

        for (i = 0; buf[i]; i++)
        {
            if (!isdigit((unsigned char)buf[i]))
            {
                invalid = true;
                break;
            }

            if (count >= 1000)
            {
                bell("Invalid repeat count!");
                count = 9999;
                while (isdigit((unsigned char)buf[i + 1]))
                    i++;
                break;
            }

            count = count * 10 + D2I(buf[i]);
        }

        if (invalid)
        {
            bell("Invalid repeat count!");
            continue;
        }

        if (count == 0)
            count = 99;

        *out_repeat_count = count;
        return true;
    }
}

void app_command_clear_pending(void)
{
    input_clear_movement_commands();
}

bool app_command_wait_input(u16b context, u16b wait_reason,
    app_movement_command* out_command, char* out_ch)
{
    return input_wait_for_movement_or_legacy(context, wait_reason, out_command,
        out_ch);
}

void app_request_player_command(void)
{
    app_movement_command movement_command;
    char ch = '\0';

    p_ptr->command_cmd = 0;
    p_ptr->command_arg = 0;
    p_ptr->command_dir = 0;

    while (!p_ptr->command_cmd)
    {
        app_movement_command_clear(&movement_command);
        ch = '\0';

        if (p_ptr->command_new)
        {
            message_flush();
            ch = (char)p_ptr->command_new;
            p_ptr->command_new = 0;
        }
        else
        {
            msg_flag = false;
            (void)app_command_wait_input(APP_MOVEMENT_CONTEXT_DUNGEON,
                APP_WAIT_REASON_COMMAND_INPUT, &movement_command, &ch);
        }

        if (app_movement_command_is_valid(&movement_command))
        {
            p_ptr->command_cmd
                = app_command_movement_legacy_command_char(&movement_command);
            if (app_movement_action_is_directional(movement_command.action))
            {
                p_ptr->command_dir = app_movement_direction_to_legacy_keypad(
                    movement_command.direction.direction);
            }
        }
        else if (!ch)
        {
            continue;
        }
        else
        {
            if (((ch == 'R') && !angband_keyset)
                || ((ch == '0') && angband_keyset))
            {
                int repeat_count = 0;

                if (!app_command_prompt_repeat_count(&repeat_count))
                {
                    p_ptr->command_arg = 0;
                    continue;
                }

                p_ptr->command_arg = repeat_count;
                continue;
            }

            if (ch == '\\')
            {
                if (!get_com("Command: ", &ch))
                {
                    p_ptr->command_arg = 0;
                    continue;
                }
            }

            if (ch == '^')
            {
                if (!get_com("Control: ", &ch))
                {
                    p_ptr->command_arg = 0;
                    continue;
                }

                ch = KTRL(ch);
            }

            p_ptr->command_cmd = ch;
        }

        if (!p_ptr->command_cmd)
            continue;

        if (app_command_requires_inscription_confirmation(p_ptr->command_cmd))
            p_ptr->command_cmd = '\n';
    }
}
