package com.silqh.silmore;

/** Sideload builds have no Google dependency or cloud mutations. */
final class PlayGamesCloudSaves {
    PlayGamesCloudSaves(SilMoreActivity activity) {}
    boolean isConfigured() { return false; }
    boolean prepare(String path) { return true; }
    void checkpoint(String path) {}
    void onResume() {}
    void onPause() {}
    void onDestroy() {}
}
