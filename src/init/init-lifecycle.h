#ifndef INCLUDED_INIT_INIT_LIFECYCLE_H
#define INCLUDED_INIT_INIT_LIFECYCLE_H

#include "../h-basic.h"

void init_angband(void);
void re_init_some_things(void);
NavResult initial_menu(bool* start_new);
void cleanup_angband(void);

#endif /* INCLUDED_INIT_INIT_LIFECYCLE_H */
