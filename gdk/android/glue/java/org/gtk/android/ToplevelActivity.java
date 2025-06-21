/*
 * Copyright (c) 2024-2025 Florian "sp1rit" <sp1rit@disroot.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

package org.gtk.android;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.ClipData;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Insets;
import android.graphics.PixelFormat;
import android.graphics.RectF;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.text.InputType;
import android.view.DragEvent;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.PointerIcon;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.UiThread;

import java.util.Arrays;
import java.util.logging.Level;
import java.util.logging.Logger;

public class ToplevelActivity extends Activity {
	public final static String toplevelIdentifierKey = "gdk/toplevel-identifier";

	public static class UnregisteredSurfaceException extends Exception {
		public UnregisteredSurfaceException(Object surface) {
			super(String.format("Surface \"%s\" was not registered within GDK", surface.toString()));
		}
	}

	public static class GdkContext {
		@GlibContext.GtkThread
		public static native void activate();
		@GlibContext.GtkThread
		public static native void open(Uri uri, String action);
	}

	public class ToplevelView extends ViewGroup {
		public class Surface extends SurfaceView implements SurfaceHolder.Callback {
			private Logger logger = Logger.getLogger("surface");

			private long surfaceIdentifier = 0;

			private RectF[] inputRegion = null;

			private ImContext activeImContext = null;

			@GlibContext.GtkThread
			private native void bindNative(long identifier) throws UnregisteredSurfaceException;
			@GlibContext.GtkThread
			private native void notifyAttached();
			@GlibContext.GtkThread
			private native void notifyLayoutSurface(int width, int height, float scale);
			@GlibContext.GtkThread
			private native void notifyLayoutPosition(int x, int y);
			@GlibContext.GtkThread
			private native void notifyDetached();

			@GlibContext.GtkThread
			private native void notifyDNDStartFailed(ClipboardProvider.NativeDragIdentifier identifier);

			@GlibContext.GtkThread
			private native void notifyKeyEvent(KeyEvent event);
			@GlibContext.GtkThread
			private native void notifyMotionEvent(int identifier, MotionEvent event);
			@GlibContext.GtkThread
			private native boolean notifyDragEvent(DragEvent event);

			@UiThread
			private native void notifyVisibility(boolean visible);

			@GlibContext.GtkThread
			public Surface(long identifier) {
				super(ToplevelActivity.this);
				setVisibility(GONE);

				try {
					bindNative(identifier);
					this.surfaceIdentifier = identifier;
				} catch (UnregisteredSurfaceException err) {
					logger.log(Level.WARNING, err.getMessage());
					drop();
				}

				setZOrderOnTop(true);
				getHolder().setFormat(PixelFormat.RGBA_8888);
				getHolder().addCallback(this);

				setFocusable(true);
				setFocusableInTouchMode(true);
			}

			private int queuedImeKeyboardState; // 0 not queued, 1 show keyboard, -1 hide keyboard
			@GlibContext.GtkThread
			public void setImeKeyboardState(boolean state) {
				boolean was_queued = queuedImeKeyboardState != 0;
				queuedImeKeyboardState = state ? 1 : -1;
				if (was_queued)
					return;
				GlibContext.runOnMain(() -> {
					boolean imeKeyboardState = queuedImeKeyboardState > 0;
					queuedImeKeyboardState = 0;
					runOnUiThread(() -> {
						WindowInsetsController controller = getWindowInsetsController();
						if (imeKeyboardState) {
							requestFocus();
							if (controller != null)
								controller.show(WindowInsets.Type.ime());
						 } else if (controller != null) {
							controller.hide(WindowInsets.Type.ime());
						 }
					});
				});
			}

			public void setVisibility(boolean visible) {
				runOnUiThread(() -> super.setVisibility(visible ? VISIBLE : GONE));
			}

			public void setInputRegion(@Nullable RectF[] region) {
				runOnUiThread(() -> {
					this.inputRegion = region;
				});
			}

			public void dropCursorIcon() {
				runOnUiThread(() -> setPointerIcon(null));
			}
			public void setCursorFromId(int cursor) {
				runOnUiThread(() -> setPointerIcon(PointerIcon.getSystemIcon(getContext(), cursor)));
			}
			public void setCursorFromBitmap(Bitmap cursor, float hotspot_x, float hotspot_y) {
				runOnUiThread(() -> setPointerIcon(PointerIcon.create(cursor, hotspot_x, hotspot_y)));
			}

			public void startDND(ClipData clipdata, View.DragShadowBuilder shadow, ClipboardProvider.NativeDragIdentifier identifier, int flags) {
				runOnUiThread(() -> {
					if (!startDragAndDrop(clipdata, shadow, identifier, flags))
						notifyDNDStartFailed(identifier);
				});
			}
			public void updateDND(View.DragShadowBuilder shadow) {
				runOnUiThread(() -> updateDragShadow(shadow));
			}
			public void cancelDND() {
				runOnUiThread(() -> cancelDragAndDrop());
			}

			public void setActiveImContext(ImContext context) {
				if (activeImContext == context)
					return;
				activeImContext = context;
				InputMethodManager imm = getSystemService(InputMethodManager.class);
				imm.restartInput(this);
			}

			public void reposition(int x, int y, int width, int height) {
				runOnUiThread(() -> setLayoutParams(new LayoutParams(x, y, width, height)));
			}

			public void drop() {
				//if (this != ToplevelView.this.toplevel)
					runOnUiThread(() -> ToplevelView.this.removeView(this));
				/*else
					logger.log(Level.SEVERE, "Attempted to drop toplevel from view");*/
			}

			private boolean motionEventProxy(MotionEvent event) {
				if (event == null)
					return false;

				if (this != ToplevelView.this.grabbed) { // grabbed surfaces get all events,
				                                         // regardless of if they are in the input
				                                         // region or not.
					if (this.inputRegion != null &&
							Arrays.stream(this.inputRegion)
									.noneMatch(region -> region.contains(event.getX(), event.getY())))
						return false;
				}

				int identitiy = System.identityHashCode(event);
				// interestingly, calling MotionEvent.obtain translates it into the current View
				// space. No idea where it gets that information from
				MotionEvent copy = MotionEvent.obtainNoHistory(event);
				GlibContext.runOnMain(() -> notifyMotionEvent(identitiy, copy));
				return true;
			}

			@Override
			public boolean onGenericMotionEvent(MotionEvent event) {
				return motionEventProxy(event);
			}
			@Override
			public boolean onHoverEvent(MotionEvent event) {
				return motionEventProxy(event);
			}
			@Override
			public boolean onTouchEvent(MotionEvent event) {
				return motionEventProxy(event);
			}
			// is there a need for onTrackballEvent?

			private boolean keyEventProxy(KeyEvent event) {
				if (event == null)
					return false;
				/* For some *mystical* reason, once we have an IME (InputConnection)
				 * attached, we receive back presses via onKeyDown/-Up, which we did
				 * not receive before. As we always return true, this results in
				 * onBackPressed() not being called, preventing us from closing the
				 * activity. Early exit in such cases, to ensure to have this handled
				 * via the onBackPressed callback instead.
				 */
				if (event.getKeyCode() == KeyEvent.KEYCODE_BACK)
					return false;
				GlibContext.runOnMain(() -> notifyKeyEvent(event));
				return true;
			}

			@Override
			public boolean onKeyDown(int keyCode, KeyEvent event) {
				return keyEventProxy(event);
			}
			@Override
			public boolean onKeyUp(int keyCode, KeyEvent event) {
				return keyEventProxy(event);
			}

			@Override
			public boolean onDragEvent(DragEvent event) {
				return GlibContext.blockForMain(() -> notifyDragEvent(event));
			}

			@Override
			public boolean onCheckIsTextEditor() {
				return this.activeImContext != null;
			}
			@Override
			public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
				if (activeImContext == null)
					return null;
				outAttrs.contentMimeTypes = new String[] { "text/plain" };
				//outAttrs.inputType = GlibContext.blockForMain(() -> activeImContext.getInputType());
				outAttrs.inputType = InputType.TYPE_NULL;
				outAttrs.imeOptions = EditorInfo.IME_FLAG_NO_FULLSCREEN;
				return activeImContext.new ImeConnection(this);
			}

			@Override
			protected void onAttachedToWindow() {
				super.onAttachedToWindow();
				GlibContext.runOnMain(this::notifyAttached);
			}

			@Override
			protected void onDetachedFromWindow() {
				super.onDetachedFromWindow();
				GlibContext.runOnMain(this::notifyDetached);
			}

			@Override
			protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
				if (changed)
					GlibContext.runOnMain(() -> notifyLayoutPosition(
						left - ToplevelView.this.insets.left,
						top - ToplevelView.this.insets.top
					));
				super.onLayout(changed, left, top, right, bottom);
			}

			@Override
			public void surfaceCreated(@NonNull SurfaceHolder holder) {
				notifyVisibility(true);
			}

			@Override
			public void surfaceChanged(@NonNull SurfaceHolder holder, int format, int width, int height) {
				float scale = Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE ?
						ToplevelActivity.this.getWindowManager().getCurrentWindowMetrics().getDensity() :
						getResources().getDisplayMetrics().density;

				GlibContext.blockForMain(() -> notifyLayoutSurface(width, height, scale));
			}

			@Override
			public void surfaceDestroyed(@NonNull SurfaceHolder holder) {
				notifyVisibility(false);
			}
		}

		private class LayoutParams extends ViewGroup.LayoutParams {
			public int x;
			public int y;

			public LayoutParams(int x, int y, int width, int height) {
				super(width, height);
				this.x = x;
				this.y = y;
			}
		}

		public Surface toplevel;
		private Surface grabbed;

		private Insets insets = Insets.NONE;

		public ToplevelView() {
			super(ToplevelActivity.this);
		}

		@Override
		protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
			if (toplevel == null) {
				setMeasuredDimension(0, 0);
				return;
			}

			measureChildren(
				widthMeasureSpec - (insets.left + insets.right),
				heightMeasureSpec - (insets.top + insets.bottom)
			);
			setMeasuredDimension(widthMeasureSpec, heightMeasureSpec);
		}

		@Override
		protected void onLayout(boolean changed, int l, int t, int r, int b) {
			int count = getChildCount();
			Logger.getLogger("view").log(Level.FINE, String.format("Layouting view, has %d children", count));
			for (int i = 0; i < count; i++) {
				View child = getChildAt(i);
				if (child.getVisibility() != GONE) {
					LayoutParams lp = (LayoutParams)child.getLayoutParams();
					child.layout(
						l + lp.x + insets.left,
						t + lp.y + insets.top,
						l + lp.x + insets.left + child.getMeasuredWidth(),
						t + lp.y + insets.top + child.getMeasuredHeight()
					);
				}
			}
		}

		@Override
		protected boolean checkLayoutParams(ViewGroup.LayoutParams p) {
			return p instanceof LayoutParams;
		}

		@Override
		public WindowInsets onApplyWindowInsets(WindowInsets insets) {
			this.insets = insets.getInsets(WindowInsets.Type.systemBars() | WindowInsets.Type.displayCutout() | WindowInsets.Type.ime());
			requestLayout();
			return WindowInsets.CONSUMED;
		}

		public void setGrabbedSurface(@Nullable Surface grabbed) {
			runOnUiThread(() -> {
				this.grabbed = grabbed;
			});
		}

		public void pushPopup(long surfaceIdentifier, int x, int y, int width, int height) {
			Surface surface = new Surface(surfaceIdentifier);
			runOnUiThread(() -> {
				addView(surface, new LayoutParams(x, y, width, height));
				requestLayout();
			});
		}
	}

	private long nativeIdentifier;

	@GlibContext.GtkThread
	private native void bindNative(long surface) throws UnregisteredSurfaceException; // if successful nativeIdentifier will be set
	@GlibContext.GtkThread
	private native void notifyConfigurationChange();
	@GlibContext.GtkThread
	private native void notifyStateChange(boolean has_focus, boolean is_fullscreen);
	@GlibContext.GtkThread
	private native void notifyOnBackPress();
	@GlibContext.GtkThread
	private native void notifyDestroy();
	@GlibContext.GtkThread
	private native void notifyActivityResult(int requestCode, int resultCode, Intent result);

	private ToplevelView view;
	private boolean fullscreenState;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		this.fullscreenState = false;

		super.onCreate(savedInstanceState);
		setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_USER);
		getWindow().setDecorFitsSystemWindows(false);

		this.view = new ToplevelView();
		setContentView(this.view);

		long identifier = getIntent().getLongExtra(toplevelIdentifierKey, 0);
		GlibContext.blockForMain(() -> {
			try {
				bindNative(identifier);
			} catch (UnregisteredSurfaceException e) {
				Intent intent = getIntent();
				if (intent.getData() != null) {
					String hint = "";
					if (intent.getAction() != null) {
						String[] action = intent.getAction().split("\\.");
						hint = action[action.length - 1].toLowerCase();
					}
					GdkContext.open(intent.getData(), hint);
				} else {
					GdkContext.activate();
				}

				if (nativeIdentifier == 0)
					Logger.getLogger("Toplevel").log(Level.SEVERE, "Call to activate did not spawn a new window");
			}
		});
	}

	@Keep
	@GlibContext.GtkThread
	private void attachToplevelSurface() {
		if (this.view.toplevel != null)
			this.view.toplevel.drop();
		this.view.toplevel = this.view.new Surface(this.nativeIdentifier);
		runOnUiThread(() -> {
			this.view.addView(this.view.toplevel, this.view.new LayoutParams(
					0, 0,
					ViewGroup.LayoutParams.MATCH_PARENT,
					ViewGroup.LayoutParams.MATCH_PARENT
			));
			this.view.requestLayout();
		});
	}

	private void updateToplevelState() {
		boolean has_focus = hasWindowFocus();
		boolean is_fullscreen = this.fullscreenState;
		GlibContext.runOnMain(() -> notifyStateChange(has_focus, is_fullscreen));
	}

	@Override
	public void onBackPressed() {
		GlibContext.runOnMain(this::notifyOnBackPress);
	}

	@Override
	public void onConfigurationChanged(@NonNull Configuration newConfig) {
		super.onConfigurationChanged(newConfig);
		GlibContext.runOnMain(this::notifyConfigurationChange);
	}

	@Override
	public void onWindowFocusChanged(boolean hasFocus) {
		super.onWindowFocusChanged(hasFocus);
		updateToplevelState();
	}

	@Override
	protected void onDestroy() {
		if (isFinishing())
			GlibContext.runOnMain(this::notifyDestroy);
		super.onDestroy();
	}

	@Override
	protected void onActivityResult(int requestCode, int resultCode, Intent data) {
		GlibContext.runOnMain(() -> notifyActivityResult(requestCode, resultCode, data));
	}

	@Override
	public void finish() {
		/*
		 * Surfaces that are spawned as a new window (task) thus without any parents
		 * should have their task dropped after they exited, as the Surface was already
		 * destroyed on the gdk side and can't be bound anymore, which'd happen if the
		 * user were able to navigate back to the nonexistent window in their history.
		 */
		if (isTaskRoot() && nativeIdentifier != 0)
			super.finishAndRemoveTask();
		else
			super.finish();
	}

	public void postWindowConfiguration(int color, boolean fullscreen) {
		runOnUiThread(() -> {
			Window window = getWindow();
			WindowInsetsController controller = window.getInsetsController();

			view.setBackgroundColor(color);
			// Taken from SO: https://stackoverflow.com/a/3943023
			// Licensed under CC-BY-SA 4.0
			boolean dark_fg = (((color >> 16) & 0xFF) * 0.299 +
			                   ((color >>  8) & 0xFF) * 0.587 +
			                   ((color >>  0) & 0xFF) * 0.114) > 186;

			int bars = WindowInsetsController.APPEARANCE_LIGHT_STATUS_BARS | WindowInsetsController.APPEARANCE_LIGHT_NAVIGATION_BARS;
			controller.setSystemBarsAppearance(dark_fg ? bars : 0, bars);

			this.fullscreenState = fullscreen;
			if (fullscreen) {
				controller.hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
				controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
			} else {
				controller.show(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
				controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_DEFAULT);
			}
			updateToplevelState();
		});
	}

	public void postTitle(String title) {
		runOnUiThread(() -> {
			setTitle(title);
			ActivityManager.TaskDescription desc = new ActivityManager.TaskDescription(title);
			setTaskDescription(desc);
		});
	}
}
