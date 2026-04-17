#ifndef INCLUDED_METARUN_STATE_H
#define INCLUDED_METARUN_STATE_H

#include "../metarun.h"

extern metarun metar;
extern metarun *metaruns;
extern s16b metarun_max;
extern s16b current_run;
extern bool metarun_created;

void metarun_prompt_label(int binding, const char* fallback, char* buf,
    size_t buflen);

#endif /* INCLUDED_METARUN_STATE_H */
