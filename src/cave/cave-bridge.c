#include "angband.h"
#include "cave/cave-bridge.h"

static const byte bridge_materials[] = {
    FEAT_WATER, FEAT_CHASM, FEAT_LAVA, FEAT_POISON, FEAT_ICE
};

bool cave_feat_is_bridge(int feat)
{
    return FEAT_IS_BRIDGE(feat);
}

int cave_bridge_underlay(int feat)
{
    return FEAT_IS_BRIDGE(feat) ? bridge_materials[(feat - FEAT_BRIDGE_HEAD) / 2] : feat;
}

bool cave_bridge_vertical(int feat)
{
    return FEAT_IS_BRIDGE(feat) && (feat - FEAT_BRIDGE_HEAD) % 2;
}

int cave_bridge_feature(int underlay, bool vertical)
{
    for (int i = 0; i < 5; i++)
        if (bridge_materials[i] == underlay) return FEAT_BRIDGE_HEAD + 2 * i + vertical;
    return FEAT_NONE;
}
