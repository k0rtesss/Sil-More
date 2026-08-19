#ifndef INCLUDED_UI_COMMAND_REFERENCE_H
#define INCLUDED_UI_COMMAND_REFERENCE_H

/*
 * Canonical keyboard command metadata shared by the keybinding screen and
 * Help.  Keep user-facing keyboard commands here so the two lists cannot
 * silently drift apart.
 */
struct keybind_entry
{
    byte key_code;
    cptr extra_default_keys;
    cptr key_name;
    cptr action;
    bool requires_keymap;
};

static const struct keybind_entry command_primary_keybinds[] = {
    {'i', NULL, "Inventory", "i", false},
    {'e', NULL, "Equipment", "e", false},
    {'u', NULL, "Use item", "u", false},
    {'x', NULL, "Examine item", "x", false},
    {'s', NULL, "Sing / change song", "s", false},
    {'S', NULL, "Toggle stealth", "S", false},
    {'h', NULL, "Character sheet", "h", false},
    {'\t', NULL, "Change active weapon", "\t", false},
    {'y', NULL, "Abilities", "y", false},
    {'f', "F", "Ranged attack (active weapon)", "f", false},
    {'l', NULL, "Look around", "l", false},
    {'T', NULL, "Tunnel / dig", "T", false},
    {'b', NULL, "Bash door", "b", false},
};

static const struct keybind_entry command_secondary_keybinds[] = {
    {'j', NULL, "Supplies overview", "j", false},
    {'w', NULL, "Wear / wield equipment", "w", false},
    {'r', NULL, "Remove / browse equipment", "r", false},
    {'g', NULL, "Pick up items", "g", false},
    {'o', NULL, "Open door / chest", "o", false},
    {'c', NULL, "Close door", "c", false},
    {'D', NULL, "Disarm trap / chest", "D", false},
    {'X', NULL, "Exchange places", "X", false},
    {'/', NULL, "Context action in a direction", "/", false},
    {';', NULL, "Walk in a direction", ";", false},
    {'.', NULL, "Run in a direction", ".", false},
    {'z', NULL, "Wait one turn", "z", false},
    {'Z', "%", "Rest", "Z", false},
    {'-', NULL, "Fletch arrows", "-", false},
    {'{', NULL, "Inscribe item", "{", false},
    {'a', NULL, "Activate Harness staff", "a", false},
    {'E', NULL, "Eat food", "E", false},
    {KTRL('F'), NULL, "Choose active arrows", "\006", false},
    {'t', NULL, "Throw item", "t", false},
    {KTRL('T'), NULL, "Quick throw at nearest target", "\024", false},
    {'p', NULL, "Blow horn", "p", false},
    {'q', NULL, "Quaff potion", "q", false},
    {'H', NULL, "Train skills", "H", false},
    {'@', NULL, "Character sheet (alternate)", "@", false},
    {'J', NULL, "Jewelry presets", "J", false},
    {'M', NULL, "View map", "M", false},
    {'L', NULL, "Pan view", "L", false},
    {KTRL('P'), NULL, "Previous messages", "\020", false},
    {KTRL('Q'), NULL, "Combat rolls", "\021", false},
    {KTRL('R'), NULL, "Redraw screen", "\022", false},
    {'0', "\004", "Smithing screen", "0", false},
    {'<', NULL, "Go upstairs", "<", false},
    {'>', NULL, "Go downstairs", ">", false},
    {'m', "\033", "Main menu", "m", false},
    {'?', NULL, "Gameplay reference", "?", false},
    {'O', NULL, "Options menu", "O", false},
    {':', NULL, "Take notes", ":", false},
    {'~', NULL, "Knowledge browser", "~", false},
    {'[', NULL, "Nearby monsters", "[", false},
    {']', NULL, "Nearby objects", "]", false},
    {KTRL('E'), NULL, "Toggle inventory / equipment pane", "\005", false},
    {KTRL('S'), NULL, "Save game", "\023", false},
    {KTRL('X'), "\003", "Save and quit", "\030", false},
};

#define COMMAND_PRIMARY_KEYBIND_COUNT \
    ((int)N_ELEMENTS(command_primary_keybinds))
#define COMMAND_SECONDARY_KEYBIND_COUNT \
    ((int)N_ELEMENTS(command_secondary_keybinds))

#endif
