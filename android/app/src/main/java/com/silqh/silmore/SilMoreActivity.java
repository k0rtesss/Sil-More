package com.silqh.silmore;

import android.app.AlertDialog;
import android.content.pm.ActivityInfo;
import android.os.Build;
import android.os.Bundle;
import android.util.SparseArray;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.libsdl.app.SDLActivity;

public class SilMoreActivity extends SDLActivity {
	@Override
	protected void messageboxCreateAndShow(Bundle args) {
		String title = args.getString("title");
		if (!"Choose Orientation".equals(title) && !"Choose Font Size".equals(title)) {
			super.messageboxCreateAndShow(args);
			return;
		}

		// These choices precede the game renderer and its font settings.
		// Fit scalable Android text to the dialog's actual screen constraints.
		int padding = Math.round(20 * getResources().getDisplayMetrics().density);
		AlertDialog dialog = new AlertDialog.Builder(this).create();
		dialog.setCancelable(false);
		dialog.setOnDismissListener(unused -> {
			synchronized (messageboxSelection) {
				messageboxSelection.notify();
			}
		});

		LinearLayout content = new LinearLayout(this) {
			@Override
			protected void onMeasure(int widthSpec, int heightSpec) {
				TextView heading = (TextView) getChildAt(0);
				TextView message = (TextView) getChildAt(1);
				int naturalHeight = View.MeasureSpec.makeMeasureSpec(
						0, View.MeasureSpec.UNSPECIFIED);
				int availableHeight = View.MeasureSpec.getMode(heightSpec)
						== View.MeasureSpec.UNSPECIFIED ? Integer.MAX_VALUE
						: View.MeasureSpec.getSize(heightSpec);
				// Start large on every measure so rotation can enlarge text again.
				// Only text scales; native button dimensions stay unchanged.
				for (float bodySp = 22; bodySp >= 1; bodySp -= 0.5f) {
					heading.setTextSize(TypedValue.COMPLEX_UNIT_SP, bodySp * 26 / 22);
					message.setTextSize(TypedValue.COMPLEX_UNIT_SP, bodySp);
					super.onMeasure(widthSpec, naturalHeight);
					if (getMeasuredHeight() <= availableHeight) break;
				}
				super.onMeasure(widthSpec, heightSpec);
			}
		};
		content.setOrientation(LinearLayout.VERTICAL);
		content.setPadding(padding, padding, padding, padding);
		TextView heading = new TextView(this);
		heading.setText(title);
		heading.setTextSize(TypedValue.COMPLEX_UNIT_SP, 26);
		heading.setPadding(0, 0, 0, padding);
		content.addView(heading);
		TextView message = new TextView(this);
		message.setText(args.getString("message"));
		message.setTextSize(TypedValue.COMPLEX_UNIT_SP, 22);
		message.setPadding(0, 0, 0, padding);
		content.addView(message);

		int[] ids = args.getIntArray("buttonIds");
		int[] flags = args.getIntArray("buttonFlags");
		String[] labels = args.getStringArray("buttonTexts");
		SparseArray<Button> keys = new SparseArray<>();
		LinearLayout buttons = new LinearLayout(this);
		buttons.setOrientation(LinearLayout.HORIZONTAL);
		buttons.setGravity(Gravity.CENTER);
		for (int i = 0; i < labels.length; i++) {
			final int id = ids[i];
			Button button = new Button(this);
			button.setText(labels[i]);
			button.setOnClickListener(view -> {
				messageboxSelection[0] = id;
				dialog.dismiss();
			});
			if ((flags[i] & 1) != 0) keys.put(KeyEvent.KEYCODE_ENTER, button);
			if ((flags[i] & 2) != 0) keys.put(KeyEvent.KEYCODE_ESCAPE, button);
			buttons.addView(button);
		}
		content.addView(buttons);
		dialog.setView(content);
		dialog.setOnKeyListener((unused, keyCode, event) -> {
			Button button = keys.get(keyCode);
			if (button == null) return false;
			if (event.getAction() == KeyEvent.ACTION_UP) button.performClick();
			return true;
		});
		dialog.show();
	}

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
					Build.VERSION.SDK_INT >= Build.VERSION_CODES.R
							? WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS
							: WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
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
