## Makefile.am for gtk+/demos
include $(top_srcdir)/Makefile.decl

AM_CPPFLAGS = 				\
	-I$(top_srcdir) 		\
	-I$(top_builddir)/gdk		\
	-DGDK_DISABLE_DEPRECATED	\
	-DGTK_DISABLE_DEPRECATED	\
	$(GTK_DEBUG_FLAGS)		\
	$(GTK_DEP_CFLAGS)

DEPS = \
	$(top_builddir)/gtk/libgtk-3.la

LDADDS = \
	$(top_builddir)/gtk/libgtk-3.la	\
	$(top_builddir)/gdk/libgdk-3.la	\
	$(GTK_DEP_LIBS)			\
	$(MATH_LIB)

noinst_PROGRAMS = 		\
	pixbuf-demo

pixbuf_demo_DEPENDENCIES = $(DEPS)

pixbuf_demo_LDADD = $(LDADDS)

pixbuf_demo_SOURCES = pixbuf-demo.c

EXTRA_DIST +=			\
	apple-red.png		\
	background.jpg		\
	gnome-applets.png	\
	gnome-calendar.png	\
	gnome-foot.png		\
	gnome-gimp.png		\
	gnome-gmush.png		\
	gnome-gsame.png		\
	gnu-keys.png

-include $(top_srcdir)/git.mk
