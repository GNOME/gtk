## Makefile.am for gtk+/demos

demos = @STRIP_BEGIN@	\
	button_box.c	\
	item_factory.c	\
	menus.c		\
	panes.c		\
@STRIP_END@

INCLUDES = @STRIP_BEGIN@ \
	-I$(top_srcdir) 				\
	-I$(top_builddir)/gdk				\
	-DGTK_DISABLE_COMPAT_H				\
	@GTK_DEBUG_FLAGS@				\
	@GTK_XIM_FLAGS@					\
	@GTK_LOCALE_FLAGS@				\
	@PANGO_CFLAGS@					\
	@GLIB_CFLAGS@					\
	@more_cflags@					\
@STRIP_END@

DEPS = \
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-1.3.la	\
	$(top_builddir)/gdk/@gdktargetlib@ 		\
	$(top_builddir)/gtk/@gtktargetlib@

LDADDS = @STRIP_BEGIN@ 					\
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-1.3.la	\
	$(top_builddir)/gdk/@gdktargetlib@        	\
	$(top_builddir)/gtk/@gtktargetlib@ 		\
	@more_ldflags@					\
	@more_libs@					\
	@GDK_WLIBS@					\
	@PANGO_LIBS@					\
	@GLIB_LIBS@					\
	@GTK_LIBS_EXTRA@				\
	-lm						\
@STRIP_END@

noinst_PROGRAMS = testgtk

BUILT_SOURCES = demos.h

EXTRA_DIST = \
	geninclude.pl

demos.h: $(demos) geninclude.pl
	(cd $(srcdir) && ./geninclude.pl $(demos) > demos.h)

testgtk_SOURCES = 		\
	$(demos)		\
	main.c			\
	demos.h

testgtk_DEPENDENCIES = $(DEPS)
testgtk_LDADD = $(LDADDS)
