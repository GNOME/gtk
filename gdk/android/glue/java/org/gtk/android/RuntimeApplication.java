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

import android.app.Application;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;

import org.gtk.android.SystemFilesystem;

import java.util.Objects;

public class RuntimeApplication extends Application {
	static final String LIBNAME_KEY = "gtk.android.lib_name";

	static {
		System.loadLibrary("gtk-4");
	}

	@Override
	public void onCreate() {
		super.onCreate();
		SystemFilesystem.writeResources(this);
		startRuntime(System.mapLibraryName(getApplicationLibrary()));
	}

	@Keep
	protected native void startRuntime(String applicationLibrary);

	protected @NonNull String getApplicationLibrary() {
		try {
			ApplicationInfo info = getPackageManager().getApplicationInfo(
					getPackageName(),
					PackageManager.GET_META_DATA
			);
			return Objects.requireNonNull(info.metaData.getString(LIBNAME_KEY));
		} catch (Exception err) {
			throw new RuntimeException(
					String.format("Unable to retrieve \"%s\" key from application manifest", LIBNAME_KEY),
					err
			);
		}
	}
}
