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

import android.app.Activity;

import androidx.annotation.NonNull;
import androidx.annotation.UiThread;

import java.util.logging.Level;
import java.util.logging.Logger;

public final class GlueLibraryContext {
	private static GlueLibraryProvider impl;

	private GlueLibraryContext() {}

	public static void setGlueLibraryProvider(@NonNull GlueLibraryProvider provider) {
		if (provider == null)
			throw new NullPointerException("provider must not be null");
		if (impl != null)
			Logger.getLogger("GlueLibraryContext").log(Level.SEVERE, "setGlueLibraryProvider can only be called once");
		impl = provider;
	}

	public static String getGlueLibraryName() {
		return impl.getGlueLibraryName();
	}

	// I don't really know yet how to best proceed with the glue code. As it's shipped with GDK, but
	// compiled by the user. As JNI symbols contain the class name, they can't be just be part of
	// the user provided glue activity as that'd constitute the need of having the user modify the
	// glue code, which I'd like to avoid.
	// There are certain cases where there is a legitimate need to modify the glue code (in case the
	// application needs to launch with specific arguments), but maybe with a bit of linker magic
	// (weak symbols) this could also be avoided.
	@UiThread
	static native void runApplication(@NonNull Activity activity);
}
