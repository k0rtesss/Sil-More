#ifndef INCLUDED_METARUN_SCORE_H
#define INCLUDED_METARUN_SCORE_H

#include "../metarun.h"

int major_blessing_capacity(void);
const major_blessing_type *major_blessing_def(int idx);
u32b runtype_threshold_for_mode(int runtype_id,
    metarun_blessing_threshold_mode mode);
u32b metarun_threshold_value(const metarun *m);
const char *threshold_mode_name(metarun_blessing_threshold_mode mode);
void update_blessing_ledger(metarun *m);
cptr major_blessing_name_str(int idx);
cptr major_blessing_short_desc(int idx);
cptr major_blessing_detail_desc(int idx);
cptr major_blessing_unlock_msg(int idx);
int major_blessing_cost(int idx);
metarun_major_effect major_blessing_effect(int idx);
void build_symbol_bar(char *out, size_t out_len, int current, int maximum,
    char filled);
void build_death_marks(char *out, size_t out_len, int deaths);
u32b get_best_run_score_from_highscores(void);
u32b compute_metarun_score(const metarun *m);
int compare_metarun_indices(const void *a, const void *b);

#endif /* INCLUDED_METARUN_SCORE_H */
