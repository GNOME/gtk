SUBDIRS=Theme

EXTRA_DIST=Makefile.msc

INCLUDES=$(MSW_CFLAGS)

enginedir=$(libdir)/gtk-2.0/$(GTK_VERSION)/engines

engine_LTLIBRARIES = libms-windows-engine.la

libms_windows_engine_la_SOURCES = \
	msw_rc_style.c		\
	msw_rc_style.h		\
	msw_style.c		\
	msw_style.h		\
	msw_theme_main.c	\
	xp_theme.c		\
	xp_theme_defs.h		\
	xp_theme.h

libms_windows_engine_la_LDFLAGS = \
	-avoid-version -module -no-undefined -export-dynamic

libms_windows_engine_la_LIBADD= $(MSW_LIBS)

