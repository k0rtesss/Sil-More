#include "angband.h"
#include "externs.h"
#include <assert.h>

static int noise_y, noise_x, perceptions, messages;

bool __wrap_los(int y1, int x1, int y2, int x2)
{
    (void)x1; (void)y2; (void)x2;
    return y1 != 20;
}

void __wrap_monster_desc(char* desc, size_t max, const monster_type* mon, int mode)
{
    (void)mon; (void)mode;
    SDL_strlcpy(desc, "a witness", max);
}

void __wrap_update_flow(int y, int x, int which)
{
    assert(which == FLOW_MONSTER_NOISE);
    noise_y = y; noise_x = x;
}

void __wrap_monster_perception(bool centered, bool main_roll, int difficulty)
{
    assert(!centered && !main_roll && difficulty == -10);
    perceptions++;
}

void __wrap_msg_format(cptr fmt, ...) { (void)fmt; messages++; }
void __wrap_msg_print(cptr msg) { (void)msg; messages++; }

int main(void)
{
    monster_type monsters[5] = {{0}};
    mon_list = monsters;
    mon_max = 5;
    for (int i = 1; i < mon_max; ++i) {
        monsters[i].r_idx = 1;
        monsters[i].fy = i + 5;
        monsters[i].fx = i + 8;
        monsters[i].min_range = 8;
        monsters[i].alertness = ALERTNESS_ALERT;
    }
    /* The final scanned slot is dead, asleep or out of sight. The noise
     * must still originate at the last eligible witness (slot 2). */
    for (int reason = 0; reason < 3; ++reason) {
        monsters[1].r_idx = reason == 0 ? 0 : 1;
        monsters[1].alertness = reason == 1 ? ALERTNESS_ALERT - 1 : ALERTNESS_ALERT;
        monsters[1].fy = reason == 2 ? 20 : 6;
        p_ptr->truce = true;
        noise_y = noise_x = -1; perceptions = messages = 0;
        break_truce(false);
        assert(!p_ptr->truce && perceptions == 1 && messages == 1);
        assert(noise_y == monsters[2].fy && noise_x == monsters[2].fx);
        for (int i = 2; i < mon_max; ++i) assert(monsters[i].min_range == 0);
    }
    mon_max = 1;
    p_ptr->truce = true;
    perceptions = messages = 0;
    break_truce(false);
    assert(p_ptr->truce && !messages && !perceptions);
    break_truce(true);
    assert(!p_ptr->truce && messages == 1 && !perceptions);
    break_truce(true);
    assert(messages == 1);
    puts("Truce: correct noise witness, range reset, no witness and obvious break: PASS");
    return 0;
}
