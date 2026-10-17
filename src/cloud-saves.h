#ifndef SIL_CLOUD_SAVES_H
#define SIL_CLOUD_SAVES_H
#include <stdbool.h>
bool sil_cloud_saves_prepare(const char* user_root);
bool sil_cloud_saves_available(void);
void sil_cloud_saves_show(void);
#endif
