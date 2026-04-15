#ifndef INCLUDED_APP_COMMAND_H
#define INCLUDED_APP_COMMAND_H

#include "app-movement.h"
#include "app-session.h"
#include "h-basic.h"

#ifdef __cplusplus
extern "C" {
#endif

void app_command_clear_pending(void);
bool app_command_wait_input(u16b context, u16b wait_reason,
    app_movement_command* out_command, char* out_ch);
void app_request_player_command(void);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_APP_COMMAND_H */
