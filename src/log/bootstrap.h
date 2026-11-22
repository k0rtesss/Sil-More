/* log/bootstrap.h - Logger initialization */

#ifndef INCLUDED_LOG_BOOTSTRAP_H
#define INCLUDED_LOG_BOOTSTRAP_H

#include "../h-basic.h"

/*
 * Use the platform's writable log location, or an explicit UTF-8 directory.
 * Quiet suppresses stderr while a log file is available.
 */
extern void init_logger(bool quiet, const char* directory);

#endif /* INCLUDED_LOG_BOOTSTRAP_H */
