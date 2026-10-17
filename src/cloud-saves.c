#include "angband.h"
#include "externs.h"
#include "cloud-saves.h"
#include "achievements.h"
#include "metarun.h"
#include "log/log.h"

#if defined(__ANDROID__) && defined(USE_SDL)
#include <jni.h>

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

bool sil_cloud_saves_available(void)
{
#if defined(__ANDROID__) && defined(USE_SDL)
    return cloud_call("isPlayGamesCloudSavesAvailable", "()Z", NULL) != 0;
#else
    return false;
#endif
}

void sil_cloud_saves_show(void)
{
#if defined(__ANDROID__) && defined(USE_SDL)
    if (!sil_cloud_saves_available()) {
        sdl_popup_notification_show("Google Play cloud saves are unavailable in this build.");
        return;
    }
    /* All local writers and native gameplay remain on this thread. Capture the
     * complete checkpoint before Java opens its asynchronous provider UI. */
    if (p_ptr && p_ptr->playing && character_generated && !p_ptr->is_dead
        && !mobile_autosave_game("cloud save checkpoint")) {
        sdl_popup_notification_show("Local progress could not be saved. Cloud saves were not opened.");
        return;
    }
    if (save_metaruns() != 0) {
        sdl_popup_notification_show("The Tale could not be saved. Cloud saves were not opened.");
        return;
    }
    sil_achievements_flush();
    if (!sil_achievements_saved()) {
        sdl_popup_notification_show("Achievement progress could not be saved. Cloud saves were not opened.");
        return;
    }
    int result = cloud_call("showPlayGamesCloudSaves", "(Ljava/lang/String;)I", ANGBAND_DIR_USER);
    if (result == 2) quit(NULL);
#endif
}
