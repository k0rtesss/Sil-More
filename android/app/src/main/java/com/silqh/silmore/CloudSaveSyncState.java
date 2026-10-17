package com.silqh.silmore;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Properties;

/** Device-local common ancestor. A clock timestamp never decides whose progress wins. */
final class CloudSaveSyncState {
    enum Action { SAME, UPLOAD, DOWNLOAD, CHOOSE, RESET_LOCAL }
    private static final String NAME = ".play-cloud-sync";
    private static final String EMPTY_PROFILE = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    private final File root;
    private String player;
    private String base;

    CloudSaveSyncState(File root) throws IOException {
        this(root, false);
    }

    CloudSaveSyncState(File root, boolean manualRecovery) throws IOException {
        this.root = root;
        File file = checked(NAME);
        if (!file.exists()) file = checked(NAME + ".old");
        if (file.exists()) {
            Properties values = new Properties();
            try (FileInputStream input = new FileInputStream(file)) { values.load(input); }
            catch (IOException | IllegalArgumentException error) {
                if (!manualRecovery) throw new IOException("The device's cloud synchronization record could not be read.", error);
                values.clear();
            }
            player = values.getProperty("player");
            base = values.getProperty("base");
            if (player == null || player.isEmpty() || base == null || !base.matches("[0-9a-f]{64}")) {
                player = null; base = null;
                if (!manualRecovery) throw new IOException("The device's cloud synchronization record is damaged.");
            }
        }
    }

    private File checked(String name) throws IOException {
        File file = new File(root, name);
        if (!file.getAbsolutePath().equals(file.getCanonicalPath()))
            throw new IOException("Cloud synchronization state cannot follow a symbolic link");
        return file;
    }

    Action action(String account, CloudSaveArchive.Profile local, CloudSaveArchive.Profile remote) {
        if (remote != null && local.fingerprint.equals(remote.fingerprint)) return Action.SAME;
        boolean sameAccount = account.equals(player);
        // Reset is deliberately local. Do not undo it by restoring the old cloud
        // profile, or erase the cloud copy without an explicit recovery choice.
        if (player == null && new File(root, ".sil-more-progress-reset").exists()
                && remote != null && !remote.empty) return Action.RESET_LOCAL;
        if (player != null && !sameAccount) return Action.CHOOSE;
        if (remote == null || remote.empty) {
            if (local.empty) return Action.SAME;
            // A previously populated cloud profile was deleted or cleared on
            // another device. Never silently resurrect the older local copy.
            if (sameAccount && !EMPTY_PROFILE.equals(base)) return Action.CHOOSE;
            return Action.UPLOAD;
        }
        if (sameAccount && local.fingerprint.equals(base)) return Action.DOWNLOAD;
        if (sameAccount && remote.fingerprint.equals(base)) return Action.UPLOAD;
        if (player == null && local.empty) return Action.DOWNLOAD;
        return Action.CHOOSE;
    }

    void remember(String account, String fingerprint) throws IOException {
        Properties values = new Properties();
        values.setProperty("player", account);
        values.setProperty("base", fingerprint);
        File temporary = checked(NAME + ".new"), file = checked(NAME), old = checked(NAME + ".old");
        try (FileOutputStream output = new FileOutputStream(temporary)) {
            values.store(output, "Sil-More cloud common profile; never synchronized");
            output.getFD().sync();
        }
        if (old.exists() && !old.delete()) throw new IOException("Cannot rotate cloud synchronization state");
        if (file.exists() && !file.renameTo(old)) throw new IOException("Cannot preserve cloud synchronization state");
        if (!temporary.renameTo(file)) throw new IOException("Cannot save cloud synchronization state");
        player = account;
        base = fingerprint;
        old.delete();
    }
}
