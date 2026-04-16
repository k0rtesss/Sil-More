/* File: cmd-ranged.h */

#ifndef INCLUDED_CMD_RANGED_H
#define INCLUDED_CMD_RANGED_H

#include "h-basic.h"

int archery_range(const object_type* j_ptr);
int throwing_range(const object_type* i_ptr);
void attacks_of_opportunity(int neutralized_y, int neutralized_x);
void do_cmd_fire(int quiver);
void do_cmd_throw(bool automatic);

#endif /* INCLUDED_CMD_RANGED_H */
