#ifndef INCLUDED_METARUN_INTERNAL_H
#define INCLUDED_METARUN_INTERNAL_H

#include "../metarun.h"

/*
 * Lane-local metarun implementation contracts. Keep non-public declarations
 * here or in the companion metarun-local headers rather than widening
 * src/externs.h.
 */

#define CURSE_MENU_LINES 3
#define METARUN_RUNTIME_CHALLENGE_FLAGS_IDX 0
#define METARUN_CHALLENGE_DISCON_FLAG 0x01
#define METARUN_CHALLENGE_SINGLE_FLAG 0x02
#define METARUN_CHALLENGE_FIXED_FLAG 0x04
#define METARUN_CHALLENGE_TULKAS_BLUNT_FLAG 0x08
#define METARUN_CHALLENGE_TORCHLIGHT_FLAG 0x10
#define METARUN_RUNTIME_CHALLENGE_COUNT_BASE 1
#define METARUN_HISTORY_PAGE_SIZE 48
#define METARUN_KNOWN_CURSE_PAGE_SIZE 12

#include "metarun-state.h"
#include "metarun-score.h"
#include "metarun-persistence.h"

#endif /* INCLUDED_METARUN_INTERNAL_H */
