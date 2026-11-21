#ifndef SIL_ACHIEVEMENTS_GAME_CENTER_H
#define SIL_ACHIEVEMENTS_GAME_CENTER_H

#include <stdbool.h>
#include <stdint.h>

/* Permanent App Store Connect IDs. The store exporter reads this same prefix. */
#define SIL_GAME_CENTER_ID_PREFIX "com.silqh.silmore.achievement."

#ifdef __cplusplus
extern "C" {
#endif

#if defined(SIL_IOS)
void sil_game_center_init(void);
void sil_game_center_sync(const uint64_t* unlocked_masks);
uint64_t sil_game_center_unlocked(unsigned word);
bool sil_game_center_show(void);
bool sil_game_center_available(void);
#else
static inline void sil_game_center_init(void) {}
static inline void sil_game_center_sync(const uint64_t* masks) { (void)masks; }
static inline uint64_t sil_game_center_unlocked(unsigned word) { (void)word; return 0; }
static inline bool sil_game_center_show(void) { return false; }
static inline bool sil_game_center_available(void) { return false; }
#endif

#ifdef __cplusplus
}
#endif
#endif
