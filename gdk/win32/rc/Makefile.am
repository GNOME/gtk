EXTRA_DIST = \
	gdk.rc \
	gdk.rc.in \
	gtk.ico

if USE_WIN32
noinst_DATA = gdk-win32res.lo
endif

noinst_PROGRAMS = foo

# Compile a dummy object file into the foo.lo libtool object so that
# the ugly hack in lt-compile-resource knows what kind of libtool
# object to produce. (It checks whether there are any .libs/*.o
# files.)

gdk-win32res.lo : gdk.rc gtk.ico foo.lo
	$(top_srcdir)/build/win32/lt-compile-resource gdk.rc gdk-win32res.lo

foo.c:
	echo 'int main (int argc, char **argv) { return 0; }' >foo.c
