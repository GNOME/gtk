## Makefile.am for gtk+/tests
include $(top_srcdir)/Makefile.decl

if PLATFORM_WIN32
no_undefined = -no-undefined
endif

moduledir = $(libdir)/gtk-2.0/modules

if !OS_WIN32
module_LTLIBRARIES = \
	libferret.la
endif

noinst_LTLIBRARIES = 		\
	libtestaction.la	\
	libtestbutton.la	\
	libtestcombo.la		\
	libtestcomponent.la	\
	libtestimage.la		\
	libtestnotebook.la	\
	libtestobject.la	\
	libtestmenuitem.la	\
	libtestoptionmenu.la	\
	libtestpaned.la		\
	libtestprops.la		\
	libtestselection.la	\
	libteststatusbar.la	\
	libtesttable.la		\
	libtesttext.la		\
	libtesttoplevel.la	\
	libtesttreetable.la	\
	libtestvalues.la

AM_CPPFLAGS = \
	-I$(top_srcdir)		\
	-I$(top_srcdir)/gdk	\
	-I$(top_builddir)/gdk	\
	-I$(top_srcdir)/gtk	\
	-I$(top_builddir)/gtk	\
	-DGDK_DISABLE_DEPRECATED\
	-DGTK_DISABLE_DEPRECATED

AM_CFLAGS = \
	$(GTK_DEP_CFLAGS)	\
	$(GTK_DEBUG_FLAGS)

if !OS_WIN32
libferret_la_SOURCES = 		\
	testlib.c		\
	testlib.h		\
	ferret.c

libferret_la_LDFLAGS =		\
	-rpath $(moduledir) -module -avoid-version \
	$(no_undefined)	\
	$(LDFLAGS)

libferret_la_LIBADD =			    \
	$(top_builddir)/gtk/$(gtktargetlib) \
        $(GTK_DEP_LIBS)  		    \
	$(GAIL_INET_LIBS)
endif

libtestaction_la_SOURCES = 	\
	testaction.c

libtestaction_la_LDFLAGS =	\
	-rpath $(moduledir) -module -avoid-version $(no_undefined) \
	$(top_builddir)/gtk/$(gtktargetlib) \
	$(top_builddir)/gdk/$(gdktargetlib) \
	$(GTK_DEP_LIBS)	\
	$(LDFLAGS)

libtestbutton_la_SOURCES = 	\
	testlib.c		\
	testlib.h		\
	testbutton.c

libtestbutton_la_LDFLAGS =	\
	-rpath $(moduledir) -module -avoid-version $(no_undefined) \
	$(top_builddir)/gtk/$(gtktargetlib) \
	$(top_builddir)/gdk/$(gdktargetlib) \
	$(GTK_DEP_LIBS)	\
	$(LDFLAGS)

libtestcombo_la_SOURCES = 	\
	testlib.c		\
	testlib.h		\
	testcombo.c

libtestcombo_la_LDFLAGS =		\
	-rpath $(moduledir) -module -avoid-version $(no_undefined) \
	$(top_builddir)/gtk/$(gtktargetlib) \
	$(top_builddir)/gdk/$(gdktargetlib) \
	$(GTK_DEP_LIBS)	\
	$(LDFLAGS)

libtestcomponent_la_SOURCES =	\
	testcomponent.c

libtestcomponent_la_LDFLAGS =	\
	-rpath $(moduledir) -module -avoid-version $(no_undefined) \
	$(top_builddir)/gtk/$(gtktargetlib) \
	$(top_builddir)/gdk/$(gdktargetlib) \
	$(GTK_DEP_LIBS)	\
	$(LDFLAGS)

libtestimage_la_SOURCES = 	\
	testimage.c

libtestimage_la_LDFLAGS =		\
	-rpath $(moduledir) -module -avoid-version $(no_undefined) \
	$(top_builddir)/gtk/$(gtktargetlib) \
	$(top_builddir)/gdk/$(gdktargetlib) \
	$(GTK_DEP_LIBS)	\
	$(LDFLAGS)

libtestmenuitem_la_SOURCES = 	\
	testlib.c		\
	testlib.h		\
	testmenuitem.c

libtestmenuitem_la_LDFLAGS =		\
	-rpath $(moduledir) -module -avoid-version $(no_undefined) \
	$(top_builddir)/gtk/$(gtktargetlib) \
	$(top_builddir)/gdk/$(gdktargetlib) \
	$(GTK_DEP_LIBS)	\
	$(LDFLAGS)

libtestnotebook_la_SOURCES = 	\
	testlib.c		\
	testlib.h		\
	testnotebook.c

libtestnotebook_la_LDFLAGS =		\
	-rpath $(moduledir) -module -avoid-version $(no_undefined) \
	$(top_builddir)/gtk/$(gtktargetlib) \
	$(top_builddir)/gdk/$(gdktargetlib) \
	$(GTK_DEP_LIBS)	\
	$(LDFLAGS)

libtestobject_la_SOURCES = 	\
	testlib.c		\
	testlib.h		\
	testobject.c

libtestobject_la_LDFLAGS =		\
	-rpath $(moduledir) -module -avoid-version $(no_undefined) \
	$(top_builddir)/gtk/$(gtktargetlib) \
	$(top_builddir)/gdk/$(gdktargetlib) \
	$(GTK_DEP_LIBS)	\
	$(LDFLAGS)

libtestoptionmenu_la_SOURCES = 	\
	testlib.c		\
	testlib.h		\
	testoptionmenu.c

libtestoptionmenu_la_LDFLAGS =	\
	-rpath $(moduledir) -module -avoid-version $(no_undefined) \
	$(top_builddir)/gtk/$(gtktargetlib) \
	$(top_builddir)/gdk/$(gdktargetlib) \
	$(GTK_DEP_LIBS)	\
	$(LDFLAGS)

libtestpaned_la_SOURCES = 	\
	testlib.c		\
	testlib.h		\
	testpaned.c

libtestpaned_la_LDFLAGS =	\
	-rpath $(moduledir) -module -avoid-version $(no_undefined) \
	$(top_builddir)/gtk/$(gtktargetlib) \
	$(top_builddir)/gdk/$(gdktargetlib) \
	$(GTK_DEP_LIBS)	\
	$(LDFLAGS)

libtestprops_la_SOURCES = 	\
	testlib.c		\
	testlib.h		\
	testprops.c

libtestprops_la_LDFLAGS =	\
	-rpath $(moduledir) -module -avoid-version $(no_undefined) \
	$(top_builddir)/gtk/$(gtktargetlib) \
	$(top_builddir)/gdk/$(gdktargetlib) \
	$(GTK_DEP_LIBS)	\
	$(LDFLAGS)

libtestselection_la_SOURCES = \
	testselection.c

libtestselection_la_LDFLAGS =	\
	-rpath $(moduledir) -module -avoid-version $(no_undefined) \
	$(top_builddir)/gtk/$(gtktargetlib) \
	$(top_builddir)/gdk/$(gdktargetlib) \
	$(GTK_DEP_LIBS)	\
	$(LDFLAGS)

libteststatusbar_la_SOURCES = \
	teststatusbar.c

libteststatusbar_la_LDFLAGS =	\
	-rpath $(moduledir) -module -avoid-version $(no_undefined) \
	$(top_builddir)/gtk/$(gtktargetlib) \
	$(top_builddir)/gdk/$(gdktargetlib) \
	$(GTK_DEP_LIBS)	\
	$(LDFLAGS)

libtesttable_la_SOURCES = 	\
	testlib.c		\
	testlib.h		\
	testtextlib.c		\
	testtextlib.h		\
	testtable.c

libtesttable_la_LDFLAGS =	\
	-rpath $(moduledir) -module -avoid-version $(no_undefined) \
	$(top_builddir)/gtk/$(gtktargetlib) \
	$(top_builddir)/gdk/$(gdktargetlib) \
	$(GTK_DEP_LIBS)	\
	$(LDFLAGS)

libtesttext_la_SOURCES = 	\
	testlib.c		\
	testlib.h		\
	testtextlib.c		\
	testtextlib.h		\
        testtext.c

libtesttext_la_LDFLAGS =	\
	-rpath $(moduledir) -module -avoid-version $(no_undefined) \
	$(top_builddir)/gtk/$(gtktargetlib) \
	$(top_builddir)/gdk/$(gdktargetlib) \
	$(GTK_DEP_LIBS)	\
	$(LDFLAGS)

libtesttoplevel_la_SOURCES = 	\
	testlib.c		\
	testlib.h		\
	testtoplevel.c

libtesttoplevel_la_LDFLAGS =	\
	-rpath $(moduledir) -module -avoid-version $(no_undefined) \
	$(top_builddir)/gtk/$(gtktargetlib) \
	$(top_builddir)/gdk/$(gdktargetlib) \
	$(GTK_DEP_LIBS)	\
	$(LDFLAGS)

libtesttreetable_la_SOURCES =	\
	testlib.c		\
	testlib.h		\
	testtreetable.c

libtesttreetable_la_LDFLAGS =	\
	-rpath $(moduledir) -module -avoid-version $(no_undefined) \
	$(top_builddir)/gtk/$(gtktargetlib) \
	$(top_builddir)/gdk/$(gdktargetlib) \
	$(GTK_DEP_LIBS)	\
	$(LDFLAGS)

libtestvalues_la_SOURCES = 	\
	testvalues.c

libtestvalues_la_LDFLAGS =	\
	-rpath $(moduledir) -module -avoid-version $(no_undefined) \
	$(top_builddir)/gtk/$(gtktargetlib) \
	$(top_builddir)/gdk/$(gdktargetlib) \
	$(GTK_DEP_LIBS)	\
	$(LDFLAGS)

-include $(top_srcdir)/git.mk
