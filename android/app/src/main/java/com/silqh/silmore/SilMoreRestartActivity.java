package com.silqh.silmore;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Process;

/** No native library is loaded here. The new SDL process starts with fresh globals. */
public final class SilMoreRestartActivity extends Activity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        int oldPid = getIntent().getIntExtra("old_pid", -1);
        if (oldPid > 0 && oldPid != Process.myPid()) Process.killProcess(oldPid);
        Intent launch = new Intent(this, SilMoreActivity.class);
        launch.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
        startActivity(launch);
        finish();
        Process.killProcess(Process.myPid());
    }
}
