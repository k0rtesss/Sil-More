#ifndef SIL_STEAMWORKS_H
#define SIL_STEAMWORKS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef SIL_STEAMWORKS
/* False means Steam is relaunching the game and this process must exit. */
bool sil_steamworks_init(void);
void sil_steamworks_run_callbacks(void);
int sil_steamworks_callback_timeout_ms(void);
bool sil_steamworks_is_steam_deck(void);
bool sil_steamworks_active(void);
void sil_steamworks_sync_achievements(unsigned int unlocked_mask);
unsigned int sil_steamworks_achievements_unlocked(void);
bool sil_steamworks_show_achievements(void);
void sil_steamworks_achievements_shutdown(void);
#else
static inline bool sil_steamworks_init(void) { return true; }
static inline void sil_steamworks_run_callbacks(void) {}
static inline int sil_steamworks_callback_timeout_ms(void) { return -1; }
static inline bool sil_steamworks_is_steam_deck(void) { return false; }
static inline bool sil_steamworks_active(void) { return false; }
static inline void sil_steamworks_sync_achievements(unsigned int mask) { (void)mask; }
static inline unsigned int sil_steamworks_achievements_unlocked(void) { return 0; }
static inline bool sil_steamworks_show_achievements(void) { return false; }
static inline void sil_steamworks_achievements_shutdown(void) {}
#endif

#ifdef __cplusplus
}
#endif

#endif
