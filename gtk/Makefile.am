# Makefile.am for gtk+/gtk

SUBDIRS=stock-icons theme-bits

if OS_UNIX
SUBDIRS += xdgmime
endif

DIST_SUBDIRS=stock-icons theme-bits xdgmime

INCLUDES =						\
	-DG_LOG_DOMAIN=\"Gtk\"				\
	-DGTK_LIBDIR=\"$(libdir)\"			\
	-DGTK_DATADIR=\"$(datadir)\"			\
	-DGTK_DATA_PREFIX=\"$(prefix)\"			\
	-DGTK_SYSCONFDIR=\"$(sysconfdir)\"		\
	-DGTK_VERSION=\"$(GTK_VERSION)\"		\
	-DGTK_BINARY_VERSION=\"$(GTK_BINARY_VERSION)\"	\
	-DGTK_HOST=\"$(host)\"				\
	-DGTK_COMPILATION				\
	-I$(top_builddir)/gtk				\
	-I$(top_srcdir) -I../gdk			\
	-I$(top_srcdir)/gdk				\
	-I$(top_srcdir)/gdk-pixbuf -I../gdk-pixbuf	\
	-DG_DISABLE_DEPRECATED				\
	-DGDK_PIXBUF_DISABLE_DEPRECATED			\
	-DGDK_DISABLE_DEPRECATED			\
	-DGTK_DISABLE_DEPRECATED			\
	-DGTK_FILE_SYSTEM_ENABLE_UNSUPPORTED		\
	$(GTK_DEBUG_FLAGS)				\
	$(GTK_DEP_CFLAGS)

gtarget=$(gdktarget)

if PLATFORM_WIN32
no_undefined = -no-undefined
endif

if OS_WIN32
gtk_def = gtk.def
gtk_win32_symbols = -export-symbols $(gtk_def)

gtk_win32res_lo = gtk-win32res.lo

gtk-win32res.lo : gtk-win32.rc
	$(top_srcdir)/build/win32/lt-compile-resource gtk-win32.rc gtk-win32res.lo

install-libtool-import-lib:
	for entry in `grep PRIVATE gtk.def | sed -e 's/PRIVATE//'`; do \
	  file=`nm -A .libs/libgtk-win32-$(GTK_API_VERSION).dll.a | tr -d '\r' | grep -m 1 -E $$entry'$$' | cut -d: -f2`; \
	  ar d .libs/libgtk-win32-$(GTK_API_VERSION).dll.a $$file; \
	done
	$(INSTALL) .libs/libgtk-win32-$(GTK_API_VERSION).dll.a $(DESTDIR)$(libdir)
uninstall-libtool-import-lib:
	-rm $(DESTDIR)$(libdir)/libtk-win32-$(GTK_API_VERSION).dll.a
else
install-libtool-import-lib:
uninstall-libtool-import-lib:
endif

if MS_LIB_AVAILABLE
noinst_DATA = gtk-win32-$(GTK_API_VERSION).lib

gtk-win32-$(GTK_API_VERSION).lib: libgtk-win32-$(GTK_API_VERSION).la gtk.def
	lib -name:libgtk-win32-$(GTK_API_VERSION)-@LT_CURRENT_MINUS_AGE@.dll -def:gtk.def -out:$@

install-ms-lib:
	$(INSTALL) gtk-win32-$(GTK_API_VERSION).lib $(DESTDIR)$(libdir)

uninstall-ms-lib:
	-rm $(DESTDIR)$(libdir)/gtk-win32-$(GTK_API_VERSION).lib
else
install-ms-lib:
uninstall-ms-lib:
endif

gtk.def: gtk.symbols
	(echo -e EXPORTS; $(CPP) -P -DINCLUDE_VARIABLES -DG_OS_WIN32 - <$(srcdir)/gtk.symbols | sed -e '/^$$/d' -e 's/^/	/' -e 's/G_GNUC_[^ ]*//g') > gtk.def

gtkalias.h: gtk.symbols
	  $(PERL) $(srcdir)/makegtkalias.pl < $(srcdir)/gtk.symbols > gtkalias.h

if OS_LINUX
TESTS = abicheck.sh
endif

# libtool stuff: set version and export symbols for resolving
# since automake doesn't support conditionalized libsomething_la_LDFLAGS
# we use the general approach here
libgtkincludedir = $(includedir)/gtk-2.0/gtk
libadd =								\
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la	\
	$(top_builddir)/gdk/$(gdktargetlib)				\
	$(GTK_DEP_LIBS)

if OS_UNIX
libadd += xdgmime/libxdgmime.la
endif

# common options for the various packages.
libtool_opts =							\
  -version-info $(LT_VERSION_INFO)				\
  -export-dynamic $(no_undefined) $(LIBTOOL_EXPORT_OPTIONS)	\
  -rpath $(libdir) $(libgtk_target_ldflags)


#
# setup source file variables
#

# GTK+ header files for public installation (non-generated, or generated
# by configure)
gtk_public_h_sources =          \
	gtk.h			\
	gtkaboutdialog.h	\
	gtkaccelgroup.h		\
	gtkaccellabel.h		\
	gtkaccelmap.h		\
	gtkaccessible.h		\
	gtkaction.h		\
	gtkactiongroup.h	\
	gtkadjustment.h		\
	gtkalignment.h		\
	gtkarrow.h		\
	gtkaspectframe.h	\
	gtkbbox.h		\
	gtkbin.h		\
	gtkbindings.h		\
	gtkbox.h		\
	gtkbutton.h		\
	gtkcalendar.h		\
	gtkcelleditable.h	\
	gtkcelllayout.h		\
	gtkcellrenderer.h	\
	gtkcellrenderercombo.h	\
	gtkcellrendererpixbuf.h	\
	gtkcellrendererprogress.h \
	gtkcellrenderertext.h	\
	gtkcellrenderertoggle.h	\
	gtkcellview.h		\
	gtkcheckbutton.h	\
	gtkcheckmenuitem.h	\
	gtkclipboard.h		\
	gtkclist.h		\
	gtkclipboard.h		\
	gtkcolorbutton.h	\
	gtkcolorsel.h		\
	gtkcolorseldialog.h	\
	gtkcombo.h		\
	gtkcombobox.h		\
	gtkcomboboxentry.h	\
	gtkcontainer.h		\
	gtkctree.h		\
	gtkcurve.h		\
	gtkdebug.h              \
	gtkdialog.h		\
	gtkdnd.h		\
	gtkdrawingarea.h	\
	gtkeditable.h           \
	gtkentry.h		\
	gtkentrycompletion.h	\
	gtkenums.h		\
	gtkeventbox.h		\
	gtkexpander.h		\
	gtkfilechooser.h        \
	gtkfilechooserbutton.h  \
	gtkfilechooserdialog.h  \
	gtkfilechooserwidget.h  \
	gtkfilefilter.h		\
	gtkfilesel.h		\
	gtkfixed.h		\
	gtkfontbutton.h		\
	gtkfontsel.h		\
	gtkframe.h		\
	gtkgamma.h		\
	gtkgc.h			\
	gtkhandlebox.h		\
	gtkhbbox.h		\
	gtkhbox.h		\
	gtkhpaned.h		\
	gtkhruler.h		\
	gtkhscale.h		\
	gtkhscrollbar.h		\
	gtkhseparator.h		\
	gtkiconfactory.h	\
	gtkicontheme.h		\
	gtkiconview.h		\
	gtkimage.h		\
	gtkimagemenuitem.h	\
	gtkimcontext.h		\
	gtkimcontextsimple.h	\
	gtkimmodule.h		\
	gtkimmulticontext.h	\
	gtkinputdialog.h	\
	gtkinvisible.h		\
	gtkitem.h		\
	gtkitemfactory.h	\
	gtklabel.h		\
	gtklayout.h             \
	gtklist.h		\
	gtklistitem.h		\
	gtkliststore.h		\
	gtkmain.h		\
	gtkmenu.h		\
	gtkmenubar.h		\
	gtkmenuitem.h		\
	gtkmenushell.h		\
	gtkmenutoolbutton.h	\
	gtkmessagedialog.h	\
	gtkmisc.h		\
	gtkmodules.h		\
	gtknotebook.h		\
	gtkobject.h		\
	gtkoldeditable.h	\
	gtkoptionmenu.h		\
	gtkpaned.h		\
	gtkpixmap.h		\
	gtkplug.h		\
	gtkpreview.h		\
	gtkprivate.h		\
	gtkprogress.h		\
	gtkprogressbar.h	\
	gtkradioaction.h	\
	gtkradiobutton.h	\
	gtkradiomenuitem.h	\
	gtkradiotoolbutton.h	\
	gtkrange.h		\
	gtkrc.h			\
	gtkruler.h		\
	gtkscale.h		\
	gtkscrollbar.h		\
	gtkscrolledwindow.h	\
	gtkselection.h		\
	gtkseparator.h		\
	gtkseparatormenuitem.h	\
	gtkseparatortoolitem.h	\
	gtksettings.h		\
	gtksignal.h		\
	gtksizegroup.h		\
	gtksocket.h		\
	gtkspinbutton.h		\
	gtkstatusbar.h		\
	gtkstock.h		\
	gtkstyle.h		\
	gtktable.h		\
	gtktearoffmenuitem.h    \
	gtktext.h		\
	gtktextbuffer.h		\
	gtktextchild.h		\
	gtktextdisplay.h	\
	gtktextiter.h		\
	gtktextmark.h		\
	gtktexttag.h		\
	gtktexttagtable.h	\
	gtktextview.h		\
	gtktipsquery.h		\
	gtktoggleaction.h	\
	gtktogglebutton.h	\
	gtktoggletoolbutton.h	\
	gtktoolbar.h		\
	gtktoolbutton.h		\
	gtktoolitem.h		\
	gtktooltips.h		\
	gtktree.h		\
	gtktreednd.h		\
	gtktreeitem.h		\
	gtktreemodel.h		\
	gtktreemodelfilter.h	\
	gtktreemodelsort.h	\
	gtktreeselection.h	\
	gtktreesortable.h	\
	gtktreestore.h		\
	gtktreeview.h		\
	gtktreeviewcolumn.h	\
	gtktypeutils.h		\
	gtkuimanager.h		\
	gtkvbbox.h		\
	gtkvbox.h		\
	gtkviewport.h		\
	gtkvpaned.h		\
	gtkvruler.h		\
	gtkvscale.h		\
	gtkvscrollbar.h		\
	gtkvseparator.h		\
	gtkwidget.h		\
	gtkwindow.h

# Installed header files without compatibility guarantees
# that are not include in gtk/gtk.h
gtk_semi_private_h_sources =    \
	gtktextlayout.h		\
	gtkfilesystem.h

# GTK+ header files that don't get installed
gtk_private_h_sources =		\
	gtkentryprivate.h	\
	gtkfilechooserembed.h	\
	gtkfilechooserentry.h	\
	gtkfilechooserdefault.h	\
	gtkfilechooserprivate.h	\
	gtkfilechooserutils.h	\
	gtkfilesystemunix.h	\
	gtkfilesystemmodel.h	\
	gtkiconcache.h		\
	gtkpathbar.h		\
	gtkrbtree.h		\
	gtksequence.h		\
	gtktextbtree.h		\
	gtktextchildprivate.h	\
	gtktextsegment.h	\
	gtktexttypes.h		\
	gtktextutil.h		\
	gtktextiterprivate.h	\
	gtktextmarkprivate.h	\
	gtktexttagprivate.h	\
	gtkthemes.h		\
	gtktreedatalist.h	\
	gtktreeprivate.h	\
	gtkwindow-decorate.h	\
	gtktoggleactionprivate.h

# GTK+ C sources to build the library from
gtk_c_sources =                 \
	fnmatch.c		\
	gtkaboutdialog.c	\
	gtkaccelgroup.c		\
	gtkaccellabel.c		\
	gtkaccelmap.c		\
	gtkaccessible.c		\
	gtkaction.c		\
	gtkactiongroup.c	\
	gtkadjustment.c		\
	gtkalignment.c		\
	gtkarrow.c		\
	gtkaspectframe.c	\
	gtkbbox.c		\
	gtkbin.c		\
	gtkbindings.c		\
	gtkbox.c		\
	gtkbutton.c		\
	gtkcalendar.c		\
	gtkcelleditable.c	\
	gtkcelllayout.c		\
	gtkcellrenderer.c	\
	gtkcellrenderercombo.c	\
	gtkcellrendererpixbuf.c	\
	gtkcellrendererprogress.c \
	gtkcellrenderertext.c	\
	gtkcellrenderertoggle.c	\
	gtkcellview.c		\
	gtkcheckbutton.c	\
	gtkcheckmenuitem.c	\
	gtkclipboard.c		\
	gtkclist.c		\
	gtkcolorbutton.c	\
	gtkcolorsel.c		\
	gtkcolorseldialog.c	\
	gtkcombo.c		\
	gtkcombobox.c		\
	gtkcomboboxentry.c	\
	gtkcontainer.c		\
	gtkctree.c		\
	gtkcurve.c		\
	gtkdialog.c		\
	gtkdnd.c		\
	gtkdrawingarea.c	\
	gtkeditable.c           \
	gtkentry.c		\
	gtkentrycompletion.c	\
	gtkeventbox.c		\
	gtkexpander.c		\
	gtkfilechooser.c	\
	gtkfilechooserbutton.c	\
	gtkfilechooserdialog.c	\
	gtkfilechooserembed.c	\
	gtkfilechooserentry.c	\
	gtkfilechooserdefault.c	\
	gtkfilechooserutils.c	\
	gtkfilechooserwidget.c	\
	gtkfilefilter.c		\
	gtkfilesel.c		\
	gtkfilesystem.c		\
	gtkfilesystemmodel.c	\
	gtkfixed.c		\
	gtkfontbutton.c         \
	gtkfontsel.c            \
	gtkframe.c		\
	gtkgamma.c		\
	gtkgc.c			\
	gtkhandlebox.c		\
	gtkhbbox.c		\
	gtkhbox.c		\
	gtkhpaned.c		\
	gtkhruler.c		\
	gtkhscale.c		\
	gtkhscrollbar.c		\
	gtkhseparator.c		\
	gtkhsv.c		\
	gtkhsv.h		\
	gtkiconfactory.c	\
	gtkiconcache.c		\
	gtkicontheme.c		\
	gtkiconview.c		\
	gtkimage.c		\
	gtkimagemenuitem.c	\
	gtkimcontext.c		\
	gtkimcontextsimple.c	\
	gtkimmodule.c		\
	gtkimmulticontext.c	\
	gtkinputdialog.c	\
	gtkintl.h		\
	gtkinvisible.c		\
	gtkitem.c		\
	gtkitemfactory.c	\
	gtkkeyhash.c		\
	gtkkeyhash.h		\
	gtklabel.c		\
	gtklayout.c		\
	gtklist.c		\
	gtklistitem.c		\
	gtkliststore.c		\
	gtkmain.c		\
	gtkmarshal.c		\
	gtkmarshalers.c		\
	gtkmenu.c		\
	gtkmenubar.c		\
	gtkmenuitem.c		\
	gtkmenushell.c		\
	gtkmenutoolbutton.c	\
	gtkmessagedialog.c	\
	gtkmisc.c		\
	gtkmnemonichash.c	\
	gtkmnemonichash.h	\
	gtkmodules.c		\
	gtknotebook.c		\
	gtkobject.c		\
	gtkoldeditable.c	\
	gtkoptionmenu.c		\
	gtkpaned.c		\
	gtkpathbar.c		\
	gtkpixmap.c		\
	gtkpreview.c		\
	gtkprogress.c		\
	gtkprogressbar.c	\
	gtkradioaction.c	\
	gtkradiobutton.c	\
	gtkradiomenuitem.c	\
	gtkradiotoolbutton.c	\
	gtkrange.c		\
	gtkrbtree.c 		\
	gtkrc.c			\
	gtkruler.c		\
	gtkscale.c		\
	gtkscrollbar.c		\
	gtkscrolledwindow.c	\
	gtksequence.c		\
	gtkselection.c		\
	gtkseparator.c		\
	gtkseparatormenuitem.c	\
	gtkseparatortoolitem.c	\
	gtksettings.c		\
	gtksignal.c		\
	gtksizegroup.c		\
	gtkspinbutton.c		\
	gtkstatusbar.c		\
	gtkstock.c		\
	gtkstyle.c		\
	gtktable.c		\
	gtktearoffmenuitem.c    \
	gtktext.c		\
	gtktextbtree.c		\
	gtktextbuffer.c		\
	gtktextchild.c		\
	gtktextdisplay.c	\
	gtktextiter.c		\
	gtktextiterprivate.h	\
	gtktextlayout.c		\
	gtktextmark.c		\
	gtktextmarkprivate.h	\
	gtktextsegment.c	\
	gtktexttag.c		\
	gtktexttagtable.c	\
	gtktexttypes.c		\
	gtktextutil.c		\
	gtktextview.c		\
	gtkthemes.c		\
	gtktipsquery.c		\
	gtktoggleaction.c	\
	gtktogglebutton.c	\
	gtktoggletoolbutton.c	\
	gtktoolbar.c		\
	gtktoolbutton.c		\
	gtktoolitem.c		\
	gtktooltips.c		\
	gtktree.c		\
	gtktreedatalist.c	\
	gtktreednd.c		\
	gtktreeitem.c		\
	gtktreemodel.c		\
	gtktreemodelfilter.c	\
	gtktreemodelsort.c	\
	gtktreeselection.c	\
	gtktreesortable.c	\
	gtktreestore.c		\
	gtktreeview.c		\
	gtktreeviewcolumn.c	\
	gtktypebuiltins.c	\
	gtktypeutils.c		\
	gtkuimanager.c		\
	gtkvbbox.c		\
	gtkvbox.c		\
	gtkviewport.c		\
	gtkvpaned.c		\
	gtkvruler.c		\
	gtkvscale.c		\
	gtkvscrollbar.c		\
	gtkvseparator.c		\
	gtkwidget.c		\
	gtkwindow-decorate.c    \
	gtkwindow.c		\
	xembed.h

if OS_UNIX
gtk_private_h_sources += gtkfilesystemunix.h
gtk_c_sources +=         gtkfilesystemunix.c
endif

if OS_WIN32
gtk_private_h_sources += gtkfilesystemwin32.h
gtk_c_sources +=         gtkfilesystemwin32.c
endif

gtk_plug_c_sources =            \
	gtkplug.c		\
	gtksocket.c		\
	gtkxembed.c		\
	gtkxembed.h

# we use our own built_sources variable rules to avoid automake's
# BUILT_SOURCES oddities
# we generate frequently rebuild files piggyback on a stamp file, so sources
# depending on them only get rebuild when the built source actually changed
# content

# built sources that get installed with the header files
gtk_built_public_sources =			\
	gtkmarshal.h				\
	gtktypebuiltins.h

# built sources that don't get installed
gtk_built_private_headers =			\
	gtkalias.h				\
	gtkmarshalers.h

gtk_built_sources =				\
	gtktypebuiltins.c			\
	gtkmarshalers.c				\
	gtkmarshalers.h				\
	gtkmarshal.c				\
	${gtk_built_private_headers}		\
	${gtk_built_public_sources}

stamp_files =					\
	stamp-gtkmarshalers.h			\
	stamp-gtkmarshal.h			\
	stamp-gtktypebuiltins.h

# non-header sources (headers should be specified in the above variables)
# that don't serve as direct make target sources, i.e. they don't have
# their own .lo rules and don't get publically installed
gtk_extra_sources =				\
	gtk.symbols				\
	gtkversion.h.in				\
	gtkmarshalers.list			\
	gtkmarshal.list

#
# setup GTK+ sources and their dependencies
#
MAINTAINERCLEANFILES = $(gtk_built_sources) $(stamp_files)
EXTRA_HEADERS =
EXTRA_DIST = $(gtk_private_h_sources) $(gtk_extra_sources)
EXTRA_DIST += $(gtk_built_sources)

#
# rules to generate built sources
#
# setup autogeneration dependencies
gen_sources = xgen-gdef xgen-gtbh xgen-gtic xgen-gmh xgen-gmc xgen-gmlh xgen-gmlc
CLEANFILES = $(gen_sources)

BUILT_SOURCES = $(gtk_built_sources)

# all autogenerated files need to be generated in the srcdir,
# so old versions get remade and are not confused with newer
# versions in the build dir. thus a development setup requires
# srcdir to be writable, passing --disable-rebuilds to
# ../configure will supress all autogeneration rules.
gtkmarshalers.h: stamp-gtkmarshalers.h
	@true
stamp-gtkmarshalers.h: @REBUILD@ gtkmarshalers.list
	$(GLIB_GENMARSHAL) --prefix=_gtk_marshal $(srcdir)/gtkmarshalers.list --header >> xgen-gmlh \
	&& (cmp -s xgen-gmlh gtkmarshalers.h || cp xgen-gmlh gtkmarshalers.h) \
	&& rm -f xgen-gmlh \
	&& echo timestamp > $(@F)	
gtkmarshalers.c: @REBUILD@ gtkmarshalers.list
	(echo "#include \"gtkalias.h\""; $(GLIB_GENMARSHAL) --prefix=_gtk_marshal $(srcdir)/gtkmarshalers.list --body) >> xgen-gmlc \
	&& cp xgen-gmlc gtkmarshalers.c \
	&& rm -f xgen-gmlc

gtkmarshal.h: stamp-gtkmarshal.h
	@true
stamp-gtkmarshal.h: @REBUILD@ gtkmarshal.list
	echo "#ifndef GTK_DISABLE_DEPRECATED" > xgen-gmh					\
        && $(GLIB_GENMARSHAL) --prefix=gtk_marshal $(srcdir)/gtkmarshal.list --header >> xgen-gmh \
	&& echo "#endif /* GTK_DISABLE_DEPRECATED */" >> xgen-gmh \
	&& (cmp -s xgen-gmh gtkmarshal.h || cp xgen-gmh gtkmarshal.h) \
	&& rm -f xgen-gmh \
	&& echo timestamp > $(@F)
gtkmarshal.c: @REBUILD@ gtkmarshal.list
	(echo "#include \"gtkalias.h\""; $(GLIB_GENMARSHAL) --prefix=gtk_marshal $(srcdir)/gtkmarshal.list --body) >> xgen-gmc \
	&& cp xgen-gmc gtkmarshal.c \
	&& rm -f xgen-gmc 

gtktypebuiltins.h: stamp-gtktypebuiltins.h
	@true
stamp-gtktypebuiltins.h: @REBUILD@ $(gtk_public_h_sources) Makefile
	( cd $(srcdir) && glib-mkenums \
			--fhead "#ifndef __GTK_TYPE_BUILTINS_H__\n#define __GTK_TYPE_BUILTINS_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
			--fprod "/* enumerations from \"@filename@\" */\n" \
			--vhead "GType @enum_name@_get_type (void) G_GNUC_CONST;\n#define GTK_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
			--ftail "G_END_DECLS\n\n#endif /* __GTK_TYPE_BUILTINS_H__ */" \
		$(gtk_public_h_sources) ) >> xgen-gtbh \
	&& (cmp -s xgen-gtbh gtktypebuiltins.h || cp xgen-gtbh gtktypebuiltins.h ) \
	&& rm -f xgen-gtbh \
	&& echo timestamp > $(@F)
gtktypebuiltins.c: @REBUILD@ $(gtk_public_h_sources) Makefile
	( cd $(srcdir) && glib-mkenums \
			--fhead "#undef GTK_DISABLE_DEPRECATED\n#define GTK_ENABLE_BROKEN\n#include \"gtkalias.h\"\n#include \"gtk.h\"\n#include \"gtkprivate.h\"" \
			--fprod "\n/* enumerations from \"@filename@\" */" \
			--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {" \
			--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
		$(gtk_public_h_sources) ) > xgen-gtbc \
	&& cp xgen-gtbc gtktypebuiltins.c  \
	&& rm -f xgen-gtbc

# target platform:
lib_LTLIBRARIES = $(gtktargetlib)

gtkincludedir = $(includedir)/gtk-2.0/gtk
gtkinclude_HEADERS = $(gtk_public_h_sources) $(gtk_semi_private_h_sources) $(gtk_built_public_sources) gtkversion.h

libgtk_x11_2_0_la_SOURCES = $(gtk_c_sources) $(gtk_plug_c_sources)
libgtk_linux_fb_2_0_la_SOURCES = $(gtk_c_sources)
libgtk_win32_2_0_la_SOURCES = $(gtk_c_sources)

libgtk_x11_2_0_la_LDFLAGS = $(libtool_opts)
libgtk_linux_fb_2_0_la_LDFLAGS = $(libtool_opts)
libgtk_win32_2_0_la_LDFLAGS = $(libtool_opts)

libgtk_x11_2_0_la_LIBADD = $(libadd)
libgtk_linux_fb_2_0_la_LIBADD = $(libadd)
libgtk_win32_2_0_la_LIBADD = $(libadd) $(gtk_win32res_lo)
libgtk_win32_2_0_la_DEPENDENCIES = $(gtk_def) $(gtk_win32res_lo)

if USE_WIN32
libgtk_target_ldflags = $(gtk_win32_symbols) -lole32 -lwsock32
endif
EXTRA_LTLIBRARIES = libgtk-x11-2.0.la libgtk-linux-fb-2.0.la libgtk-win32-2.0.la

install-exec-hook: 
if DISABLE_EXPLICIT_DEPS
	$(SHELL) $(top_srcdir)/sanitize-la.sh $(DESTDIR)$(libdir)/$(gtktargetlib)
endif

# Install a RC file for the default GTK+ theme, and key themes
install-data-local: install-ms-lib install-libtool-import-lib
	$(mkinstalldirs) $(DESTDIR)$(datadir)/themes/Default/gtk-2.0
	$(INSTALL_DATA) $(srcdir)/gtkrc.default $(DESTDIR)$(datadir)/themes/Default/gtk-2.0/gtkrc
	$(mkinstalldirs) $(DESTDIR)$(datadir)/themes/Default/gtk-2.0-key
	$(INSTALL_DATA) $(srcdir)/gtkrc.key.default $(DESTDIR)$(datadir)/themes/Default/gtk-2.0-key/gtkrc
	$(mkinstalldirs) $(DESTDIR)$(datadir)/themes/Emacs/gtk-2.0-key
	$(INSTALL_DATA) $(srcdir)/gtkrc.key.emacs $(DESTDIR)$(datadir)/themes/Emacs/gtk-2.0-key/gtkrc

uninstall-local: uninstall-ms-lib uninstall-libtool-import-lib
	rm -f $(DESTDIR)$(datadir)/themes/Default/gtk-2.0/gtkrc
	rm -f $(DESTDIR)$(datadir)/themes/Default/gtk-2.0-key/gtkrc
	rm -f $(DESTDIR)$(datadir)/themes/Emacs/gtk-2.0-key/gtkrc

# if srcdir!=builddir, clean out maintainer-clean files from builddir
# this allows dist to pass.
distclean-local:
	if test $(srcdir) != .; then \
	  rm -f $(MAINTAINERCLEANFILES); \
	fi

DEPS = $(gtktargetlib) $(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la $(top_builddir)/gdk/$(gdktargetlib)

TEST_DEPS = $(DEPS) gtk.immodules

LDADDS =								\
	$(gtktargetlib)							\
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la	\
	$(top_builddir)/gdk/$(gdktargetlib)

#
# Installed tools
#
bin_PROGRAMS = gtk-query-immodules-2.0 gtk-update-icon-cache

gtk_query_immodules_2_0_DEPENDENCIES = $(DEPS)
gtk_query_immodules_2_0_LDADD = $(LDADDS)

gtk_query_immodules_2_0_SOURCES = queryimmodules.c


gtk_update_icon_cache_DEPENDENCIES = $(DEPS)
gtk_update_icon_cache_LDADD = $(LDADDS)

gtk_update_icon_cache_SOURCES = updateiconcache.c

.PHONY: files test test-debug

files:
	@files=`ls $(DISTFILES) 2> /dev/null `; for p in $$files; do \
	  echo $$p; \
	done

EXTRA_DIST +=                   \
	line-arrow.xbm 		\
	line-wrap.xbm 		\
	tree_plus.xbm		\
	tree_minus.xbm          \
	tree_minus.xpm		\
	tree_plus.xpm		\
	gtk.def			\
	gtk-win32.rc		\
	gtk-win32.rc.in		\
	gtkfilesystemwin32.h	\
	gtkfilesystemwin32.c	\
	gtkrc.default		\
	gtkrc.key.default	\
	gtkrc.key.emacs		\
	makefile.msc		\
	makefile.msc.in		\
	makegtkalias.pl		\
	abicheck.sh

install-data-local:
