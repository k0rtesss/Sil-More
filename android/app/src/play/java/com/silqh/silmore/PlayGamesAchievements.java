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
    private final long allMask;
    private final long availableMask;
    // The game thread only reads this snapshot; no Java callback mutates game state.
    private volatile long remoteMask;
    private long localMask;
    private long confirmedMask;
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
            activity.getString(R.string.play_ach_the_long_tale),
            activity.getString(R.string.play_ach_no_rings_attached),
            activity.getString(R.string.play_ach_cabed_en_aras),
            activity.getString(R.string.play_ach_oft_evil_will),
            activity.getString(R.string.play_ach_the_hasty_stroke),
            activity.getString(R.string.play_ach_a_few_leaves),
            activity.getString(R.string.play_ach_two_weapons),
            activity.getString(R.string.play_ach_for_external_use_only),
            activity.getString(R.string.play_ach_fly_you_fool),
            activity.getString(R.string.play_ach_knock_with_your_head),
            activity.getString(R.string.play_ach_down_to_goblin_town),
            activity.getString(R.string.play_ach_athelas_out_of_stock),
            activity.getString(R.string.play_ach_angband_maintenance),
            activity.getString(R.string.play_ach_the_road_goes_ever_on),
            activity.getString(R.string.play_ach_something_tookish),
            activity.getString(R.string.play_ach_flies_and_spiders),
            activity.getString(R.string.play_ach_not_even_carcharoth),
            activity.getString(R.string.play_ach_the_fords_of_sirion),
            activity.getString(R.string.play_ach_helcaraxe),
            activity.getString(R.string.play_ach_fire_and_water),
            activity.getString(R.string.play_ach_the_fires_of_thangorodrim),
            activity.getString(R.string.play_ach_through_nan_dungortheb),
            activity.getString(R.string.play_ach_a_journey_in_the_dark),
            activity.getString(R.string.play_ach_a_map_of_the_iron_hells),
            activity.getString(R.string.play_ach_a_elbereth_gilthoniel),
            activity.getString(R.string.play_ach_release_from_bondage),
            activity.getString(R.string.play_ach_two_themes_one_voice),
            activity.getString(R.string.play_ach_you_cannot_pass),
            activity.getString(R.string.play_ach_felagunds_answer),
            activity.getString(R.string.play_ach_a_will_of_your_own),
            activity.getString(R.string.play_ach_hurins_defiance),
            activity.getString(R.string.play_ach_no_longer_binding),
            activity.getString(R.string.play_ach_a_fools_hope),
            activity.getString(R.string.play_ach_a_single_spark),
            activity.getString(R.string.play_ach_on_the_other_hand),
            activity.getString(R.string.play_ach_iron_cleaver),
            activity.getString(R.string.play_ach_out_of_thin_ore),
            activity.getString(R.string.play_ach_clad_in_the_night),
            activity.getString(R.string.play_ach_a_dream_upon_the_dark_lord),
            activity.getString(R.string.play_ach_neither_voice_diminished),
            activity.getString(R.string.play_ach_doom_can_wait)
        };
        if (ids.length == 0 || ids.length >= Long.SIZE) {
            throw new IllegalStateException("Achievement catalogue exceeds the JNI bitmask");
        }
        allMask = (1L << ids.length) - 1;
        long mapped = 0;
        for (int i = 0; i < ids.length; i++) {
            if (!ids[i].isEmpty()) mapped |= 1L << i;
        }
        availableMask = mapped;
    }

    boolean isConfigured() { return PlayGamesApplication.initialized; }
    long unlockedMask() { return remoteMask; }

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

    void sync(long unlockedMask) {
        long newBits = (unlockedMask & allMask) & ~localMask;
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
                                if (!ids[i].isEmpty() && ids[i].equals(achievement.getAchievementId())) {
                                    confirmedMask |= 1L << i;
                                    remoteMask |= 1L << i;
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
        long pending = localMask & ~confirmedMask & availableMask;
        if (pending == 0) {
            if (showRequested) openUi(token, id);
            else succeeded(AUTH_POLL_MS);
            return;
        }
        final long bit = Long.lowestOneBit(pending);
        final int index = Long.numberOfTrailingZeros(bit);
        try {
            // unlockImmediate reveals hidden achievements only when earned. Never call reveal.
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
