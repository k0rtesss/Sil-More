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
#else
static inline bool sil_steamworks_init(void) { return true; }
static inline void sil_steamworks_run_callbacks(void) {}
static inline int sil_steamworks_callback_timeout_ms(void) { return -1; }
#endif

#ifdef __cplusplus
}
#endif

#endif
