## Process this file with automake to produce Makefile.in

gtkincludedir = $(includedir)/gtk

lib_LTLIBRARIES = libgtk.la

libgtk_la_SOURCES = \
	gtkaccelerator.c	\
	gtkadjustment.c		\
	gtkaspectframe.c	\
	gtkalignment.c		\
	gtkarrow.c		\
	gtkbin.c		\
	gtkbbox.c		\
	gtkbox.c		\
	gtkbutton.c		\
	gtkcheckbutton.c	\
	gtkcheckmenuitem.c	\
	gtkclist.c		\
	gtkcolorsel.c		\
	gtkcombo.c		\
	gtkcontainer.c		\
	gtkctree.c		\
	gtkcurve.c		\
	gtkdata.c		\
	gtkdialog.c		\
	gtkdrawingarea.c	\
	gtkeditable.c           \
	gtkentry.c		\
	gtkeventbox.c		\
	gtkfilesel.c		\
	gtkfixed.c		\
	gtkframe.c		\
	gtkgamma.c		\
	gtkgc.c			\
	gtkhandlebox.c		\
	gtkhbbox.c		\
	gtkhbox.c		\
	gtkhpaned.c		\
	gtkhruler.c		\
	gtkhscale.c		\
	gtkhscrollbar.c		\
	gtkhseparator.c		\
	gtkimage.c		\
	gtkinputdialog.c	\
	gtkitem.c		\
	gtklabel.c		\
	gtklist.c		\
	gtklistitem.c		\
	gtkmain.c		\
	gtkmenu.c		\
	gtkmenubar.c		\
	gtkmenufactory.c	\
	gtkmenuitem.c		\
	gtkmenushell.c		\
	gtkmisc.c		\
	gtknotebook.c		\
	gtkobject.c		\
	gtkoptionmenu.c		\
	gtkpaned.c		\
	gtkpixmap.c		\
	gtkpreview.c		\
	gtkprogressbar.c	\
	gtkradiobutton.c	\
	gtkradiomenuitem.c	\
	gtkrange.c		\
	gtkrc.c			\
	gtkruler.c		\
	gtkscale.c		\
	gtkscrollbar.c		\
	gtkscrolledwindow.c	\
	gtkselection.c		\
	gtkseparator.c		\
	gtksignal.c		\
	gtkspinbutton.c		\
	gtkstyle.c		\
	gtkstatusbar.c		\
	gtktable.c		\
	gtktext.c		\
	gtktipsquery.c		\
	gtktogglebutton.c	\
	gtktoolbar.c		\
	gtktooltips.c		\
	gtktree.c		\
	gtktreeitem.c		\
	gtktypeutils.c		\
	gtkvbbox.c		\
	gtkvbox.c		\
	gtkviewport.c		\
	gtkvpaned.c		\
	gtkvruler.c		\
	gtkvscale.c		\
	gtkvscrollbar.c		\
	gtkvseparator.c		\
	gtkwidget.c		\
	gtkwindow.c		\
	fnmatch.c		\
	fnmatch.h

gtkinclude_HEADERS = \
	gtk.h			\
	gtkaccelerator.h	\
	gtkadjustment.h		\
	gtkaspectframe.h	\
	gtkalignment.h		\
	gtkarrow.h		\
	gtkbin.h		\
	gtkbbox.h		\
	gtkbox.h		\
	gtkbutton.h		\
	gtkcheckbutton.h	\
	gtkcheckmenuitem.h	\
	gtkclist.h		\
	gtkcolorsel.h		\
	gtkcombo.h		\
	gtkcontainer.h		\
	gtkctree.h		\
	gtkcurve.h		\
	gtkdata.h		\
	gtkdebug.h              \
	gtkdialog.h		\
	gtkdrawingarea.h	\
	gtkeditable.h           \
	gtkentry.h		\
	gtkenums.h		\
	gtkeventbox.h		\
	gtkfeatures.h		\
	gtkfilesel.h		\
	gtkfixed.h		\
	gtkframe.h		\
	gtkgamma.h		\
	gtkgc.h			\
	gtkhandlebox.h		\
	gtkhbbox.h		\
	gtkhbox.h		\
	gtkhpaned.h		\
	gtkhruler.h		\
	gtkhscale.h		\
	gtkhscrollbar.h		\
	gtkhseparator.h		\
	gtkimage.h		\
	gtkinputdialog.h	\
	gtkitem.h		\
	gtklabel.h		\
	gtklist.h		\
	gtklistitem.h		\
	gtkmain.h		\
	gtkmenu.h		\
	gtkmenubar.h		\
	gtkmenufactory.h	\
	gtkmenuitem.h		\
	gtkmenushell.h		\
	gtkmisc.h		\
	gtknotebook.h		\
	gtkobject.h		\
	gtkoptionmenu.h		\
	gtkpaned.h		\
	gtkpixmap.h		\
	gtkpreview.h		\
	gtkprivate.h		\
	gtkprogressbar.h	\
	gtkradiobutton.h	\
	gtkradiomenuitem.h	\
	gtkrange.h		\
	gtkrc.h			\
	gtkruler.h		\
	gtkscale.h		\
	gtkscrollbar.h		\
	gtkscrolledwindow.h	\
	gtkselection.h		\
	gtkseparator.h		\
	gtksignal.h		\
	gtkspinbutton.h		\
	gtkstyle.h		\
	gtkstatusbar.h		\
	gtktable.h		\
	gtktext.h		\
	gtktipsquery.h		\
	gtktogglebutton.h	\
	gtktoolbar.h		\
	gtktooltips.h		\
	gtktree.h		\
	gtktreeitem.h		\
	gtktypeutils.h		\
	gtkvbbox.h		\
	gtkvbox.h		\
	gtkviewport.h		\
	gtkvpaned.h		\
	gtkvruler.h		\
	gtkvscale.h		\
	gtkvscrollbar.h		\
	gtkvseparator.h		\
	gtkwidget.h		\
	gtkwindow.h             \
	gtktypebuiltins.h

$(srcdir)/gtktypebuiltins.h: @MAINT@ $(srcdir)/gtk.defs $(srcdir)/gentypeinfo.el
	$(SHELL) $(srcdir)/runelisp $(srcdir)/gentypeinfo.el idmac $< $@

$(srcdir)/gtktypebuiltins.c: @MAINT@ $(srcdir)/gtk.defs $(srcdir)/gentypeinfo.el
	$(SHELL) $(srcdir)/runelisp $(srcdir)/gentypeinfo.el id $< $@

libgtk_la_LDFLAGS = -postfix $(LT_RELEASE) -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)

EXTRA_DIST = \
	line-arrow.xbm 		\
	line-wrap.xbm 		\
	testgtkrc 		\
	testgtkrc2 		\
	gtk.defs		\
	gtkfeatures.h.in	\
	runelisp 		\
	gentypeinfo.el 		\
	gtktypebuiltins.c	\
	test.xpm		\
	marble.xpm		\
	3DRings.xpm		\
	FilesQueue.xpm		\
	Modeller.xpm		\
	tree_plus.xpm		\
	tree_minus.xpm		\
	tree_plus.xbm		\
	tree_minus.xbm          \
        circles.xbm

INCLUDES = -I$(top_srcdir) -I../glib -I$(top_srcdir)/glib @x_cflags@ 

noinst_PROGRAMS = testgtk testinput testselection simple 

testgtk_LDADD = \
	libgtk.la				\
	$(top_builddir)/gdk/libgdk.la		\
	@x_ldflags@				\
	@x_libs@				\
	$(top_builddir)/glib/libglib.la		\
	-lm

testinput_LDADD = \
	libgtk.la				\
	$(top_builddir)/gdk/libgdk.la		\
	@x_ldflags@				\
	@x_libs@				\
	$(top_builddir)/glib/libglib.la		\
	-lm

testselection_LDADD = \
	libgtk.la				\
	$(top_builddir)/gdk/libgdk.la		\
	@x_ldflags@				\
	@x_libs@				\
	$(top_builddir)/glib/libglib.la		\
	-lm

simple_LDADD = \
	libgtk.la				\
	$(top_builddir)/gdk/libgdk.la		\
	@x_ldflags@				\
	@x_libs@				\
	$(top_builddir)/glib/libglib.la		\
	-lm

DEPS = \
	libgtk.la		                \
	$(top_builddir)/gdk/libgdk.la		\
	$(top_builddir)/glib/libglib.la

testgtk_DEPENDENCIES = $(DEPS)
testinput_DEPENDENCIES = $(DEPS)
testselection_DEPENDENCIES = $(DEPS)
simple_DEPENDENCIES = $(DEPS)

.PHONY: files test test-debug

files:
	@files=`ls $(DISTFILES) 2> /dev/null `; for p in $$files; do \
	  echo $$p; \
	done

test: testgtk
	builddir=`pwd`; cd $(top_builddir); top_builddir=`pwd`; \
	cd $$builddir; cd $(srcdir); \
	$(SHELL) $$top_builddir/libtool --mode=execute $$builddir/testgtk

test-debug: testgtk
	builddir=`pwd`; cd $(top_builddir); top_builddir=`pwd`; \
	cd $$builddir; cd $(srcdir); \
	$(SHELL) $$top_builddir/libtool --mode=execute gdb $$builddir/testgtk
