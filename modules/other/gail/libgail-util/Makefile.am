include $(top_srcdir)/Makefile.decl

EXTRA_DIST += gailutil.def
if PLATFORM_WIN32
no_undefined = -no-undefined
endif
if OS_WIN32
export_symbols = -export-symbols $(srcdir)/gailutil.def
install-def-file: gailutil.def
	$(INSTALL) $(srcdir)/gailutil.def $(DESTDIR)$(libdir)
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


lib_LTLIBRARIES = libgailutil.la

util_c_sources =		\
	gailmisc.c		\
	gailtextutil.c

libgailutilincludedir=$(includedir)/gail-1.0/libgail-util

util_public_h_sources =		\
	gailmisc.h		\
	gailtextutil.h		\
	gail-util.h

libgailutil_la_SOURCES = 	\
	$(util_c_sources)

libgailutilinclude_HEADERS =	\
	$(util_public_h_sources)

libgailutil_la_CPPFLAGS = \
	-I$(top_srcdir)		\
	-I$(top_srcdir)/gdk	\
	-I$(top_builddir)/gdk	\
	-I$(top_srcdir)/gtk	\
	-I$(top_builddir)/gtk	\
	$(AM_CPPFLAGS) 		\
	-DGDK_DISABLE_DEPRECATED\
	-DGTK_DISABLE_DEPRECATED

libgailutil_la_CFLAGS = \
	$(GTK_DEP_CFLAGS)	\
	$(GTK_DEBUG_FLAGS)	\
	$(AM_CFLAGS)

libgailutil_la_LIBADD = \
	$(top_builddir)/gtk/$(gtktargetlib)	\
	$(top_builddir)/gdk/$(gdktargetlib)	\
	$(GTK_DEP_LIBS)

libgailutil_la_LDFLAGS = \
	-version-info $(GAIL_LT_VERSION_INFO)	\
	$(no_undefined)				\
	$(export_symbols)			\
        $(LDFLAGS)

gailutil.lib: libgailutil.la gailutil.def
	lib -machine:@LIB_EXE_MACHINE_FLAG@ -name:libgailutil-$(GAIL_LT_CURRENT_MINUS_AGE).dll -def:gailutil.def -out:$@

install-data-local: install-ms-lib install-def-file

uninstall-local: uninstall-ms-lib uninstall-def-file

-include $(top_srcdir)/git.mk
