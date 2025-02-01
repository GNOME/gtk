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

import android.content.Context;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Arrays;
import java.util.logging.Level;
import java.util.logging.Logger;

import android.os.Build;

public final class SystemFilesystem {
	private static final String fingerprint = "afpr";
	private static final int fprSize = 128;

	private SystemFilesystem() {}

	private static void cleanDirectory(File directory) {
		for (File file: directory.listFiles()) {
			if (file.isDirectory())
				cleanDirectory(file);
			file.delete();
		}
	}

	private static void copyStream(InputStream input, OutputStream output) throws IOException {
		if (Build.VERSION.SDK_INT >= 33) {
			input.transferTo(output);
		} else {
			byte[] buffer = new byte[4096];
			int count = 0;
			do {
				output.write(buffer, 0, count);
				count = input.read(buffer);
				} while (count > 0);
			}
	}
	private static void copyFromAssets(Context context, String path, File destination) throws IOException {
		String[] files = context.getAssets().list(path);
		if (files.length == 0) {
			InputStream istream = context.getAssets().open(path);

			destination.createNewFile();
			OutputStream ostream = new FileOutputStream(destination);

			copyStream(istream, ostream);

			istream.close();
			ostream.close();
		} else {
			destination.mkdir();
			for (String file : files)
				copyFromAssets(context, path.isEmpty() ?  file : path + File.separator + file, new File(destination, file));
		}
	}

	private static void doWriteResources(Context context, Logger logger) {
		try {
			File dir = context.getFilesDir();
			cleanDirectory(dir);
			copyFromAssets(context, "", dir);
		} catch (IOException e) {
			logger.log(Level.SEVERE, "Failed writing assets to filesystem: " + e);
		}
	}

	public static void writeResources(Context context) {
		Logger logger = Logger.getLogger("glue");
		try {
			InputStream assetfprStream = context.getAssets().open(fingerprint);
			try {
				File filefpr = new File(context.getFilesDir(), fingerprint);
				InputStream filefprStream = new FileInputStream(filefpr);

				byte[] assetfprData = new byte[fprSize];
				assetfprStream.read(assetfprData);
				assetfprStream.close();

				byte[] filefprData = new byte[fprSize];
				filefprStream.read(filefprData);
				filefprStream.close();

				if (!Arrays.equals(assetfprData, filefprData))
					doWriteResources(context, logger);
			} catch (IOException e) {
				doWriteResources(context, logger);
			}
		} catch (IOException e) {
			logger.log(Level.WARNING, "No fingerprint file in assets, assuming no filesystem copy needed");
		}
	}
}
