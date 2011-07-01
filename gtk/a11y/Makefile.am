include $(top_srcdir)/Makefile.decl

noinst_LTLIBRARIES = libgail.la

gail_c_sources =			\
	gail.c				\
	gtkarrowaccessible.c		\
	gailbooleancell.c		\
	gtkboxaccessible.c		\
	gailbutton.c			\
	gailcell.c			\
	gailcellparent.c		\
	gailcheckmenuitem.c		\
	gailchecksubmenuitem.c		\
	gtkcomboboxaccessible.c		\
	gailcontainer.c			\
	gailcontainercell.c		\
	gtkentryaccessible.c		\
	gtkexpanderaccessible.c		\
	gtkframeaccessible.c		\
	gtkimageaccessible.c		\
	gailimagecell.c			\
	gtklabelaccessible.c		\
	gtklinkbuttonaccessible.c	\
	gailmisc.c			\
	gailmenu.c			\
	gailmenushell.c			\
	gailmenuitem.c			\
	gtknotebookaccessible.c		\
	gtknotebookpageaccessible.c	\
	gtkpanedaccessible.c		\
	gtkprogressbaraccessible.c	\
	gtkradiobuttonaccessible.c	\
	gtkradiomenuitemaccessible.c	\
	gtkradiosubmenuitemaccessible.c	\
	gtkrangeaccessible.c		\
	gailrenderercell.c		\
	gtkscaleaccessible.c		\
	gtkscalebuttonaccessible.c	\
	gtkscrollbaraccessible.c	\
	gtkscrolledwindowaccessible.c	\
	gtkspinbuttonaccessible.c	\
	gailsubmenuitem.c		\
	gtkstatusbaraccessible.c	\
	gailtextcell.c			\
	gailtextutil.c			\
	gtktextviewaccessible.c		\
	gtktogglebuttonaccessible.c	\
	gailtoplevel.c			\
	gtktreeviewaccessible.c		\
	gailutil.c			\
	gailwidget.c			\
	gailwindow.c

libgailincludedir=$(includedir)/gail-3.0/gail

gail_private_h_sources =		\
	gtkarrowaccessible.h		\
	gailbooleancell.h		\
	gtkboxaccessible.h		\
	gailbutton.h			\
	gailcell.h			\
	gailcellparent.h		\
	gailcheckmenuitem.h		\
	gailchecksubmenuitem.h		\
	gtkcomboboxaccessible.h		\
	gailcontainercell.h		\
	gailcontainer.h			\
	gtkentryaccessible.h		\
	gtkexpanderaccessible.h		\
	gailfactory.h			\
	gtkframeaccessible.h		\
	gtkimageaccessible.h		\
	gailimagecell.h			\
	gtklabelaccessible.h		\
	gtklinkbuttonaccessible.h	\
	gailmisc.h			\
	gailmenu.h			\
	gailmenushell.h			\
	gailmenuitem.h			\
	gtknotebookaccessible.h		\
	gtknotebookpageaccessible.h	\
	gtkpanedaccessible.h		\
	gtkprogressbaraccessible.h	\
	gtkradiobuttonaccessible.h	\
	gtkradiomenuitemaccessible.h	\
	gtkradiosubmenuitemaccesisble.h	\
	gtkrangeaccessible.h		\
	gailrenderercell.h		\
	gtkscaleaccessible.h		\
	gtkscalebuttonaccessible.h	\
	gtkscrollbaraccessible.h	\
	gtkscrolledwindowaccessible.h	\
	gtkspinbuttonaccessible.h	\
	gailsubmenuitem.h		\
	gtkstatusbaraccessible.h	\
	gailtextcell.h			\
	gailtextutil.h			\
	gtktextviewaccessible.h		\
	gtktogglebuttonaccessible.h	\
	gailtoplevel.h			\
	gtktreeviewaccessible.h		\
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
	$(GTK_DEP_LIBS)		\
	$(INTLLIBS)

libgail_la_LDFLAGS =    \
        $(LDFLAGS)

-include $(top_srcdir)/git.mk
