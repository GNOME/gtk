# Makefile.am for gtk+/gtk

SUBDIRS=stock-icons

INCLUDES = @STRIP_BEGIN@ \
	-DG_LOG_DOMAIN=\"Gtk\"				\
	-DGTK_DISABLE_COMPAT_H				\
	-DGTK_LIBDIR=\"$(libdir)\"			\
	-DGTK_DATA_PREFIX=\"$(prefix)\"			\
	-DGTK_SYSCONFDIR=\"$(sysconfdir)\"		\
	-DGTK_LOCALEDIR=\"$(gtklocaledir)\"		\
	-DGTK_VERSION=\"@GTK_VERSION@\"			\
	-DTESTGTK_RCFILE=\"`pwd`/$(srcdir)/testgtkrc\"	\
	-I$(top_builddir)/gtk				\
	-I$(top_srcdir) -I../gdk			\
	-I$(top_srcdir)/gdk				\
	-I$(top_srcdir)/gdk-pixbuf -I../gdk-pixbuf	\
	@GTK_DEBUG_FLAGS@				\
	@GTK_XIM_FLAGS@					\
	@GTK_LOCALE_FLAGS@				\
	@PANGO_CFLAGS@					\
	@GLIB_CFLAGS@					\
	@more_cflags@					\
@STRIP_END@

gtarget=@gdktarget@

# libtool stuff: set version and export symbols for resolving
# since automake doesn't support conditionalized libsomething_la_LDFLAGS
# we use the general approach here
libgtkincludedir = $(includedir)/gtk-2.0/gtk
LDFLAGS = @STRIP_BEGIN@ \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)	\
	-export-dynamic						\
	-rpath @prefix@/lib					\
	@LIBTOOL_EXPORT_OPTIONS@				\
	@PANGO_LIBS@						\
	@GLIB_DEPLIBS@ 						\
	@more_ldflags@						\
	@more_libs@ 						\
	-lm							\
@STRIP_END@


#
# setup source file variables
#

# GTK+ header files for public installation (non-generated, or generated
# by configure)
gtk_public_h_sources = @STRIP_BEGIN@ \
	gtk.h			\
	gtkaccelgroup.h		\
	gtkaccellabel.h		\
	gtkadjustment.h		\
	gtkalignment.h		\
	gtkarg.h		\
	gtkarrow.h		\
	gtkaspectframe.h	\
	gtkbin.h		\
	gtkbindings.h		\
	gtkbbox.h		\
	gtkbox.h		\
	gtkbutton.h		\
	gtkcalendar.h		\
	gtkcheckbutton.h	\
	gtkcheckmenuitem.h	\
	gtkseparatormenuitem.h	\
	gtkclist.h		\
	gtkclipboard.h		\
	gtkcolorsel.h		\
	gtkcolorseldialog.h	\
	gtkcombo.h		\
	gtkcontainer.h		\
	gtkctree.h		\
	gtkcurve.h		\
	gtkcellrenderer.h	\
	gtkcellrenderertext.h	\
	gtkcellrenderertextpixbuf.h	\
	gtkcellrenderertoggle.h	\
	gtkcellrendererpixbuf.h	\
	gtkdata.h		\
	gtkdebug.h              \
	gtkdialog.h		\
	gtkdnd.h		\
	gtkdrawingarea.h	\
	gtkeditable.h           \
	gtkentry.h		\
	gtkenums.h		\
	gtkeventbox.h		\
	gtkfilesel.h		\
	gtkfixed.h		\
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
	gtkhsv.h		\
	gtkiconfactory.h	\
	gtkimage.h		\
	gtkimagemenuitem.h	\
	gtkimcontext.h		\
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
	gtkmenufactory.h	\
	gtkmenuitem.h		\
	gtkmenushell.h		\
	gtkmessagedialog.h	\
	gtkmisc.h		\
	gtknotebook.h		\
	gtkoldeditable.h	\
	gtkobject.h		\
	gtkoptionmenu.h		\
	gtkpacker.h		\
	gtkpaned.h		\
	gtkpixmap.h		\
	gtkplug.h		\
	gtkpreview.h		\
	gtkprivate.h		\
	gtkprogress.h		\
	gtkprogressbar.h	\
	gtkradiobutton.h	\
	gtkradiomenuitem.h	\
	gtkrange.h		\
	gtkrc.h			\
	gtkruler.h		\
	gtkscale.h		\
	gtkscrollbar.h		\
	gtkscrolledwindow.h	\
	gtkselection.h		\
	gtkseparator.h		\
	gtksignal.h		\
	gtksocket.h		\
	gtkspinbutton.h		\
	gtkstyle.h		\
	gtkstatusbar.h		\
	gtkstock.h		\
	gtktable.h		\
	gtktearoffmenuitem.h    \
	gtktextbuffer.h		\
	gtktextchild.h		\
	gtktextdisplay.h	\
	gtktextiter.h		\
	gtktextlayout.h		\
	gtktextmark.h		\
	gtktexttag.h		\
	gtktexttagtable.h	\
	gtktextview.h		\
	gtktext.h		\
	gtktipsquery.h		\
	gtktogglebutton.h	\
	gtktoolbar.h		\
	gtktooltips.h		\
	gtktree.h		\
	gtktreednd.h		\
	gtktreeitem.h		\
	gtktreemodel.h		\
	gtktreemodelsort.h	\
	gtktreeselection.h	\
	gtktreesortable.h	\
	gtktreestore.h		\
	gtktreeview.h		\
	gtktreeviewcolumn.h	\
	gtktypeutils.h		\
	gtkvbbox.h		\
	gtkvbox.h		\
	gtkviewport.h		\
	gtkvpaned.h		\
	gtkvruler.h		\
	gtkvscale.h		\
	gtkvscrollbar.h		\
	gtkvseparator.h		\
	gtkwidget.h		\
	gtkwindow.h		\
@STRIP_END@
# GTK+ header files that don't get installed
gtk_private_h_sources = @STRIP_BEGIN@ \
	gtkrbtree.h 		\
	gtktextbtree.h		\
	gtktextchildprivate.h   \
	gtktextsegment.h	\
	gtktexttypes.h		\
	gtktextiterprivate.h	\
	gtktextmarkprivate.h	\
	gtktexttagprivate.h	\
	gtkthemes.h		\
	gtktreedatalist.h	\
	gtktreeprivate.h	\
	gtkwindow-decorate.h    \
@STRIP_END@
# GTK+ C sources to build the library from
gtk_c_sources = @STRIP_BEGIN@   \
	gtkaccelgroup.c		\
	gtkaccellabel.c		\
	gtkadjustment.c		\
	gtkalignment.c		\
	gtkarg.c		\
	gtkarrow.c		\
	gtkaspectframe.c	\
	gtkbin.c		\
	gtkbindings.c		\
	gtkbbox.c		\
	gtkbox.c		\
	gtkbutton.c		\
	gtkcalendar.c		\
	gtkcellrenderer.c	\
	gtkcellrenderertext.c	\
	gtkcellrenderertextpixbuf.c	\
	gtkcellrenderertoggle.c	\
	gtkcellrendererpixbuf.c	\
	gtkcheckbutton.c	\
	gtkcheckmenuitem.c	\
	gtkseparatormenuitem.c	\
	gtkclipboard.c		\
	gtkclist.c		\
	gtkcolorsel.c		\
	gtkcolorseldialog.c	\
	gtkcombo.c		\
	gtkcontainer.c		\
	gtkctree.c		\
	gtkcurve.c		\
	gtkdata.c		\
	gtkdialog.c		\
	gtkdnd.c		\
	gtkdrawingarea.c	\
	gtkeditable.c           \
	gtkentry.c		\
	gtkeventbox.c		\
	gtkfilesel.c		\
	gtkfixed.c		\
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
	gtkiconfactory.c	\
	gtkimage.c		\
	gtkimagemenuitem.c	\
	gtkimcontext.c		\
	gtkimcontextsimple.c	\
	gtkimcontextsimple.h	\
	gtkimmodule.c		\
	gtkimmulticontext.c	\
	gtkinputdialog.c	\
	gtkintl.h		\
	gtkinvisible.c		\
	gtkitem.c		\
	gtkitemfactory.c	\
	gtklabel.c		\
	gtklayout.c		\
	gtklist.c		\
	gtklistitem.c		\
	gtkliststore.c		\
	gtkmain.c		\
	gtkmenu.c		\
	gtkmenubar.c		\
	gtkmenufactory.c	\
	gtkmenuitem.c		\
	gtkmenushell.c		\
	gtkmessagedialog.c	\
	gtkmisc.c		\
	gtknotebook.c		\
	gtkobject.c		\
	gtkoldeditable.c	\
	gtkoptionmenu.c		\
	gtkpacker.c		\
	gtkpaned.c		\
	gtkpixmap.c		\
	gtkplug.c               \
	gtkpreview.c		\
	gtkprogress.c		\
	gtkprogressbar.c	\
	gtkradiobutton.c	\
	gtkradiomenuitem.c	\
	gtkrange.c		\
	gtkrbtree.c 		\
	gtkrc.c			\
	gtkruler.c		\
	gtkscale.c		\
	gtkscrollbar.c		\
	gtkscrolledwindow.c	\
	gtkselection.c		\
	gtkseparator.c		\
	gtksignal.c		\
	gtksocket.c             \
	gtkspinbutton.c		\
	gtkstock.c		\
	gtkstyle.c		\
	gtkstatusbar.c		\
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
	gtktextview.c		\
	gtkthemes.c		\
	gtktipsquery.c		\
	gtktogglebutton.c	\
	gtktoolbar.c		\
	gtktooltips.c		\
	gtktree.c		\
	gtktreeitem.c		\
	gtktreedatalist.c	\
	gtktreednd.c		\
	gtktreemodel.c		\
	gtktreemodelsort.c	\
	gtktreeselection.c	\
	gtktreestore.c		\
	gtktreeview.c		\
	gtktreeviewcolumn.c	\
	gtktypeutils.c		\
	gtkvbbox.c		\
	gtkvbox.c		\
	gtkviewport.c		\
	gtkvpaned.c		\
	gtkvruler.c		\
	gtkvscale.c		\
	gtkvscrollbar.c		\
	gtkvseparator.c		\
	gtkwidget.c		\
	gtkwindow.c		\
	gtkwindow-decorate.c    \
	fnmatch.c		\
	fnmatch.h		\
@STRIP_END@
# Extra headers that are used for enum type array/id generation
gdk_headers = @STRIP_BEGIN@ 	\
	../gdk/gdkcolor.h	\
	../gdk/gdkcursor.h     	\
	../gdk/gdkdnd.h		\
	../gdk/gdkdrawable.h	\
	../gdk/gdkevents.h	\
	../gdk/gdkfont.h	\
	../gdk/gdkgc.h		\
	../gdk/gdkimage.h	\
	../gdk/gdkinput.h	\
	../gdk/gdkpixmap.h	\
	../gdk/gdkproperty.h	\
	../gdk/gdkregion.h	\
	../gdk/gdkrgb.h		\
	../gdk/gdkselection.h	\
	../gdk/gdktypes.h	\
	../gdk/gdkvisual.h	\
	../gdk/gdkwindow.h	\
@STRIP_END@

# we use our own built_sources variable rules to avoid automake's
# BUILT_SOURCES oddities
# we generate frequently rebuild files piggyback on a stamp file, so sources
# depending on them only get rebuild when the built source actually changed
# content

# built sources that get installed with the header files
gtk_built_public_sources = @STRIP_BEGIN@ \
	gtkmarshal.h		\
	gtktypebuiltins.h	\
@STRIP_END@
# built sources that don't get installed
gtk_built_sources = @STRIP_BEGIN@ \
	stamp-gtk.defs		\
	stamp-gtktypebuiltins.h	\
	stamp-gtkmarshal.h	\
	gtktypebuiltins_vars.c	\
	gtktypebuiltins_ids.c	\
	gtktypebuiltins_evals.c	\
	gtkmarshal.c		\
	gtk.defs		\
	${gtk_built_public_sources} \
@STRIP_END@
# non-header sources (headers should be specified in the above variables)
# that don't serve as direct make target sources, i.e. they don't have
# their own .lo rules and don't get publically installed
gtk_extra_sources = @STRIP_BEGIN@ \
	gtkcompat.h.in		\
	makeenums.pl		\
	makeenums.awk		\
	maketypes.awk		\
	makeenums.h		\
	gtkargcollector.c	\
	gtk-boxed.defs		\
	gtkmarshal.list		\
@STRIP_END@


#
# setup GTK+ sources and their dependancies
#
gtk_target_headers = $(gtk_public_h_sources) $(gtk_built_public_sources) gtkcompat.h
gtk_target_sources = $(gtk_c_sources)
MAINTAINERCLEANFILES += $(gtk_built_sources)
EXTRA_HEADERS +=
EXTRA_DIST += $(gtk_private_h_sources) $(gtk_extra_sources)
EXTRA_DIST += $(gtk_built_sources)

#
# rules to generate built sources
#
# setup autogeneration dependancies
gen_sources = xgen-gdef xgen-gtbh xgen-gtbvc xgen-gtbic xgen-gtbec xgen-gmh xgen-gmc
CLEANFILES += $(gen_sources)
Makefile: oldest-source-stamp	# oh boy, does automake SUCK!
oldest-source-stamp: $(gtk_built_sources)
$(OBJECTS): oldest-source-stamp  ${gtk_built_public_sources} # this is our oldest file, used for implicit auto-generation deps
# initial creation of the real stamp-* files
gtk.defs gtkmarshal.h gtktypebuiltins.h:          # never add deps here
	test -f "$(srcdir)/$@" || touch $(srcdir)/$@
# normal autogeneration rules
# all autogenerated files need to be generated in the srcdir,
# so old versions get remade and are not confused with newer
# versions in the build dir. thus a development setup requires
# srcdir to be writable, passing --disable-rebuilds to
# ../configure will supress all autogeneration rules.
$(srcdir)/stamp-gtk.defs: @REBUILD@ makeenums.pl gtk.defs gtk-boxed.defs $(gtk_public_h_sources) $(gdk_headers)
	cd $(srcdir) \
	&& $(PERL) makeenums.pl defs $(gtk_public_h_sources) $(gdk_headers) > xgen-gdef \
	&& cat gtk-boxed.defs >> xgen-gdef \
	&& (cmp -s xgen-gdef gtk.defs || cp xgen-gdef gtk.defs) \
	&& rm -f xgen-gdef \
	&& echo timestamp > $(@F)
$(srcdir)/stamp-gtkmarshal.h: @REBUILD@ gtkmarshal.list gtkmarshal.h
	cd $(srcdir) \
	&& glib-genmarshal --prefix=gtk_marshal gtkmarshal.list --header >> xgen-gmh \
	&& (cmp -s xgen-gmh gtkmarshal.h || cp xgen-gmh gtkmarshal.h) \
	&& rm -f xgen-gmh xgen-gmh~ \
	&& echo timestamp > $(@F)
$(srcdir)/gtkmarshal.c: @REBUILD@ $(srcdir)/stamp-gtkmarshal.h
	cd $(srcdir) \
	&& glib-genmarshal --prefix=gtk_marshal gtkmarshal.list --body >> xgen-gmc \
	&& cp xgen-gmc gtkmarshal.c \
	&& rm -f xgen-gmc xgen-gmc~
$(srcdir)/stamp-gtktypebuiltins.h: @REBUILD@ maketypes.awk stamp-gtk.defs gtktypebuiltins.h
	cd $(srcdir) \
	&& LC_ALL=C $(AWK) -f maketypes.awk gtk.defs macros > xgen-gtbh \
	&& (cmp -s xgen-gtbh gtktypebuiltins.h || cp xgen-gtbh gtktypebuiltins.h) \
	&& rm -f xgen-gtbh \
	&& echo timestamp > $(@F)
$(srcdir)/gtktypebuiltins_vars.c: @REBUILD@ maketypes.awk stamp-gtk.defs
	cd $(srcdir) \
	&& LC_ALL=C $(AWK) -f maketypes.awk gtk.defs variables > xgen-gtbvc \
	&& cp xgen-gtbvc $(@F) \
	&& rm -f xgen-gtbvc
$(srcdir)/gtktypebuiltins_ids.c: @REBUILD@ maketypes.awk stamp-gtk.defs
	cd $(srcdir) \
	&& LC_ALL=C $(AWK) -f maketypes.awk gtk.defs entries > xgen-gtbic \
	&& cp xgen-gtbic $(@F) \
	&& rm -f xgen-gtbic
$(srcdir)/gtktypebuiltins_evals.c: @REBUILD@ makeenums.pl $(gtk_public_h_sources) $(gdk_headers)
	cd $(srcdir) \
	&& $(PERL) makeenums.pl arrays $(gtk_public_h_sources) $(gdk_headers) > xgen-gtbec \
	&& cp xgen-gtbec $(@F) \
	&& rm -f xgen-gtbec


# target platform:
lib_ltlibraries = @gtktargetlib@
# now define the real one to workaround automake's mishandling
lib_LTLIBRARIES = $(lib_ltlibraries)
if USE_X11
libgtk_x11_includedir = $(includedir)/gtk-2.0/gtk
libgtk_x11_include_HEADERS = $(gtk_target_headers)
libgtk_x11_1_3_la_SOURCES = $(gtk_target_sources)
endif
if USE_NANOX
libgtk_nanox_includedir = $(includedir)/gtk-2.0/gtk
libgtk_nanox_include_HEADERS = $(gtk_target_headers)
libgtk_nanox_1_3_la_SOURCES = $(gtk_target_sources)
endif
if USE_LINUX_FB
libgtk_linux_fb_includedir = $(includedir)/gtk-2.0/gtk
libgtk_linux_fb_include_HEADERS = $(gtk_target_headers)
libgtk_linux_fb_1_3_la_SOURCES = $(gtk_target_sources)
endif
EXTRA_LTLIBRARIES = libgtk-x11-1.3.la libgtk-nanox-1.3.la libgtk-linux-fb-1.3.la


# We create a dummy theme for the default GTK+ theme
install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(datadir)/themes/Default/gtk-2.0
	echo "# Empty gtkrc for default theme" > $(DESTDIR)$(datadir)/themes/Default/gtk-2.0/gtkrc
	$(mkinstalldirs) $(DESTDIR)$(sysconfdir)/gtk-2.0
	$(top_builddir)/gtk/gtk-query-immodules-2.0 > $(DESTDIR)$(sysconfdir)/gtk-2.0/gtk.immodules
uninstall-local:
	rm -f $(DESTDIR)$(datadir)/themes/Default/gtk-2.0/gtkrc

DEPS = @gtktargetlib@ $(top_builddir)/gdk-pixbuf/libgdk_pixbuf-1.3.la $(top_builddir)/gdk/@gdktargetlib@
TEST_DEPS = $(DEPS) gtk.immodules

LDADDS = @STRIP_BEGIN@ \
	@gtktargetlib@					\
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-1.3.la	\
	$(top_builddir)/gdk/@gdktargetlib@ 		\
	@more_ldflags@					\
	@more_libs@					\
	@GDK_WLIBS@					\
	@PANGO_LIBS@					\
	@GLIB_LIBS@					\
	@GTK_LIBS_EXTRA@				\
	-lm						\
@STRIP_END@

#
# Installed tools
#
bin_PROGRAMS = gtk-query-immodules-2.0

gtk_query_immodules_2_0_DEPENDENCIES = $(DEPS)
gtk_query_immodules_2_0_LDADD = $(LDADDS)

gtk_query_immodules_2_0_SOURCES = queryimmodules.c

gtk.immodules: gtk-query-immodules-2.0 ../modules/input/Makefile.am
	./gtk-query-immodules-2.0 ../modules/input/.libs/*.so > gtk.immodules

#
# test programs, not to be installed
#
noinst_PROGRAMS = testgtk testcalendar testinput testselection testrgb testdnd testtext simple treestoretest testtextbuffer # testthreads

testcalendar_DEPENDENCIES = $(TEST_DEPS)
testgtk_DEPENDENCIES = $(TEST_DEPS)
testinput_DEPENDENCIES = $(TEST_DEPS)
testrgb_DEPENDENCIES = $(TEST_DEPS)
testselection_DEPENDENCIES = $(TEST_DEPS)
testtext_DEPENDENCIES = $(TEST_DEPS)
testtextbuffer_DEPENDENCIES = $(TEST_DEPS)
treestoretest_DEPENDENCIES = $(TEST_DEPS)
testdnd_DEPENDENCIES = $(TEST_DEPS)
simple_DEPENDENCIES = $(TEST_DEPS)
#testthreads_DEPENDENCIES = $(TEST_DEPS)

testcalendar_LDADD = $(LDADDS)
testgtk_LDADD = $(LDADDS)
testinput_LDADD = $(LDADDS)
testrgb_LDADD = $(LDADDS)
testselection_LDADD = $(LDADDS)
testtext_LDADD = $(LDADDS)
testtextbuffer_LDADD = $(LDADDS)
treestoretest_LDADD = $(LDADDS)
testdnd_LDADD = $(LDADDS)
simple_LDADD = $(LDADDS)
#testthreads_LDADD = $(LDADDS)

.PHONY: files test test-debug

files:
	@files=`ls $(DISTFILES) 2> /dev/null `; for p in $$files; do \
	  echo $$p; \
	done

test: testgtk
	builddir=`pwd`; cd $(top_builddir); top_builddir=`pwd`; \
	cd $$builddir; cd $(srcdir); \
	$(SHELL) $$top_builddir/libtool --mode=execute $$builddir/testgtk

test-debug: testgtk
	builddir=`pwd`; cd $(top_builddir); top_builddir=`pwd`; \
	cd $$builddir; cd $(srcdir); \
	$(SHELL) $$top_builddir/libtool --mode=execute gdb $$builddir/testgtk

EXTRA_DIST += @STRIP_BEGIN@ 	\
	oldest-source-stamp	\
	testgtk.1 		\
	testgtkrc 		\
	testgtkrc2 		\
	circles.xbm		\
	line-arrow.xbm 		\
	line-wrap.xbm 		\
	tree_plus.xbm		\
	tree_minus.xbm          \
	3DRings.xpm		\
	FilesQueue.xpm		\
	Modeller.xpm		\
	check-y.xpm		\
	check-n.xpm		\
	marble.xpm		\
	tree_minus.xpm		\
	tree_plus.xpm		\
	test.xpm		\
	check-y.xpm		\
	check-n.xpm		\
	test.xpm		\
	gtk.def			\
	gtk-win32.rc		\
	gtk-win32.rc.in		\
	makefile.mingw		\
	makefile.mingw.in	\
	makefile.msc		\
	makefile.msc.in		\
@STRIP_END@

install-data-local:

