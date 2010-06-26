## Makefile.am for gtk+/perf
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
	$(top_builddir)/gdk/$(gdktargetlib)				\
	$(top_builddir)/gtk/$(gtktargetlib)

LDADDS =								\
	$(top_builddir)/gdk/$(gdktargetlib)				\
	$(top_builddir)/gtk/$(gtktargetlib)				\
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
	marshalers.c		\
	marshalers.h		\
	textview.c		\
	treeview.c		\
	typebuiltins.c		\
	typebuiltins.h		\
	widgets.h

BUILT_SOURCES =			\
	marshalers.c		\
	marshalers.h		\
	typebuiltins.c		\
	typebuiltins.h

stamp_files =			\
	stamp-marshalers.h	\
	stamp-typebuiltins.h

headers_with_enums = 		\
	gtkwidgetprofiler.h

MAINTAINERCLEANFILES = $(BUILT_SOURCES) $(stamp_files)

marshalers.h: stamp-marshalers.h
	@true

stamp-marshalers.h: @REBUILD@ marshalers.list
	$(GLIB_GENMARSHAL) --prefix=_gtk_marshal $(srcdir)/marshalers.list --header >> xgen-gmlh \
	&& (cmp -s xgen-gmlh marshalers.h || cp xgen-gmlh marshalers.h) \
	&& rm -f xgen-gmlh \
	&& echo timestamp > $(@F)	
marshalers.c: @REBUILD@ marshalers.list
	$(GLIB_GENMARSHAL) --prefix=_gtk_marshal $(srcdir)/marshalers.list --body >> xgen-gmlc \
	&& cp xgen-gmlc marshalers.c \
	&& rm -f xgen-gmlc

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
	marshalers.list	\
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
