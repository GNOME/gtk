## Makefile.am for gtk+/tests

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

noinst_PROGRAMS = 		\
##	testsocket		\
##	testsocket_child	\
	testtreeview		\
	testtreefocus		\
	testtreecolumns		\
	testtreesort

#testsocket_DEPENDENCIES = $(DEPS)
#testsocket_child_DEPENDENCIES = $(DEPS)
testtreeview_DEPENDENCIES = $(DEPS)
testtreefocus_DEPENDENCIES = $(DEPS)
testtreecolumns_DEPENDENCIES = $(DEPS)
testtreesort_DEPENDENCIES = $(DEPS)

#testsocket_LDADD = $(LDADDS)
#testsocket_child_LDADD = $(LDADDS)
testtreeview_LDADD = $(LDADDS)
testtreefocus_LDADD = $(LDADDS)
testtreecolumns_LDADD = $(LDADDS)
testtreesort_LDADD = $(LDADDS)

