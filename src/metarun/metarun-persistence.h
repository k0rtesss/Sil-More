#ifndef INCLUDED_METARUN_PERSISTENCE_H
#define INCLUDED_METARUN_PERSISTENCE_H

#include "../metarun.h"

void reset_defaults(metarun *m);
void apply_difficulty_curses(metarun *m);
void ensure_run_dir(const metarun *m);
bool sync_current_metarun_slot(bool stamp_time);

#endif /* INCLUDED_METARUN_PERSISTENCE_H */
