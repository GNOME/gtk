## Makefile.am for gtk+/tests/mpx
include $(top_srcdir)/Makefile.decl

INCLUDES =				\
	-I$(top_srcdir)			\
	-I$(top_builddir)/gdk		\
	-I$(top_srcdir)/gdk		\
	-DGDK_DISABLE_DEPRECATED	\
	-DGTK_DISABLE_DEPRECATED	\
	$(GTK_DEBUG_FLAGS)		\
	$(GTK_DEP_CFLAGS)

DEPS =									\
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la	\
	$(top_builddir)/gdk/$(gdktargetlib)				\
	$(top_builddir)/gtk/$(gtktargetlib)

LDADDS =								\
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la	\
	$(top_builddir)/gdk/$(gdktargetlib)				\
	$(top_builddir)/gtk/$(gtktargetlib)

noinst_PROGRAMS = 			\
	testangle			\
	testcoordinates			\
	testphotoalbum

testangle_DEPENDENCIES = $(TEST_DEPS)
testcoordinates_DEPENDENCIES = $(TEST_DEPS)
testphotoalbum_DEPENDENCIES = $(TEST_DEPS)

testangle_LDADD = $(LDADDS)
testcoordinates_LDADD = $(LDADDS)
testphotoalbum_LDADD = $(LDADDS)

testangle_SOURCES =		\
	testangle.c

testcoordinates_SOURCES = 	\
	testcoordinatewidget.c	\
	testcoordinatewidget.h	\
	testcoordinates.c

testphotoalbum_SOURCES =	\
	testphotoalbumwidget.c	\
	testphotoalbumwidget.h	\
	testphotoalbum.c
