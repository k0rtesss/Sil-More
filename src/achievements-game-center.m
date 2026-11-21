#include "achievements-game-center.h"
#include "achievements-game-center-state.h"
#include "log/log.h"
#include <SDL3/SDL.h>
#import <GameKit/GameKit.h>
#import <UIKit/UIKit.h>

static const char* const achievement_keys[SIL_ACH_COUNT] = {
#define SIL_ACHIEVEMENT_ORDER(key, category) [SIL_ACH_##key] = #key,
#include "achievement-order.h"
#undef SIL_ACHIEVEMENT_ORDER
};

/* All GameKit, UIKit and reconciliation state is owned by the main queue.
 * Only this small snapshot crosses to SDL's event/engine thread. */
static sil_game_center_state state;
static SDL_SpinLock snapshot_lock;
static sil_game_center_masks remote_snapshot;
static bool authenticated_snapshot;
static NSString* account;
static UIViewController* authentication_controller;
static GKGameCenterViewController* dashboard;

static void pump(void);

static UIViewController* root_controller(void)
{
    int count = 0;
    SDL_Window** windows = SDL_GetWindows(&count);
    UIViewController* root = nil;
    for (int i = 0; windows && i < count && !root; ++i)
    {
        UIWindow* window = (__bridge UIWindow*)SDL_GetPointerProperty(
            SDL_GetWindowProperties(windows[i]), SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, NULL);
        if (!window.hidden) root = window.rootViewController;
    }
    SDL_free(windows);
    return root;
}

static NSString* identifier_for(int id)
{
    return [NSString stringWithFormat:@"%s%s", SIL_GAME_CENTER_ID_PREFIX, achievement_keys[id]];
}

static int index_for(NSString* identifier)
{
    for (int id = 0; id < SIL_ACH_COUNT; ++id)
        if ([identifier isEqualToString:identifier_for(id)]) return id;
    return -1;
}

static void publish_snapshot(void)
{
    SDL_LockSpinlock(&snapshot_lock);
    remote_snapshot = state.remote;
    authenticated_snapshot = account.length != 0;
    SDL_UnlockSpinlock(&snapshot_lock);
}

static bool current_request(uint64_t serial)
{
    GKLocalPlayer* player = GKLocalPlayer.localPlayer;
    return state.serial == serial && player.isAuthenticated
        && [account isEqualToString:player.gamePlayerID];
}

static void reconcile_account(void)
{
    GKLocalPlayer* player = GKLocalPlayer.localPlayer;
    NSString* current = player.isAuthenticated ? player.gamePlayerID : nil;
    if ((account || current) && ![account isEqualToString:current])
    {
        sil_gc_reset(&state); /* Invalidates replies from the previous account. */
        account = [current copy];
        publish_snapshot();
    }
}

static void present_authentication(void)
{
    UIViewController* root = root_controller();
    if (authentication_controller && root && !root.presentedViewController
        && root.view.window)
    {
        [root presentViewController:authentication_controller animated:YES completion:nil];
        authentication_controller = nil;
    }
}

static void load_failed(uint64_t serial, NSError* error)
{
    if (!current_request(serial)) return;
    sil_gc_finish_load(&state, serial, NULL, NULL, false, SDL_GetTicks());
    log_warn("Game Center achievement download failed: %s; will retry",
        error.localizedDescription.UTF8String);
}

static void load_achievements(uint64_t serial)
{
    [GKAchievementDescription loadAchievementDescriptionsWithCompletionHandler:
        ^(NSArray<GKAchievementDescription*>* descriptions, NSError* error) {
        dispatch_async(dispatch_get_main_queue(), ^{
            if (!current_request(serial)) return;
            if (error) { load_failed(serial, error); return; }
            sil_game_center_masks available = {0};
            for (GKAchievementDescription* description in descriptions)
            {
                int id = index_for(description.identifier);
                if (id >= 0) available.words[id / 64] |= UINT64_C(1) << (id % 64);
            }
            /* Unregistered/draft entries never enter the report batch. */
            [GKAchievement loadAchievementsWithCompletionHandler:
                ^(NSArray<GKAchievement*>* achievements, NSError* load_error) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    if (!current_request(serial)) return;
                    if (load_error) { load_failed(serial, load_error); return; }
                    sil_game_center_masks remote = {0};
                    for (GKAchievement* achievement in achievements)
                    {
                        int id = index_for(achievement.identifier);
                        if (id >= 0 && achievement.percentComplete >= 100.0)
                            remote.words[id / 64] |= UINT64_C(1) << (id % 64);
                    }
                    sil_gc_finish_load(&state, serial, &available, &remote, true, SDL_GetTicks());
                    publish_snapshot();
                    pump();
                });
            }];
        });
    }];
}

static void pump(void)
{
    reconcile_account();
    present_authentication();
    if (!account.length) return;
    uint64_t now = SDL_GetTicks();
    if (sil_gc_begin_load(&state, now))
    {
        load_achievements(state.serial);
        return;
    }
    if (!sil_gc_begin_report(&state, now)) return;
    uint64_t serial = state.serial;
    NSMutableArray<GKAchievement*>* batch = [NSMutableArray array];
    for (int id = 0; id < SIL_ACH_COUNT; ++id)
    {
        if (!(state.pending.words[id / 64] & (UINT64_C(1) << (id % 64)))) continue;
        GKAchievement* achievement = [[GKAchievement alloc] initWithIdentifier:identifier_for(id)];
        achievement.percentComplete = 100.0;
        /* The game already displays its own unlock notifications. */
        achievement.showsCompletionBanner = NO;
        [batch addObject:achievement];
    }
    [GKAchievement reportAchievements:batch withCompletionHandler:^(NSError* error) {
        dispatch_async(dispatch_get_main_queue(), ^{
            if (!current_request(serial)) return;
            sil_gc_finish_report(&state, serial, error == nil, SDL_GetTicks());
            if (error) log_warn("Game Center achievement upload failed: %s; will retry",
                error.localizedDescription.UTF8String);
            publish_snapshot();
        });
    }];
}

@interface SilGameCenterDelegate : NSObject <GKGameCenterControllerDelegate>
@end
@implementation SilGameCenterDelegate
- (void)gameCenterViewControllerDidFinish:(GKGameCenterViewController*)controller
{
    [controller dismissViewControllerAnimated:YES completion:nil];
    dashboard = nil;
    state.refresh_at = 0;
    pump();
}
@end
static SilGameCenterDelegate* dashboard_delegate;

void sil_game_center_init(void)
{
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        dispatch_async(dispatch_get_main_queue(), ^{
            sil_gc_reset(&state);
            dashboard_delegate = [[SilGameCenterDelegate alloc] init];
            [NSNotificationCenter.defaultCenter
                addObserverForName:GKPlayerAuthenticationDidChangeNotificationName
                object:nil queue:NSOperationQueue.mainQueue
                usingBlock:^(NSNotification* notification) {
                    (void)notification;
                    pump();
                }];
            GKLocalPlayer.localPlayer.authenticateHandler =
                ^(UIViewController* controller, NSError* error) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    authentication_controller = controller;
                    if (error) log_info("Game Center sign-in unavailable: %s",
                        error.localizedDescription.UTF8String);
                    pump();
                });
            };
        });
    });
}

void sil_game_center_sync(const uint64_t* unlocked_masks)
{
    if (!unlocked_masks) return;
    sil_game_center_masks masks;
    for (unsigned word = 0; word < SIL_ACH_WORD_COUNT; ++word)
        masks.words[word] = unlocked_masks[word] & sil_gc_achievement_word_mask(word);
    dispatch_async(dispatch_get_main_queue(), ^{
        reconcile_account();
        sil_gc_want(&state, &masks);
        pump();
    });
}

uint64_t sil_game_center_unlocked(unsigned word)
{
    if (word >= SIL_ACH_WORD_COUNT) return 0;
    SDL_LockSpinlock(&snapshot_lock);
    uint64_t result = remote_snapshot.words[word];
    SDL_UnlockSpinlock(&snapshot_lock);
    return result & sil_gc_achievement_word_mask(word);
}

bool sil_game_center_available(void)
{
    SDL_LockSpinlock(&snapshot_lock);
    bool result = authenticated_snapshot;
    SDL_UnlockSpinlock(&snapshot_lock);
    return result;
}

bool sil_game_center_show(void)
{
    if (!sil_game_center_available()) return false;
    dispatch_async(dispatch_get_main_queue(), ^{
        reconcile_account();
        UIViewController* root = root_controller();
        if (!account.length || !root || !root.view.window || root.presentedViewController) return;
        dashboard = [[GKGameCenterViewController alloc]
            initWithState:GKGameCenterViewControllerStateAchievements];
        dashboard.gameCenterDelegate = dashboard_delegate;
        [root presentViewController:dashboard animated:YES completion:nil];
    });
    return true;
}
