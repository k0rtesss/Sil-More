#include "achievements.h"
#include "steamworks.h"
#include <chrono>
#include <array>
#include <steam/steam_api.h>
#include <steam/steam_api_flat.h>

extern "C" {
#include "log/log.h"
}

namespace {
using Clock = std::chrono::steady_clock;
using Masks = std::array<uint64_t, SIL_ACH_WORD_COUNT>;

static bool any(const Masks& masks)
{
    for (uint64_t word : masks) if (word) return true;
    return false;
}

/* GetAchievement reads the client's cache, including uncommitted SetAchievement
 * calls. Track those separately until StoreStats is acknowledged. The local
 * profile remains the durable retry queue across process restarts. */
class AchievementSync {
public:
    CCallbackManual<AchievementSync, UserStatsStored_t> stored_callback;
    Masks remote{}, confirmed{}, staged{}, pending{}, wanted{};
    uint64 account = 0;
    bool registered = false;
    Clock::time_point next_attempt = Clock::time_point::min();

    void on_stored(UserStatsStored_t *result)
    {
        if (result->m_nGameID != SIL_STEAM_APP_ID || !any(pending)) return;
        if (result->m_eResult == k_EResultOK)
        {
            for (unsigned word = 0; word < SIL_ACH_WORD_COUNT; ++word)
            {
                confirmed[word] |= pending[word];
                remote[word] |= pending[word];
                staged[word] &= ~pending[word];
            }
        }
        else
            log_warn("Steam achievement upload failed (%d); will retry", (int)result->m_eResult);
        pending.fill(0);
        /* Failure and API throttling must not cause a retry on every frame. */
        next_attempt = Clock::now() + std::chrono::seconds(60);
    }

    void sync(const uint64_t* masks)
    {
        if (!sil_steamworks_active()) return;
        ISteamUser *user = SteamAPI_SteamUser_v023();
        uint64 current = user ? SteamAPI_ISteamUser_GetSteamID(user) : 0;
        if (!current) return;
        if (!registered)
        {
            stored_callback.Register(this, &AchievementSync::on_stored);
            registered = true;
        }
        if (account != current)
        {
            remote = confirmed = staged = pending = wanted = Masks{};
            account = current;
            next_attempt = Clock::time_point::min();
        }
        auto now = Clock::now();
        /* A fresh unlock can be uploaded immediately unless a request is active. */
        for (unsigned word = 0; word < SIL_ACH_WORD_COUNT; ++word)
        {
            uint64_t mask = masks[word] & sil_achievement_word_mask(word);
            if ((mask & ~wanted[word]) && !any(pending)) next_attempt = Clock::time_point::min();
            wanted[word] |= mask;
        }
        if (now < next_attempt) return;
        pending.fill(0); /* A lost callback is retried after the bounded timeout. */
        next_attempt = now + std::chrono::seconds(60);
        ISteamUserStats *stats = SteamAPI_SteamUserStats_v013();
        if (!stats) return;
        for (int i = 0; i < SIL_ACH_COUNT; ++i)
        {
            unsigned word = i / 64;
            uint64_t bit = UINT64_C(1) << (i % 64);
            bool achieved = false;
            const char *key = sil_achievement_get(i)->key;
            /* Modern SDKs preload the local user's stats before launch. If
             * they aren't ready (or a schema isn't published), try again later. */
            if (!SteamAPI_ISteamUserStats_GetAchievement(stats, key, &achieved)) continue;
            if (achieved)
            {
                remote[word] |= bit;
                if (!(staged[word] & bit)) confirmed[word] |= bit;
            }
            if ((wanted[word] & bit) && !(confirmed[word] & bit))
            {
                if (SteamAPI_ISteamUserStats_SetAchievement(stats, key)) staged[word] |= bit;
                else log_warn("Steam could not unlock '%s'; will retry", key);
            }
        }
        if (any(staged) && SteamAPI_ISteamUserStats_StoreStats(stats)) pending = staged;
    }

    void shutdown()
    {
        if (registered) stored_callback.Unregister();
        registered = false;
        remote = confirmed = staged = pending = wanted = Masks{};
        account = 0;
    }
};

AchievementSync achievements;
}

void sil_steamworks_sync_achievements(const uint64_t* unlocked_masks)
{
    achievements.sync(unlocked_masks);
}

uint64_t sil_steamworks_achievements_unlocked(unsigned word)
{
    return word < SIL_ACH_WORD_COUNT && sil_steamworks_active() ? achievements.remote[word] : 0;
}

bool sil_steamworks_show_achievements(void)
{
    if (!sil_steamworks_active()) return false;
    ISteamUtils *utils = SteamAPI_SteamUtils_v011();
    ISteamFriends *friends = SteamAPI_SteamFriends_v018();
    if (!utils || !friends || !SteamAPI_ISteamUtils_IsOverlayEnabled(utils)) return false;
    SteamAPI_ISteamFriends_ActivateGameOverlay(friends, "achievements");
    return true;
}

void sil_steamworks_achievements_shutdown(void)
{
    achievements.shutdown();
}
