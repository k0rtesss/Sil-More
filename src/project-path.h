/* project-path.h - projection helpers with optional path masking */

#ifndef INCLUDED_PROJECT_PATH_H
#define INCLUDED_PROJECT_PATH_H

#include "h-basic.h"

typedef struct project_path_mask {
    int y;
    int x;
} project_path_mask;

byte projectable_with_ignore(int y1, int x1, int y2, int x2, u32b flg,
    const project_path_mask* ignore);

#endif /* INCLUDED_PROJECT_PATH_H */
