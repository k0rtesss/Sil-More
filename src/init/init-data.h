#ifndef INCLUDED_INIT_INIT_DATA_H
#define INCLUDED_INIT_INIT_DATA_H

#include "h-basic.h"

extern maxima* z_info;

extern vault_type* v_info;
extern char* v_name;
extern char* v_text;

extern feature_type* f_info;
extern char* f_name;
extern char* f_text;

extern object_kind* k_info;
extern char* k_name;
extern char* k_text;

extern ability_type* b_info;
extern char* b_name;
extern char* b_text;

extern artefact_type* a_info;
extern char* a_text;
extern bool* valar_reserved_artifacts;

extern ego_item_type* e_info;
extern char* e_name;
extern char* e_text;

extern monster_race* r_info;
extern monster_race* r_base;
extern char* r_name;
extern char* r_text;

extern player_race* p_info;
extern char* p_name;
extern char* p_text;

extern character_profile* c_info;
extern char* c_name;
extern char* c_text;

extern hist_type* h_info;
extern char* h_text;

extern story_type* st_info;
extern char* st_text;
extern char* st_name;

extern curse_type* cu_info;
extern char* cu_text;
extern char* cu_name;

extern major_blessing_type* mb_info;
extern char* mb_text;
extern char* mb_name;

extern quest_type* quest_info;
extern char* quest_name_text;
extern char* quest_desc_text;
extern char* q_text;

extern oath_type* oath_info;
extern char* oath_name_text;
extern char* oath_desc_text;

extern flavor_type* flavor_info;
extern char* flavor_name;
extern char* flavor_text;

extern names_type* n_info;
extern style_type* style_info;
extern char* style_name;
extern skeleton_note_template* skeleton_note_info;
extern char* skeleton_note_text;

extern byte misc_to_attr[256];
extern char misc_to_char[256];
extern byte tval_to_attr[128];

extern runtype_type* runtype_info;

#endif /* INCLUDED_INIT_INIT_DATA_H */
