## Makefile.am for gtk+/demos

democodedir=$(datadir)/gtk-2.0/demo

## These should be in the order you want them to appear in the 
## demo app, which means alphabetized by demo title, not filename
demos =						\
	appwindow.c				\
	assistant.c				\
	button_box.c				\
	changedisplay.c				\
	clipboard.c				\
	colorsel.c				\
	combobox.c				\
	dialog.c				\
	drawingarea.c				\
	editable_cells.c			\
	entry_completion.c			\
	expander.c  				\
	hypertext.c				\
	iconview.c				\
	iconview_edit.c				\
	images.c				\
	list_store.c				\
	menus.c					\
	panes.c					\
	pickers.c				\
	pixbufs.c				\
	printing.c				\
	rotated_text.c				\
	sizegroup.c				\
	stock_browser.c				\
	textview.c				\
	textscroll.c				\
	tree_store.c				\
	ui_manager.c

INCLUDES = \
	-DDEMOCODEDIR="\"$(democodedir)\""		\
	-I$(top_srcdir) 				\
	-I$(top_builddir)/gdk				\
	-DGDK_PIXBUF_DISABLE_DEPRECATED			\
	-DGDK_DISABLE_DEPRECATED			\
	-DGTK_DISABLE_DEPRECATED 			\
	$(GTK_DEBUG_FLAGS)				\
	$(GTK_DEP_CFLAGS)

DEPS = \
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la	\
	$(top_builddir)/gdk/$(gdktargetlib) 				\
	$(top_builddir)/gtk/$(gtktargetlib)

LDADDS = \
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la	\
	$(top_builddir)/gdk/$(gdktargetlib)        			\
	$(top_builddir)/gtk/$(gtktargetlib)

bin_PROGRAMS = gtk-demo

BUILT_SOURCES = demos.h

EXTRA_DIST = 		\
	$(IMAGEFILES)

demos.h: @REBUILD@ $(demos) geninclude.pl
	(here=`pwd` ; cd $(srcdir) && $(PERL) $$here/geninclude.pl $(demos)) > demos.h

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
		gnome-fs-directory.png	\
		gnome-fs-regular.png	\
		gnome-foot.png		\
		gnome-gimp.png		\
		gnome-gmush.png		\
		gnome-gsame.png		\
		gnu-keys.png		\
		gtk-logo-rgb.gif		

democode_DATA = $(demos) $(IMAGEFILES)

DISTCLEANFILES = demos.h
