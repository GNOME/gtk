## Makefile.am for gtk+/demos
include $(top_srcdir)/Makefile.decl

## These should be in the order you want them to appear in the
## demo app, which means alphabetized by demo title, not filename
demos_base =					\
	application_demo.c			\
	assistant.c				\
	builder.c				\
	button_box.c				\
	changedisplay.c				\
	clipboard.c				\
	colorsel.c				\
	combobox.c				\
	css_accordion.c                         \
	css_basics.c				\
	css_blendmodes.c			\
	css_multiplebgs.c			\
	css_pixbufs.c				\
	css_shadows.c				\
	cursors.c				\
	dialog.c				\
	drawingarea.c				\
	editable_cells.c			\
	entry_buffer.c				\
	entry_completion.c			\
	event_axes.c				\
	expander.c  				\
        filtermodel.c				\
	fishbowl.c				\
	foreigndrawing.c			\
	gestures.c				\
	glarea.c				\
	headerbar.c				\
	hypertext.c				\
	iconview.c				\
	iconview_edit.c				\
	images.c				\
	infobar.c				\
	links.c					\
	listbox.c				\
	flowbox.c				\
	list_store.c				\
	markup.c				\
	menus.c					\
	modelbutton.c				\
	overlay.c				\
	overlay2.c				\
	panes.c					\
	pickers.c				\
	pixbufs.c				\
	popover.c				\
	printing.c				\
	revealer.c				\
	rotated_text.c				\
	scale.c					\
	search_entry.c				\
	search_entry2.c				\
	shortcuts.c				\
	sidebar.c				\
	sizegroup.c				\
	spinbutton.c				\
	spinner.c				\
	stack.c					\
	tabs.c					\
	textmask.c				\
	textview.c				\
	textscroll.c				\
	theming_style_classes.c			\
	toolpalette.c				\
	transparent.c				\
	tree_store.c

demos_opt =

if BUILD_FONT_DEMO
demos_opt += font_features.c
endif

if OS_UNIX
demos_opt += pagesetup.c
endif

demos = $(demos_base) $(demos_opt)

AM_CPPFLAGS = \
	-I$(top_srcdir) 			\
	-I$(top_builddir)/gdk			\
	-DGDK_DISABLE_DEPRECATED		\
	-DGTK_DISABLE_DEPRECATED 		\
	$(GTK_DEBUG_FLAGS)			\
	$(GTK_DEP_CFLAGS)

DEPS = \
	$(top_builddir)/gtk/libgtk-4.la

LDADDS = \
	$(top_builddir)/gtk/libgtk-4.la		\
	$(GTK_DEP_LIBS)				\
	$(GDK_DEP_LIBS)				\
	-lm

if BUILD_FONT_DEMO
AM_CPPFLAGS += $(FONTDEMO_CFLAGS)
LDADDS += $(FONTDEMO_LIBS)
endif

bin_PROGRAMS = gtk4-demo gtk4-demo-application

desktopdir = $(datadir)/applications
dist_desktop_DATA = gtk4-demo.desktop

BUILT_SOURCES = demos.h demo_resources.c

EXTRA_DIST += 				\
	data/source.svg			\
	data/symbolic-source.svg	\
	demo.gresource.xml		\
	$(resource_files)		\
	org.gtk.Demo.gschema.xml	\
	demos.h.win32

gsettings_SCHEMAS = \
	org.gtk.Demo.gschema.xml

@GSETTINGS_RULES@

demos.h: $(demos) geninclude.pl
	 $(AM_V_GEN) (here=`pwd` ; cd $(srcdir) && $(PERL) $$here/geninclude.pl $(demos)) > demos.h

demos.h.win32: $(demos_base) geninclude.pl
	 $(AM_V_GEN) (here=`pwd` ; cd $(srcdir) && $(PERL) $$here/geninclude.pl $(demos_base)) > demos.h.win32

nodist_gtk4_demo_SOURCES = demos.h

gtk4_demo_SOURCES = 		\
	$(demos)		\
	gtkfishbowl.c		\
	gtkfishbowl.h		\
	demo_resources.c	\
	main.c

gtk4_demo_DEPENDENCIES = $(DEPS)
gtk4_demo_LDADD = $(LDADDS)
gtk4_demo_LDFLAGS = -export-dynamic

gtk4_demo_application_SOURCES = \
	application.c 		\
	demo_resources.c

gtk4_demo_application_LDADD = $(LDADDS)

resource_files = $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(builddir)/demo.gresource.xml)

demo_resources.c: demo.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $(srcdir)/demo.gresource.xml

iconthemedir = $(datadir)/icons/hicolor

appsicon16dir = $(iconthemedir)/16x16/apps
appsicon22dir = $(iconthemedir)/22x22/apps
appsicon24dir = $(iconthemedir)/24x24/apps
appsicon32dir = $(iconthemedir)/32x32/apps
appsicon48dir = $(iconthemedir)/48x48/apps
appsicon256dir = $(iconthemedir)/256x256/apps
appsiconscalabledir = $(iconthemedir)/scalable/apps

dist_appsicon16_DATA = data/16x16/gtk4-demo.png data/16x16/gtk4-demo-symbolic.symbolic.png
dist_appsicon22_DATA = data/22x22/gtk4-demo.png data/22x22/gtk4-demo-symbolic.symbolic.png
dist_appsicon24_DATA = data/24x24/gtk4-demo.png data/24x24/gtk4-demo-symbolic.symbolic.png
dist_appsicon32_DATA = data/32x32/gtk4-demo.png data/32x32/gtk4-demo-symbolic.symbolic.png
dist_appsicon48_DATA = data/48x48/gtk4-demo.png data/48x48/gtk4-demo-symbolic.symbolic.png
dist_appsicon256_DATA = data/256x256/gtk4-demo.png data/256x256/gtk4-demo-symbolic.symbolic.png

update_icon_cache = $(top_builddir)/gtk/gtk4-update-icon-cache$(EXEEXT) --ignore-theme-index --force

install-data-hook: install-update-icon-cache
uninstall-hook: uninstall-update-icon-cache

install-update-icon-cache:
	$(AM_V_at)$(POST_INSTALL)
	test -n "$(DESTDIR)" || $(update_icon_cache) "$(iconthemedir)"

uninstall-update-icon-cache:
	$(AM_V_at)$(POST_UNINSTALL)
	test -n "$(DESTDIR)" || $(update_icon_cache) "$(iconthemedir)"

# ------------------- MSVC Build Items ----------------
MSVCPROJS = gtk4-demo gtk4-demo-application

gtk4_demo_FILES = $(gtk4_demo_SOURCES)
gtk4_demo_EXCLUDES = font_features.c|pagesetup.c

gtk4_demo_application_FILES = $(gtk4_demo_application_SOURCES)
gtk4_demo_application_EXCLUDES = dummy

include $(top_srcdir)/win32/Makefile.msvcproj

dist-hook: \
	$(top_builddir)/win32/vs12/gtk4-demo.vcxproj	\
	$(top_builddir)/win32/vs12/gtk4-demo-application.vcxproj

DISTCLEANFILES = demos.h demos.h.win32

-include $(top_srcdir)/git.mk
