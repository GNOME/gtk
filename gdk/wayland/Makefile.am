## Process this file with automake to produce Makefile.in
include $(top_srcdir)/Makefile.decl

libgdkincludedir = $(includedir)/gtk-3.0/gdk
libgdkwaylandincludedir = $(includedir)/gtk-3.0/gdk/wayland

AM_CPPFLAGS =					\
	-DG_LOG_DOMAIN=\"Gdk\"			\
	-DGDK_COMPILATION			\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/gdk			\
	-I$(top_builddir)/gdk			\
	$(GDK_HIDDEN_VISIBILITY_CFLAGS)		\
	$(GTK_DEBUG_FLAGS)			\
	$(GDK_DEP_CFLAGS)

LDADDS = $(GDK_DEP_LIBS)

noinst_LTLIBRARIES =				\
	libgdk-wayland.la

BUILT_SOURCES = 				\
	xdg-shell-client-protocol.h		\
	xdg-shell-protocol.c			\
	gtk-shell-client-protocol.h		\
	gtk-shell-protocol.c

nodist_libgdk_wayland_la_SOURCES =		\
	$(BUILT_SOURCES)

DISTCLEANFILES =				\
	$(BUILT_SOURCES)

libgdk_wayland_la_SOURCES =			\
	gdkapplaunchcontext-wayland.c		\
	gdkcursor-wayland.c			\
	gdkdevice-wayland.c			\
	gdkdisplay-wayland.c			\
	gdkdisplay-wayland.h			\
	gdkdnd-wayland.c			\
	gdkeventsource.c			\
	gdkkeys-wayland.c			\
	gdkscreen-wayland.c			\
	gdkselection-wayland.c			\
	gdkwindow-wayland.c			\
	gdkwayland.h				\
	gdkprivate-wayland.h			\
	wm-button-layout-translation.c		\
	wm-button-layout-translation.h

libgdkinclude_HEADERS =				\
	gdkwayland.h

libgdkwaylandinclude_HEADERS = 			\
	gdkwaylanddevice.h			\
	gdkwaylanddisplay.h			\
	gdkwaylandselection.h			\
	gdkwaylandwindow.h

%-protocol.c : $(srcdir)/protocol/%.xml
	$(AM_V_GEN)$(WAYLAND_SCANNER) code < $< > $@
	$(AM_V_GEN)$(SED) -i -e 's/WL_EXPORT //' $@

%-server-protocol.h : $(srcdir)/protocol/%.xml
	$(AM_V_GEN)$(WAYLAND_SCANNER) server-header < $< > $@
%-client-protocol.h : $(srcdir)/protocol/%.xml
	$(AM_V_GEN)$(WAYLAND_SCANNER) client-header < $< > $@

EXTRA_DIST += 					\
	protocol/xdg-shell.xml			\
	protocol/gtk-shell.xml

-include $(top_srcdir)/git.mk
