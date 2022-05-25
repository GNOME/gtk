AM_CPPFLAGS = \
	-I$(top_srcdir)			\
	-I$(top_srcdir)/gdk		\
	-I$(top_builddir)/gdk		\
	$(GTK_DEBUG_FLAGS)		\
	$(GTK_DEP_CFLAGS)

LDADD = \
        $(top_builddir)/gtk/libgtk-3.la \
        $(top_builddir)/gdk/libgdk-3.la \
        $(GTK_DEP_LIBS)


bin_PROGRAMS = gtk3-icon-browser

desktopdir = $(datadir)/applications
dist_desktop_DATA = gtk3-icon-browser.desktop

nodist_gtk3_icon_browser_SOURCES = resources.c

gtk3_icon_browser_SOURCES = \
	main.c \
	iconbrowserapp.c iconbrowserapp.h \
	iconbrowserwin.c iconbrowserwin.h \
	iconstore.c iconstore.h

BUILT_SOURCES = \
	resources.c

DISTCLEANFILES = resources.c

resources.c: iconbrowser.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/iconbrowser.gresource.xml)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/iconbrowser.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --generate-source

EXTRA_DIST = \
	menus.ui \
	iconbrowser.gresource.xml \
	window.ui \
	icon.list \
	meson.build

# ------------------- MSVC Build Items ----------------
MSVCPROJS = gtk3-icon-browser

gtk3_icon_browser_FILES = $(gtk3_icon_browser_SOURCES)
gtk3_icon_browser_EXCLUDES = dummy

include $(top_srcdir)/win32/Makefile.msvcproj

dist-hook: $(top_builddir)/win32/vs9/gtk3-icon-browser.vcproj

-include $(top_srcdir)/git.mk
