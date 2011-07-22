## Makefile.am for gtk+/perf
include $(top_srcdir)/Makefile.decl

AM_CPPFLAGS =				\
	-I$(top_srcdir)			\
	-I$(top_builddir)/gdk		\
	-I$(top_srcdir)/gdk		\
	-DGDK_DISABLE_DEPRECATED	\
	-DGTK_DISABLE_DEPRECATED	\
	$(GTK_DEBUG_FLAGS)		\
	$(GTK_DEP_CFLAGS)

DEPS = \
	$(top_builddir)/gtk/libgtk-3.la

LDADDS = \
	$(top_builddir)/gtk/libgtk-3.la	\
	$(top_builddir)/gdk/libgdk-3.la	\
	$(GTK_DEP_LIBS)

noinst_PROGRAMS	= 	\
	testperf

testperf_DEPENDENCIES = $(TEST_DEPS)

testperf_LDADD = $(LDADDS)

testperf_SOURCES =		\
	appwindow.c		\
	gtkwidgetprofiler.c	\
	gtkwidgetprofiler.h	\
	main.c			\
	textview.c		\
	treeview.c		\
	typebuiltins.c		\
	typebuiltins.h		\
	widgets.h

BUILT_SOURCES =			\
	typebuiltins.c		\
	typebuiltins.h

stamp_files =			\
	stamp-typebuiltins.h

headers_with_enums = 		\
	gtkwidgetprofiler.h

MAINTAINERCLEANFILES = $(BUILT_SOURCES) $(stamp_files)


typebuiltins.h: stamp-typebuiltins.h
	@true
stamp-typebuiltins.h: @REBUILD@ $(headers_with_enums) typebuiltins.h.template
	( cd $(srcdir) && $(GLIB_MKENUMS) --template typebuiltins.h.template \
		$(headers_with_enums) ) >> xgen-gtbh \
	&& (cmp -s xgen-gtbh typebuiltins.h || cp xgen-gtbh typebuiltins.h ) \
	&& rm -f xgen-gtbh \
	&& echo timestamp > $(@F)
typebuiltins.c: @REBUILD@ $(headers_with_enums) typebuiltins.c.template
	( cd $(srcdir) && $(GLIB_MKENUMS) --template typebuiltins.c.template \
		$(headers_with_enums) ) > xgen-gtbc \
	&& cp xgen-gtbc typebuiltins.c  \
	&& rm -f xgen-gtbc


EXTRA_DIST += 		\
	README		\
	typebuiltins.c.template	\
	typebuiltins.h.template	\
	$(BUILT_SOURCES)

# if srcdir!=builddir, clean out maintainer-clean files from builddir
# this allows dist to pass.
distclean-local:
	if test $(srcdir) != .; then \
	  rm -f $(MAINTAINERCLEANFILES); \
	fi

-include $(top_srcdir)/git.mk
