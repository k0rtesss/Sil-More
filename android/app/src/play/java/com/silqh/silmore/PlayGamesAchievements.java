package com.silqh.silmore;

import android.os.SystemClock;
import android.util.Log;
import android.widget.Toast;
import com.google.android.gms.games.PlayGames;
import com.google.android.gms.games.achievement.Achievement;
import com.google.android.gms.games.achievement.AchievementBuffer;
import com.silmore.myapp.R;

/** All mutable synchronization state belongs to Android's UI thread. */
final class PlayGamesAchievements {
    private static final String TAG = "SilMoreAchievements";
    private static final long AUTH_POLL_MS = 30000;
    private static final long REMOTE_POLL_MS = 300000;
    private static final long TASK_TIMEOUT_MS = 30000;
    private final SilMoreActivity activity;
    private final String[] ids;
    private final int allMask;
    // The game thread only reads this snapshot; no Java callback mutates game state.
    private volatile int remoteMask;
    private int localMask;
    private int confirmedMask;
    private int generation;
    private String playerId;
    private boolean resumed;
    private boolean busy;
    private boolean loaded;
    private boolean showRequested;
    private boolean allowSignIn;
    private long operationStarted;
    private long nextAttempt;
    private long nextRemoteLoad;
    private long retryDelay = 5000;

    PlayGamesAchievements(SilMoreActivity activity) {
        this.activity = activity;
        ids = new String[] {
            activity.getString(R.string.play_ach_into_the_deep),
            activity.getString(R.string.play_ach_light_in_darkness),
            activity.getString(R.string.play_ach_back_to_starlight),
            activity.getString(R.string.play_ach_three_lights_in_exile),
            activity.getString(R.string.play_ach_the_shadow_falls),
            activity.getString(R.string.play_ach_the_quiet_road),
            activity.getString(R.string.play_ach_word_unbroken),
            activity.getString(R.string.play_ach_forged_by_your_hand),
            activity.getString(R.string.play_ach_a_name_worth_remembering),
            activity.getString(R.string.play_ach_song_beneath_the_mountain),
            activity.getString(R.string.play_ach_there_and_back_again),
            activity.getString(R.string.play_ach_by_a_thread),
            activity.getString(R.string.play_ach_light_of_the_eldar),
            activity.getString(R.string.play_ach_the_long_tale)
        };
        if (ids.length == 0 || ids.length >= Integer.SIZE) {
            throw new IllegalStateException("Achievement catalogue exceeds the JNI bitmask");
        }
        allMask = (1 << ids.length) - 1;
    }

    boolean isConfigured() { return PlayGamesApplication.initialized; }
    int unlockedMask() { return remoteMask; }

    void onResume() {
        resumed = true;
        generation++;
        busy = false;
        nextAttempt = 0;
    }

    void onPause() {
        resumed = false;
        generation++;
        busy = false;
        // An explicit sign-in can pause this Activity. Resume its UI request,
        // but never launch the sign-in prompt repeatedly if the user cancelled.
        allowSignIn = false;
        resetAccount(null);
    }

    void sync(int unlockedMask) {
        int newBits = (unlockedMask & allMask) & ~localMask;
        localMask |= unlockedMask & allMask;
        if (newBits != 0 && retryDelay == 5000) nextAttempt = 0;
        pump();
    }

    void show() {
        if (!isConfigured()) return;
        showRequested = true;
        allowSignIn = true;
        nextAttempt = 0;
        pump();
    }

    private void resetAccount(String id) {
        playerId = id;
        confirmedMask = 0;
        remoteMask = 0;
        loaded = false;
        nextRemoteLoad = 0;
    }

    private boolean current(int token, String id) {
        return resumed && generation == token &&
                (id == null || id.equals(playerId));
    }

    private void pump() {
        if (!resumed || !isConfigured()) return;
        long now = SystemClock.elapsedRealtime();
        if (busy && now - operationStarted >= TASK_TIMEOUT_MS) {
            // Late callbacks must never update a later authentication session.
            generation++;
            failed("Play Games request timed out", null);
        }
        if (busy || now < nextAttempt) return;
        busy = true;
        operationStarted = now;
        final int token = generation;
        try {
            // This check does not open sign-in UI. Only show() permits signIn().
            PlayGames.getGamesSignInClient(activity).isAuthenticated()
                .addOnCompleteListener(task -> {
                    if (!current(token, null)) return;
                    if (!task.isSuccessful() || !task.getResult().isAuthenticated()) {
                        resetAccount(null);
                        if (showRequested && allowSignIn) signIn(token);
                        else failed("Play Games is not authenticated", task.getException());
                        return;
                    }
                    identifyPlayer(token);
                });
        } catch (RuntimeException error) {
            failed("Cannot check Play Games authentication", error);
        }
    }

    private void signIn(int token) {
        allowSignIn = false;
        try {
            PlayGames.getGamesSignInClient(activity).signIn()
                .addOnCompleteListener(task -> {
                    if (!current(token, null)) return;
                    if (!task.isSuccessful() || !task.getResult().isAuthenticated()) {
                        failed("Play Games sign-in did not complete", task.getException());
                        return;
                    }
                    identifyPlayer(token);
                });
        } catch (RuntimeException error) {
            failed("Cannot open Play Games sign-in", error);
        }
    }

    private void identifyPlayer(int token) {
        try {
            PlayGames.getPlayersClient(activity).getCurrentPlayer()
                .addOnCompleteListener(task -> {
                    if (!current(token, null)) return;
                    if (!task.isSuccessful() || task.getResult() == null) {
                        resetAccount(null);
                        failed("Cannot identify Play Games player", task.getException());
                        return;
                    }
                    String id = task.getResult().getPlayerId();
                    if (id == null || id.isEmpty()) {
                        resetAccount(null);
                        failed("Play Games returned an empty player ID", null);
                        return;
                    }
                    if (!id.equals(playerId)) resetAccount(id);
                    if (!loaded || showRequested ||
                            SystemClock.elapsedRealtime() >= nextRemoteLoad) load(token, id);
                    else upload(token, id);
                });
        } catch (RuntimeException error) {
            failed("Cannot request Play Games player", error);
        }
    }

    private void load(int token, String id) {
        try {
            PlayGames.getAchievementsClient(activity).load(showRequested)
                .addOnCompleteListener(task -> {
                    AchievementBuffer buffer = task.isSuccessful() && task.getResult() != null
                            ? task.getResult().get() : null;
                    try {
                        if (!current(token, id)) return;
                        if (buffer == null) {
                            failed("Cannot load Play Games achievements", task.getException());
                            return;
                        }
                        for (Achievement achievement : buffer) {
                            if (achievement.getState() != Achievement.STATE_UNLOCKED) continue;
                            for (int i = 0; i < ids.length; i++) {
                                if (ids[i].equals(achievement.getAchievementId())) {
                                    confirmedMask |= 1 << i;
                                    remoteMask |= 1 << i;
                                }
                            }
                        }
                        loaded = true;
                        nextRemoteLoad = SystemClock.elapsedRealtime() + REMOTE_POLL_MS;
                    } finally {
                        if (buffer != null) buffer.release();
                    }
                    if (current(token, id)) upload(token, id);
                });
        } catch (RuntimeException error) {
            failed("Cannot request Play Games achievements", error);
        }
    }

    private void upload(int token, String id) {
        int pending = localMask & ~confirmedMask & allMask;
        if (pending == 0) {
            if (showRequested) openUi(token, id);
            else succeeded(AUTH_POLL_MS);
            return;
        }
        final int bit = Integer.lowestOneBit(pending);
        final int index = Integer.numberOfTrailingZeros(bit);
        try {
            // Only server acknowledgement clears pending work. The durable native
            // local mask is resent after offline errors, process restarts or account changes.
            PlayGames.getAchievementsClient(activity).unlockImmediate(ids[index])
                .addOnCompleteListener(task -> {
                    if (!current(token, id)) return;
                    if (!task.isSuccessful()) {
                        failed("Cannot unlock Play Games achievement", task.getException());
                        return;
                    }
                    confirmedMask |= bit;
                    remoteMask |= bit;
                    if (showRequested) upload(token, id);
                    else succeeded(0);
                });
        } catch (RuntimeException error) {
            failed("Cannot submit Play Games achievement", error);
        }
    }

    private void openUi(int token, String id) {
        try {
            PlayGames.getAchievementsClient(activity).getAchievementsIntent()
                .addOnCompleteListener(task -> {
                    if (!current(token, id)) return;
                    if (!task.isSuccessful()) {
                        failed("Cannot display Play Games achievements", task.getException());
                        return;
                    }
                    showRequested = false;
                    allowSignIn = false;
                    try {
                        activity.startActivityForResult(task.getResult(), 9003);
                        succeeded(AUTH_POLL_MS);
                    } catch (RuntimeException error) {
                        showRequested = true;
                        failed("Cannot launch Play Games achievements", error);
                    }
                });
        } catch (RuntimeException error) {
            failed("Cannot request Play Games achievements UI", error);
        }
    }

    private void succeeded(long delay) {
        busy = false;
        retryDelay = 5000;
        nextAttempt = SystemClock.elapsedRealtime() + delay;
    }

    private void failed(String message, Exception error) {
        Log.w(TAG, message, error);
        busy = false;
        nextAttempt = SystemClock.elapsedRealtime() + retryDelay;
        retryDelay = Math.min(300000, retryDelay * 2);
        if (showRequested) {
            Toast.makeText(activity,
                "Google Play achievements are unavailable. Please try again later.",
                Toast.LENGTH_LONG).show();
            showRequested = false;
            allowSignIn = false;
        }
    }
}
