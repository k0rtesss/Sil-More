#!/usr/bin/env python3
"""Run production Android cloud controller against disposable Java provider stubs.

No account, emulator, device, network, or real player profile is used. Provider
callbacks and Android timers are explicitly controlled; archive/state/controller
sources are compiled unchanged. This checks ordering, not Google SDK behavior.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMMON = ROOT / "android/app/src/main/java/com/silqh/silmore"
PLAY = ROOT / "android/app/src/play/java/com/silqh/silmore"

SOURCES = {
"fixture/Env.java": r'''
package fixture;
import java.util.*;
import java.util.concurrent.*;
public final class Env {
 public static final Queue<Runnable> queue=new ConcurrentLinkedQueue<>();
 public static final Map<Runnable,Long> timers=new LinkedHashMap<>();
 public static long now;
 public static void post(Runnable r){queue.add(r);}
 public static void drain(){for(int i=0;i<10000;i++){Runnable r=queue.poll();if(r==null)return;r.run();}throw new AssertionError("Callback loop");}
 public static void advance(long n){now+=n;List<Runnable> due=new ArrayList<>();for(var e:timers.entrySet())if(e.getValue()<=now)due.add(e.getKey());for(Runnable r:due){timers.remove(r);post(r);}drain();}
 public static void clear(){queue.clear();timers.clear();now=0;}
}
''',
"android/os/Looper.java": "package android.os; public class Looper { public static Looper getMainLooper(){return new Looper();}}",
"android/os/SystemClock.java": "package android.os; public class SystemClock {public static long elapsedRealtime(){return fixture.Env.now;}}",
"android/os/Handler.java": r'''package android.os; public class Handler {public Handler(Looper l){} public void postDelayed(Runnable r,long ms){fixture.Env.timers.put(r,fixture.Env.now+ms);}public void removeCallbacks(Runnable r){fixture.Env.timers.remove(r);}}''',
"android/content/DialogInterface.java": r'''package android.content; public interface DialogInterface {interface OnClickListener{void onClick(DialogInterface d,int which);}interface OnCancelListener{void onCancel(DialogInterface d);}}''',
"android/app/AlertDialog.java": r'''
package android.app;
import android.content.DialogInterface;
public class AlertDialog implements DialogInterface {
 public static AlertDialog last;public static int shows;public String title;public DialogInterface.OnClickListener positive,neutral,negative,item;public DialogInterface.OnCancelListener cancel;
 public void dismiss(){}public void choose(int which){(which==1?positive:which==0?neutral:negative).onClick(this,which);}
 public static class Builder {
  final AlertDialog d=new AlertDialog();public Builder(Object a){}public Builder setTitle(String s){d.title=s;return this;}public Builder setMessage(String s){return this;}
  public Builder setPositiveButton(String s,DialogInterface.OnClickListener l){d.positive=l;return this;}public Builder setNeutralButton(String s,DialogInterface.OnClickListener l){d.neutral=l;return this;}
  public Builder setNegativeButton(String s,DialogInterface.OnClickListener l){d.negative=l;return this;}public Builder setOnCancelListener(DialogInterface.OnCancelListener l){d.cancel=l;return this;}
  public Builder setItems(String[] s,DialogInterface.OnClickListener l){d.item=l;return this;}public AlertDialog show(){last=d;shows++;return d;}
 }
}
''',
"android/util/Log.java": r'''package android.util; public class Log {public static int i(String t,String s){return 0;}public static int w(String t,String s){return 0;}public static int w(String t,String s,Throwable e){return 0;}public static int e(String t,String s,Throwable e){return 0;}}''',
"android/widget/Toast.java": r'''package android.widget;public class Toast {public static int LENGTH_LONG=1;public static Toast makeText(Object a,String s,int n){return new Toast();}public void show(){}}''',
"com/google/android/gms/tasks/Task.java": r'''
package com.google.android.gms.tasks;
import java.util.*;import java.util.function.Consumer;
public class Task<T> {boolean done;boolean ok=true;T value;List<Consumer<Task<T>>> listeners=new ArrayList<>();
 public static <T> Task<T> ready(T v){Task<T> t=new Task<>();t.complete(v);return t;}
 public Task<T> addOnCompleteListener(Consumer<Task<T>> l){if(done)fixture.Env.post(()->l.accept(this));else listeners.add(l);return this;}
 public boolean isSuccessful(){return ok;}public T getResult(){return value;}
 public void complete(T v){value=v;done=true;for(var l:listeners)fixture.Env.post(()->l.accept(this));listeners.clear();}
}
''',
"com/google/android/gms/games/snapshot/SnapshotMetadataChange.java": "package com.google.android.gms.games.snapshot; public class SnapshotMetadataChange {public static class Builder {public Builder setDescription(String s){return this;}public SnapshotMetadataChange build(){return new SnapshotMetadataChange();}}}",
"com/google/android/gms/games/snapshot/Snapshot.java": r'''
package com.google.android.gms.games.snapshot;
public class Snapshot {public byte[] bytes;public boolean discarded;public Snapshot(byte[] b){bytes=b.clone();}public Contents getSnapshotContents(){return new Contents();}public Metadata getMetadata(){return new Metadata();}
 public class Contents {public byte[] readFully() throws java.io.IOException{return bytes.clone();}public boolean writeBytes(byte[] b){bytes=b.clone();return true;}}
 public static class Metadata {public long getLastModifiedTimestamp(){return 1;}}
}
''',
"com/google/android/gms/games/SnapshotsClient.java": r'''
package com.google.android.gms.games;
import com.google.android.gms.tasks.Task;import com.google.android.gms.games.snapshot.*;
public class SnapshotsClient {
 public static int RESOLUTION_POLICY_MANUAL=0;
 public byte[] remote=new byte[0],conflictingRemote;public int opens,commits,discards,resolves;public boolean holdOpen,holdCommit;public Task<DataOrConflict<Snapshot>> pendingOpen;public Task<Object> pendingCommit;public Snapshot lastOpen;public SnapshotConflict lastConflict;public DataOrConflict<Snapshot> lastResult;
 public Task<DataOrConflict<Snapshot>> open(String n,boolean create,int p){opens++;lastOpen=new Snapshot(remote);lastConflict=conflictingRemote==null?null:new SnapshotConflict(lastOpen,new Snapshot(conflictingRemote));lastResult=lastConflict==null?new DataOrConflict<>(lastOpen):new DataOrConflict<>(lastConflict);if(holdOpen){pendingOpen=new Task<>();return pendingOpen;}return Task.ready(lastResult);}
 public void releaseOpen(){pendingOpen.complete(lastResult);}
 public Task<Object> commitAndClose(Snapshot s,SnapshotMetadataChange c){commits++;remote=s.bytes.clone();if(holdCommit){pendingCommit=new Task<>();return pendingCommit;}return Task.ready(new Object());}
 public Task<Object> discardAndClose(Snapshot s){s.discarded=true;discards++;return Task.ready(new Object());}
 public Task<DataOrConflict<Snapshot>> resolveConflict(String id,Snapshot s){resolves++;remote=s.bytes.clone();conflictingRemote=null;return Task.ready(new DataOrConflict<>(new Snapshot(remote)));}
 public static class DataOrConflict<T>{T data;SnapshotConflict conflict;public DataOrConflict(T d){data=d;}public DataOrConflict(SnapshotConflict c){conflict=c;}public boolean isConflict(){return conflict!=null;}public T getData(){return data;}public SnapshotConflict getConflict(){return conflict;}}
 public static class SnapshotConflict {final Snapshot first,second;public SnapshotConflict(Snapshot a,Snapshot b){first=a;second=b;}public Snapshot getSnapshot(){return first;}public Snapshot getConflictingSnapshot(){return second;}public String getConflictId(){return "conflict";}}
}
''',
"com/google/android/gms/games/PlayGames.java": r'''
package com.google.android.gms.games;
import com.google.android.gms.tasks.Task;
public class PlayGames {
 public static boolean authenticated=true;public static String account="A";public static int signIns,playerCalls,holdPlayerCall;public static Task<Player> pendingPlayer;public static SnapshotsClient snapshots=new SnapshotsClient();
 public static SignIn getGamesSignInClient(Object a){return new SignIn();}public static Players getPlayersClient(Object a){return new Players();}public static SnapshotsClient getSnapshotsClient(Object a){return snapshots;}
 public static class SignIn {public Task<Auth> isAuthenticated(){return Task.ready(new Auth());}public Task<Auth> signIn(){signIns++;return Task.ready(new Auth());}}
 public static class Auth{public boolean isAuthenticated(){return authenticated;}}
 public static class Player{String id;public Player(String s){id=s;}public String getPlayerId(){return id;}}
 public static class Players{public Task<Player> getCurrentPlayer(){playerCalls++;if(playerCalls==holdPlayerCall){pendingPlayer=new Task<>();return pendingPlayer;}return Task.ready(new Player(account));}}
 public static void reset(){authenticated=true;account="A";signIns=playerCalls=holdPlayerCall=0;pendingPlayer=null;snapshots=new SnapshotsClient();}
}
''',
"com/silqh/silmore/SilMoreActivity.java": r'''package com.silqh.silmore;public class SilMoreActivity {boolean finishing,destroyed;public void runOnUiThread(Runnable r){fixture.Env.post(r);}public boolean isFinishing(){return finishing;}public boolean isDestroyed(){return destroyed;}public void finish(){finishing=true;}}''',
"com/silqh/silmore/PlayGamesApplication.java": "package com.silqh.silmore;public class PlayGamesApplication {static boolean initialized=true;}",
"com/silqh/silmore/CloudSyncFixture.java": r'''
package com.silqh.silmore;
import fixture.Env;import java.io.*;import java.nio.file.*;import java.util.*;import java.util.concurrent.*;import java.util.function.BooleanSupplier;
import com.google.android.gms.games.*;import android.app.AlertDialog;
public class CloudSyncFixture {
 static File home;static int passed;static List<String> failures=new ArrayList<>();
 static void check(boolean v,String m){if(!v)throw new AssertionError(m);}
 static void until(BooleanSupplier b,String m)throws Exception{long end=System.nanoTime()+TimeUnit.SECONDS.toNanos(20);while(!b.getAsBoolean()){Env.drain();if(System.nanoTime()>end)throw new AssertionError("Timed out: "+m);Thread.sleep(2);}Env.drain();}
 static Object field(Object o,String n){try{var f=o.getClass().getDeclaredField(n);f.setAccessible(true);return f.get(o);}catch(Exception e){throw new RuntimeException(e);}}
 static boolean idle(PlayGamesCloudSaves c){return field(c,"current")==null;}
 static File root(String n)throws Exception{File r=new File(home,n);r.mkdirs();return r;}
 static void put(File r,String value)throws Exception{File f=new File(r,"save/hero");f.getParentFile().mkdirs();Files.writeString(f.toPath(),value);}
 static String read(File r)throws Exception{return Files.readString(new File(r,"save/hero").toPath());}
 static byte[] archive(String n,String value)throws Exception{File r=root(n);put(r,value);return CloudSaveArchive.capture(r);}
 static String fingerprint(File root,byte[] b)throws Exception{return CloudSaveArchive.inspect(root,b).fingerprint;}
 static FutureTask<Boolean> prepare(PlayGamesCloudSaves c,File r){FutureTask<Boolean> f=new FutureTask<>(()->c.prepare(r.getPath()));new Thread(f).start();return f;}
 static void stop(PlayGamesCloudSaves c){c.onDestroy();Env.drain();}
 static void test(String n,Checked action)throws Exception{Env.clear();PlayGames.reset();AlertDialog.last=null;AlertDialog.shows=0;try{action.run();passed++;System.out.println("PASS "+n);}catch(Throwable e){failures.add(n+": "+e);System.out.println("FAIL "+n+": "+e);}finally{Env.clear();}}
 interface Checked{void run()throws Exception;}
 public static void main(String[] args)throws Exception {
  home=new File(args[0]);
  test("startup download applied before prepare returns",()->{
   File r=root("startup");PlayGames.snapshots.remote=archive("source","cloud");var c=new PlayGamesCloudSaves(new SilMoreActivity());var f=prepare(c,r);until(f::isDone,"startup");check(f.get()&&read(r).equals("cloud"),"Startup returned before activation");check(new CloudSaveSyncState(r).action("A",CloudSaveArchive.inspect(r,CloudSaveArchive.capture(r)),CloudSaveArchive.inspect(r,PlayGames.snapshots.remote))==CloudSaveSyncState.Action.SAME,"Wrong common ancestor");stop(c);
  });
  test("upload immutable capture and coalesced newer checkpoint",()->{
   File r=root("coalesce");put(r,"one");var c=new PlayGamesCloudSaves(new SilMoreActivity());c.onResume();PlayGames.snapshots.holdCommit=true;c.checkpoint(r.getPath());until(()->PlayGames.snapshots.commits==1,"first upload");put(r,"two");c.checkpoint(r.getPath());Env.drain();check(fingerprint(r,PlayGames.snapshots.remote).equals(fingerprint(r,archive("one","one"))),"First upload read mutable profile");PlayGames.snapshots.pendingCommit.complete(new Object());until(()->{Env.advance(0);return PlayGames.snapshots.commits==2;},"coalesced upload");PlayGames.snapshots.pendingCommit.complete(new Object());until(()->idle(c),"upload completion");check(fingerprint(r,PlayGames.snapshots.remote).equals(fingerprint(r,CloudSaveArchive.capture(r))),"Latest checkpoint lost");stop(c);
  });
  test("paused checkpoint coalesces after in-flight upload",()->{
   File r=root("paused");put(r,"one");var c=new PlayGamesCloudSaves(new SilMoreActivity());c.onResume();PlayGames.snapshots.holdCommit=true;c.checkpoint(r.getPath());until(()->PlayGames.snapshots.commits==1,"first upload");c.onPause();put(r,"background-final");c.checkpoint(r.getPath());Env.drain();PlayGames.snapshots.pendingCommit.complete(new Object());until(()->{Env.advance(0);return PlayGames.snapshots.commits==2;},"background checkpoint submission after first upload");PlayGames.snapshots.pendingCommit.complete(new Object());until(()->idle(c),"second completion");stop(c);
  });
  test("common ancestor chooses upload/download and rejects divergence",()->{
   File r=root("ancestry");byte[] base=archive("base","base"),local=archive("local","local"),remote=archive("remote","remote");var s=new CloudSaveSyncState(r);var b=CloudSaveArchive.inspect(r,base);var l=CloudSaveArchive.inspect(r,local);var p=CloudSaveArchive.inspect(r,remote);s.remember("A",b.fingerprint);check(s.action("A",l,b)==CloudSaveSyncState.Action.UPLOAD,"Local change not uploaded");check(s.action("A",b,p)==CloudSaveSyncState.Action.DOWNLOAD,"Remote change not downloaded");check(s.action("A",l,p)==CloudSaveSyncState.Action.CHOOSE,"Divergence overwritten");check(s.action("B",l,p)==CloudSaveSyncState.Action.CHOOSE,"New account inherited old ancestor");
  });
  test("automatic unauthenticated checks never launch sign-in",()->{
   File r=root("offline");put(r,"local");PlayGames.authenticated=false;var c=new PlayGamesCloudSaves(new SilMoreActivity());c.checkpoint(r.getPath());until(()->idle(c)&&PlayGames.signIns==0,"auto offline");var f=prepare(c,r);until(()->field(c,"current")!=null,"startup auth");Env.advance(15000);until(f::isDone,"startup timeout");check(PlayGames.signIns==0&&PlayGames.snapshots.opens==0,"Automatic flow launched sign-in");stop(c);
  });
  test("timeout discards late opened snapshot",()->{
   File r=root("late-open");put(r,"local");var c=new PlayGamesCloudSaves(new SilMoreActivity());PlayGames.snapshots.holdOpen=true;c.checkpoint(r.getPath());until(()->PlayGames.snapshots.pendingOpen!=null,"held open");Env.advance(15000);check(idle(c),"Timeout did not finish");PlayGames.snapshots.releaseOpen();Env.drain();check(PlayGames.snapshots.discards==1&&PlayGames.snapshots.commits==0,"Late snapshot not discarded");stop(c);
  });
  test("uncertain late commit never advances common ancestor",()->{
   File r=root("late-commit");put(r,"local");var c=new PlayGamesCloudSaves(new SilMoreActivity());PlayGames.snapshots.holdCommit=true;c.checkpoint(r.getPath());until(()->PlayGames.snapshots.commits==1,"held commit");Env.advance(15000);PlayGames.snapshots.pendingCommit.complete(new Object());Env.drain();check(!new File(r,".play-cloud-sync").exists(),"Late callback established unconfirmed ancestor");stop(c);
  });
  test("account switch blocks startup restore publication",()->{
   File r=root("account");PlayGames.snapshots.remote=archive("account-source","remote");PlayGames.holdPlayerCall=2;var c=new PlayGamesCloudSaves(new SilMoreActivity());var f=prepare(c,r);until(()->PlayGames.pendingPlayer!=null,"restore account check");PlayGames.account="B";PlayGames.pendingPlayer.complete(new PlayGames.Player("B"));until(f::isDone,"account mismatch");check(!new File(r,"save/hero").exists()&&!new File(r,".play-cloud-pending.zip").exists(),"Wrong account restore published");stop(c);
  });
  test("completed reset stays local",()->{
   File r=root("reset");Files.writeString(new File(r,".sil-more-progress-reset").toPath(),"complete\n");byte[] remote=archive("reset-source","old-cloud");PlayGames.snapshots.remote=remote;var c=new PlayGamesCloudSaves(new SilMoreActivity());var f=prepare(c,r);until(()->AlertDialog.last!=null&&"Cloud sync needs attention".equals(AlertDialog.last.title),"reset contextual recovery");check(AlertDialog.last.positive==null&&!f.isDone(),"Empty reset profile offered cloud overwrite or skipped choice");AlertDialog.last.choose(-1);until(f::isDone,"reset continue locally");c.checkpoint(r.getPath());c.onResume();Env.drain();Env.advance(300000);check(!new File(r,"save/hero").exists()&&PlayGames.snapshots.commits==0&&Arrays.equals(remote,PlayGames.snapshots.remote),"Reset was undone or cloud erased");stop(c);
  });
  test("startup divergence requires choice and offline preserves both",()->{
   File r=root("diverged");put(r,"local");byte[] base=archive("diverged-base","base");new CloudSaveSyncState(r).remember("A",fingerprint(r,base));byte[] remote=archive("diverged-remote","remote");PlayGames.snapshots.remote=remote;var c=new PlayGamesCloudSaves(new SilMoreActivity());var f=prepare(c,r);until(()->AlertDialog.last!=null&&"Choose progress to keep".equals(AlertDialog.last.title),"divergence choice");check(!f.isDone()&&PlayGames.snapshots.commits==0,"Divergence auto-resolved");AlertDialog.last.choose(-1);until(f::isDone,"offline choice");check(read(r).equals("local")&&Arrays.equals(remote,PlayGames.snapshots.remote),"Offline choice changed progress");stop(c);
  });
  test("contextual startup restore activates before return and keeps backup",()->{
   File r=root("context-restore");put(r,"old-local");Files.writeString(new File(r,".play-cloud-sync").toPath(),"broken-state");PlayGames.snapshots.remote=archive("context-cloud","cloud");var c=new PlayGamesCloudSaves(new SilMoreActivity());var f=prepare(c,r);until(()->AlertDialog.last!=null&&"Cloud sync needs attention".equals(AlertDialog.last.title),"contextual restore recovery");AlertDialog.last.choose(0);until(()->AlertDialog.last!=null&&"Restore cloud progress?".equals(AlertDialog.last.title),"restore confirmation");check(read(r).equals("old-local")&&!f.isDone(),"Restore changed live files before confirmation");AlertDialog.last.choose(1);until(f::isDone,"contextual startup restore");File[] backups=r.listFiles(file->file.getName().startsWith(".play-cloud-backup-"));check(f.get()&&read(r).equals("cloud")&&!new File(r,".play-cloud-pending.zip").exists(),"Startup returned before activating cloud restore");check(backups.length==1&&Files.readString(new File(backups[0],"backup/save/hero").toPath()).equals("old-local")&&PlayGames.snapshots.commits==0,"Restore lost original profile or wrote cloud");stop(c);
  });
  test("defaults after failed startup cannot overwrite existing cloud",()->{
   File r=root("late-auth");byte[] remote=archive("late-auth-cloud","existing-cloud-character");PlayGames.snapshots.remote=remote;PlayGames.authenticated=false;var c=new PlayGamesCloudSaves(new SilMoreActivity());var f=prepare(c,r);until(()->field(c,"current")!=null,"startup authentication");Env.advance(15000);until(f::isDone,"offline startup");File defaults=new File(r,"meta/achievements.json");defaults.getParentFile().mkdirs();Files.writeString(defaults.toPath(),"{\"unlocked\":{}}");PlayGames.authenticated=true;c.checkpoint(r.getPath());until(()->Boolean.TRUE.equals(field(c,"needsAttention")),"post-startup default conflict");check(PlayGames.snapshots.commits==0&&Arrays.equals(remote,PlayGames.snapshots.remote),"New defaults silently replaced existing cloud profile");stop(c);
  });
  test("known ancestor protects missing and deliberately empty cloud",()->{
   File r=root("cloud-deleted");put(r,"previous-common");byte[] local=CloudSaveArchive.capture(r);var profile=CloudSaveArchive.inspect(r,local);var state=new CloudSaveSyncState(r);state.remember("A",profile.fingerprint);byte[] empty=CloudSaveArchive.capture(root("empty-cloud"));var emptyProfile=CloudSaveArchive.inspect(r,empty);check(state.action("A",profile,null)==CloudSaveSyncState.Action.CHOOSE,"Missing cloud with known ancestor was overwritten");check(state.action("A",profile,emptyProfile)==CloudSaveSyncState.Action.CHOOSE,"Empty cloud with known ancestor was overwritten");PlayGames.snapshots.remote=empty;var c=new PlayGamesCloudSaves(new SilMoreActivity());c.checkpoint(r.getPath());until(()->Boolean.TRUE.equals(field(c,"needsAttention")),"empty-cloud attention");check(PlayGames.snapshots.commits==0&&Arrays.equals(empty,PlayGames.snapshots.remote),"Controller resurrected intentionally emptied cloud");stop(c);
  });
  test("background provider conflict closes both snapshots without dialog",()->{
   File r=root("background-conflict");put(r,"local");byte[] first=archive("background-first","first-cloud"),second=archive("background-second","second-cloud");PlayGames.snapshots.remote=first;PlayGames.snapshots.conflictingRemote=second;var c=new PlayGamesCloudSaves(new SilMoreActivity());c.onPause();c.checkpoint(r.getPath());until(()->Boolean.TRUE.equals(field(c,"needsAttention")),"background conflict attention");var conflict=PlayGames.snapshots.lastConflict;check(conflict.getSnapshot().discarded&&conflict.getConflictingSnapshot().discarded,"Conflict handles were not both closed");check(AlertDialog.shows==0&&PlayGames.snapshots.commits==0&&PlayGames.snapshots.resolves==0,"Background conflict opened UI or mutated cloud");check(Arrays.equals(first,PlayGames.snapshots.remote)&&Arrays.equals(second,PlayGames.snapshots.conflictingRemote)&&read(r).equals("local"),"Background conflict changed progress");stop(c);
  });
  test("startup provider conflict applies chosen validated cloud profile",()->{
   File r=root("startup-conflict");PlayGames.snapshots.remote=archive("startup-first","first-cloud");PlayGames.snapshots.conflictingRemote=archive("startup-second","chosen-second-cloud");var c=new PlayGamesCloudSaves(new SilMoreActivity());var f=prepare(c,r);until(()->AlertDialog.last!=null&&"Two cloud versions need a choice".equals(AlertDialog.last.title),"cloud conflict choice");check(!f.isDone()&&PlayGames.snapshots.resolves==0,"Conflict was resolved without choice");AlertDialog.last.item.onClick(AlertDialog.last,1);until(f::isDone,"selected cloud startup");check(f.get()&&read(r).equals("chosen-second-cloud")&&PlayGames.snapshots.resolves==1&&PlayGames.snapshots.commits==0,"Selected cloud profile was not applied safely");stop(c);
  });
  test("cancel provider conflict preserves both cloud versions and local",()->{
   File r=root("cancel-conflict");put(r,"local");byte[] first=archive("cancel-first","first-cloud"),second=archive("cancel-second","second-cloud");PlayGames.snapshots.remote=first;PlayGames.snapshots.conflictingRemote=second;var c=new PlayGamesCloudSaves(new SilMoreActivity());var f=prepare(c,r);until(()->AlertDialog.last!=null&&"Two cloud versions need a choice".equals(AlertDialog.last.title),"cancellable cloud conflict");AlertDialog.last.cancel.onCancel(AlertDialog.last);until(f::isDone,"conflict cancellation");var conflict=PlayGames.snapshots.lastConflict;check(conflict.getSnapshot().discarded&&conflict.getConflictingSnapshot().discarded,"Cancel leaked a conflict handle");check(Arrays.equals(first,PlayGames.snapshots.remote)&&Arrays.equals(second,PlayGames.snapshots.conflictingRemote)&&read(r).equals("local")&&PlayGames.snapshots.resolves==0&&PlayGames.snapshots.commits==0,"Cancel changed cloud or local progress");stop(c);
  });
  test("explicit startup offline choice blocks later session uploads",()->{
   File r=root("offline-choice");PlayGames.authenticated=false;var c=new PlayGamesCloudSaves(new SilMoreActivity());var f=prepare(c,r);until(()->AlertDialog.last!=null&&"Google Play cloud sync".equals(AlertDialog.last.title),"startup offline button");AlertDialog.last.choose(-1);until(f::isDone,"deliberate offline startup");PlayGames.authenticated=true;put(r,"new-offline-character");c.checkpoint(r.getPath());c.onResume();Env.drain();Env.advance(300000);check(PlayGames.snapshots.opens==0&&PlayGames.snapshots.commits==0,"Explicit offline session later uploaded automatically");stop(c);
  });
  test("malformed cloud state needs explicit contextual backup to recover",()->{
   File r=root("malformed-state");put(r,"local");Files.writeString(new File(r,".play-cloud-sync").toPath(),"player=A\nbase=\\uNOPE\n");var c=new PlayGamesCloudSaves(new SilMoreActivity());c.checkpoint(r.getPath());until(()->PlayGames.playerCalls>0&&idle(c),"malformed automatic state refusal");check(PlayGames.snapshots.opens==0&&PlayGames.snapshots.commits==0,"Malformed state uploaded automatically");var f=prepare(c,r);until(()->AlertDialog.last!=null&&"Cloud sync needs attention".equals(AlertDialog.last.title),"contextual state recovery");check(PlayGames.snapshots.commits==0&&!f.isDone(),"Malformed state bypassed recovery choice");AlertDialog.last.choose(1);until(f::isDone,"contextual state recovery commit");check(PlayGames.snapshots.commits==1&&new CloudSaveSyncState(r).action("A",CloudSaveArchive.inspect(r,CloudSaveArchive.capture(r)),CloudSaveArchive.inspect(r,PlayGames.snapshots.remote))==CloudSaveSyncState.Action.SAME,"Explicit backup did not repair common state");stop(c);
  });
  test("invalid conflict selection is rejected before provider resolution",()->{
   File r=root("invalid-conflict");put(r,"local");byte[] first=archive("invalid-conflict-first","valid-cloud"),second=new byte[]{1,2,3};PlayGames.snapshots.remote=first;PlayGames.snapshots.conflictingRemote=second;var c=new PlayGamesCloudSaves(new SilMoreActivity());var f=prepare(c,r);until(()->AlertDialog.last!=null&&"Two cloud versions need a choice".equals(AlertDialog.last.title),"invalid conflict choice");AlertDialog.last.item.onClick(AlertDialog.last,1);until(()->AlertDialog.last!=null&&"Cloud sync needs attention".equals(AlertDialog.last.title),"invalid conflict contextual recovery");var conflict=PlayGames.snapshots.lastConflict;check(PlayGames.snapshots.resolves==0&&PlayGames.snapshots.commits==0,"Invalid selected archive replaced the valid cloud version");check(Arrays.equals(first,PlayGames.snapshots.remote)&&Arrays.equals(second,PlayGames.snapshots.conflictingRemote)&&read(r).equals("local"),"Invalid selection changed cloud/local progress");check(conflict.getSnapshot().discarded&&conflict.getConflictingSnapshot().discarded,"Invalid selection leaked conflict handles");AlertDialog.last.choose(-1);until(f::isDone,"invalid conflict continue locally");stop(c);
  });
  test("damaged cloud is repaired only by confirmed contextual backup",()->{
   File r=root("damaged-backup");put(r,"valid-local");byte[] damaged=new byte[]{1,2,3};PlayGames.snapshots.remote=damaged;var c=new PlayGamesCloudSaves(new SilMoreActivity());c.checkpoint(r.getPath());until(()->PlayGames.snapshots.opens==1&&idle(c),"automatic damaged-cloud refusal");check(PlayGames.snapshots.commits==0&&Arrays.equals(damaged,PlayGames.snapshots.remote),"Automatic sync replaced damaged cloud");var f=prepare(c,r);until(()->AlertDialog.last!=null&&"Cloud sync needs attention".equals(AlertDialog.last.title),"contextual damaged-cloud recovery");AlertDialog.last.choose(1);until(()->AlertDialog.last!=null&&"Replace cloud progress?".equals(AlertDialog.last.title),"damaged-cloud replacement confirmation");check(!f.isDone()&&PlayGames.snapshots.commits==0&&Arrays.equals(damaged,PlayGames.snapshots.remote),"Backup replaced damaged cloud before confirmation");AlertDialog.last.choose(1);until(f::isDone,"confirmed damaged-cloud repair");check(PlayGames.snapshots.commits==1&&fingerprint(r,PlayGames.snapshots.remote).equals(fingerprint(r,CloudSaveArchive.capture(r)))&&read(r).equals("valid-local"),"Confirmed contextual backup did not repair cloud safely");stop(c);
  });
  test("oversized local profile can recover cloud with original preserved",()->{
   File r=root("oversized-local");byte[] oversized=new byte[4*1024*1024];new Random(19).nextBytes(oversized);File hero=new File(r,"save/hero");hero.getParentFile().mkdirs();Files.write(hero.toPath(),oversized);PlayGames.snapshots.remote=archive("oversized-cloud","cloud");var c=new PlayGamesCloudSaves(new SilMoreActivity());var f=prepare(c,r);until(()->AlertDialog.last!=null&&"Cloud sync needs attention".equals(AlertDialog.last.title),"oversized local recovery");check(AlertDialog.last.positive==null&&PlayGames.snapshots.opens==0,"Oversized local profile was offered as a cloud backup");AlertDialog.last.choose(0);until(()->AlertDialog.last!=null&&"Restore cloud progress?".equals(AlertDialog.last.title),"oversized profile restore confirmation");AlertDialog.last.choose(1);until(f::isDone,"oversized profile restore");File[] backups=r.listFiles(file->file.getName().startsWith(".play-cloud-backup-"));check(f.get()&&read(r).equals("cloud")&&PlayGames.snapshots.commits==0,"Oversized local profile prevented startup restore");check(backups.length==1&&Arrays.equals(oversized,Files.readAllBytes(new File(backups[0],"backup/save/hero").toPath())),"Oversized original profile was not preserved");stop(c);
  });
  test("timed-out recovery commit cannot finish newer recovery prompt",()->{
   File r=root("recovery-late-commit");put(r,"local");File state=new File(r,".play-cloud-sync");Files.writeString(state.toPath(),"broken-state");PlayGames.snapshots.holdCommit=true;var c=new PlayGamesCloudSaves(new SilMoreActivity());var f=prepare(c,r);until(()->AlertDialog.last!=null&&"Cloud sync needs attention".equals(AlertDialog.last.title),"initial malformed-state recovery");AlertDialog.last.choose(1);until(()->PlayGames.snapshots.commits==1,"held explicit backup commit");var retired=field(c,"current");Env.advance(15000);check(AlertDialog.last!=null&&"Cloud sync needs attention".equals(AlertDialog.last.title),"Commit timeout did not reopen recovery");var renewed=field(c,"current");var prompt=AlertDialog.last;check(renewed!=retired&&!f.isDone(),"Timed-out recovery request was reused or startup completed");PlayGames.snapshots.pendingCommit.complete(new Object());Env.drain();check(!f.isDone()&&field(c,"current")==renewed&&AlertDialog.last==prompt&&Files.readString(state.toPath()).equals("broken-state"),"Late retired callback completed newer recovery or wrote a stale ancestor");prompt.choose(-1);until(f::isDone,"new recovery continue locally");check(f.get()&&PlayGames.snapshots.commits==1&&read(r).equals("local"),"Recovery completion latch did not release safely");stop(c);
  });
  System.out.println(passed+" scenarios passed; "+failures.size()+" failed");
  if(!failures.isEmpty()){for(String f:failures)System.err.println(f);System.exit(1);}System.exit(0);
 }
}
''',
}


def main():
    jdk = Path("C:/Program Files/BellSoft/LibericaJDK-17/bin")
    javac = str(jdk / "javac.exe") if jdk.exists() else shutil.which("javac")
    java = str(jdk / "java.exe") if jdk.exists() else shutil.which("java")
    if not javac or not java:
        raise SystemExit("A JDK with javac and java on PATH is required")
    with tempfile.TemporaryDirectory(prefix="sil-cloud-sync-") as temporary:
        work = Path(temporary)
        sources = []
        for name, content in SOURCES.items():
            path = work / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding="utf-8")
            sources.append(str(path))
        sources.extend(str(COMMON / name) for name in ("CloudSaveArchive.java", "CloudSaveSyncState.java"))
        sources.append(str(PLAY / "PlayGamesCloudSaves.java"))
        classes = work / "classes"
        subprocess.run([javac, "-d", str(classes), *sources], check=True)
        result = subprocess.run([java, "-cp", str(classes), "com.silqh.silmore.CloudSyncFixture", str(work / "profiles")])
        raise SystemExit(result.returncode)


if __name__ == "__main__":
    main()
