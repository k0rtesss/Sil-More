package com.silqh.silmore;

import android.app.AlertDialog;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.widget.Toast;
import com.google.android.gms.games.PlayGames;
import com.google.android.gms.games.SnapshotsClient;
import com.google.android.gms.games.snapshot.Snapshot;
import com.google.android.gms.games.snapshot.SnapshotMetadataChange;
import java.io.File;
import java.io.IOException;
import java.text.DateFormat;
import java.util.Arrays;
import java.util.Date;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** Explicit complete-profile backup/restore. Native play is paused until it ends. */
final class PlayGamesCloudSaves {
    private static final String NAME = "sil_more_progress_v1";
    private static final long TIMEOUT_MS = 45000;
    private final SilMoreActivity activity;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private volatile Request current;

    private static final class Request {
        final CountDownLatch finished = new CountDownLatch(1);
        final File root;
        final byte[] local;
        final String captureError;
        volatile int result;
        AlertDialog dialog;
        SnapshotsClient client;
        Snapshot snapshot;
        String playerId;
        boolean upload;
        int conflicts;
        Runnable timeout;
        Request(File root, byte[] local, String error) {
            this.root = root; this.local = local; this.captureError = error;
        }
    }

    PlayGamesCloudSaves(SilMoreActivity activity) { this.activity = activity; }
    boolean isConfigured() { return PlayGamesApplication.initialized; }

    boolean prepare(String path) {
        try {
            if (CloudSaveArchive.applyPending(new File(path))) Log.i("SilMoreCloud", "Cloud progress restored before native initialization");
            return true;
        } catch (IOException error) {
            Log.e("SilMoreCloud", "Could not safely apply cloud restore", error);
            activity.runOnUiThread(() -> new AlertDialog.Builder(activity)
                    .setTitle("Cloud restore needs attention")
                    .setMessage("The game could not safely finish the cloud restore. Your recovery copies are preserved. Please check the app's log before trying again.")
                    .setPositiveButton("Close", (dialog, which) -> activity.finish()).show());
            return false;
        }
    }

    /** Called on SDL's game thread after the complete local checkpoint is saved. */
    int show(String path) {
        if (!isConfigured() || current != null) return 0;
        byte[] bytes = null;
        String captureError = null;
        try { bytes = CloudSaveArchive.capture(new File(path)); }
        catch (IOException error) { captureError = error.getMessage(); }
        Request request = new Request(new File(path), bytes, captureError);
        current = request;
        activity.runOnUiThread(() -> {
            if (!valid(request)) { finish(request, 0); return; }
            request.dialog = new AlertDialog.Builder(activity).setTitle("Google Play cloud saves")
                    .setMessage("Save or restore all characters, Tales and progress using your Play Games account. Restoring keeps a local recovery copy and restarts the game.")
                    .setPositiveButton("Save to cloud", (dialog, which) -> begin(request, true))
                    .setNeutralButton("Restore from cloud", (dialog, which) -> begin(request, false))
                    .setNegativeButton("Cancel", (dialog, which) -> finish(request, 0))
                    .setOnCancelListener(dialog -> finish(request, 0)).show();
        });
        try { request.finished.await(); }
        catch (InterruptedException error) {
            Thread.currentThread().interrupt();
            activity.runOnUiThread(() -> finish(request, 0));
        }
        return request.result;
    }

    void onDestroy() {
        Request request = current;
        if (request != null) finish(request, 0);
        worker.shutdown();
    }

    private boolean valid(Request request) {
        return current == request && !activity.isFinishing() && !activity.isDestroyed();
    }

    private void guard(Request request, Runnable action) {
        try { action.run(); }
        catch (RuntimeException error) {
            Log.w("SilMoreCloud", "Play Games cloud request failed", error);
            if (valid(request)) fail(request, "Google Play cloud saves are unavailable. Local progress is safe; please try again.");
        }
    }

    private void discard(Request request, Snapshot snapshot) {
        if (request.client == null || snapshot == null) return;
        try { request.client.discardAndClose(snapshot); }
        catch (RuntimeException error) { Log.w("SilMoreCloud", "Could not close cloud snapshot", error); }
    }

    private void waiting(Request request, String message) {
        if (request.dialog != null) request.dialog.dismiss();
        request.dialog = new AlertDialog.Builder(activity).setTitle("Google Play cloud saves")
                .setMessage(message).setNegativeButton("Cancel", (dialog, which) -> finish(request, 0))
                .setOnCancelListener(dialog -> finish(request, 0)).show();
        if (request.timeout != null) handler.removeCallbacks(request.timeout);
        request.timeout = () -> fail(request, "Google has not confirmed the request yet. Local progress is safe. Check the cloud version before retrying a save.");
        handler.postDelayed(request.timeout, TIMEOUT_MS);
    }

    private void begin(Request request, boolean upload) {
        request.upload = upload;
        if (upload && request.local == null) { fail(request, request.captureError); return; }
        waiting(request, "Connecting to your Play Games account…");
        try {
            PlayGames.getGamesSignInClient(activity).isAuthenticated().addOnCompleteListener(task -> {
                if (!valid(request)) return;
                if (!task.isSuccessful()) { fail(request, "Cannot check Play Games sign-in."); return; }
                if (task.getResult().isAuthenticated()) identify(request);
                else PlayGames.getGamesSignInClient(activity).signIn().addOnCompleteListener(signIn -> {
                    if (!valid(request)) return;
                    if (!signIn.isSuccessful() || !signIn.getResult().isAuthenticated()) fail(request, "Play Games sign-in was cancelled or failed.");
                    else identify(request);
                });
            });
        } catch (RuntimeException error) { fail(request, error.getMessage()); }
    }

    private void identify(Request request) {
        guard(request, () -> PlayGames.getPlayersClient(activity).getCurrentPlayer().addOnCompleteListener(task -> guard(request, () -> {
            if (!valid(request)) return;
            if (!task.isSuccessful() || task.getResult() == null) { fail(request, "Cannot identify the Play Games account."); return; }
            request.playerId = task.getResult().getPlayerId();
            request.client = PlayGames.getSnapshotsClient(activity);
            request.client.open(NAME, request.upload, SnapshotsClient.RESOLUTION_POLICY_MANUAL)
                    .addOnCompleteListener(opened -> {
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
            if (!result.isConflict() && result.getData() != null) discard(request, result.getData());
            else if (result.isConflict()) {
                discard(request, result.getConflict().getSnapshot());
                discard(request, result.getConflict().getConflictingSnapshot());
            }
            return;
        }
        if (result.isConflict()) {
            if (++request.conflicts > 3) { fail(request, "The cloud save keeps changing. Try again after the other device finishes syncing."); return; }
            SnapshotsClient.SnapshotConflict conflict = result.getConflict();
            Snapshot first = conflict.getSnapshot(), second = conflict.getConflictingSnapshot();
            pauseTimeout(request);
            request.dialog.dismiss();
            request.dialog = new AlertDialog.Builder(activity).setTitle("Two cloud versions need a choice")
                    .setMessage("Choose the cloud version to keep before continuing.")
                    .setItems(new String[]{label(first), label(second)}, (dialog, which) -> guard(request, () -> {
                        waiting(request, "Resolving the cloud versions…");
                        request.client.resolveConflict(conflict.getConflictId(), which == 0 ? first : second)
                                .addOnCompleteListener(resolved -> {
                                    if (!resolved.isSuccessful()) { if (valid(request)) fail(request, "Could not resolve the cloud versions."); return; }
                                    opened(request, resolved.getResult());
                                });
                    })).setNegativeButton("Cancel", (dialog, which) -> {
                        discard(request, first);
                        discard(request, second);
                        finish(request, 0);
                    }).setOnCancelListener(dialog -> {
                        discard(request, first);
                        discard(request, second);
                        finish(request, 0);
                    }).show();
            return;
        }
        request.snapshot = result.getData();
        if (request.snapshot == null) { fail(request, "Google returned no cloud save."); return; }
        final byte[] remote;
        try { remote = request.snapshot.getSnapshotContents().readFully(); }
        catch (IOException error) { fail(request, "Cannot read the cloud save."); return; }
        if (request.upload && (remote.length == 0 || Arrays.equals(remote, request.local))) commit(request);
        else if (!request.upload && remote.length == 0) fail(request, "There is no cloud progress to restore yet.");
        else {
            pauseTimeout(request);
            request.dialog.dismiss();
            request.dialog = new AlertDialog.Builder(activity)
                    .setTitle(request.upload ? "Replace cloud progress?" : "Restore cloud progress?")
                    .setMessage(label(request.snapshot) + (request.upload
                            ? "\n\nReplace this cloud version with this device's current characters, Tales and progress?"
                            : "\n\nReplace this device's progress and restart? A recovery copy of its current progress will be kept on this device."))
                    .setPositiveButton(request.upload ? "Replace cloud save" : "Restore and restart", (dialog, which) -> {
                        if (request.upload) { waiting(request, "Saving progress…"); commit(request); }
                        else restore(request, remote);
                    }).setNegativeButton("Cancel", (dialog, which) -> finish(request, 0))
                    .setOnCancelListener(dialog -> finish(request, 0)).show();
        }
    }

    private static String label(Snapshot snapshot) {
        return "Saved " + DateFormat.getDateTimeInstance().format(new Date(snapshot.getMetadata().getLastModifiedTimestamp()));
    }

    private void commit(Request request) {
        // Re-check the account after any human confirmation. Never commit a
        // snapshot opened for one player after the signed-in account changes.
        guard(request, () -> PlayGames.getPlayersClient(activity).getCurrentPlayer().addOnCompleteListener(player -> guard(request, () -> {
            if (!valid(request)) return;
            if (!player.isSuccessful() || player.getResult() == null
                    || !request.playerId.equals(player.getResult().getPlayerId())) { fail(request, "The Play Games account changed. Please try again."); return; }
            if (!request.snapshot.getSnapshotContents().writeBytes(request.local)) { fail(request, "Cannot write cloud progress."); return; }
            Snapshot snapshot = request.snapshot;
            request.snapshot = null;
            request.client.commitAndClose(snapshot, new SnapshotMetadataChange.Builder()
                    .setDescription("Sil-More characters, Tales and progress").build())
                    .addOnCompleteListener(saved -> {
                        if (!valid(request)) return;
                        if (!saved.isSuccessful()) { fail(request, "The cloud save was not stored. Your local progress is safe; try again."); return; }
                        finish(request, 0);
                        Toast.makeText(activity, "Cloud save stored. Google Play syncs it when connected.", Toast.LENGTH_LONG).show();
                    });
        })));
    }

    private void restore(Request request, byte[] bytes) {
        waiting(request, "Checking and preparing the complete cloud save…");
        worker.execute(() -> {
            try {
                File candidate = CloudSaveArchive.prepareRestore(request.root, bytes);
                activity.runOnUiThread(() -> {
                    if (!valid(request)) { candidate.delete(); return; }
                    try { CloudSaveArchive.publishRestore(request.root, candidate); }
                    catch (IOException error) { fail(request, error.getMessage()); return; }
                    // A queued restore is applied before the next native launch.
                    if (!activity.restartAfterCloudRestore()) { fail(request, "Cloud progress is ready. Restart the app to finish restoring it."); return; }
                    finish(request, 2);
                });
            } catch (IOException error) { activity.runOnUiThread(() -> { if (valid(request)) fail(request, error.getMessage()); }); }
        });
    }

    private void pauseTimeout(Request request) {
        if (request.timeout != null) handler.removeCallbacks(request.timeout);
    }

    private void finish(Request request, int result) {
        if (current != request) return;
        pauseTimeout(request);
        discard(request, request.snapshot);
        request.snapshot = null;
        if (request.dialog != null) request.dialog.dismiss();
        request.result = result;
        current = null;
        request.finished.countDown();
    }

    private void fail(Request request, String message) {
        if (current != request) return;
        Log.w("SilMoreCloud", message == null ? "Cloud operation failed" : message);
        finish(request, 0);
        if (!activity.isFinishing() && !activity.isDestroyed()) new AlertDialog.Builder(activity)
                .setTitle("Cloud saves").setMessage(message == null ? "Cloud save failed. Your local progress is unchanged." : message)
                .setPositiveButton("OK", null).show();
    }
}
