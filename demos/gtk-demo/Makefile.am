## Makefile.am for gtk+/demos

democodedir=$(datadir)/gtk-2.0/demo

## These should be in the order you want them to appear in the 
## demo app, which means alphabetized by demo title, not filename
demos = @STRIP_BEGIN@	\
	button_box.c	\
	dialog.c	\
	drawingarea.c	\
	images.c	\
	item_factory.c	\
	menus.c		\
	panes.c		\
	pixbufs.c	\
	textview.c	\
@STRIP_END@

INCLUDES = @STRIP_BEGIN@ \
	-DDEMOCODEDIR="\"$(democodedir)\""		\
	-I$(top_srcdir) 				\
	-I$(top_builddir)/gdk				\
	-DGDK_DISABLE_COMPAT_H				\
	-DGDK_DISABLE_DEPRECATED			\
	-DGTK_DISABLE_COMPAT_H				\
	-DGTK_DISABLE_DEPRECATED 			\
	@GTK_DEBUG_FLAGS@				\
	@GTK_XIM_FLAGS@					\
	@GTK_LOCALE_FLAGS@				\
	@PANGO_CFLAGS@					\
	@GLIB_CFLAGS@					\
	@more_cflags@					\
@STRIP_END@

DEPS = \
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-1.3.la	\
	$(top_builddir)/gdk/@gdktargetlib@ 		\
	$(top_builddir)/gtk/@gtktargetlib@

LDADDS = @STRIP_BEGIN@ 					\
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-1.3.la	\
	$(top_builddir)/gdk/@gdktargetlib@        	\
	$(top_builddir)/gtk/@gtktargetlib@ 		\
	@more_ldflags@					\
	@more_libs@					\
	@GDK_WLIBS@					\
	@PANGO_LIBS@					\
	@GLIB_LIBS@					\
	@GTK_LIBS_EXTRA@				\
	-lm						\
@STRIP_END@

bin_PROGRAMS = gtk-demo

BUILT_SOURCES = demos.h

EXTRA_DIST = \
	geninclude.pl

demos.h: $(demos) geninclude.pl
	(cd $(srcdir) && ./geninclude.pl $(demos) > demos.h)

gtk_demo_SOURCES = 		\
	$(demos)		\
	main.c			\
	demos.h

gtk_demo_DEPENDENCIES = $(DEPS)
gtk_demo_LDADD = $(LDADDS)

IMAGEFILES=	apple-red.png		\
		background.jpg		\
		gnome-applets.png	\
		gnome-calendar.png	\
		gnome-foot.png		\
		gnome-gimp.png		\
		gnome-gmush.png		\
		gnome-gsame.png		\
		gnu-keys.png		\
		gtk-logo-rgb.gif

democode_DATA = $(demos) $(IMAGEFILES)

EXTRA_DIST = $(IMAGEFILES)
