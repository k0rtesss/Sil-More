package com.silqh.silmore;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Arrays;
import java.util.Enumeration;
import java.util.HashSet;
import java.util.Set;
import java.util.TreeMap;
import java.util.UUID;
import java.util.zip.CRC32;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import java.util.zip.ZipOutputStream;

/** Complete progress bundles. Configuration, caches and logs stay on the device. */
final class CloudSaveArchive {
    static final int MAX_BYTES = 3 * 1024 * 1024;
    private static final long MAX_EXPANDED = 64L * 1024 * 1024;
    private static final int MAX_FILES = 10000;
    private static final String FORMAT = "sil-more-cloud-v1\n";
    private static final String MARKER = "sil-more-cloud-format";
    private static final String PENDING = ".play-cloud-pending.zip";
    private static final String TRANSACTION = ".play-cloud-restore";
    private static final String[] ROOTS = {"save", "meta"};

    private CloudSaveArchive() {}

    static final class Profile {
        final String fingerprint;
        final boolean empty;
        Profile(String fingerprint, boolean empty) {
            this.fingerprint = fingerprint;
            this.empty = empty;
        }
    }

    private static MessageDigest sha256() {
        try { return MessageDigest.getInstance("SHA-256"); }
        catch (NoSuchAlgorithmException error) { throw new AssertionError(error); }
    }

    /** Hash file names and contents, independently of ZIP order/compression/device. */
    static Profile inspect(File root, byte[] bytes) throws IOException {
        File temporary = writeCandidate(root, bytes);
        try { return extract(temporary, null); }
        finally { temporary.delete(); }
    }

    static byte[] capture(File root) throws IOException {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        long[] budget = {0, 0};
        try (ZipOutputStream zip = new ZipOutputStream(bytes)) {
            ZipEntry marker = new ZipEntry(MARKER);
            marker.setTime(0);
            zip.putNextEntry(marker);
            zip.write(FORMAT.getBytes(StandardCharsets.UTF_8));
            zip.closeEntry();
            for (String name : ROOTS) add(zip, root, new File(root, name), name, bytes, budget);
        }
        if (bytes.size() > MAX_BYTES) throw new IOException("Progress is larger than the Google cloud save limit. Local saves are unchanged.");
        return bytes.toByteArray();
    }

    private static void add(ZipOutputStream zip, File root, File file, String name,
                            ByteArrayOutputStream bytes, long[] budget) throws IOException {
        if (!file.exists()) return;
        requireInside(root, file);
        if (file.isDirectory()) {
            File[] children = file.listFiles();
            if (children == null) throw new IOException("Cannot read progress directory");
            Arrays.sort(children, (left, right) -> left.getName().compareTo(right.getName()));
            for (File child : children) add(zip, root, child, name + "/" + child.getName(), bytes, budget);
            return;
        }
        String leaf = file.getName();
        if (leaf.endsWith(".new") || leaf.endsWith(".old") || leaf.endsWith(".lok")
                || leaf.endsWith(".tmp") || leaf.contains(".tmp.")) return;
        if (!validName(name) || ++budget[0] > MAX_FILES) throw new IOException("Unsupported progress filename or too many files");
        ZipEntry entry = new ZipEntry(name);
        entry.setTime(0);
        zip.putNextEntry(entry);
        try (FileInputStream input = new FileInputStream(file)) {
            byte[] buffer = new byte[16384];
            int count;
            while ((count = input.read(buffer)) != -1) {
                budget[1] += count;
                if (budget[1] > MAX_EXPANDED || bytes.size() > MAX_BYTES) throw new IOException("Progress exceeds the cloud save size limit");
                zip.write(buffer, 0, count);
            }
        }
        zip.closeEntry();
    }

    private static boolean validName(String name) {
        if (name.indexOf('\\') >= 0 || name.indexOf(':') >= 0 || name.indexOf('\0') >= 0
                || !(name.startsWith("save/") || name.startsWith("meta/"))) return false;
        for (String part : name.split("/", -1)) if (part.isEmpty() || part.equals(".") || part.equals("..")) return false;
        return true;
    }

    private static void requireInside(File root, File file) throws IOException {
        String base = root.getCanonicalPath() + File.separator;
        if (!file.getAbsolutePath().equals(file.getCanonicalPath())
                || !file.getCanonicalPath().startsWith(base)) throw new IOException("Cloud saves cannot follow a symbolic link");
    }

    private static void mkdir(File file) throws IOException {
        if (!file.isDirectory() && !file.mkdirs()) throw new IOException("Cannot create cloud restore directory");
    }

    private static void write(File file, byte[] bytes) throws IOException {
        try (FileOutputStream output = new FileOutputStream(file)) {
            output.write(bytes);
            output.getFD().sync();
        }
    }

    private static void move(File from, File to) throws IOException {
        if (!from.renameTo(to)) throw new IOException("Cannot safely move progress during cloud restore");
    }

    /** Validate everything before publishing the pending restore. Never edits live saves. */
    static void stageRestore(File root, byte[] bytes) throws IOException {
        publishRestore(root, prepareRestore(root, bytes));
    }

    static File prepareRestore(File root, byte[] bytes) throws IOException {
        File temporary = writeCandidate(root, bytes);
        try { extract(temporary, null); }
        catch (IOException error) { temporary.delete(); throw error; }
        return temporary;
    }

    private static File writeCandidate(File root, byte[] bytes) throws IOException {
        if (bytes == null || bytes.length == 0 || bytes.length > MAX_BYTES) throw new IOException("Invalid cloud save size");
        mkdir(root);
        File temporary = new File(root, ".play-cloud-candidate-" + UUID.randomUUID() + ".zip");
        requireInside(root, temporary);
        write(temporary, bytes);
        return temporary;
    }

    static void publishRestore(File root, File temporary) throws IOException {
        requireInside(root, temporary);
        if (!temporary.getName().startsWith(".play-cloud-candidate-")) throw new IOException("Invalid cloud restore candidate");
        File pending = new File(root, PENDING);
        requireInside(root, pending);
        // On Android both files are on the same filesystem; rename replaces the
        // previous pending file atomically. The current profile is untouched.
        move(temporary, pending);
    }

    private static Profile extract(File archive, File destination) throws IOException {
        if (archive.length() > MAX_BYTES) throw new IOException("Cloud save exceeds the size limit");
        Set<String> seen = new HashSet<>();
        long total = 0;
        boolean formatFound = false;
        TreeMap<String, byte[]> fingerprints = new TreeMap<>();
        try (ZipFile zip = new ZipFile(archive)) {
            Enumeration<? extends ZipEntry> entries = zip.entries();
            while (entries.hasMoreElements()) {
                ZipEntry entry = entries.nextElement();
                String name = entry.getName();
                if (!seen.add(name) || seen.size() > MAX_FILES + 1 || entry.isDirectory()
                        || (!name.equals(MARKER) && !validName(name))) throw new IOException("Invalid cloud save contents");
                File outputFile = destination == null || name.equals(MARKER) ? null : new File(destination, name);
                if (outputFile != null) {
                    requireInside(destination, outputFile);
                    mkdir(outputFile.getParentFile());
                }
                ByteArrayOutputStream marker = name.equals(MARKER) ? new ByteArrayOutputStream() : null;
                CRC32 crc = new CRC32();
                MessageDigest content = sha256();
                long length = 0;
                try (InputStream input = zip.getInputStream(entry);
                     FileOutputStream output = outputFile == null ? null : new FileOutputStream(outputFile)) {
                    byte[] buffer = new byte[16384];
                    int count;
                    while ((count = input.read(buffer)) != -1) {
                        total += count;
                        length += count;
                        if (total > MAX_EXPANDED || (marker != null && length > 64)) throw new IOException("Cloud save expands beyond its limit");
                        crc.update(buffer, 0, count);
                        content.update(buffer, 0, count);
                        if (output != null) output.write(buffer, 0, count);
                        if (marker != null) marker.write(buffer, 0, count);
                    }
                    if (output != null) output.getFD().sync();
                }
                if (length != entry.getSize() || crc.getValue() != entry.getCrc()) throw new IOException("Cloud save checksum failed");
                if (marker != null) {
                    if (!FORMAT.equals(new String(marker.toByteArray(), StandardCharsets.UTF_8))) throw new IOException("Unsupported cloud save version");
                    formatFound = true;
                } else fingerprints.put(name, content.digest());
            }
        }
        if (!formatFound) throw new IOException("This is not a Sil-More cloud save");
        MessageDigest profile = sha256();
        for (String name : fingerprints.keySet()) {
            profile.update(name.getBytes(StandardCharsets.UTF_8));
            profile.update((byte)0);
            profile.update(fingerprints.get(name));
        }
        StringBuilder hex = new StringBuilder();
        for (byte value : profile.digest()) hex.append(String.format(java.util.Locale.ROOT, "%02x", value & 255));
        return new Profile(hex.toString(), fingerprints.isEmpty());
    }

    /** Runs on the native startup thread, before any progress files are loaded. */
    static boolean applyPending(File root) throws IOException {
        File transaction = new File(root, TRANSACTION);
        File pending = new File(root, PENDING);
        requireInside(root, transaction);
        requireInside(root, pending);
        if (transaction.exists()) recover(root, transaction, pending);
        if (!pending.exists()) return false;
        File incoming = new File(transaction, "incoming");
        File backup = new File(transaction, "backup");
        mkdir(incoming);
        mkdir(backup);
        try {
            for (String name : ROOTS) {
                mkdir(new File(incoming, name));
                File current = new File(root, name);
                requireInside(root, current);
                if (current.exists()) write(new File(transaction, "original-" + name), new byte[0]);
            }
            extract(pending, incoming);
            write(new File(transaction, "ready"), new byte[0]);
            for (String name : ROOTS) {
                File current = new File(root, name);
                if (current.exists()) move(current, new File(backup, name));
                move(new File(incoming, name), current);
            }
            write(new File(transaction, "committed"), new byte[0]);
        } catch (IOException error) {
            recover(root, transaction, pending);
            throw error;
        }
        recover(root, transaction, pending);
        return true;
    }

    private static void recover(File root, File transaction, File pending) throws IOException {
        if (new File(transaction, "committed").exists()) {
            if (pending.exists() && !pending.delete()) throw new IOException("Cannot finish cloud restore");
        } else if (new File(transaction, "ready").exists()) {
            for (String name : ROOTS) {
                File current = new File(root, name);
                File old = new File(transaction, "backup/" + name);
                File incoming = new File(transaction, "incoming/" + name);
                if (old.exists()) {
                    if (current.exists()) move(current, new File(transaction, "discarded-" + name));
                    move(old, current);
                } else if (!new File(transaction, "original-" + name).exists() && !incoming.exists() && current.exists()) {
                    move(current, new File(transaction, "discarded-" + name));
                }
            }
        }
        // Keep the previous profile outside save/ and meta/, including interrupted
        // transactions. Neither provider uploads these recovery copies.
        move(transaction, new File(root, ".play-cloud-backup-" + UUID.randomUUID()));
    }
}
