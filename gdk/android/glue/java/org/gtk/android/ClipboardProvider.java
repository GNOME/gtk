/*
 * Copyright (c) 2024 Florian "sp1rit" <sp1rit@disroot.org>
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

import android.content.ClipData;
import android.content.ClipboardManager;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.Point;
import android.net.Uri;
import android.view.View;

import androidx.annotation.NonNull;

public final class ClipboardProvider {
	public static class ClipboardChangeListener implements ClipboardManager.OnPrimaryClipChangedListener {
		public long nativePtr;
		public ClipboardChangeListener(long nativePtr) {
			this.nativePtr = nativePtr;
		}

		@GlibContext.GtkThread
		public native void notifyClipboardChange();

		@Override
		public void onPrimaryClipChanged() {
			GlibContext.runOnMain(this::notifyClipboardChange);
		}
	}

	public static class ClipboardBitmapDragShadow extends View.DragShadowBuilder {
		Bitmap bitmap;
		int hot_x;
		int hot_y;

		// The bitmap produced by the OpenGL renderer is vertically flipped,
		// so we need a mechanism to unflip it.
		public static Bitmap vflip(Bitmap bitmap) {
			Matrix vflip = new Matrix();
			vflip.setScale(1.f, -1.f);
			return Bitmap.createBitmap(bitmap, 0, 0, bitmap.getWidth(), bitmap.getHeight(), vflip, false);
		}

		public ClipboardBitmapDragShadow(View view, Bitmap bitmap, int hot_x, int hot_y) {
			super(view);
			this.bitmap = bitmap;
			this.hot_x = hot_x;
			this.hot_y = hot_y;
		}

		@Override
		public void onDrawShadow(@NonNull Canvas canvas) {
			canvas.drawBitmap(this.bitmap, 0, 0, null);
		}

		@Override
		public void onProvideShadowMetrics(Point outShadowSize, Point outShadowTouchPoint) {
			// Right now this method never gets called, as a drag shadow passed
			// into View.updateDragShadow retains the size of the previous drag
			// shadow. As this isn't actually specified anywhere I hope that
			// this changes at some point and I can get rid of that ugly
			// ClipboardEmptyDragShadow hack, but I doubt this will be the case.

			outShadowSize.set(this.bitmap.getWidth(), this.bitmap.getHeight());
			outShadowTouchPoint.set(this.hot_x, this.hot_y);
		}
	}

	public static class ClipboardEmptyDragShadow extends View.DragShadowBuilder {
		public ClipboardEmptyDragShadow(View view) {
			super(view);
		}

		@Override
		public void onDrawShadow(@NonNull Canvas canvas) {
			// no-op
		}

		@Override
		public void onProvideShadowMetrics(Point outShadowSize, Point outShadowTouchPoint) {
			// As the EmptyDragShadow is the one used for View.startDragAndDrop,
			// it decides what the size of the drag surface is going to be,
			// as View.updateDragShadow can not resize the surface. For this
			// reason we are allocating a *very large* drag surface here, under
			// the assumption that the dragged contents are likely smaller than
			// the surface they're being dragged from.
			outShadowSize.set(getView().getWidth(), getView().getHeight());

			// Similar for the shadow hotspot, which also can't be changed
			// later on, so (0|0) is our only safe option.
			outShadowTouchPoint.set(0, 0);
		}
	}

	public static class InternalClipdata extends ClipData {
		public InternalClipdata() {
			super(ClipData.newRawUri(null, Uri.parse("gobject://internal")));
		}
	}

	public static class NativeDragIdentifier {
		public long nativeIdentifier;

		public NativeDragIdentifier(long nativeIdentifier) {
			this.nativeIdentifier = nativeIdentifier;
		}
	}
}
