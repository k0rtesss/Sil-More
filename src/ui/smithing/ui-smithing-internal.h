/* File: ui-smithing-internal.h */

/*
 * Internal scaffolding for the smithing UI split.
 * The menu constants are copied from cmd4.c so later UI file moves can stay
 * mechanical.
 */

#ifndef INCLUDED_UI_SMITHING_INTERNAL_H
#define INCLUDED_UI_SMITHING_INTERNAL_H

#include "smithing/smithing-internal.h"

#define SMT_MENU_CREATE 1
#define SMT_MENU_ENCHANT 2
#define SMT_MENU_ARTEFACT 3
#define SMT_MENU_NUMBERS 4
#define SMT_MENU_MELT 5
#define SMT_MENU_ACCEPT 6

#define SMT_MENU_MAX 6

#define SMT_NUM_MENU_I_ATT 1
#define SMT_NUM_MENU_D_ATT 2
#define SMT_NUM_MENU_I_DS 3
#define SMT_NUM_MENU_D_DS 4
#define SMT_NUM_MENU_I_EVN 5
#define SMT_NUM_MENU_D_EVN 6
#define SMT_NUM_MENU_I_PS 7
#define SMT_NUM_MENU_D_PS 8
#define SMT_NUM_MENU_I_PVAL 9
#define SMT_NUM_MENU_D_PVAL 10
#define SMT_NUM_MENU_ALLOY_CYCLE 13
#define SMT_NUM_MENU_ALLOY_CLEAR 14
#define SMT_NUM_MENU_EDIT_BONUSES 15

#define SMT_NUM_MENU_MAX 15

#define COL_SMT1 2
#define COL_SMT2 16
#define COL_SMT3 36
#define COL_SMT4 66

#endif /* INCLUDED_UI_SMITHING_INTERNAL_H */
