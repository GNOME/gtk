## Makefile.am for gtk+/perf

INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_builddir)/gdk			\
	-I$(top_srcdir)/gdk			\
	-DG_DISABLE_DEPRECATED			\
	-DGDK_PIXBUF_DISABLE_DEPRECATED		\
	-DGDK_DISABLE_DEPRECATED		\
	-DGTK_DISABLE_DEPRECATED		\
	$(GTK_DEBUG_FLAGS)			\
	$(GTK_DEP_CFLAGS)

DEPS =									\
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la	\
	$(top_builddir)/gdk/$(gdktargetlib)				\
	$(top_builddir)/gtk/$(gtktargetlib)

LDADDS =								\
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la	\
	$(top_builddir)/gdk/$(gdktargetlib)				\
	$(top_builddir)/gtk/$(gtktargetlib)

noinst_PROGRAMS	= 	\
	testperf

testperf_DEPENDENCIES = $(TEST_DEPS)

testperf_LDADD = $(LDADDS)

testperf_SOURCES =	\
	appwindow.c	\
	appwindow.h	\
	main.c		\
	timers.c	\
	timers.h

EXTRA_DIST = 	\
	README
