package com.silqh.silmore;

/** Sideload installations have local achievements without a Play SDK dependency. */
final class PlayGamesAchievements {
    PlayGamesAchievements(SilMoreActivity activity) { }
    boolean isConfigured() { return false; }
    int unlockedMask() { return 0; }
    void onResume() { }
    void onPause() { }
    void sync(int unlockedMask) { }
    void show() { }
}
