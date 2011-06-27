include $(top_srcdir)/Makefile.decl

noinst_LTLIBRARIES = libgail.la

gail_c_sources =			\
	gail-private-macros.h		\
	gail.c				\
	gailadjustment.c		\
	gtkarrowaccessible.c		\
	gailbooleancell.c		\
	gailbutton.c			\
	gailcell.c			\
	gailcellparent.c		\
	gailcheckmenuitem.c		\
	gailchecksubmenuitem.c		\
	gailcombobox.c			\
	gailcontainer.c			\
	gailcontainercell.c		\
	gtkentryaccessible.c		\
	gailexpander.c			\
	gailframe.c			\
	gailimage.c			\
	gailimagecell.c			\
	gtklabelaccessible.c		\
	gaillinkbutton.c		\
	gailmisc.c			\
	gailmenu.c			\
	gailmenushell.c			\
	gailmenuitem.c			\
	gailnotebook.c			\
	gailnotebookpage.c		\
	gailpaned.c			\
	gailprogressbar.c		\
	gailradiobutton.c		\
	gailradiomenuitem.c		\
	gailradiosubmenuitem.c		\
	gailrange.c			\
	gailrenderercell.c		\
	gtkscaleaccessible.c		\
	gailscalebutton.c		\
	gailscrollbar.c			\
	gailscrolledwindow.c		\
	gtkspinbuttonaccessible.c	\
	gailsubmenuitem.c		\
	gailstatusbar.c			\
	gailtextcell.c			\
	gailtextutil.c			\
	gtktextviewaccessible.c		\
	gailtogglebutton.c		\
	gailtoplevel.c			\
	gailtreeview.c			\
	gailutil.c			\
	gailwidget.c			\
	gailwindow.c

libgailincludedir=$(includedir)/gail-3.0/gail

gail_private_h_sources =		\
	gailadjustment.h		\
	gtkarrowaccessible.h		\
	gailbooleancell.h		\
	gailbutton.h			\
	gailcell.h			\
	gailcellparent.h		\
	gailcheckmenuitem.h		\
	gailchecksubmenuitem.h		\
	gailcombobox.h			\
	gailcontainercell.h		\
	gailcontainer.h			\
	gtkentryaccessible.h		\
	gailexpander.h			\
	gailfactory.h			\
	gailframe.h			\
	gailimage.h			\
	gailimagecell.h			\
	gtklabelaccessible.h		\
	gaillinkbutton.h		\
	gailmisc.h			\
	gailmenu.h			\
	gailmenushell.h			\
	gailmenuitem.h			\
	gailnotebook.h			\
	gailnotebookpage.h		\
	gailpaned.h			\
	gailprogressbar.h		\
	gailradiobutton.h		\
	gailradiomenuitem.h		\
	gailradiosubmenuitem.h		\
	gailrange.h			\
	gailrenderercell.h		\
	gtkscaleaccessible.h		\
	gailscalebutton.h		\
	gailscrollbar.h			\
	gailscrolledwindow.h		\
	gtkspinbuttonaccessible.h	\
	gailsubmenuitem.h		\
	gailstatusbar.h			\
	gailtextcell.h			\
	gailtextutil.h			\
	gtktextviewaccessible.h		\
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
	-I$(top_srcdir)/gdk		\
	-I$(top_builddir)/gdk		\
	-I$(top_srcdir)/gtk		\
	-I$(top_builddir)/gtk		\
	-DGTK_VERSION=\"$(GTK_VERSION)\"\
	-DGTK_COMPILATION		\
	-DGDK_DISABLE_DEPRECATED	\
	-DGTK_DISABLE_DEPRECATED	\
	$(AM_CPPFLAGS)

libgail_la_CFLAGS = \
	$(GTK_DEP_CFLAGS)	\
	$(GTK_DEBUG_FLAGS)	\
	$(AM_CFLAGS)

libgail_la_LIBADD =  \
	$(GTK_DEP_LIBS)						\
	$(INTLLIBS)

libgail_la_LDFLAGS =    \
        $(LDFLAGS)

-include $(top_srcdir)/git.mk
