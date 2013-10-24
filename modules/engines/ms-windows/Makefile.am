include $(top_srcdir)/Makefile.decl

SUBDIRS=Theme

EXTRA_DIST += Makefile.msc

INCLUDES = \
	-I$(top_srcdir) 		\
	-I$(top_srcdir)/gdk		\
	-I$(top_builddir)/gdk		\
	-DGDK_DISABLE_DEPRECATED	\
	$(GTK_DEP_CFLAGS)	\
	$(GMODULE_CFLAGS)


LDADDS = \
	$(top_builddir)/gdk/$(gdktargetlib)		\
	$(top_builddir)/gtk/$(gtktargetlib)		\
	$(GTK_DEP_LIBS)		\
	$(GMODULE_LIBS)

enginedir = $(libdir)/gtk-2.0/$(GTK_BINARY_VERSION)/engines

engine_LTLIBRARIES = libwimp.la

libwimp_la_SOURCES = \
	msw_rc_style.c		\
	msw_rc_style.h		\
	msw_style.c		\
	msw_style.h		\
	msw_theme_main.c	\
	xp_theme.c		\
	xp_theme_defs.h		\
	xp_theme.h

libwimp_la_LDFLAGS = \
	-avoid-version -module -no-undefined -export-dynamic

libwimp_la_LIBADD = $(LDADDS) -lgdi32

-include $(top_srcdir)/git.mk
