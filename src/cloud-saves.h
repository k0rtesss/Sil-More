#ifndef SIL_CLOUD_SAVES_H
#define SIL_CLOUD_SAVES_H
#include <stdbool.h>
bool sil_cloud_saves_prepare(const char* user_root);
void sil_cloud_saves_init(void);
bool sil_cloud_saves_available(void);
void sil_cloud_saves_checkpoint(void);
#endif
