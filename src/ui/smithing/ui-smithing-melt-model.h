/* File: ui-smithing-melt-model.h */
/* Lane-local implementation fragment included by ui-smithing-screen.c. */

typedef struct smith_ui_melt_menu_state
{
    int count;
    int slots[INVEN_TOTAL];
} smith_ui_melt_menu_state;

typedef struct smith_ui_enchant_menu_state
{
    bool selecting_prefix;
    int entry_count;
    int choice[26];
    bool valid[26];
    byte row_attr[26];
    int fixed_prefix;
    int fixed_suffix;
    const object_type* base_o_ptr;
} smith_ui_enchant_menu_state;

typedef struct smith_ui_reforge_menu_state
{
    int entry_count;
    int choice[26];
    bool valid[26];
    byte row_attr[26];
    reforge_preview_type previews[26];
    const object_type* source;
} smith_ui_reforge_menu_state;

static void smith_ui_melt_build_state(smith_ui_melt_menu_state* state)
{
    int item;

    if (!state)
        return;

    memset(state, 0, sizeof(*state));

    for (item = 0; item < INVEN_TOTAL; item++)
    {
        object_type* o_ptr = &inventory[item];
        u32b f1, f2, f3;

        object_flags(o_ptr, &f1, &f2, &f3);
        if ((f3 & (TR3_MITHRIL | TR3_STAR_IRON))
            && !(o_ptr->ident & IDENT_CANT_MELT))
        {
            state->slots[state->count++] = item;
        }
    }
}

static bool smith_ui_melt_add_selected_detail(app_ui_panel* panel,
    const smith_ui_melt_menu_state* state, int highlight)
{
    char buf[APP_UI_TEXT_MAX];
    char desc[80];
    int slot;
    object_type* o_ptr;
    u32b f1, f2, f3;
    cptr metal_name;

    if (!panel || !state)
        return false;

    if (state->count <= 0)
    {
        app_ui_panel_set_detail_title(panel, TERM_L_BLUE, "Melt");
        return smith_ui_panel_try_add_detail_line(panel, TERM_L_DARK,
            "You are not carrying any meltable mithril or star-iron items.");
    }

    if (highlight < 1 || highlight > state->count)
        highlight = 1;

    slot = state->slots[highlight - 1];
    o_ptr = &inventory[slot];
    object_flags(o_ptr, &f1, &f2, &f3);
    metal_name = (f3 & TR3_STAR_IRON) ? "Star Iron" : "Mithril";
    object_desc(desc, sizeof(desc), o_ptr, false, 2);

    app_ui_panel_set_detail_title(panel, TERM_L_BLUE, desc);
    strnfmt(buf, sizeof(buf), "Returns %d.%d lb of %s.", o_ptr->weight / 10,
        o_ptr->weight % 10, metal_name);
    if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE, buf))
        return false;

    if (slot >= INVEN_WIELD)
        strnfmt(buf, sizeof(buf), "Location: %s", mention_use(slot));
    else
        strnfmt(buf, sizeof(buf), "Location: inventory slot %c",
            index_to_label(slot));
    if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE, buf))
        return false;

    return smith_ui_panel_try_add_detail_line(panel, TERM_WHITE,
        "Selecting this item opens the normal melt confirmation prompt.");
}
