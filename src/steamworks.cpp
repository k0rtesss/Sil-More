#include "steamworks.h"

#include <cstdlib>
#include <steam/steam_api.h>
#include <steam/steam_api_flat.h>

extern "C" {
#include "log/log.h"
}

static bool steamworks_active = false;

static void steamworks_shutdown(void)
{
    if (!steamworks_active)
        return;

    sil_steamworks_achievements_shutdown();
    SteamAPI_Shutdown();
    steamworks_active = false;
    log_info("Steamworks shut down");
}

bool sil_steamworks_init(void)
{
    if (steamworks_active)
        return true;

    if (SteamAPI_RestartAppIfNecessary(SIL_STEAM_APP_ID))
    {
        log_info("Steam is relaunching App ID %u", (unsigned)SIL_STEAM_APP_ID);
        return false;
    }

    SteamErrMsg error = {};
    ESteamAPIInitResult result = SteamAPI_InitEx(&error);
    if (result != k_ESteamAPIInitResult_OK)
    {
        log_warn("Steamworks initialization failed (%d): %s; continuing without Steam features",
            (int)result, error);
        return true;
    }

    steamworks_active = true;
    if (std::atexit(steamworks_shutdown) != 0)
    {
        log_warn("Could not register Steamworks shutdown; disabling Steam features");
        steamworks_shutdown();
        return true;
    }
    log_info("Steamworks initialized for App ID %u", (unsigned)SIL_STEAM_APP_ID);
    return true;
}

void sil_steamworks_run_callbacks(void)
{
    if (steamworks_active)
        SteamAPI_RunCallbacks();
}

int sil_steamworks_callback_timeout_ms(void)
{
    /* Keep callbacks flowing while a turn-based menu waits for input. */
    return steamworks_active ? 100 : -1;
}

bool sil_steamworks_is_steam_deck(void)
{
    if (!steamworks_active)
        return false;

    ISteamUtils* utils = SteamAPI_SteamUtils_v011();
    return utils && SteamAPI_ISteamUtils_IsRunningOnSteamHardware(utils)
        == k_ESteamHardwareTypeSteamDeck;
}

bool sil_steamworks_active(void)
{
    return steamworks_active;
}
