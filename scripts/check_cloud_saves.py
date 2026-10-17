#!/usr/bin/env python3
"""Exercise production cloud archive validation, transaction recovery and capacity.

No platform account is used. --profile measures a real profile without writing it.
"""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "android/app/src/main/java/com/silqh/silmore/CloudSaveArchive.java"

FIXTURE = r'''
package com.silqh.silmore;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;
import java.util.zip.*;

public final class CloudSaveFixture {
    static void check(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
    }
    static void put(File root, String name, String value) throws Exception {
        File file = new File(root, name);
        file.getParentFile().mkdirs();
        Files.write(file.toPath(), value.getBytes(StandardCharsets.UTF_8));
    }
    static String read(File root, String name) throws Exception {
        return new String(Files.readAllBytes(new File(root, name).toPath()), StandardCharsets.UTF_8);
    }
    static byte[] zip(String name, byte[] content, boolean marker) throws Exception {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        try (ZipOutputStream zip = new ZipOutputStream(bytes)) {
            if (marker) {
                zip.putNextEntry(new ZipEntry("sil-more-cloud-format"));
                zip.write("sil-more-cloud-v1\n".getBytes(StandardCharsets.UTF_8));
                zip.closeEntry();
            }
            zip.putNextEntry(new ZipEntry(name)); zip.write(content); zip.closeEntry();
        }
        return bytes.toByteArray();
    }
    static void reject(File root, byte[] bytes) throws Exception {
        try { CloudSaveArchive.stageRestore(root, bytes); throw new AssertionError("Accepted invalid archive"); }
        catch (IOException expected) {}
        check(!new File(root, ".play-cloud-pending.zip").exists(), "Invalid archive became pending");
    }
    static int countBackups(File root) {
        return root.listFiles(file -> file.getName().startsWith(".play-cloud-backup-")).length;
    }
    static byte[] repack(byte[] bytes) throws Exception {
        Map<String, byte[]> files = new TreeMap<>(Collections.reverseOrder());
        try (ZipInputStream zip = new ZipInputStream(new ByteArrayInputStream(bytes))) {
            ZipEntry entry;
            while ((entry = zip.getNextEntry()) != null) files.put(entry.getName(), zip.readAllBytes());
        }
        ByteArrayOutputStream bytesOut = new ByteArrayOutputStream();
        try (ZipOutputStream zip = new ZipOutputStream(bytesOut)) {
            zip.setLevel(0);
            for (String name : files.keySet()) {
                ZipEntry entry = new ZipEntry(name);
                entry.setTime(1700000000000L);
                zip.putNextEntry(entry); zip.write(files.get(name)); zip.closeEntry();
            }
        }
        return bytesOut.toByteArray();
    }
    static void tests(File root) throws Exception {
        root.mkdirs();
        put(root, "save/hero", "character and retained levels");
        put(root, "save/hero.new", "incomplete write");
        put(root, "meta/meta.raw", "Tale state");
        put(root, "meta/achievements.json", "achievements");
        put(root, "meta/artefacts.db", "custom artefacts");
        put(root, "meta/metaruns/00000001/history", "Tale history");
        put(root, "sil_sdl.json", "device preferences");
        put(root, "data/monster.raw", "generated data");
        byte[] original = CloudSaveArchive.capture(root);
        check(Arrays.equals(original, CloudSaveArchive.capture(root)), "Archive was not deterministic");
        CloudSaveArchive.Profile identity = CloudSaveArchive.inspect(root, original);
        check(!identity.empty && identity.fingerprint.length() == 64, "Profile fingerprint missing");
        byte[] differentlyPacked = repack(original);
        check(!Arrays.equals(original, differentlyPacked), "Fixture did not change ZIP representation");
        check(identity.fingerprint.equals(CloudSaveArchive.inspect(root, differentlyPacked).fingerprint), "ZIP metadata/order/compression changed logical profile identity");
        put(root, "save/hero", "changed gameplay");
        check(!identity.fingerprint.equals(CloudSaveArchive.inspect(root, CloudSaveArchive.capture(root)).fingerprint), "Changed character content retained profile identity");
        put(root, "save/hero", "character and retained levels");
        File empty = new File(root.getParentFile(), "empty-profile"); empty.mkdirs();
        check(CloudSaveArchive.inspect(empty, CloudSaveArchive.capture(empty)).empty, "Empty profile not recognized");
        Set<String> names = new HashSet<>();
        try (ZipInputStream zip = new ZipInputStream(new ByteArrayInputStream(original))) {
            ZipEntry entry; while ((entry = zip.getNextEntry()) != null) names.add(entry.getName());
        }
        check(names.size() == 6 && !names.contains("save/hero.new") && !names.contains("sil_sdl.json"), "Wrong profile allowlist");
        CloudSaveArchive.stageRestore(root, original);
        put(root, "save/hero", "newer local progress");
        put(root, "save/stale-character", "remove from restored profile");
        check(read(root, "save/hero").equals("newer local progress"), "Staging edited live progress");
        check(CloudSaveArchive.applyPending(root), "Pending restore was not applied");
        check(read(root, "save/hero").equals("character and retained levels"), "Character restore failed");
        check(read(root, "meta/metaruns/00000001/history").equals("Tale history"), "Tale restore failed");
        check(!new File(root, "save/stale-character").exists(), "Restored profile retained unrelated live characters");
        check(read(root, "sil_sdl.json").equals("device preferences"), "Restore changed device configuration");
        check(countBackups(root) == 1 && !CloudSaveArchive.applyPending(root), "Restore was not idempotent");
        File backup = root.listFiles(file -> file.getName().startsWith(".play-cloud-backup-"))[0];
        check(read(backup, "backup/save/hero").equals("newer local progress"), "Original local progress was not preserved");
        reject(root, zip("../outside", new byte[]{1}, true));
        reject(root, zip("save/../../outside", new byte[]{1}, true));
        reject(root, zip("save\\outside", new byte[]{1}, true));
        reject(root, zip("save/a:b", new byte[]{1}, true));
        reject(root, zip("sil_sdl.json", new byte[]{1}, true));
        reject(root, zip("save/hero", new byte[]{1}, false));
        reject(root, new byte[]{1, 2, 3});
        reject(root, new byte[CloudSaveArchive.MAX_BYTES + 1]);
        reject(root, Arrays.copyOf(original, original.length - 10));
        reject(root, zip("save/bomb", new byte[65 * 1024 * 1024], true));
        check(!new File(root.getParentFile(), "outside").exists(), "Archive escaped profile root");

        // Simulate a process dying after save/ was activated but before meta/.
        File interrupted = new File(root.getParentFile(), "interrupted");
        interrupted.mkdirs();
        put(interrupted, "save/hero", "before transaction");
        put(interrupted, "meta/meta.raw", "before Tale");
        CloudSaveArchive.stageRestore(interrupted, original);
        File txn = new File(interrupted, ".play-cloud-restore");
        put(txn, "original-save", ""); put(txn, "original-meta", ""); put(txn, "ready", "");
        new File(txn, "backup").mkdirs(); new File(txn, "incoming/meta").mkdirs();
        check(new File(interrupted, "save").renameTo(new File(txn, "backup/save")), "Fixture save move failed");
        check(new File(interrupted, "meta").renameTo(new File(txn, "backup/meta")), "Fixture meta move failed");
        put(interrupted, "save/hero", "partially activated cloud save");
        // A broken pending archive must still roll the interrupted transaction back.
        put(interrupted, ".play-cloud-pending.zip", "corrupt after crash");
        try { CloudSaveArchive.applyPending(interrupted); throw new AssertionError("Corrupt pending restore accepted"); }
        catch (IOException expected) {}
        check(read(interrupted, "save/hero").equals("before transaction"), "Interrupted restore did not recover save");
        check(read(interrupted, "meta/meta.raw").equals("before Tale"), "Interrupted restore did not recover Tale");

        File tooLarge = new File(root.getParentFile(), "large");
        new File(tooLarge, "save").mkdirs();
        byte[] random = new byte[4 * 1024 * 1024]; new Random(7).nextBytes(random);
        Files.write(new File(tooLarge, "save/hero").toPath(), random);
        try { CloudSaveArchive.capture(tooLarge); throw new AssertionError("Oversized cloud save accepted"); }
        catch (IOException expected) {}
        check(new File(tooLarge, "save/hero").length() == random.length, "Size failure changed local progress");
        System.out.println("PASS: complete profile, deterministic capture, representation-independent fingerprints, validation, size bounds, local backup, atomic activation and crash rollback");
    }
    public static void main(String[] args) throws Exception {
        if (args.length == 2 && args[0].equals("measure")) {
            byte[] bytes = CloudSaveArchive.capture(new File(args[1]));
            System.out.println("{\"compressed_bytes\":" + bytes.length + ",\"google_limit_bytes\":" + CloudSaveArchive.MAX_BYTES + "}");
        } else tests(new File(args[0]));
    }
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", type=Path)
    args = parser.parse_args()
    jdk = Path("C:/Program Files/BellSoft/LibericaJDK-17/bin")
    javac = str(jdk / "javac.exe") if jdk.exists() else shutil.which("javac")
    java = str(jdk / "java.exe") if jdk.exists() else shutil.which("java")
    if not javac or not java:
        parser.error("A JDK is required")
    with tempfile.TemporaryDirectory(prefix="sil-cloud-") as temporary:
        work = Path(temporary)
        fixture = work / "CloudSaveFixture.java"
        fixture.write_text(FIXTURE, encoding="utf-8")
        subprocess.run([javac, "-d", str(work), str(SOURCE), str(fixture)], check=True)
        command = [java, "-ea", "-cp", str(work), "com.silqh.silmore.CloudSaveFixture"]
        subprocess.run([*command, str(work / "profile")], check=True)
        if args.profile:
            result = subprocess.run([*command, "measure", str(args.profile.resolve())], check=True, text=True, capture_output=True)
            receipt = json.loads(result.stdout)
            receipt["profile"] = str(args.profile.resolve())
            print(json.dumps(receipt))


if __name__ == "__main__":
    main()
