#!/usr/bin/env python3
"""Compile the production Android achievement controller against provider doubles.

Checks multiword masks, partial provider mapping, retry and stale callbacks.
No Android device, Google account or network is used.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile

from check_cloud_sync import SOURCES as CLOUD_STUBS

ROOT = Path(__file__).resolve().parents[1]
PLAY = ROOT / "android/app/src/play/java/com/silqh/silmore/PlayGamesAchievements.java"

STUBS = {key: CLOUD_STUBS[key] for key in (
    "fixture/Env.java", "android/os/SystemClock.java", "android/util/Log.java",
    "android/widget/Toast.java", "com/google/android/gms/tasks/Task.java",
    "com/silqh/silmore/PlayGamesApplication.java")}
STUBS["com/google/android/gms/tasks/Task.java"] = STUBS["com/google/android/gms/tasks/Task.java"].replace(
    "public boolean isSuccessful()", "public void fail(){ok=false;complete(null);}public Exception getException(){return ok?null:new RuntimeException(\"offline\");}public boolean isSuccessful()")
STUBS.update({
    "com/silqh/silmore/SilMoreActivity.java": r'''
package com.silqh.silmore;
public class SilMoreActivity {
 public String getString(int resource){return resource==64?"":"ID"+resource;}
 public void startActivityForResult(Object intent,int request){}
}
''',
    "com/google/android/gms/games/achievement/Achievement.java": r'''
package com.google.android.gms.games.achievement;
public class Achievement {
 public static final int STATE_UNLOCKED=0;private final String id;
 public Achievement(String value){id=value;}public int getState(){return STATE_UNLOCKED;}
 public String getAchievementId(){return id;}
}
''',
    "com/google/android/gms/games/achievement/AchievementBuffer.java": r'''
package com.google.android.gms.games.achievement;
public class AchievementBuffer extends java.util.ArrayList<Achievement> {
 public void release(){}
}
''',
    "com/google/android/gms/games/PlayGames.java": r'''
package com.google.android.gms.games;
import com.google.android.gms.tasks.Task;
import com.google.android.gms.games.achievement.*;
import java.util.*;
public class PlayGames {
 public static String account="A";public static boolean hold;
 public static final Set<String> remote=new LinkedHashSet<>();
 public static final List<String> submitted=new ArrayList<>();
 public static Task<Object> pending;
 public static SignIn getGamesSignInClient(Object a){return new SignIn();}
 public static Players getPlayersClient(Object a){return new Players();}
 public static Client getAchievementsClient(Object a){return new Client();}
 public static class SignIn {
  public Task<Auth> isAuthenticated(){return Task.ready(new Auth());}
  public Task<Auth> signIn(){throw new AssertionError("Gameplay opened sign-in");}
 }
 public static class Auth{public boolean isAuthenticated(){return true;}}
 public static class Player{public String getPlayerId(){return account;}}
 public static class Players{public Task<Player> getCurrentPlayer(){return Task.ready(new Player());}}
 public static class Data {
  public AchievementBuffer get(){var b=new AchievementBuffer();for(String id:remote)b.add(new Achievement(id));return b;}
 }
 public static class Client {
  public Task<Data> load(boolean force){return Task.ready(new Data());}
  public Task<Object> unlockImmediate(String id){
   if(id.isEmpty())throw new AssertionError("Unmapped achievement submitted");
   submitted.add(id);if(hold){pending=new Task<>();return pending;}
   remote.add(id);return Task.ready(new Object());
  }
  public Task<Object> getAchievementsIntent(){return Task.ready(new Object());}
 }
}
''',
    "com/silqh/silmore/AchievementFixture.java": r'''
package com.silqh.silmore;
import fixture.Env;import com.google.android.gms.games.PlayGames;
public class AchievementFixture {
 static void check(boolean value,String why){if(!value)throw new AssertionError(why);}
 static void pump(PlayGamesAchievements c,long[] masks){c.sync(masks);Env.drain();}
 public static void main(String[] args){
  int count=Integer.parseInt(args[0]);
  long highMask=((1L<<(count-64))-1)&~1L;
  var c=new PlayGamesAchievements(new SilMoreActivity());
  PlayGames.remote.add("ID1");
  long[] masks={Long.MIN_VALUE,-1L,-1L};
  c.onResume();pump(c,masks);
  check(PlayGames.submitted.size()==1 && PlayGames.submitted.get(0).equals("ID63"),"Bit 63 aliases another index");
  check(c.unlockedMask(0)==(Long.MIN_VALUE|2),"First word lost sign bit or remote import");
  check(c.unlockedMask(1)==0,"Later word aliases first word");
  for(int i=65;i<count;i++)pump(c,masks);
  check(c.unlockedMask(1)==highMask,"Mapped indexes 65+ did not synchronize or invalid bits leaked");
  check(!PlayGames.submitted.contains("ID64") && !PlayGames.submitted.contains("ID0"),"Blank ID or wrapped shift submitted");
  check(c.unlockedMask(-1)==0 && c.unlockedMask(2)==0,"Invalid word accepted");

  // A new account needs a fresh upload; the provider acknowledgement may fail.
  c.onPause();PlayGames.account="B";PlayGames.remote.clear();PlayGames.hold=true;
  c.onResume();pump(c,masks);check(PlayGames.pending!=null,"No retry pending");
  var failed=PlayGames.pending;failed.fail();Env.drain();
  check(c.unlockedMask(0)==0 && c.unlockedMask(1)==0,"Failed upload was confirmed");
  int before=PlayGames.submitted.size();pump(c,masks);
  check(before==PlayGames.submitted.size(),"Backoff ignored");
  Env.advance(5000);PlayGames.hold=false;
  for(int i=0;i<count-64;i++)pump(c,masks);
  check(c.unlockedMask(0)==Long.MIN_VALUE && c.unlockedMask(1)==highMask,"Offline retry lost a word");

  c.onPause();PlayGames.account="C";PlayGames.remote.clear();PlayGames.hold=true;
  c.onResume();pump(c,masks);var stale=PlayGames.pending;
  c.onPause();stale.complete(new Object());Env.drain();
  check(c.unlockedMask(0)==0 && c.unlockedMask(1)==0,"Stale callback restored previous account bits");
  System.out.println("PASS production Play Games: bit 63/64+, partial IDs, remote import, bounds, offline retry and stale callbacks");
 }
}
''',
})


def main():
    jdk = Path("C:/Program Files/BellSoft/LibericaJDK-17/bin")
    javac = str(jdk / "javac.exe") if jdk.exists() else shutil.which("javac")
    java = str(jdk / "java.exe") if jdk.exists() else shutil.which("java")
    assert javac and java, "A JDK is required for the Android controller check"
    resources = re.findall(r"R\.string\.(play_ach_[a-z_]+)", PLAY.read_text())
    assert 64 < len(resources) < 128
    stubs = dict(STUBS)
    stubs["com/silmore/myapp/R.java"] = "package com.silmore.myapp;public class R {public static class string {" + "".join(
        f"public static final int {name}={i};" for i, name in enumerate(resources)) + "}}"
    with tempfile.TemporaryDirectory(prefix="sil-achievements-java-") as temp:
        work = Path(temp)
        sources = [str(PLAY)]
        for name, content in stubs.items():
            path = work / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding="utf-8")
            sources.append(str(path))
        subprocess.run([javac, "-d", str(work), *sources], check=True)
        subprocess.run([java, "-cp", str(work), "com.silqh.silmore.AchievementFixture", str(len(resources))], check=True, timeout=30)


if __name__ == "__main__":
    main()
