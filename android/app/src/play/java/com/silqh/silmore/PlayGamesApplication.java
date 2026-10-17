package com.silqh.silmore;

import android.app.Application;
import android.util.Log;
import com.google.android.gms.games.PlayGamesSdk;
import com.silmore.myapp.R;

/** Unconfigured Play builds remain fully usable without starting the SDK. */
public final class PlayGamesApplication extends Application {
    static volatile boolean initialized;

    @Override public void onCreate() {
        super.onCreate();
        if (!getResources().getBoolean(R.bool.play_games_configured)) return;
        try {
            PlayGamesSdk.initialize(this);
            initialized = true;
        } catch (RuntimeException error) {
            Log.w("SilMoreAchievements", "Play Games initialization failed", error);
        }
    }
}
