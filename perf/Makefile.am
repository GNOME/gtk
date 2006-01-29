## Makefile.am for gtk+/perf

INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_builddir)/gdk			\
	-I$(top_srcdir)/gdk			\
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
stamp-typebuiltins.h: @REBUILD@ $(headers_with_enums) Makefile
	( cd $(srcdir) && $(GLIB_MKENUMS) \
			--fhead "#ifndef __TYPE_BUILTINS_H__\n#define __TYPE_BUILTINS_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
			--fprod "/* enumerations from \"@filename@\" */\n" \
			--vhead "GType @enum_name@_get_type (void) G_GNUC_CONST;\n#define GTK_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
			--ftail "G_END_DECLS\n\n#endif /* __TYPE_BUILTINS_H__ */" \
		$(headers_with_enums) ) >> xgen-gtbh \
	&& (cmp -s xgen-gtbh typebuiltins.h || cp xgen-gtbh typebuiltins.h ) \
	&& rm -f xgen-gtbh \
	&& echo timestamp > $(@F)
typebuiltins.c: @REBUILD@ $(headers_with_enums) Makefile
	( cd $(srcdir) && $(GLIB_MKENUMS) \
			--fhead "#include \"gtkwidgetprofiler.h\"" \
			--ftail "#define __TYPE_BUILTINS_C__\n" \
			--fprod "\n/* enumerations from \"@filename@\" */" \
			--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {" \
			--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
		$(headers_with_enums) ) > xgen-gtbc \
	&& cp xgen-gtbc typebuiltins.c  \
	&& rm -f xgen-gtbc


EXTRA_DIST = 		\
	README		\
	marshalers.list	\
	$(BUILT_SOURCES)

# if srcdir!=builddir, clean out maintainer-clean files from builddir
# this allows dist to pass.
distclean-local:
	if test $(srcdir) != .; then \
	  rm -f $(MAINTAINERCLEANFILES); \
	fi
