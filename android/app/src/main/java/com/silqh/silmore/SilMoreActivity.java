package com.silqh.silmore;

import android.content.pm.ActivityInfo;
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;

public class SilMoreActivity extends SDLActivity {
	private volatile int gameRequestedOrientation =
			ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		applyFullscreenLayout();
	}

	@Override
	protected void onResume() {
		super.onResume();
		applyFullscreenLayout();
		applyGameOrientation();
	}

	@Override
	public void onWindowFocusChanged(boolean hasFocus) {
		super.onWindowFocusChanged(hasFocus);
		if (hasFocus) {
			applyFullscreenLayout();
		}
	}

	@Override
	public void setOrientationBis(int w, int h, boolean resizable, String hint) {
		int orientation = getGameOrientationFromHint(hint);

		if (orientation != ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED) {
			requestGameOrientation(
					orientation == ActivityInfo.SCREEN_ORIENTATION_USER_PORTRAIT);
		} else if (gameRequestedOrientation !=
				ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED) {
			applyGameOrientation();
		} else {
			super.setOrientationBis(w, h, resizable, hint);
		}
	}

    /** Called from native code after it updates SDL_HINT_ORIENTATIONS. */
    public void requestGameOrientation(final boolean portrait) {
		gameRequestedOrientation = portrait
				? ActivityInfo.SCREEN_ORIENTATION_USER_PORTRAIT
				: ActivityInfo.SCREEN_ORIENTATION_USER_LANDSCAPE;
		applyGameOrientation();
    }

	private int getGameOrientationFromHint(String hint) {
		if (hint == null) {
			return ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;
		}

		boolean landscape = hint.contains("LandscapeLeft")
				|| hint.contains("LandscapeRight");
		boolean portrait = hint.contains("Portrait");

		if (landscape == portrait) {
			return ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;
		}
		return portrait
				? ActivityInfo.SCREEN_ORIENTATION_USER_PORTRAIT
				: ActivityInfo.SCREEN_ORIENTATION_USER_LANDSCAPE;
	}

	private void applyGameOrientation() {
		final int orientation = gameRequestedOrientation;
		if (orientation == ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED) {
			return;
		}

        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                setRequestedOrientation(orientation);
            }
        });
	}

	private void applyFullscreenLayout() {
		View decorView = getWindow().getDecorView();

		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
			WindowManager.LayoutParams attrs = getWindow().getAttributes();
			attrs.layoutInDisplayCutoutMode =
					WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
			getWindow().setAttributes(attrs);
		}

		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
			getWindow().setDecorFitsSystemWindows(false);
			WindowInsetsController controller = decorView.getWindowInsetsController();
			if (controller != null) {
				controller.hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
				controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
			}
		} else {
			decorView.setSystemUiVisibility(
					View.SYSTEM_UI_FLAG_FULLSCREEN
							| View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
							| View.SYSTEM_UI_FLAG_LAYOUT_STABLE
							| View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
							| View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
							| View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
			getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
		}
	}

	@Override
	protected String[] getLibraries() {
		return new String[] { "SDL3", "main" };
	}
}
