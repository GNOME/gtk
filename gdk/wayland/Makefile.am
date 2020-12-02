## Process this file with automake to produce Makefile.in
include $(top_srcdir)/Makefile.decl

libgdkincludedir = $(includedir)/gtk-3.0/gdk
libgdkwaylandincludedir = $(includedir)/gtk-3.0/gdk/wayland

AM_CPPFLAGS =					\
	-DG_LOG_DOMAIN=\"Gdk\"			\
	-DG_LOG_USE_STRUCTURED=1		\
	-DGDK_COMPILATION			\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/gdk			\
	-I$(top_builddir)/gdk			\
	$(GDK_HIDDEN_VISIBILITY_CFLAGS)		\
	$(GTK_DEBUG_FLAGS)			\
	$(GDK_DEP_CFLAGS)

LDADDS = $(GDK_DEP_LIBS) $(SHM_LIBS)

noinst_LTLIBRARIES =				\
	libgdk-wayland.la

BUILT_SOURCES = 				\
	pointer-gestures-unstable-v1-client-protocol.h		\
	pointer-gestures-unstable-v1-protocol.c			\
	xdg-activation-v1-client-protocol.h			\
	xdg-activation-v1-protocol.c				\
	xdg-shell-client-protocol.h				\
	xdg-shell-protocol.c					\
	xdg-shell-unstable-v6-client-protocol.h			\
	xdg-shell-unstable-v6-protocol.c			\
	xdg-foreign-unstable-v1-client-protocol.h		\
	xdg-foreign-unstable-v1-protocol.c			\
	gtk-primary-selection-client-protocol.h			\
	gtk-primary-selection-protocol.c			\
	tablet-unstable-v2-client-protocol.h	\
	tablet-unstable-v2-protocol.c		\
	xdg-output-unstable-v1-protocol.c			\
	xdg-output-unstable-v1-client-protocol.h		\
	keyboard-shortcuts-inhibit-unstable-v1-client-protocol.h \
	keyboard-shortcuts-inhibit-unstable-v1-protocol.c	\
	server-decoration-client-protocol.h		\
	server-decoration-protocol.c			\
	gtk-shell-client-protocol.h		\
	gtk-shell-protocol.c			\
	primary-selection-unstable-v1-client-protocol.h		\
	primary-selection-unstable-v1-protocol.c

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
	gdkkeys-wayland.c                       \
	gdkglcontext-wayland.c			\
	gdkglcontext-wayland.h			\
	gdkmonitor-wayland.c			\
	gdkmonitor-wayland.h			\
	gdkscreen-wayland.c			\
	gdkseat-wayland.h			\
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
	gdkwaylandglcontext.h			\
	gdkwaylandmonitor.h			\
	gdkwaylandselection.h			\
	gdkwaylandwindow.h

.SECONDEXPANSION:

define protostability
$(if $(findstring unstable,$1),unstable,stable)
endef

define protoname
$(shell echo $1 | sed 's/\([a-z\-]\{1,\}\)-[a-z]\{1,\}-v[0-9]\{1,\}/\1/')
endef

%-protocol.c : $(WAYLAND_PROTOCOLS_DATADIR)/$$(call protostability,$$*)/$$(call protoname,$$*)/$$*.xml
	$(AM_V_GEN)$(WAYLAND_SCANNER) private-code < $< > $@
%-client-protocol.h : $(WAYLAND_PROTOCOLS_DATADIR)/$$(call protostability,$$*)/$$(call protoname,$$*)/$$*.xml
	$(AM_V_GEN)$(WAYLAND_SCANNER) client-header < $< > $@

xdg-activation-v1-protocol.c : $(WAYLAND_PROTOCOLS_DATADIR)/staging/xdg-activation/xdg-activation-v1.xml
	$(AM_V_GEN)$(WAYLAND_SCANNER) private-code < $< > $@
xdg-activation-v1-client-protocol.h : $(WAYLAND_PROTOCOLS_DATADIR)/staging/xdg-activation/xdg-activation-v1.xml
	$(AM_V_GEN)$(WAYLAND_SCANNER) client-header < $< > $@

%-protocol.c : $(srcdir)/protocol/%.xml
	$(AM_V_GEN)$(WAYLAND_SCANNER) private-code < $< > $@

%-server-protocol.h : $(srcdir)/protocol/%.xml
	$(AM_V_GEN)$(WAYLAND_SCANNER) server-header < $< > $@
%-client-protocol.h : $(srcdir)/protocol/%.xml
	$(AM_V_GEN)$(WAYLAND_SCANNER) client-header < $< > $@

EXTRA_DIST += 					\
	protocol/gtk-primary-selection.xml	\
	protocol/gtk-shell.xml			\
	protocol/server-decoration.xml	\
	meson.build

-include $(top_srcdir)/git.mk
