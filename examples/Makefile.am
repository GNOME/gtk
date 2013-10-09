SUBDIRS = \
	application1 application2 application3 \
	application4 application5 application6 \
	application7 application8 application9

AM_CPPFLAGS = \
	-I$(top_srcdir)				\
	-I$(top_srcdir)/gdk			\
	-I$(top_builddir)/gdk			\
	-DGDK_DISABLE_DEPRECATED		\
	-DGTK_DISABLE_DEPRECATED		\
	$(GTK_DEBUG_FLAGS)			\
	$(GTK_DEP_CFLAGS)

LDADD = \
	$(top_builddir)/gtk/libgtk-3.la	\
	$(top_builddir)/gdk/libgdk-3.la \
	$(GTK_DEP_LIBS)

noinst_PROGRAMS = \
	hello-world				\
	window-default				\
	bloatpad				\
	plugman					\
	sunny					\
	action-namespace			\
	grid-packing				\
	drawing					\
	builder					\
	search-bar

EXTRA_DIST = builder.ui

-include $(top_srcdir)/git.mk
