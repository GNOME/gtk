EXTRA_DIST = \
	gdk.rc \
	gtk.ico

if PLATFORM_WIN32
noinst_DATA = gdk-win32res.lo
endif

gdk-win32res.lo : gdk.rc gtk.ico
	$(top_srcdir)/build/win32/lt-compile-resource gdk.rc gdk-win32res.lo

files:
	@files=`ls $(DISTFILES) 2> /dev/null `; for p in $$files; do \
	  echo $$p; \
	done
