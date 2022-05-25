AUTOMAKE_OPTIONS = subdir-objects

include $(top_srcdir)/Makefile.decl

GTK_BASE_CFLAGS_DEFINES =				\
	-DG_LOG_USE_STRUCTURED=1			\
	-DGTK_VERSION=\"$(GTK_VERSION)\"		\
	-DGTK_BINARY_VERSION=\"$(GTK_BINARY_VERSION)\"	\
	-DGTK_COMPILATION				\
	-DGTK_PRINT_BACKEND_ENABLE_UNSUPPORTED

GTK_PLAT_CFLAGS_DEFINES =				\
	-DGTK_LIBDIR=\"$(libdir)\"			\
	-DGTK_LOCALEDIR=\"$(localedir)\"		\
	-DGTK_DATADIR=\"$(datadir)\"			\
	-DGTK_DATA_PREFIX=\"$(prefix)\"			\
	-DGTK_SYSCONFDIR=\"$(sysconfdir)\"		\
	-DGTK_HOST=\"$(host)\"				\
	-DGTK_PRINT_BACKENDS=\"$(GTK_PRINT_BACKENDS)\"

AM_CPPFLAGS =						\
	-DG_LOG_DOMAIN=\"Gtk\"				\
	$(GTK_BASE_CFLAGS_DEFINES)			\
	$(GTK_PLAT_CFLAGS_DEFINES)			\
	-DX11_DATA_PREFIX=\"$(X11_PREFIX)\"		\
	-DISO_CODES_PREFIX=\"$(ISO_CODES_PREFIX)\"	\
	-I$(top_builddir)				\
	-I$(top_builddir)/gtk				\
	-I$(top_srcdir) 				\
	-I$(top_srcdir)/gdk				\
	$(GMODULE_CFLAGS)				\
	$(GTK_DEBUG_FLAGS)				\
	$(GTK_DEP_CFLAGS)				\
	$(INCLUDED_IMMODULE_DEFINE)

if OS_WIN32
gtk_def = gtk.def
gtk_win32_symbols = -export-symbols $(srcdir)/gtk.def

gtk_win32_res = gtk-win32-res.o
gtk_win32_res_ldflag = -Wl,gtk-win32-res.o

gtk-win32-res.o : gtk-win32.rc libgtk3.manifest
	$(WINDRES) gtk-win32.rc $@

gtk-win32.rc: gtk-win32.rc.body
	cat $< >>$@
	echo "ISOLATIONAWARE_MANIFEST_RESOURCE_ID RT_MANIFEST libgtk3.manifest" >>$@

gtk.def: libgtk-3.la
	echo "LIBRARY libgtk-$(GTK_MAJOR_VERSION)-@LT_CURRENT_MINUS_AGE@" >$@
	echo "EXPORTS" >>$@
	objdump -p .libs/libgtk-$(GTK_MAJOR_VERSION)-@LT_CURRENT_MINUS_AGE@.dll | sed -n -e 's/\t\[\s\+\([0-9]\+\)\]\s\([A-Za-z_][A-Za-z0-9_]\)/\2/p' >>$@

install-def-file: gtk.def
	$(INSTALL) gtk.def $(DESTDIR)$(libdir)/gtk-win32-3.0.def
uninstall-def-file:
	-rm $(DESTDIR)$(libdir)/gtk-win32-3.0.def
else
install-def-file:
uninstall-def-file:
endif

if MS_LIB_AVAILABLE
noinst_DATA = gtk-win32-$(GTK_API_VERSION).lib

gtk-win32-$(GTK_API_VERSION).lib: libgtk-3.la gtk.def
	lib -machine:@LIB_EXE_MACHINE_FLAG@ -name:libgtk-win32-$(GTK_API_VERSION)-@LT_CURRENT_MINUS_AGE@.dll -def:gtk.def -out:$@

install-ms-lib:
	$(INSTALL) gtk-win32-$(GTK_API_VERSION).lib $(DESTDIR)$(libdir)

uninstall-ms-lib:
	-rm $(DESTDIR)$(libdir)/gtk-win32-$(GTK_API_VERSION).lib
else
install-ms-lib:
uninstall-ms-lib:
endif

libadd = 				\
	$(top_builddir)/gdk/libgdk-3.la	\
	$(GMODULE_LIBS) 		\
	$(GTK_DEP_LIBS)
deps = 					\
	$(top_builddir)/gdk/libgdk-3.la

# libtool stuff: set version and export symbols for resolving
# since automake doesn't support conditionalized libsomething_la_LDFLAGS
# we use the general approach here
libtool_opts =							\
  $(GTK_LINK_FLAGS)						\
  -version-info $(LT_VERSION_INFO)				\
  -export-dynamic $(no_undefined)				\
  -rpath $(libdir)


included-modules:
if HAVE_INCLUDED_IMMMODULES
	@cd $(top_builddir)/modules/input && $(MAKE) $(AM_MAKEFLAGS) included-modules

libadd += $(INCLUDED_IMMODULE_OBJ)
deps += $(INCLUDED_IMMODULE_OBJ)

$(INCLUDED_IMMODULE_OBJ): included-modules
	@true
endif

.PHONY: included-modules

#
# setup source file variables
#

include $(srcdir)/a11y/Makefile.inc
include $(srcdir)/deprecated/Makefile.inc
include $(srcdir)/inspector/Makefile.inc
include $(srcdir)/gtk-sources.inc

# GTK+ header files for public installation (non-generated, or generated
# by configure)

gtk_public_h_sources = $(GTK_PUB_HDRS)

if OS_UNIX
gtk_unix_print_public_h_sources = $(GTK_UNIX_PRINT_PUB_HDRS)
endif

gtk_private_type_h_sources = $(GTK_PRIVATE_TYPE_HDRS)

# GTK+ header files that don't get installed
gtk_private_h_sources =		\
	$(inspector_h_sources)	\
	$(gtk_private_type_h_sources) \
	$(gtk_base_private_h_sources)

# GTK+ C sources to build the library from
gtk_base_c_sources = 		\
	$(a11y_c_sources)	\
	$(gtk_deprecated_c_sources)	\
	$(inspector_c_sources)	\
	$(gtk_base_c_sources_base)

if USE_QUARTZ
gtk_base_c_sources += $(gtk_clipboard_dnd_c_sources_quartz)
else
gtk_base_c_sources += $(gtk_clipboard_dnd_c_sources_generic)
endif

nodist_gtk_c_sources =
gtk_c_sources = $(gtk_base_c_sources)

$(gtk_dbus_built_sources) : Makefile.am gtkdbusinterfaces.xml
	$(AM_V_GEN) gdbus-codegen \
	--interface-prefix org.Gtk. \
	--c-namespace _Gtk \
	--generate-c-code gtkdbusgenerated \
	$(srcdir)/gtkdbusinterfaces.xml

nodist_gtk_c_sources += $(gtk_dbus_built_sources)

gtk_os_unix_c_sources = $(gtk_os_unix_c_sources_base)

if OS_UNIX
gtk_private_h_sources += $(gtk_os_unix_private_h_sources)

if HAVE_TRACKER3
gtk_private_h_sources += $(gtk_os_unix_tracker3_private_h_sources)
gtk_os_unix_c_sources += $(gtk_os_unix_tracker3_c_sources)
endif

gtk_c_sources += $(gtk_os_unix_c_sources)
endif

if OS_WIN32
gtk_private_h_sources += $(gtk_os_win32_private_h_sources)
gtk_c_sources += $(gtk_os_win32_c_sources)
endif

if USE_X11
gtk_c_sources += $(gtk_use_x11_c_sources)
gtk_private_h_sources += $(gtk_use_x11_private_h_sources)
endif

if USE_WAYLAND
gtk_c_sources += $(gtk_use_wayland_c_sources)
endif

# pretty tricky way to write USE_WAYLAND || USE_X11...
if USE_WAYLAND
gtk_c_sources += $(gtk_use_wayland_or_x11_c_sources)
else
if USE_X11
gtk_c_sources += $(gtk_use_wayland_or_x11_c_sources)
else
endif
endif

if USE_WIN32
gtk_c_sources += $(gtk_use_win32_c_sources)
gtk_private_h_sources += $(gtk_use_win32_private_h_sources)
endif

if USE_QUARTZ
gtk_c_sources += $(gtk_use_quartz_c_sources)
gtk_private_h_sources += $(gtk_use_quartz_private_h_sources)
endif

gtk_all_private_h_sources = \
	$(a11y_private_h_sources) \
	$(gtk_deprecated_private_h_sources) \
	$(gtk_private_h_sources) \
	$(gtk_use_x11_private_h_sources) \
	$(gtk_use_win32_private_h_sources) \
	$(gtk_use_quartz_private_h_sources)

if !USE_X11
if !USE_WIN32
if !USE_QUARTZ
gtk_c_sources += $(gtk_use_stub_c_sources)
endif
endif
endif

# we use our own built_sources variable rules to avoid automake's
# BUILT_SOURCES oddities
# we generate frequently rebuild files piggyback on a stamp file, so sources
# depending on them only get rebuild when the built source actually changed
# content

gtk_built_sources =				\
	$(gtk_built_private_headers)		\
	$(gtk_built_public_sources)		\
	$(gtk_other_built_sources)

nodist_gtk_c_sources += $(gtk_built_sources)

stamp_files =					\
	stamp-gtkmarshalers.h			\
	stamp-gtktypebuiltins.h			\
	stamp-gtkprivatetypebuiltins.h

#
# setup GTK+ sources and their dependencies
#
MAINTAINERCLEANFILES = \
	$(gtk_built_sources) 			\
	$(gtk_dbus_built_sources)		\
	$(print_portal_built_sources)		\
	$(stamp_files)

DISTCLEANFILES = gtktypefuncs.inc

if OS_WIN32
DISTCLEANFILES += gtk-win32.rc
endif

EXTRA_DIST += $(gtk_all_private_h_sources) $(gtk_extra_sources)
EXTRA_DIST += $(gtk_built_sources)


pkgdatadir = $(datadir)/gtk-$(GTK_API_VERSION)
pkgdata_DATA = gtkbuilder.rng

EXTRA_DIST += gtkbuilder.rnc gtkbuilder.rng

itsdir = $(datadir)/gettext/its
its_DATA = gtkbuilder.loc gtkbuilder.its

EXTRA_DIST += $(its_DATA)

templates =				\
	$(inspector_templates)		\
	$(gtk_base_ui_templates)

#
# rules to generate built sources
#
# setup autogeneration dependencies
gen_sources = xgen-gdef xgen-gtbh xgen-gtic xgen-gmh xgen-gmc xgen-gmlh xgen-gmlc xgen-gtfsrc.c xgen-gtf
CLEANFILES = $(gen_sources)

BUILT_SOURCES = \
	$(gtk_built_sources) \
	$(gtk_dbus_built_sources) \
	$(print_portal_built_sources)

# all autogenerated files need to be generated in the srcdir,
# so old versions get remade and are not confused with newer
# versions in the build dir. thus a development setup requires
# srcdir to be writable, passing --disable-rebuilds to
# ../configure will supress all autogeneration rules.
gtkmarshalers.h: stamp-gtkmarshalers.h
	@true
stamp-gtkmarshalers.h: gtkmarshalers.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) \
		--prefix=_gtk_marshal \
		--header \
		--valist-marshallers \
		$(srcdir)/gtkmarshalers.list >> xgen-gmlh \
	&& (cmp -s xgen-gmlh gtkmarshalers.h || cp xgen-gmlh gtkmarshalers.h) \
	&& rm -f xgen-gmlh \
	&& echo timestamp > $(@F)
gtkmarshalers.c: gtkmarshalers.list
	$(AM_V_GEN) (echo "#undef G_ENABLE_DEBUG"; \
	 $(GLIB_GENMARSHAL) \
	 	--prefix=_gtk_marshal \
		--body \
		--valist-marshallers $(srcdir)/gtkmarshalers.list) >> xgen-gmlc \
	&& cp xgen-gmlc gtkmarshalers.c \
	&& rm -f xgen-gmlc

gtktypebuiltins.h: stamp-gtktypebuiltins.h
	@true
stamp-gtktypebuiltins.h: $(gtk_public_h_sources) $(a11y_h_sources) $(gtk_deprecated_h_sources) gtktypebuiltins.h.template
	$(AM_V_GEN) ( cd $(srcdir) && $(GLIB_MKENUMS) --template gtktypebuiltins.h.template \
		$(gtk_public_h_sources) $(a11y_h_sources) $(gtk_deprecated_h_sources) ) > xgen-gtbh \
	&& (cmp -s xgen-gtbh gtktypebuiltins.h || cp xgen-gtbh gtktypebuiltins.h ) \
	&& rm -f xgen-gtbh \
	&& echo timestamp > $(@F)
gtktypebuiltins.c: $(gtk_public_h_sources) $(a11y_h_sources) $(gtk_deprecated_h_sources) gtktypebuiltins.c.template
	$(AM_V_GEN) ( cd $(srcdir) && $(GLIB_MKENUMS) --template gtktypebuiltins.c.template \
		$(gtk_public_h_sources) $(a11y_h_sources) $(gtk_deprecated_h_sources) ) > xgen-gtbc \
	&& cp xgen-gtbc gtktypebuiltins.c  \
	&& rm -f xgen-gtbc

gtk.gresource.xml: Makefile.am inspector/Makefile.inc
	$(AM_V_GEN) echo "<?xml version='1.0' encoding='UTF-8'?>" > $@; \
	echo "<gresources>" >> $@; \
	echo "  <gresource prefix='/org/gtk/libgtk'>" >> $@; \
        echo "    <file>theme/Adwaita/gtk.css</file>" >> $@; \
        echo "    <file>theme/Adwaita/gtk-dark.css</file>" >> $@; \
        echo "    <file>theme/Adwaita/gtk-contained.css</file>" >> $@; \
        echo "    <file>theme/Adwaita/gtk-contained-dark.css</file>" >> $@; \
        for f in $(srcdir)/theme/Adwaita/assets/*.png; do \
          n=`basename $$f`; \
          echo "    <file preprocess='to-pixdata'>theme/Adwaita/assets/$$n</file>" >> $@; \
        done; \
        for f in $(srcdir)/theme/Adwaita/assets/*.svg; do \
          n=`basename $$f`; \
          echo "    <file>theme/Adwaita/assets/$$n</file>" >> $@; \
        done; \
	echo "    <file>theme/HighContrast/gtk.css</file>" >> $@; \
	echo "    <file alias='theme/HighContrastInverse/gtk.css'>theme/HighContrast/gtk-inverse.css</file>" >> $@; \
	echo "    <file>theme/HighContrast/gtk-contained.css</file>" >> $@; \
	echo "    <file>theme/HighContrast/gtk-contained-inverse.css</file>" >> $@; \
	      for f in $(srcdir)/theme/HighContrast/assets/*.png; do \
          n=`basename $$f`; \
          echo "    <file preprocess='to-pixdata'>theme/HighContrast/assets/$$n</file>" >> $@; \
        done; \
        for f in $(srcdir)/theme/HighContrast/assets/*.svg; do \
          n=`basename $$f`; \
          echo "    <file>theme/HighContrast/assets/$$n</file>" >> $@; \
        done; \
	echo "    <file>theme/win32/gtk-win32-base.css</file>" >> $@; \
	echo "    <file>theme/win32/gtk.css</file>" >> $@; \
	for f in $(srcdir)/cursor/*.png; do \
	  n=`basename $$f`; \
	  echo "    <file>cursor/$$n</file>" >> $@; \
	done; \
	for f in $(srcdir)/gesture/*.symbolic.png; do \
	  n=`basename $$f`; \
	  echo "    <file alias='icons/64x64/actions/$$n'>gesture/$$n</file>" >> $@; \
	done; \
	for f in $(srcdir)/ui/*.ui; do \
	  n=`basename $$f`; \
	  echo "    <file preprocess='xml-stripblanks'>ui/$$n</file>" >> $@; \
	done; \
	for s in 16x16 22x22 24x24 32x32 48x48; do \
	  for c in actions status categories; do \
	    for f in $(srcdir)/icons/$$s/$$c/*.png; do \
	      test "$$f" = "$(srcdir)/icons/$$s/$$c/*.png" && continue; \
	      n=`basename $$f`; \
	      echo "    <file>icons/$$s/$$c/$$n</file>" >> $@; \
	    done; \
	  done; \
	done; \
	for s in scalable; do \
	  for c in status; do \
	    for f in $(srcdir)/icons/$$s/$$c/*.svg; do \
	      test "$$f" = "$(srcdir)/icons/$$s/$$c/*.svg" && continue; \
	      n=`basename $$f`; \
	      echo "    <file>icons/$$s/$$c/$$n</file>" >> $@; \
	    done; \
	  done; \
	done; \
	for f in $(srcdir)/inspector/*.ui; do \
	  n=`basename $$f`; \
	  echo "    <file compressed='true' preprocess='xml-stripblanks'>inspector/$$n</file>" >> $@; \
	done; \
	echo "    <file>inspector/logo.png</file>" >> $@; \
	for f in $(srcdir)/emoji/*.data; do \
	  n=`basename $$f`; \
	  echo "    <file>emoji/$$n</file>" >> $@; \
	done; \
	echo "  </gresource>" >> $@; \
	echo "</gresources>" >> $@;

adwaita_theme_scss = \
	$(adwaita_theme_scss_sources)	\
	$()

highcontrast_theme_scss = \
	$(highcontrast_theme_scss_sources)	\
	$()

if REBUILD_SCSS
# If we have sassc then we can rebuild the theme CSS as soon as the SCSS
# files have been changed

scss_verbose = $(scss_verbose_@AM_V@)
scss_verbose_ = $(scss_verbose_@AM_DEFAULT_V@)
scss_verbose_0 = @echo "  SCSS     $@";

SASSC_OPTS = -a -M -t compact

theme/Adwaita/gtk-contained.css: $(top_srcdir)/gtk/theme/Adwaita/gtk-contained.scss $(adwaita_theme_scss)
	$(scss_verbose) $(MKDIR_P) $(dir $@) && $(SASSC) $(SASSC_OPTS) $< $@ || rm -f $@

theme/Adwaita/gtk-contained-dark.css: $(top_srcdir)/gtk/theme/Adwaita/gtk-contained-dark.scss $(adwaita_theme_scss)
	$(scss_verbose) $(MKDIR_P) $(dir $@) && $(SASSC) $(SASSC_OPTS) $< $@ || rm -f $@

theme/HighContrast/gtk-contained.css: $(top_srcdir)/gtk/theme/HighContrast/gtk-contained.scss $(highcontrast_theme_scss)
	$(scss_verbose) $(MKDIR_P) $(dir $@) && $(SASSC) $(SASSC_OPTS) $< $@ || rm -f $@

theme/HighContrast/gtk-contained-inverse.css: $(top_srcdir)/gtk/theme/HighContrast/gtk-contained-inverse.scss $(highcontrast_theme_scss)
	$(scss_verbose) $(MKDIR_P) $(dir $@) && $(SASSC) $(SASSC_OPTS) $< $@ || rm -f $@

endif # REBUILD_SCSS

theme_sources = 				\
	$(win32_theme_css_sources)	\
	$(adwaita_theme_scss)			\
	$(adwaita_theme_files) 		\
	$(highcontrast_theme_scss)		\
	$(highcontrast_theme_css_sources)

resource_files = $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(builddir)/gtk.gresource.xml)

gtkresources.h: gtk.gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $< \
		--target=$@ --sourcedir=$(srcdir) --c-name _gtk --generate-header --manual-register --internal
gtkresources.c: gtk.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $< \
		--target=$@ --sourcedir=$(srcdir) --c-name _gtk --generate-source --manual-register --internal

gtkprivatetypebuiltins.h: stamp-gtkprivatetypebuiltins.h
	@true
stamp-gtkprivatetypebuiltins.h: $(gtk_private_type_h_sources) gtkprivatetypebuiltins.h.template
	$(AM_V_GEN) ( cd $(srcdir) && $(GLIB_MKENUMS) --template gtkprivatetypebuiltins.h.template \
		$(gtk_private_type_h_sources) ) > xgen-gptbh \
	&& (cmp -s xgen-gptbh gtkprivatetypebuiltins.h || cp xgen-gptbh gtkprivatetypebuiltins.h ) \
	&& rm -f xgen-gptbh \
	&& echo timestamp > $(@F)
gtkprivatetypebuiltins.c: $(gtk_private_type_h_sources)  gtkprivatetypebuiltins.c.template
	$(AM_V_GEN) ( cd $(srcdir) && $(GLIB_MKENUMS) --template gtkprivatetypebuiltins.c.template \
		$(gtk_private_type_h_sources) ) > xgen-gptbc \
	&& cp xgen-gptbc gtkprivatetypebuiltins.c  \
	&& rm -f xgen-gptbc


gtktypefuncs.inc: stamp-gtktypebuiltins.h stamp-gtkprivatetypebuiltins.h $(top_srcdir)/gtk/*.h $(top_srcdir)/gtk/a11y/*.h $(top_srcdir)/gtk/deprecated/*.h $(top_srcdir)/gdk/*.h Makefile
	$(AM_V_GEN) export LC_ALL=C ; \
	  (echo '#undef GTK_COMPILATION' && echo '#include <gtk/gtkx.h>') > xgen-gtfsrc.c && \
	  echo 'G_GNUC_BEGIN_IGNORE_DEPRECATIONS' > xgen-gtf && \
	  ${CPP} $(DEFS) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) xgen-gtfsrc.c | \
	  $(GREP) -o '\bg[td]k_[a-zA-Z0-9_]*_get_type\b' | \
	  sort | uniq | \
	  $(SED) '{ s/^/*tp++ = /; s/$$/();/; s/^.*\(gdk_x11\|gtk_plug_\|gtk_socket_\).*$$/#ifdef GDK_WINDOWING_X11\n&\n#endif/; }' >> xgen-gtf \
	&& cp xgen-gtf $@ && rm -f xgen-gtf
$(srcdir)/gtktestutils.c: gtktypefuncs.inc

# target platform:
lib_LTLIBRARIES = libgtk-3.la

gtkincludedir = $(includedir)/gtk-3.0/gtk
gtkinclude_HEADERS = $(gtk_public_h_sources) $(gtk_semi_private_h_sources) $(gtk_built_public_sources) gtkversion.h

a11yincludedir = $(includedir)/gtk-3.0/gtk/a11y
a11yinclude_HEADERS= $(a11y_h_sources)

deprecatedincludedir = $(includedir)/gtk-3.0/gtk/deprecated
deprecatedinclude_HEADERS= $(gtk_deprecated_h_sources)

gtkunixprintincludedir = $(includedir)/gtk-3.0/unix-print/gtk
gtkunixprintinclude_HEADERS = $(gtk_unix_print_public_h_sources)

nodist_libgtk_3_la_SOURCES = $(nodist_gtk_c_sources)
libgtk_3_la_SOURCES = $(gtk_c_sources)
libgtk_3_la_CFLAGS = $(AM_CFLAGS) $(GDK_HIDDEN_VISIBILITY_CFLAGS)
libgtk_3_la_LDFLAGS = $(libtool_opts)
libgtk_3_la_LIBADD = $(libadd)
libgtk_3_la_DEPENDENCIES = $(deps)

if USE_QUARTZ
libgtk_3_la_CFLAGS += "-xobjective-c"
endif

if USE_WIN32
libgtk_3_la_LIBADD += -lole32 -lgdi32 -lcomdlg32 -lwinspool -lcomctl32
libgtk_3_la_LDFLAGS += -Wl,-luuid $(gtk_win32_res_ldflag)
libgtk_3_la_DEPENDENCIES += $(gtk_win32_res)
endif

install-exec-hook:
if DISABLE_EXPLICIT_DEPS
	$(SHELL) $(top_srcdir)/sanitize-la.sh $(DESTDIR)$(libdir)/libgtk-3.la
endif

if USE_QUARTZ
install-mac-key-theme:
	$(MKDIR_P) $(DESTDIR)$(datadir)/themes/Mac/gtk-3.0
	$(INSTALL_DATA) $(srcdir)/gtk-keys.css.mac $(DESTDIR)$(datadir)/themes/Mac/gtk-3.0/gtk-keys.css
uninstall-mac-key-theme:
	rm -f $(DESTDIR)$(datadir)/themes/Mac/gtk-3.0/gtk-keys.css
else
install-mac-key-theme:
uninstall-mac-key-theme:
endif

# ------------------- MSVC Build Items ----------------
MSVCPROJS = gtk-3

gtk_3_FILES = \
	$(gtk_base_c_sources)		\
	$(gtk_os_win32_c_sources)	\
	$(gtk_use_win32_c_sources)

gtk_3_EXCLUDES = dummy

include $(top_srcdir)/win32/Makefile.msvcproj

dist-hook: \
	$(top_builddir)/win32/vs9/gtk-3.vcproj

# Install a RC file for the default GTK+ theme, and key themes
install-data-local: install-ms-lib install-def-file install-mac-key-theme
	$(MKDIR_P) $(DESTDIR)$(datadir)/themes/Default/gtk-3.0
	$(INSTALL_DATA) $(srcdir)/gtk-keys.css.default $(DESTDIR)$(datadir)/themes/Default/gtk-3.0/gtk-keys.css
	$(MKDIR_P) $(DESTDIR)$(datadir)/themes/Emacs/gtk-3.0
	$(INSTALL_DATA) $(srcdir)/gtk-keys.css.emacs $(DESTDIR)$(datadir)/themes/Emacs/gtk-3.0/gtk-keys.css

uninstall-local: uninstall-ms-lib uninstall-def-file uninstall-mac-key-theme
	rm -f $(DESTDIR)$(datadir)/themes/Default/gtk-3.0/gtk-keys.css
	rm -f $(DESTDIR)$(datadir)/themes/Emacs/gtk-3.0/gtk-keys.css

# if srcdir!=builddir, clean out maintainer-clean files from builddir
# this allows dist to pass.
distclean-local:
	if test $(srcdir) != .; then \
	  rm -f $(MAINTAINERCLEANFILES); \
	fi

if HAVE_INTROSPECTION
introspected_pub_headers = $(filter-out %private.h gtktextdisplay.h gtktextlayout.h gtkx.h, $(gtkinclude_HEADERS) $(a11yinclude_HEADERS) $(deprecatedinclude_HEADERS))

introspection_files = \
    $(introspected_pub_headers) \
    $(filter-out %win32.c, $(gtk_base_c_sources)) \
    gtkprintoperation-unix.c \
    gtktypebuiltins.h \
    gtktypebuiltins.c

if USE_X11
introspection_files += \
    gtksocket.c \
    gtkplug.c
endif

include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_SCANNER_ENV = CC="$(CC)"
INTROSPECTION_COMPILER_ARGS = \
	--includedir=$(srcdir) \
	--includedir=. \
	--includedir=../gdk

Gtk-3.0.gir: $(INTROSPECTION_SCANNER) libgtk-3.la $(top_builddir)/gdk/Gdk-3.0.gir Makefile
Gtk_3_0_gir_SCANNERFLAGS = 			\
	--warn-all				\
	--add-include-path=$(top_builddir)/gdk	\
	--include-uninstalled=$(top_builddir)/gdk/Gdk-3.0.gir
Gtk_3_0_gir_INCLUDES = Atk-1.0
Gtk_3_0_gir_C_INCLUDES = gtk/gtk.h gtk/gtk-a11y.h
if USE_X11
Gtk_3_0_gir_SCANNERFLAGS += --add-include-path=$(top_builddir)/gdk/x11
Gtk_3_0_gir_INCLUDES += xlib-2.0
Gtk_3_0_gir_C_INCLUDES += gtk/gtkx.h
endif
Gtk_3_0_gir_CFLAGS = \
		$(AM_CPPFLAGS) \
		-DGTK_TEXT_USE_INTERNAL_UNSUPPORTED_API
Gtk_3_0_gir_FILES = $(introspection_files)
Gtk_3_0_gir_LIBS = libgtk-3.la $(top_builddir)/gdk/libgdk-3.la
Gtk_3_0_gir_EXPORT_PACKAGES = gtk+-3.0
INTROSPECTION_GIRS = Gtk-3.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibsdir = $(libdir)/girepository-1.0
typelibs_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelibs_DATA)
endif

#
# Installed tools
#
bin_PROGRAMS = \
	gtk-query-immodules-3.0	\
	gtk-update-icon-cache \
	gtk-encode-symbolic-svg \
	gtk-builder-tool \
	gtk-query-settings \
	gtk-launch

gtk_query_immodules_3_0_SOURCES = queryimmodules.c gtkutils.c
gtk_query_immodules_3_0_LDADD = 		\
	libgtk-3.la				\
	$(top_builddir)/gdk/libgdk-3.la		\
	$(GMODULE_LIBS)				\
	$(GTK_DEP_LIBS)

gtk_update_icon_cache_SOURCES = updateiconcache.c
gtk_update_icon_cache_LDADD = $(GDK_PIXBUF_LIBS)

gtk_encode_symbolic_svg_SOURCES = encodesymbolic.c
gtk_encode_symbolic_svg_LDADD =			\
	$(GDK_PIXBUF_LIBS)			\
	$(top_builddir)/gdk/libgdk-3.la		\
	$(GTK_DEP_LIBS)

gtk_builder_tool_SOURCES = gtk-builder-tool.c
gtk_builder_tool_LDADD =			\
	libgtk-3.la				\
	$(top_builddir)/gdk/libgdk-3.la		\
	$(GTK_DEP_LIBS)

gtk_query_settings_SOURCES = gtk-query-settings.c
gtk_query_settings_LDADD= 			\
	libgtk-3.la				\
	$(top_builddir)/gdk/libgdk-3.la		\
	$(GTK_DEP_LIBS)

gtk_launch_SOURCES = gtk-launch.c
gtk_launch_LDADD =				\
	libgtk-3.la				\
	$(top_builddir)/gdk/libgdk-3.la		\
	$(GTK_DEP_LIBS)

if OS_WIN32

# Workaround for UAC silliness: programs with "update" in their name
# are believed to be installers and require elevated privileges to be
# used... Embed a manifest file into executable to tell Windows that
# gtk-update-icon-cache.exe doesn't require any special privileges.

GTK_UPDATE_ICON_CACHE_MANIFEST = gtk-update-icon-cache.exe.manifest
GTK_UPDATE_ICON_CACHE_RC = gtk-update-icon-cache.rc
GTK_UPDATE_ICON_CACHE_MANIFEST_OBJECT = gtk-update-icon-cache_manifest.o

$(GTK_UPDATE_ICON_CACHE_MANIFEST): Makefile generate-uac-manifest.py
	$(PYTHON) $(srcdir)/generate-uac-manifest.py -p=gtk3 -n=gtk-update-icon-cache --pkg-version=$(GTK_VERSION) --output-dir=$(builddir)

$(GTK_UPDATE_ICON_CACHE_RC): $(GTK_UPDATE_ICON_CACHE_MANIFEST)

$(GTK_UPDATE_ICON_CACHE_MANIFEST_OBJECT): $(GTK_UPDATE_ICON_CACHE_RC) $(GTK_UPDATE_ICON_CACHE_MANIFEST)
	$(WINDRES) --input $< --output $@ --output-format=coff

gtk_update_icon_cache_LDADD += $(GTK_UPDATE_ICON_CACHE_MANIFEST_OBJECT)

endif

.PHONY: files

files:
	@files=`ls $(DISTFILES) 2> /dev/null `; for p in $$files; do \
	  echo $$p; \
	done

gsettings_SCHEMAS = \
	org.gtk.Settings.FileChooser.gschema.xml \
	org.gtk.Settings.ColorChooser.gschema.xml \
	org.gtk.Settings.EmojiChooser.gschema.xml \
	org.gtk.Settings.Debug.gschema.xml

@GSETTINGS_RULES@

EXTRA_DIST +=                   \
	$(resource_files)	\
	$(gesture_sources)	\
	$(theme_sources)	\
	$(gsettings_SCHEMAS)	\
	compose/compose-parse.py	\
	emoji/convert-emoji.c	\
	emoji/gresource.xml.in	\
	emoji/README.md	\
	emoji/de.data	\
	emoji/en.data	\
	emoji/es.data	\
	emoji/fr.data	\
	emoji/zh.data	\
	a11y/Makefile.inc	\
	deprecated/Makefile.inc	\
	glade/README.glade	\
	glade/gtk-private-widgets.xml \
	compose/gtk-compose-lookaside.txt \
	inspector/Makefile.inc	\
	libgtk3.manifest.in	\
	gtk-win32.rc.body.in	\
	gtk-win32.rc.body	\
	gtkwin32embed.h		\
	gtkwin32embedwidget.h	\
	gtkwin32embedwidget.c	\
	gtkprint-win32.h	\
	gtkprint-win32.c	\
	gtksearchenginetracker3.c \
	gtksearchenginetracker3.h \
	gtksearchenginequartz.h \
	gtkdbusinterfaces.xml	\
	gtk-keys.css.default	\
	gtk-keys.css.emacs	\
	gtk-keys.css.mac	\
	makefile.msc		\
	makefile.msc.in		\
	gtktypebuiltins.c.template \
	gtktypebuiltins.h.template \
	gtkprivatetypebuiltins.c.template \
	gtkprivatetypebuiltins.h.template \
	meson.build \
	gen-gtk-gresources-xml.py \
	gen-rc.py \
	generate-uac-manifest.py \
	gentypefuncs.py \
	a11y/meson.build \
	deprecated/meson.build \
	inspector/meson.build \
	theme/Adwaita/meson.build \
	theme/Adwaita/parse-sass.sh \
	theme/Adwaita/render-assets.sh \
	theme/HighContrast/assets.svg \
	theme/HighContrast/assets.txt \
	theme/HighContrast/meson.build \
	theme/HighContrast/parse-sass.sh \
	theme/HighContrast/render-assets.sh


-include $(top_srcdir)/git.mk
