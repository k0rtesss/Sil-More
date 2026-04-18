#ifndef INCLUDED_SCORE_UI_BROWSER_H
#define INCLUDED_SCORE_UI_BROWSER_H

#include "app/app-ui.h"

void score_ui_prompt_label(int binding, const char* fallback, char* buf,
    size_t buflen);
app_ui_panel* score_ui_begin_browser_scene(app_ui_scene* scene,
    u16b panel_flags);

#endif /* INCLUDED_SCORE_UI_BROWSER_H */
