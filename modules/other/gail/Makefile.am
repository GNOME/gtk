include $(top_srcdir)/Makefile.decl

SUBDIRS = libgail-util tests

if PLATFORM_WIN32
no_undefined = -no-undefined
endif

moduledir = $(libdir)/gtk-2.0/modules
module_LTLIBRARIES = libgail.la

gail_c_sources =			\
	gail-private-macros.h		\
	gail.c				\
	gailadjustment.c		\
	gailarrow.c			\
	gailbooleancell.c		\
	gailbox.c			\
	gailbutton.c			\
	gailcalendar.c			\
	gailcell.c			\
	gailcellparent.c		\
	gailcheckmenuitem.c		\
	gailchecksubmenuitem.c		\
	gailclist.c			\
	gailclistcell.c			\
	gailcombo.c			\
	gailcombobox.c			\
	gailcontainer.c			\
	gailcontainercell.c		\
	gailentry.c			\
	gailexpander.c			\
	gailframe.c			\
	gailimage.c			\
	gailimagecell.c			\
	gailitem.c			\
	gaillabel.c			\
	gaillist.c			\
	gailmenu.c			\
	gailmenushell.c			\
	gailmenuitem.c			\
	gailnotebook.c			\
	gailnotebookpage.c		\
	gailobject.c			\
	gailoptionmenu.c		\
	gailpaned.c			\
	gailpixmap.c			\
	gailprogressbar.c		\
	gailradiobutton.c		\
	gailradiomenuitem.c		\
	gailradiosubmenuitem.c		\
	gailrange.c			\
	gailrenderercell.c		\
	gailscale.c			\
	gailscalebutton.c		\
	gailscrollbar.c			\
	gailscrolledwindow.c		\
	gailseparator.c			\
	gailspinbutton.c		\
	gailsubmenuitem.c		\
	gailstatusbar.c			\
	gailtextcell.c			\
	gailtextview.c			\
	gailtogglebutton.c		\
	gailtoplevel.c			\
	gailtreeview.c			\
	gailutil.c			\
	gailwidget.c			\
	gailwindow.c

libgailincludedir=$(includedir)/gail-1.0/gail

gail_private_h_sources =		\
	gail.h				\
	gailadjustment.h		\
	gailarrow.h			\
	gailbooleancell.h		\
	gailbox.h			\
	gailbutton.h			\
	gailcalendar.h			\
	gailcell.h			\
	gailcellparent.h		\
	gailcheckmenuitem.h		\
	gailchecksubmenuitem.h		\
	gailclist.h			\
	gailclistcell.h			\
	gailcombo.h			\
	gailcombobox.h			\
	gailcontainercell.h		\
	gailcontainer.h			\
	gailentry.h			\
	gailexpander.h			\
	gailfactory.h			\
	gailframe.h			\
	gailimage.h			\
	gailimagecell.h			\
	gailitem.h			\
	gaillabel.h			\
	gaillist.h			\
	gailmenu.h			\
	gailmenushell.h			\
	gailmenuitem.h			\
	gailnotebook.h			\
	gailnotebookpage.h		\
	gailobject.h			\
	gailoptionmenu.h		\
	gailpaned.h			\
	gailpixmap.h			\
	gailprogressbar.h		\
	gailradiobutton.h		\
	gailradiomenuitem.h		\
	gailradiosubmenuitem.h		\
	gailrange.h			\
	gailrenderercell.h		\
	gailscale.h			\
	gailscalebutton.h		\
	gailscrollbar.h			\
	gailscrolledwindow.h		\
	gailseparator.h			\
	gailspinbutton.h		\
	gailsubmenuitem.h		\
	gailstatusbar.h			\
	gailtextcell.h			\
	gailtextview.h			\
	gailtogglebutton.h		\
	gailtoplevel.h			\
	gailtreeview.h			\
	gailutil.h			\
	gailwindow.h

gail_public_h_sources =	\
	gailwidget.h

libgail_la_SOURCES = 			\
	$(gail_c_sources)		\
	$(gail_public_h_sources)	\
	$(gail_private_h_sources)

libgailinclude_HEADERS =		\
	$(gail_public_h_sources)

libgail_la_CPPFLAGS = \
	-I$(top_srcdir)			\
	-I$(top_srcdir)/modules/other	\
	-I$(top_srcdir)/gdk		\
	-I$(top_builddir)/gdk		\
	-I$(top_srcdir)/gtk		\
	-I$(top_builddir)/gtk		\
	-DGTK_VERSION=\"$(GTK_VERSION)\"\
	-DGDK_DISABLE_DEPRECATED	\
	-DGTK_DISABLE_DEPRECATED	\
	$(AM_CPPFLAGS)

libgail_la_CFLAGS = \
	$(GTK_DEP_CFLAGS)	\
	$(GTK_DEBUG_FLAGS)	\
	$(AM_CFLAGS)

libgail_la_LIBADD =  \
	$(top_builddir)/gdk/$(gdktargetlib) 				\
	$(top_builddir)/gtk/$(gtktargetlib)				\
	$(top_builddir)/modules/other/gail/libgail-util/libgailutil.la	\
	$(GTK_DEP_LIBS)							\
	$(INTLLIBS)

libgail_la_LDFLAGS =    \
	-rpath $(moduledir) -module -avoid-version \
	$(no_undefined) \
        $(LDFLAGS)

-include $(top_srcdir)/git.mk
