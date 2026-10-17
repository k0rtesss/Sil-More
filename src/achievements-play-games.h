#ifndef SIL_ACHIEVEMENTS_PLAY_GAMES_H
#define SIL_ACHIEVEMENTS_PLAY_GAMES_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bits use the stable shared achievement order. Safe no-ops outside Android. */
void sil_play_games_sync(unsigned int unlocked_mask);
unsigned int sil_play_games_unlocked(void);
bool sil_play_games_show(void);
bool sil_play_games_available(void);

#ifdef __cplusplus
}
#endif
#endif
