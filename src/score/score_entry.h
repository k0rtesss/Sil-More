#ifndef INCLUDED_SCORE_ENTRY_H
#define INCLUDED_SCORE_ENTRY_H

#include "h-basic.h"

struct high_score;

bool highscore_is_empty(void);
errr create_score(struct high_score* the_score);
bool build_live_preview_score(struct high_score* out);
errr score_entry_submit(struct high_score* the_score);
const char* kinslayer_try_kill(uint8_t n_sils, bool do_roll);

#endif /* INCLUDED_SCORE_ENTRY_H */
