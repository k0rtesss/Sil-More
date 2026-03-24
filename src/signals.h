/* File: signals.h */

#ifndef INCLUDED_SIGNALS_H
#define INCLUDED_SIGNALS_H

#include "h-basic.h"

#ifdef HANDLE_SIGNALS
typedef void (*signal_handler_t)(int);
extern signal_handler_t (*signal_aux)(int, signal_handler_t);
#endif

extern void signals_ignore_tstp(void);
extern void signals_handle_tstp(void);
extern void signals_init(void);

#endif /* INCLUDED_SIGNALS_H */
