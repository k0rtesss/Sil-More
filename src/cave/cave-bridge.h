#ifndef INCLUDED_CAVE_BRIDGE_H
#define INCLUDED_CAVE_BRIDGE_H

#include "angband.h"

/* The feature byte is the complete persistent state. A bridge is dry floor;
 * its encoded underlay is used only for rendering and terrain planning. */
bool cave_feat_is_bridge(int feat);
int cave_bridge_underlay(int feat);
bool cave_bridge_vertical(int feat);
/* Return FEAT_NONE for an unsupported material, never silently plain floor. */
int cave_bridge_feature(int underlay, bool vertical);

#endif
