include $(top_srcdir)/Makefile.decl

EXTRA_DIST += gailutil.def
if OS_WIN32
export_symbols = -export-symbols $(srcdir)/gailutil.def
gailutil.def: libgailutil-4.la
	echo "LIBRARY libgailutil-$(GTK_MAJOR_VERSION)-$(GAIL_LT_CURRENT_MINUS_AGE)" >$@
	echo "EXPORTS" >>$@
	objdump -p .libs/libgailutil-$(GTK_MAJOR_VERSION)-$(GAIL_LT_CURRENT_MINUS_AGE).dll | sed -n -e 's/\t\[\s\+\([0-9]\+\)\]\s\([A-Za-z_][A-Za-z0-9_]\)/\2/p' >>$@
install-def-file: gailutil.def
	$(INSTALL) gailutil.def $(DESTDIR)$(libdir)
uninstall-def-file:
	-rm $(DESTDIR)$(libdir)/gailutil.def
else
install-def-file:
uninstall-def-file:
endif

if MS_LIB_AVAILABLE
noinst_DATA = gailutil.lib

install-ms-lib:
	$(INSTALL) gailutil.lib $(DESTDIR)$(libdir)

uninstall-ms-lib:
	-rm $(DESTDIR)$(libdir)/gailutil.lib
else
install-ms-lib:
uninstall-ms-lib:
endif


lib_LTLIBRARIES = libgailutil-4.la

util_c_sources =		\
	gailmisc.c		\
	gailtextutil.c

libgailutilincludedir=$(includedir)/gail-4.0/libgail-util

util_public_h_sources =		\
	gailmisc.h		\
	gailtextutil.h		\
	gail-util.h

libgailutil_4_la_SOURCES = 	\
	$(util_c_sources)

libgailutilinclude_HEADERS =	\
	$(util_public_h_sources)

libgailutil_4_la_CPPFLAGS = \
	-I$(top_srcdir)		\
	-I$(top_srcdir)/gdk	\
	-I$(top_builddir)/gdk	\
	-I$(top_srcdir)/gtk	\
	-I$(top_builddir)/gtk	\
	$(AM_CPPFLAGS) 		\
	-DGDK_DISABLE_DEPRECATED\
	-DGTK_DISABLE_DEPRECATED

libgailutil_4_la_CFLAGS = \
	$(GTK_DEP_CFLAGS)	\
	$(GTK_DEBUG_FLAGS)	\
	$(AM_CFLAGS)

libgailutil_4_la_LIBADD = \
	$(top_builddir)/gtk/libgtk-4.la	\
	$(top_builddir)/gdk/libgdk-4.la	\
	$(GTK_DEP_LIBS)

libgailutil_4_la_LDFLAGS = \
	-version-info $(GAIL_LT_VERSION_INFO)	\
	$(no_undefined)				\
        $(LDFLAGS)

gailutil.lib: libgailutil-4.la gailutil.def
	lib -machine:@LIB_EXE_MACHINE_FLAG@ -name:libgailutil-4.0-$(GAIL_LT_CURRENT_MINUS_AGE).dll -def:gailutil.def -out:$@

install-data-local: install-ms-lib install-def-file

uninstall-local: uninstall-ms-lib uninstall-def-file

# ------------------- MSVC Build Items ----------------
MSVCPROJS = gailutil-4

gailutil_4_FILES = $(libgailutil_4_la_SOURCES)

gailutil_4_EXCLUDES = dummy
gailutil_4_HEADERS_DIR = $(libgailutilincludedir)

gailutil_4_HEADERS_INST = $(libgailutilinclude_HEADERS)

gailutil_4_HEADERS_EXCLUDES = dummy

include $(top_srcdir)/build/Makefile.msvcproj

dist-hook: \
	$(top_builddir)/build/win32/vs9/gailutil-3.vcproj	\
	$(top_builddir)/build/win32/vs9/gailutil-3.headers

-include $(top_srcdir)/git.mk
