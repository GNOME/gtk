AM_CPPFLAGS = \
	-I$(top_srcdir)			\
	-I$(top_srcdir)/gdk		\
	-I$(top_builddir)/gdk		\
	$(GTK_DEBUG_FLAGS)		\
	$(GTK_DEP_CFLAGS)

LDADD = \
        $(top_builddir)/gtk/libgtk-4.la \
        $(GTK_DEP_LIBS)


bin_PROGRAMS = gtk4-icon-browser

desktopdir = $(datadir)/applications
dist_desktop_DATA = gtk4-icon-browser.desktop

gtk4_icon_browser_SOURCES = \
	main.c \
	iconbrowserapp.c iconbrowserapp.h \
	iconbrowserwin.c iconbrowserwin.h \
	iconstore.c iconstore.h \
	resources.c

BUILT_SOURCES = \
	resources.c

resources.c: iconbrowser.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/iconbrowser.gresource.xml)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/iconbrowser.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --generate-source

EXTRA_DIST = \
	menus.ui \
	iconbrowser.gresource.xml \
	window.ui

# ------------------- MSVC Build Items ----------------
MSVCPROJS = gtk4-icon-browser

gtk4_icon_browser_FILES = $(gtk4_icon_browser_SOURCES)
gtk4_icon_browser_EXCLUDES = dummy

include $(top_srcdir)/win32/Makefile.msvcproj

dist-hook: $(top_builddir)/win32/vs12/gtk4-icon-browser.vcxproj

-include $(top_srcdir)/git.mk
