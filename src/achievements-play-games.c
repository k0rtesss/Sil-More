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
static uint64_t play_games_call(const char* name, const char* signature,
    unsigned word, const uint64_t* masks)
{
    JNIEnv* env = (JNIEnv*)SDL_GetAndroidJNIEnv();
    jobject activity = NULL;
    jclass activity_class = NULL;
    jlongArray values = NULL;
    uint64_t result = 0;
    if (!env) return 0;
    activity = (jobject)SDL_GetAndroidActivity();
    if (play_games_exception(env) || !activity) goto cleanup;
    activity_class = (*env)->GetObjectClass(env, activity);
    if (play_games_exception(env) || !activity_class) goto cleanup;
    {
        jmethodID method = (*env)->GetMethodID(env, activity_class, name, signature);
        if (play_games_exception(env) || !method) goto cleanup;
        if (masks)
        {
            jlong words[SIL_ACH_WORD_COUNT];
            for (unsigned i = 0; i < SIL_ACH_WORD_COUNT; ++i)
                words[i] = (jlong)(masks[i] & sil_achievement_word_mask(i));
            values = (*env)->NewLongArray(env, SIL_ACH_WORD_COUNT);
            if (play_games_exception(env) || !values) goto cleanup;
            (*env)->SetLongArrayRegion(env, values, 0, SIL_ACH_WORD_COUNT, words);
            if (play_games_exception(env)) goto cleanup;
            (*env)->CallVoidMethod(env, activity, method, values);
        }
        else if (signature[2] == 'Z')
            result = (*env)->CallBooleanMethod(env, activity, method) == JNI_TRUE;
        else
            result = (*env)->CallLongMethod(env, activity, method, (jint)word);
        if (play_games_exception(env)) result = 0;
    }
cleanup:
    if (values) (*env)->DeleteLocalRef(env, values);
    if (activity_class) (*env)->DeleteLocalRef(env, activity_class);
    if (activity) (*env)->DeleteLocalRef(env, activity);
    return result;
}
#endif

void sil_play_games_sync(const uint64_t* unlocked_masks)
{
#if defined(__ANDROID__) && defined(USE_SDL)
    play_games_call("syncPlayGamesAchievements", "([J)V", 0, unlocked_masks);
#else
    (void)unlocked_masks;
#endif
}

uint64_t sil_play_games_unlocked(unsigned word)
{
    if (word >= SIL_ACH_WORD_COUNT) return 0;
#if defined(__ANDROID__) && defined(USE_SDL)
    return play_games_call("getPlayGamesAchievementMask", "(I)J", word, NULL)
        & sil_achievement_word_mask(word);
#else
    return 0;
#endif
}

bool sil_play_games_show(void)
{
#if defined(__ANDROID__) && defined(USE_SDL)
    return play_games_call("showPlayGamesAchievements", "()Z", 0, NULL) != 0;
#else
    return false;
#endif
}

bool sil_play_games_available(void)
{
#if defined(__ANDROID__) && defined(USE_SDL)
    return play_games_call("isPlayGamesAchievementsAvailable", "()Z", 0, NULL) != 0;
#else
    return false;
#endif
}
