/* File: init1.c */

/*
 * This file used to contain the bulk of the ascii template parsing logic
 * (parse_*_info for lib/edit/*.txt) plus style helpers.
 *
 * It has been split into focused modules under `src/init/`:
 *   - init-parser-core.c (init_info_txt, add_text/add_name, parse_tile_line)
 *   - init-flags.c (flag table, grab_one_flag, dbg_show_active_flags)
 *   - init-parse-limits.c (parse_z_info)
 *   - init-parse-runtypes.c (rt_head, parse_rt_info)
 *   - init-style.c (parse_style_info, parse_style_levels, style display helpers)
 *   - init-parse-terrain.c (parse_f_info)
 *   - init-parse-object-kind.c (parse_k_info)
 *   - init-parse-vault.c (parse_v_info)
 *   - init-parse-ability.c (parse_b_info)
 *   - init-parse-artefact.c (parse_a_info)
 *   - init-parse-names.c (parse_n_info)
 *   - init-parse-skeleton-notes.c (parse_skeleton_note_info)
 *   - init-parse-ego.c (parse_e_info)
 *   - init-parse-monster.c (parse_r_info)
 *   - init-parse-player.c (parse_p_info, parse_c_info, parse_h_info)
 *   - init-parse-stores.c (parse_st_info)
 *   - init-parse-curses.c (parse_cu_info)
 *   - init-parse-major-blessings.c (parse_mb_info)
 *   - init-parse-flavor.c (parse_flavor_info)
 *   - init-parse-effects.c (parse_effect_info)
 *   - init-parse-quests.c (parse_quest_info, parse_oath_info)
 *
 * The build now compiles those modules directly (see `CMakeLists.txt`).
 */

