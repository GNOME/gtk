EXTRA_DIST = \
	gdk.rc \
	gdk.rc.in \
	gtk.ico

if USE_WIN32
noinst_DATA = gdk-win32res.lo
endif

gdk-win32res.lo : gdk.rc gtk.ico
	$(top_srcdir)/build/win32/lt-compile-resource gdk.rc gdk-win32res.lo
