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

-include $(top_srcdir)/git.mk
