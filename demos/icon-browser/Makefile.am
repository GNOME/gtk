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

gtk3_icon_browser_SOURCES = \
	main.c \
	iconbrowserapp.c iconbrowserapp.h \
	iconbrowserwin.c iconbrowserwin.h \
	resources.c

BUILT_SOURCES = \
	resources.c

resources.c: iconbrowser.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/iconbrowser.gresource.xml)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/iconbrowser.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --generate-source

EXTRA_DIST = \
	iconbrowser.gresource.xml \
	window.ui

-include $(top_srcdir)/git.mk
