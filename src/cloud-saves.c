#include "angband.h"
#include "externs.h"
#include "cloud-saves.h"
#include "achievements.h"
#include "metarun.h"
#include "game/progress-reset.h"
#include "log/log.h"

#if defined(__ANDROID__) && defined(USE_SDL)
#include <jni.h>

static bool cloud_ready;
static bool cloud_checkpoint_busy;

static int cloud_call(const char* method_name, const char* signature, const char* path)
{
    JNIEnv* env = (JNIEnv*)SDL_GetAndroidJNIEnv();
    if (!env) return 0;
    jobject activity = (jobject)SDL_GetAndroidActivity();
    jclass cls = NULL;
    jstring root = NULL;
    int result = 0;
    if ((*env)->ExceptionCheck(env) || !activity) goto done;
    cls = (*env)->GetObjectClass(env, activity);
    if ((*env)->ExceptionCheck(env) || !cls) goto done;
    jmethodID method = (*env)->GetMethodID(env, cls, method_name, signature);
    if ((*env)->ExceptionCheck(env) || !method) goto done;
    if (path) {
        root = (*env)->NewStringUTF(env, path);
        if ((*env)->ExceptionCheck(env) || !root) goto done;
    }
    if (signature[strlen(signature) - 1] == 'Z')
        result = (*env)->CallBooleanMethod(env, activity, method, root) == JNI_TRUE;
    else
        result = (*env)->CallIntMethod(env, activity, method, root);
done:
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        log_warn("Android cloud save bridge failed");
        result = 0;
    }
    if (root) (*env)->DeleteLocalRef(env, root);
    if (cls) (*env)->DeleteLocalRef(env, cls);
    if (activity) (*env)->DeleteLocalRef(env, activity);
    return result;
}
#endif

bool sil_cloud_saves_prepare(const char* user_root)
{
#if defined(__ANDROID__) && defined(USE_SDL)
    return cloud_call("preparePlayGamesCloudSaves", "(Ljava/lang/String;)Z", user_root) != 0;
#else
    (void)user_root;
    return true;
#endif
}

void sil_cloud_saves_init(void)
{
#if defined(__ANDROID__) && defined(USE_SDL)
    cloud_ready = true;
#endif
}

bool sil_cloud_saves_available(void)
{
#if defined(__ANDROID__) && defined(USE_SDL)
    return cloud_call("isPlayGamesCloudSavesAvailable", "()Z", NULL) != 0;
#else
    return false;
#endif
}

void sil_cloud_saves_checkpoint(void)
{
#if defined(__ANDROID__) && defined(USE_SDL)
    if (!cloud_ready || cloud_checkpoint_busy || sil_progress_reset_restarting()
        || !sil_cloud_saves_available()) return;
    cloud_checkpoint_busy = true;
    /* Capture only on the game thread, after the character and score writers.
     * Java uploads these immutable bytes without blocking native gameplay. */
    if (save_metaruns() == 0) {
        sil_achievements_flush();
        if (sil_achievements_saved())
            (void)cloud_call("checkpointPlayGamesCloudSaves", "(Ljava/lang/String;)I", ANGBAND_DIR_USER);
    } else log_warn("Cloud checkpoint deferred: Tale could not be saved");
    cloud_checkpoint_busy = false;
#endif
}
