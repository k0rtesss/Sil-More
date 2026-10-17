#include "achievements.h"
#include "steamworks.h"
#include <chrono>
#include <steam/steam_api.h>
#include <steam/steam_api_flat.h>

extern "C" {
#include "log/log.h"
}

namespace {
using Clock = std::chrono::steady_clock;

/* GetAchievement reads the client's cache, including uncommitted SetAchievement
 * calls. Track those separately until StoreStats is acknowledged. The local
 * profile remains the durable retry queue across process restarts. */
class AchievementSync {
public:
    CCallbackManual<AchievementSync, UserStatsStored_t> stored_callback;
    uint64_t remote = 0;
    uint64_t confirmed = 0;
    uint64_t staged = 0;
    uint64_t pending = 0;
    uint64_t wanted = 0;
    uint64 account = 0;
    bool registered = false;
    Clock::time_point next_attempt = Clock::time_point::min();

    void on_stored(UserStatsStored_t *result)
    {
        if (result->m_nGameID != SIL_STEAM_APP_ID || !pending) return;
        if (result->m_eResult == k_EResultOK)
        {
            confirmed |= pending;
            staged &= ~pending;
        }
        else
            log_warn("Steam achievement upload failed (%d); will retry", (int)result->m_eResult);
        pending = 0;
        /* Failure and API throttling must not cause a retry on every frame. */
        next_attempt = Clock::now() + std::chrono::seconds(60);
    }

    void sync(uint64_t mask)
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
            remote = confirmed = staged = pending = wanted = 0;
            account = current;
            next_attempt = Clock::time_point::min();
        }
        auto now = Clock::now();
        /* A fresh unlock can be uploaded immediately unless a request is active. */
        if ((mask & ~wanted) && !pending) next_attempt = Clock::time_point::min();
        wanted |= mask & SIL_ACH_ALL_MASK;
        if (now < next_attempt) return;
        pending = 0; /* A lost callback is retried after the bounded timeout. */
        next_attempt = now + std::chrono::seconds(60);
        ISteamUserStats *stats = SteamAPI_SteamUserStats_v013();
        if (!stats) return;
        for (int i = 0; i < SIL_ACH_COUNT; ++i)
        {
            uint64_t bit = UINT64_C(1) << i;
            bool achieved = false;
            const char *key = sil_achievement_get(i)->key;
            /* Modern SDKs preload the local user's stats before launch. If
             * they aren't ready (or a schema isn't published), try again later. */
            if (!SteamAPI_ISteamUserStats_GetAchievement(stats, key, &achieved)) continue;
            if (achieved)
            {
                remote |= bit;
                if (!(staged & bit)) confirmed |= bit;
            }
            if ((wanted & bit) && !(confirmed & bit))
            {
                if (SteamAPI_ISteamUserStats_SetAchievement(stats, key)) staged |= bit;
                else log_warn("Steam could not unlock '%s'; will retry", key);
            }
        }
        if (staged && SteamAPI_ISteamUserStats_StoreStats(stats)) pending = staged;
    }

    void shutdown()
    {
        if (registered) stored_callback.Unregister();
        registered = false;
        remote = confirmed = staged = pending = wanted = 0;
        account = 0;
    }
};

AchievementSync achievements;
}

void sil_steamworks_sync_achievements(uint64_t unlocked_mask)
{
    achievements.sync(unlocked_mask);
}

uint64_t sil_steamworks_achievements_unlocked(void)
{
    return sil_steamworks_active() ? achievements.remote : 0;
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
