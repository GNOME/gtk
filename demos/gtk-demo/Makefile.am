## Makefile.am for gtk+/demos

democodedir=$(datadir)/gtk-2.0/demo

## These should be in the order you want them to appear in the 
## demo app, which means alphabetized by demo title, not filename
demos = @STRIP_BEGIN@	\
	appwindow.c	\
	button_box.c	\
	colorsel.c	\
	dialog.c	\
	drawingarea.c	\
	editable_cells.c \
	images.c	\
	item_factory.c	\
	list_store.c	\
	menus.c		\
	panes.c		\
	pixbufs.c	\
	sizegroup.c	\
	stock_browser.c	\
	textview.c	\
	tree_store.c	\
@STRIP_END@

INCLUDES = @STRIP_BEGIN@ \
	-DDEMOCODEDIR="\"$(democodedir)\""		\
	-I$(top_srcdir) 				\
	-I$(top_builddir)/gdk				\
	-DG_DISABLE_DEPRECATED	 			\
	-DGDK_DISABLE_DEPRECATED			\
	-DGDK_PIXBUF_DISABLE_DEPRECATED			\
	-DGTK_DISABLE_DEPRECATED 			\
	@GTK_DEBUG_FLAGS@				\
	@GTK_DEP_CFLAGS@				\
@STRIP_END@

DEPS = \
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-1.3.la	\
	$(top_builddir)/gdk/@gdktargetlib@ 		\
	$(top_builddir)/gtk/@gtktargetlib@

LDADDS = @STRIP_BEGIN@ 					\
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-1.3.la	\
	$(top_builddir)/gdk/@gdktargetlib@        	\
	$(top_builddir)/gtk/@gtktargetlib@ 		\
@STRIP_END@

bin_PROGRAMS = gtk-demo

BUILT_SOURCES = demos.h

EXTRA_DIST = 		\
	geninclude.pl	\
	$(IMAGEFILES)

demos.h: $(demos) geninclude.pl
	(cd $(srcdir) && ./geninclude.pl $(demos) > demos.h)

gtk_demo_SOURCES = 		\
	$(demos)		\
	demo-common.h		\
	main.c			\
	demos.h

gtk_demo_DEPENDENCIES = $(DEPS)
gtk_demo_LDADD = $(LDADDS)

IMAGEFILES=	alphatest.png		\
	        apple-red.png		\
		background.jpg		\
		floppybuddy.gif		\
		gnome-applets.png	\
		gnome-calendar.png	\
		gnome-foot.png		\
		gnome-gimp.png		\
		gnome-gmush.png		\
		gnome-gsame.png		\
		gnu-keys.png		\
		gtk-logo-rgb.gif

democode_DATA = $(demos) $(IMAGEFILES)
