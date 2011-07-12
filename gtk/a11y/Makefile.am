include $(top_srcdir)/Makefile.decl

noinst_LTLIBRARIES = libgail.la

gail_c_sources =			\
	gail.c				\
	gtkarrowaccessible.c		\
	gtkbooleancellaccessible.c	\
	gtkboxaccessible.c		\
	gtkbuttonaccessible.c		\
	gtkcellaccessible.c		\
	gtkcellaccessibleparent.c	\
	gtkcheckmenuitemaccessible.c	\
	gtkchecksubmenuitemaccessible.c	\
	gtkcomboboxaccessible.c		\
	gtkcontaineraccessible.c	\
	gtkcontainercellaccessible.c	\
	gtkentryaccessible.c		\
	gtkexpanderaccessible.c		\
	gtkframeaccessible.c		\
	gtkimageaccessible.c		\
	gtkimagecellaccessible.c	\
	gtklabelaccessible.c		\
	gtklinkbuttonaccessible.c	\
	gtkmenuaccessible.c		\
	gtkmenushellaccessible.c	\
	gtkmenuitemaccessible.c		\
	gtknotebookaccessible.c		\
	gtknotebookpageaccessible.c	\
	gtkpanedaccessible.c		\
	gtkprogressbaraccessible.c	\
	gtkradiobuttonaccessible.c	\
	gtkradiomenuitemaccessible.c	\
	gtkradiosubmenuitemaccessible.c	\
	gtkrangeaccessible.c		\
	gtkrenderercellaccessible.c	\
	gtkscaleaccessible.c		\
	gtkscalebuttonaccessible.c	\
	gtkscrollbaraccessible.c	\
	gtkscrolledwindowaccessible.c	\
	gtkspinbuttonaccessible.c	\
	gtkspinneraccessible.c		\
	gtkstatusbaraccessible.c	\
	gtksubmenuitemaccessible.c	\
	gtkswitchaccessible.c		\
	gtktextcellaccessible.c		\
	gtktextviewaccessible.c		\
	gtktogglebuttonaccessible.c	\
	gtktoplevelaccessible.c		\
	gtktreeviewaccessible.c		\
	gtkwidgetaccessible.c		\
	gtkwindowaccessible.c		\
	gailutil.c			\
	gailmisc.c

gail_private_h_sources =		\
	gtkarrowaccessible.h		\
	gtkbooleancellaccessible.h	\
	gtkboxaccessible.h		\
	gtkbuttonaccessible.h		\
	gtkcellaccessible.h		\
	gtkcellaccessibleparent.h	\
	gtkcheckmenuitemaccessible.h	\
	gtkchecksubmenuitemaccessible.h	\
	gtkcomboboxaccessible.h		\
	gtkcontaineraccessible.h	\
	gtkcontainercellaccessible.h	\
	gtkentryaccessible.h		\
	gtkexpanderaccessible.h		\
	gtkframeaccessible.h		\
	gtkimageaccessible.h		\
	gtkimagecellaccessible.h	\
	gtklabelaccessible.h		\
	gtklinkbuttonaccessible.h	\
	gtkmenuaccessible.h		\
	gtkmenushellaccessible.h	\
	gtkmenuitemaccessible.h		\
	gtknotebookaccessible.h		\
	gtknotebookpageaccessible.h	\
	gtkpanedaccessible.h		\
	gtkprogressbaraccessible.h	\
	gtkradiobuttonaccessible.h	\
	gtkradiomenuitemaccessible.h	\
	gtkradiosubmenuitemaccessible.h	\
	gtkrangeaccessible.h		\
	gtkrenderercellaccessible.h	\
	gtkscaleaccessible.h		\
	gtkscalebuttonaccessible.h	\
	gtkscrollbaraccessible.h	\
	gtkscrolledwindowaccessible.h	\
	gtkspinbuttonaccessible.h	\
	gtkspinneraccessible.h		\
	gtkstatusbaraccessible.h	\
	gtksubmenuitemaccessible.h	\
	gtkswitchaccessible.h		\
	gtktextcellaccessible.h		\
	gtktextviewaccessible.h		\
	gtktogglebuttonaccessible.h	\
	gtktoplevelaccessible.h		\
	gtktreeviewaccessible.h		\
	gtkwidgetaccessible.h		\
	gtkwindowaccessible.h		\
	gailutil.h			\
	gailmisc.h

libgail_la_SOURCES = 			\
	$(gail_c_sources)		\
	$(gail_private_h_sources)

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
