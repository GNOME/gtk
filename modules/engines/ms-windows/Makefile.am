SUBDIRS=Theme

EXTRA_DIST=Makefile.msc

INCLUDES=$(WIMP_CFLAGS)

enginedir=$(libdir)/gtk-2.0/$(GTK_VERSION)/engines

engine_LTLIBRARIES = libwimp.la

libwimp_la_SOURCES = 		\
	wimp_rc_style.c		\
	wimp_rc_style.h		\
	wimp_style.c		\
	wimp_style.h		\
	wimp_theme_main.c	\
	xp_theme.c		\
	xp_theme_defs.h		\
	xp_theme.h

libwimp_la_LDFLAGS = -avoid-version -module -no-undefined -export-dynamic
libwimp_la_LIBADD= $(WIMP_LIBS)

