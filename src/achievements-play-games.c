#include "angband.h"
#include "achievements.h"
#include "achievements-play-games.h"
#include "log/log.h"

#if defined(__ANDROID__) && defined(USE_SDL)
#include <SDL3/SDL.h>
#include <jni.h>

static bool play_games_exception(JNIEnv* env)
{
    if (!(*env)->ExceptionCheck(env)) return false;
    (*env)->ExceptionClear(env);
    log_warn("Android Play Games achievement bridge failed");
    return true;
}

/* Resolve on the current SDL thread; never retain JNI local refs across frames. */
static int play_games_call(const char* name, const char* signature, int value,
    bool returns_value)
{
    JNIEnv* env = (JNIEnv*)SDL_GetAndroidJNIEnv();
    jobject activity = NULL;
    jclass activity_class = NULL;
    int result = 0;
    if (!env) return 0;
    activity = (jobject)SDL_GetAndroidActivity();
    if (play_games_exception(env) || !activity) goto cleanup;
    activity_class = (*env)->GetObjectClass(env, activity);
    if (play_games_exception(env) || !activity_class) goto cleanup;
    {
        jmethodID method = (*env)->GetMethodID(env, activity_class, name, signature);
        if (play_games_exception(env) || !method) goto cleanup;
        if (returns_value && signature[2] == 'Z')
            result = (*env)->CallBooleanMethod(env, activity, method) == JNI_TRUE;
        else if (returns_value)
            result = (*env)->CallIntMethod(env, activity, method);
        else
            (*env)->CallVoidMethod(env, activity, method, (jint)value);
        if (play_games_exception(env)) result = 0;
    }
cleanup:
    if (activity_class) (*env)->DeleteLocalRef(env, activity_class);
    if (activity) (*env)->DeleteLocalRef(env, activity);
    return result;
}
#endif

void sil_play_games_sync(unsigned int unlocked_mask)
{
#if defined(__ANDROID__) && defined(USE_SDL)
    play_games_call("syncPlayGamesAchievements", "(I)V", (int)(unlocked_mask & SIL_ACH_ALL_MASK), false);
#else
    (void)unlocked_mask;
#endif
}

unsigned int sil_play_games_unlocked(void)
{
#if defined(__ANDROID__) && defined(USE_SDL)
    return (unsigned int)play_games_call("getPlayGamesAchievementMask", "()I", 0, true) & SIL_ACH_ALL_MASK;
#else
    return 0;
#endif
}

bool sil_play_games_show(void)
{
#if defined(__ANDROID__) && defined(USE_SDL)
    return play_games_call("showPlayGamesAchievements", "()Z", 0, true) != 0;
#else
    return false;
#endif
}

bool sil_play_games_available(void)
{
#if defined(__ANDROID__) && defined(USE_SDL)
    return play_games_call("isPlayGamesAchievementsAvailable", "()Z", 0, true) != 0;
#else
    return false;
#endif
}
