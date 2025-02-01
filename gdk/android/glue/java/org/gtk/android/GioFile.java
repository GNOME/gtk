package org.gtk.android;

import android.content.ContentResolver;
import android.content.Context;
import android.content.res.AssetFileDescriptor;
import android.database.Cursor;
import android.net.Uri;
import android.os.CancellationSignal;
import android.provider.DocumentsContract;

import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;

public final class GioFile {
	private Context context;
	private Uri uri;

	public GioFile(Context context, Uri uri) {
		this.context = context;
		this.uri = uri.normalizeScheme();
	}

	private AssetFileDescriptor openDescriptor(String mode, CancellationSignal cancellable) throws IOException {
		return context.getContentResolver().openAssetFileDescriptor(uri, mode, cancellable);
	}

	public FileOutputStream append(CancellationSignal cancellable) throws IOException {
		return openDescriptor("wa", cancellable).createOutputStream();
	}

	public FileInputStream read_fn(CancellationSignal cancellable) throws IOException {
		return openDescriptor("r", cancellable).createInputStream();
	}

	public FileOutputStream replace(CancellationSignal cancellable) throws IOException {
		return openDescriptor("wt", cancellable).createOutputStream();
	}

	private Uri function(String filename) {
		final ContentResolver resolver = context.getContentResolver();
		final Uri childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(uri, DocumentsContract.getDocumentId(uri));
		Cursor c = null;
		try {
			c = resolver.query(childrenUri, new String[] {
					DocumentsContract.Document.COLUMN_DISPLAY_NAME,
					DocumentsContract.Document.COLUMN_DOCUMENT_ID,
			}, null, null, null);
			while (c.moveToNext()) {
				final String name = c.getString(0);
				if (filename.equals(name)) {
					final String documentId = c.getString(1);
					final Uri documentUri = DocumentsContract.buildDocumentUriUsingTree(uri, documentId);
					return documentUri;
				}

			}
		} catch (Exception e) {
			// nN
		} finally {
			if (c != null)
				c.close();
		}

		return null;
	}
}
