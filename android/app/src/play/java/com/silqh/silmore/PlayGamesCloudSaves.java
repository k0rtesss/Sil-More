package com.silqh.silmore;

import android.app.AlertDialog;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.Log;
import android.widget.Toast;
import com.google.android.gms.games.PlayGames;
import com.google.android.gms.games.SnapshotsClient;
import com.google.android.gms.games.snapshot.Snapshot;
import com.google.android.gms.games.snapshot.SnapshotMetadataChange;
import java.io.File;
import java.io.IOException;
import java.text.DateFormat;
import java.util.Date;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** Provider state belongs to the UI thread; only immutable checkpoints cross threads. */
final class PlayGamesCloudSaves {
    private static final String TAG = "SilMoreCloud";
    private static final String NAME = "sil_more_progress_v1";
    private static final long TIMEOUT_MS = 15000;
    private static final long RETRY_MS = 30000;
    private static final long POLL_MS = 300000;
    private enum Mode { STARTUP, AUTOMATIC }
    private final SilMoreActivity activity;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private Request current;
    private Request blocking;
    private Checkpoint latest;
    private boolean resumed;
    private boolean needsAttention;
    private String attentionMessage;
    private volatile boolean destroyed;
    private volatile boolean restoring;
    private long nextAttempt;
    private final Runnable retry = this::pump;

    private static final class Checkpoint {
        final File root;
        final byte[] bytes;
        final CloudSaveArchive.Profile profile;
        Checkpoint(String path) throws IOException {
            root = new File(path);
            bytes = CloudSaveArchive.capture(root);
            profile = CloudSaveArchive.inspect(root, bytes);
        }
    }

    private static final class Request {
        final CountDownLatch finished = new CountDownLatch(1);
        final Mode mode;
        final File root;
        final Checkpoint local;
        final String captureError;
        Request completion = this;
        volatile int result;
        AlertDialog dialog;
        SnapshotsClient client;
        Snapshot snapshot;
        Snapshot firstConflict, secondConflict;
        File candidate;
        String playerId;
        CloudSaveSyncState state;
        CloudSaveArchive.Profile remoteProfile;
        boolean upload;
        boolean recovery;
        int conflicts;
        Runnable timeout;
        Request(Mode mode, String path, Checkpoint local, String error) {
            this.mode = mode; root = new File(path); this.local = local; captureError = error;
        }
    }

    PlayGamesCloudSaves(SilMoreActivity activity) { this.activity = activity; }
    boolean isConfigured() { return PlayGamesApplication.initialized; }

    /** Runs before any native progress is loaded, and never replaces live game state. */
    boolean prepare(String path) {
        File root = new File(path);
        try {
            if (CloudSaveArchive.applyPending(root)) Log.i(TAG, "Cloud progress restored before native initialization");
        } catch (IOException error) { return restoreFailed(error); }
        if (!isConfigured()) return true;
        Checkpoint local = null;
        String captureError = null;
        try { local = new Checkpoint(path); }
        catch (IOException error) { captureError = error.getMessage(); }
        Request request = new Request(Mode.STARTUP, path, local, captureError);
        await(request);
        if (request.result == 1) {
            try {
                CloudSaveArchive.applyPending(root);
                remember(request, request.remoteProfile.fingerprint);
            } catch (IOException error) { return restoreFailed(error); }
            activity.runOnUiThread(() -> { restoring = false; needsAttention = false; });
        }
        return true;
    }

    private boolean restoreFailed(IOException error) {
        Log.e(TAG, "Could not safely apply cloud restore", error);
        activity.runOnUiThread(() -> new AlertDialog.Builder(activity)
                .setTitle("Cloud restore needs attention")
                .setMessage("The game could not safely finish the cloud restore. Your recovery copies are preserved. Please check the app's log before trying again.")
                .setPositiveButton("Close", (dialog, which) -> activity.finish()).show());
        return false;
    }

    /** Capture on SDL's game thread only, after all local writers have finished. */
    void checkpoint(String path) {
        if (!isConfigured() || destroyed || restoring) return;
        final Checkpoint captured;
        try { captured = new Checkpoint(path); }
        catch (IOException error) { Log.w(TAG, "Cloud checkpoint deferred; local progress is safe", error); return; }
        activity.runOnUiThread(() -> {
            if (destroyed || restoring) return;
            latest = captured; // Coalesce a save/death arriving during an older upload.
            nextAttempt = 0;
            pump();
        });
    }

    private void await(Request request) {
        activity.runOnUiThread(() -> {
            if (destroyed || restoring) { request.finished.countDown(); return; }
            if (current != null) blocking = request;
            else start(request);
        });
        try { request.finished.await(); }
        catch (InterruptedException error) {
            Thread.currentThread().interrupt();
            activity.runOnUiThread(() -> {
                if (blocking == request) { blocking = null; request.finished.countDown(); }
                else if (current != null && current.completion == request) finish(current, 0);
            });
        }
    }

    void onResume() {
        resumed = true;
        if (attentionMessage != null) {
            Toast.makeText(activity, attentionMessage, Toast.LENGTH_LONG).show();
            attentionMessage = null;
        }
        nextAttempt = 0;
        pump();
    }

    void onPause() { resumed = false; handler.removeCallbacks(retry); }

    void onDestroy() {
        destroyed = true;
        handler.removeCallbacks(retry);
        if (blocking != null) { blocking.finished.countDown(); blocking = null; }
        if (current != null) finish(current, 0);
        worker.shutdown();
    }

    private void pump() {
        handler.removeCallbacks(retry);
        if (destroyed || restoring || needsAttention || latest == null || current != null) return;
        long delay = nextAttempt - SystemClock.elapsedRealtime();
        if (delay > 0) { if (resumed) handler.postDelayed(retry, delay); return; }
        start(new Request(Mode.AUTOMATIC, latest.root.getPath(), latest, null));
    }

    private void start(Request request) {
        current = request;
        if (request.local == null) recovery(request, request.captureError);
        else begin(request, true);
    }

    private void recovery(Request previous, String message) {
        pauseTimeout(previous);
        discard(previous, previous.snapshot);
        discard(previous, previous.firstConflict);
        discard(previous, previous.secondConflict);
        previous.snapshot = previous.firstConflict = previous.secondConflict = null;
        if (previous.candidate != null) { previous.candidate.delete(); previous.candidate = null; }
        dismiss(previous);
        // Retire timed-out callbacks before allowing another explicit action.
        // The startup caller still waits on its original completion latch.
        Request request = new Request(previous.mode, previous.root.getPath(), previous.local, previous.captureError);
        request.completion = previous.completion;
        current = request;
        request.recovery = true;
        AlertDialog.Builder dialog = new AlertDialog.Builder(activity).setTitle("Cloud sync needs attention")
                .setMessage(message + "\n\nYou can continue locally or recover cloud progress. A restore keeps a recovery copy of this device's progress.")
                .setNeutralButton("Restore from cloud", (unused, which) -> begin(request, false))
                .setNegativeButton("Continue locally", (unused, which) -> cancel(request))
                .setOnCancelListener(unused -> cancel(request));
        if (request.local != null && !request.local.profile.empty)
            dialog.setPositiveButton("Back up this device", (unused, which) -> begin(request, true));
        request.dialog = dialog.show();
    }

    private boolean valid(Request request) {
        return current == request && !destroyed && !activity.isFinishing() && !activity.isDestroyed();
    }

    private void guard(Request request, Runnable action) {
        try { action.run(); }
        catch (RuntimeException error) {
            Log.w(TAG, "Play Games cloud request failed", error);
            if (valid(request)) fail(request, "Google Play cloud sync is unavailable. Local progress is safe; please try again.");
        }
    }

    private void discard(Request request, Snapshot snapshot) {
        if (request.client == null || snapshot == null) return;
        try { request.client.discardAndClose(snapshot); }
        catch (RuntimeException error) { Log.w(TAG, "Could not close cloud snapshot", error); }
    }

    private void dismiss(Request request) {
        if (request.dialog != null) request.dialog.dismiss();
        request.dialog = null;
    }

    private void waiting(Request request, String message) {
        dismiss(request);
        if (request.mode != Mode.AUTOMATIC) {
            request.dialog = new AlertDialog.Builder(activity).setTitle("Google Play cloud sync")
                    .setMessage(message).setNegativeButton(request.mode == Mode.STARTUP ? "Play offline" : "Cancel", (dialog, which) -> cancel(request))
                    .setOnCancelListener(dialog -> cancel(request)).show();
        }
        pauseTimeout(request);
        request.timeout = () -> fail(request, "Cloud sync has not finished. Local progress is safe and will be checked again later.");
        handler.postDelayed(request.timeout, TIMEOUT_MS);
    }

    private void begin(Request request, boolean upload) {
        request.upload = upload;
        if (upload && request.local == null) { fail(request, request.captureError); return; }
        waiting(request, "Checking your Play Games cloud progress…");
        authenticate(request);
    }

    private void authenticate(Request request) {
        guard(request, () -> PlayGames.getGamesSignInClient(activity).isAuthenticated().addOnCompleteListener(task -> guard(request, () -> {
            if (!valid(request)) return;
            if (!task.isSuccessful()) { fail(request, "Cannot check Play Games sign-in."); return; }
            if (task.getResult().isAuthenticated()) identify(request);
            else if (request.mode == Mode.STARTUP && !request.recovery) {
                // The SDK's automatic sign-in may still be completing. Never
                // launch another sign-in UI from an automatic checkpoint.
                handler.postDelayed(() -> { if (valid(request)) authenticate(request); }, 500);
            } else if (request.mode == Mode.AUTOMATIC) fail(request, "Play Games is not signed in; cloud sync will retry later.");
            else PlayGames.getGamesSignInClient(activity).signIn().addOnCompleteListener(signIn -> guard(request, () -> {
                if (!valid(request)) return;
                if (!signIn.isSuccessful() || !signIn.getResult().isAuthenticated()) fail(request, "Play Games sign-in was cancelled or failed.");
                else identify(request);
            }));
        })));
    }

    private void identify(Request request) {
        guard(request, () -> PlayGames.getPlayersClient(activity).getCurrentPlayer().addOnCompleteListener(task -> guard(request, () -> {
            if (!valid(request)) return;
            if (!task.isSuccessful() || task.getResult() == null) { fail(request, "Cannot identify the Play Games account."); return; }
            request.playerId = task.getResult().getPlayerId();
            try { request.state = new CloudSaveSyncState(request.root, request.recovery); }
            catch (IOException error) {
                problem(request, error.getMessage());
                return;
            }
            request.client = PlayGames.getSnapshotsClient(activity);
            request.client.open(NAME, request.upload, SnapshotsClient.RESOLUTION_POLICY_MANUAL).addOnCompleteListener(opened -> {
                if (!opened.isSuccessful()) { if (valid(request)) fail(request, "Cannot open the cloud save. Check your connection and try again."); return; }
                opened(request, opened.getResult());
            });
        })));
    }

    private void opened(Request request, SnapshotsClient.DataOrConflict<Snapshot> result) {
        guard(request, () -> handleOpened(request, result));
    }

    private void handleOpened(Request request, SnapshotsClient.DataOrConflict<Snapshot> result) {
        if (result == null) { if (valid(request)) fail(request, "Google returned no cloud save."); return; }
        if (!valid(request)) {
            if (!result.isConflict()) discard(request, result.getData());
            else {
                discard(request, result.getConflict().getSnapshot());
                discard(request, result.getConflict().getConflictingSnapshot());
            }
            return;
        }
        if (result.isConflict()) {
            SnapshotsClient.SnapshotConflict conflict = result.getConflict();
            request.firstConflict = conflict.getSnapshot();
            request.secondConflict = conflict.getConflictingSnapshot();
            if (request.mode == Mode.AUTOMATIC) { attention(request, "Cloud progress needs a choice. It will be reviewed next time the game starts."); return; }
            if (++request.conflicts > 3) { fail(request, "The cloud save keeps changing. Try again after the other device finishes syncing."); return; }
            pauseTimeout(request);
            dismiss(request);
            request.dialog = new AlertDialog.Builder(activity).setTitle("Two cloud versions need a choice")
                    .setMessage("Choose the cloud version to keep before continuing.")
                    .setItems(new String[]{label(request.firstConflict), label(request.secondConflict)}, (dialog, which) -> guard(request, () -> {
                        Snapshot chosen = which == 0 ? request.firstConflict : request.secondConflict;
                        waiting(request, "Resolving the cloud versions…");
                        resolveChoice(request, conflict.getConflictId(), chosen);
                    })).setNegativeButton("Play offline", (dialog, which) -> cancel(request))
                    .setOnCancelListener(dialog -> cancel(request)).show();
            return;
        }
        request.snapshot = result.getData();
        if (request.snapshot == null) { fail(request, "Google returned no cloud save."); return; }
        final Snapshot snapshot = request.snapshot;
        worker.execute(() -> {
            try {
                byte[] remote = snapshot.getSnapshotContents().readFully();
                CloudSaveArchive.Profile inspected = null;
                try {
                    if (remote.length != 0) inspected = CloudSaveArchive.inspect(request.root, remote);
                } catch (IOException error) {
                    // Preserve explicit backup as a way to repair a damaged
                    // cloud copy. Automatic sync and restores never accept it.
                    if (!request.recovery || !request.upload) throw error;
                }
                final CloudSaveArchive.Profile profile = inspected;
                activity.runOnUiThread(() -> guard(request, () -> {
                    if (!valid(request)) return;
                    request.remoteProfile = profile;
                    compare(request, remote);
                }));
            } catch (IOException | RuntimeException error) {
                activity.runOnUiThread(() -> { if (valid(request)) problem(request, "The cloud copy could not be validated. Local progress is unchanged."); });
            }
        });
    }

    private void compare(Request request, byte[] remote) {
        if (!request.recovery) {
            CloudSaveSyncState.Action action = request.state.action(request.playerId, request.local.profile, request.remoteProfile);
            switch (action) {
                case SAME:
                    remember(request, request.local.profile.fingerprint);
                    completed(request);
                    return;
                case UPLOAD: commit(request); return;
                case DOWNLOAD:
                    if (request.mode == Mode.STARTUP) restore(request, remote);
                    else attention(request, "Cloud progress changed on another device. It will be checked next time the game starts.");
                    return;
                case RESET_LOCAL:
                    problem(request, "Local progress was reset. Your previous cloud copy is unchanged.");
                    return;
                case CHOOSE:
                    if (request.mode == Mode.AUTOMATIC) { attention(request, "Cloud progress needs a choice. It will be reviewed next time the game starts."); return; }
                    choose(request, remote);
                    return;
            }
        }
        if (request.upload && (remote.length == 0 || (request.local != null && request.remoteProfile != null
                && request.remoteProfile.fingerprint.equals(request.local.profile.fingerprint)))) commit(request);
        else if (!request.upload && remote.length == 0) fail(request, "There is no cloud progress to restore yet.");
        else confirm(request, remote);
    }

    private void resolveChoice(Request request, String conflictId, Snapshot chosen) {
        worker.execute(() -> {
            try {
                // Resolving discards a provider version. Validate the selected
                // archive before allowing that irreversible provider change.
                CloudSaveArchive.inspect(request.root, chosen.getSnapshotContents().readFully());
                activity.runOnUiThread(() -> {
                    if (!valid(request)) return;
                    withAccount(request, () -> request.client.resolveConflict(conflictId, chosen).addOnCompleteListener(resolved -> {
                        if (!resolved.isSuccessful()) { if (valid(request)) fail(request, "Could not resolve the cloud versions."); return; }
                        request.firstConflict = null; request.secondConflict = null;
                        opened(request, resolved.getResult());
                    }));
                });
            } catch (IOException | RuntimeException error) {
                activity.runOnUiThread(() -> { if (valid(request)) problem(request, "This cloud version is invalid. The existing cloud versions and local progress are preserved."); });
            }
        });
    }

    private void choose(Request request, byte[] remote) {
        pauseTimeout(request);
        dismiss(request);
        AlertDialog.Builder choice = new AlertDialog.Builder(activity).setTitle("Choose progress to keep")
                .setMessage("This device and your Play Games cloud have different progress. Choose which complete profile to continue with. Loading cloud progress keeps a recovery copy of this device's progress.")
                .setPositiveButton("Use this device", (dialog, which) -> { waiting(request, "Saving progress…"); commit(request); })
                .setNegativeButton("Play offline", (dialog, which) -> cancel(request))
                .setOnCancelListener(dialog -> cancel(request));
        if (remote.length != 0) choice.setNeutralButton("Use cloud", (dialog, which) -> restore(request, remote));
        request.dialog = choice.show();
    }

    private void confirm(Request request, byte[] remote) {
        pauseTimeout(request);
        dismiss(request);
        request.dialog = new AlertDialog.Builder(activity)
                .setTitle(request.upload ? "Replace cloud progress?" : "Restore cloud progress?")
                .setMessage((request.remoteProfile == null && remote.length != 0 ? "The cloud copy is damaged." : label(request.snapshot)) + (request.upload
                        ? "\n\nReplace this cloud version with this device's current characters, Tales and progress?"
                        : "\n\nLoad cloud progress on this device? A recovery copy of its current progress will be kept on this device."))
                .setPositiveButton(request.upload ? "Replace cloud save" : "Restore progress", (dialog, which) -> {
                    if (request.upload) { waiting(request, "Saving progress…"); commit(request); }
                    else restore(request, remote);
                }).setNegativeButton("Continue locally", (dialog, which) -> cancel(request))
                .setOnCancelListener(dialog -> cancel(request)).show();
    }

    private static String label(Snapshot snapshot) {
        return "Saved " + DateFormat.getDateTimeInstance().format(new Date(snapshot.getMetadata().getLastModifiedTimestamp()));
    }

    private void withAccount(Request request, Runnable action) {
        guard(request, () -> PlayGames.getPlayersClient(activity).getCurrentPlayer().addOnCompleteListener(player -> guard(request, () -> {
            if (!valid(request)) return;
            if (!player.isSuccessful() || player.getResult() == null
                    || !request.playerId.equals(player.getResult().getPlayerId())) { fail(request, "The Play Games account changed. Please try again."); return; }
            action.run();
        })));
    }

    private void commit(Request request) {
        withAccount(request, () -> {
            if (!request.snapshot.getSnapshotContents().writeBytes(request.local.bytes)) { fail(request, "Cannot write cloud progress."); return; }
            Snapshot snapshot = request.snapshot;
            request.snapshot = null;
            request.client.commitAndClose(snapshot, new SnapshotMetadataChange.Builder()
                    .setDescription("Sil-More characters, Tales and progress").build()).addOnCompleteListener(saved -> {
                        if (!valid(request)) return;
                        if (!saved.isSuccessful()) { fail(request, "The cloud save was not stored. Your local progress is safe; sync will retry later."); return; }
                        remember(request, request.local.profile.fingerprint);
                        if (request.recovery) Toast.makeText(activity, "Cloud save stored. Google Play syncs it when connected.", Toast.LENGTH_LONG).show();
                        completed(request);
                    });
        });
    }

    private void remember(Request request, String fingerprint) {
        try { request.state.remember(request.playerId, fingerprint); }
        catch (IOException error) { Log.w(TAG, "Cannot remember common cloud profile; future changes will need review", error); }
    }

    private void restore(Request request, byte[] bytes) {
        waiting(request, "Checking and preparing the complete cloud save…");
        worker.execute(() -> {
            try {
                File candidate = CloudSaveArchive.prepareRestore(request.root, bytes);
                activity.runOnUiThread(() -> {
                    if (!valid(request)) { candidate.delete(); return; }
                    request.candidate = candidate;
                    withAccount(request, () -> {
                        try { CloudSaveArchive.publishRestore(request.root, candidate); }
                        catch (IOException error) { fail(request, error.getMessage()); return; }
                        request.candidate = null;
                        restoring = true;
                        finish(request, 1);
                    });
                });
            } catch (IOException error) { activity.runOnUiThread(() -> { if (valid(request)) fail(request, error.getMessage()); }); }
        });
    }

    private void pauseTimeout(Request request) {
        if (request.timeout != null) handler.removeCallbacks(request.timeout);
    }

    private void cancel(Request request) {
        if (request.mode == Mode.STARTUP) {
            // An explicit offline choice lasts until manual recovery or the
            // next launch; a network timeout alone remains eligible for retry.
            needsAttention = true;
            attentionMessage = null;
        }
        finish(request, 0);
    }

    private void completed(Request request) {
        needsAttention = false;
        attentionMessage = null;
        nextAttempt = SystemClock.elapsedRealtime() + POLL_MS;
        if (latest != null && request.local != null && !latest.profile.fingerprint.equals(request.local.profile.fingerprint)) nextAttempt = 0;
        finish(request, 0);
    }

    private void attention(Request request, String message) {
        needsAttention = true;
        attentionMessage = resumed ? null : message;
        Log.i(TAG, message);
        if (resumed) Toast.makeText(activity, message, Toast.LENGTH_LONG).show();
        finish(request, 0);
    }

    private void problem(Request request, String message) {
        if (request.mode == Mode.STARTUP) recovery(request, message);
        else attention(request, message + " Recovery will be offered next time the game starts.");
    }

    private void finish(Request request, int result) {
        if (current != request) return;
        pauseTimeout(request);
        discard(request, request.snapshot);
        discard(request, request.firstConflict);
        discard(request, request.secondConflict);
        request.snapshot = null;
        if (request.candidate != null) request.candidate.delete();
        dismiss(request);
        Request completion = request.completion;
        completion.result = result;
        completion.playerId = request.playerId;
        completion.state = request.state;
        completion.remoteProfile = request.remoteProfile;
        current = null;
        completion.finished.countDown();
        if (blocking != null && !destroyed && !restoring) {
            Request next = blocking;
            blocking = null;
            start(next);
        } else if ((resumed || nextAttempt == 0) && !destroyed && !restoring && !needsAttention && latest != null) {
            handler.removeCallbacks(retry);
            handler.postDelayed(retry, Math.max(0, nextAttempt - SystemClock.elapsedRealtime()));
        }
    }

    private void fail(Request request, String message) {
        if (current != request) return;
        Log.w(TAG, message == null ? "Cloud operation failed" : message);
        if (request.recovery) { recovery(request, message); return; }
        nextAttempt = SystemClock.elapsedRealtime() + RETRY_MS;
        finish(request, 0);
    }
}
