## Makefile.am for gtk+/demos
include $(top_srcdir)/Makefile.decl

## These should be in the order you want them to appear in the
## demo app, which means alphabetized by demo title, not filename
demos =						\
	application.c				\
	assistant.c				\
	builder.c				\
	button_box.c				\
	changedisplay.c				\
	clipboard.c				\
	colorsel.c				\
	combobox.c				\
	css_accordion.c                         \
	css_basics.c				\
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
	menus.c					\
	offscreen_window.c			\
	offscreen_window2.c			\
	overlay.c				\
	panes.c					\
	pickers.c				\
	pixbufs.c				\
	popover.c				\
	printing.c				\
	revealer.c				\
	rotated_text.c				\
	search_entry.c				\
	search_entry2.c				\
	sidebar.c				\
	sizegroup.c				\
	spinner.c				\
	stack.c					\
	textview.c				\
	textscroll.c				\
	theming_style_classes.c			\
	toolpalette.c				\
	transparent.c				\
	tree_store.c

AM_CPPFLAGS = \
	-I$(top_srcdir) 			\
	-I$(top_builddir)/gdk			\
	-DGDK_DISABLE_DEPRECATED		\
	-DGTK_DISABLE_DEPRECATED 		\
	$(GTK_DEBUG_FLAGS)			\
	$(GTK_DEP_CFLAGS)

DEPS = \
	$(top_builddir)/gtk/libgtk-3.la

LDADDS = \
	$(top_builddir)/gtk/libgtk-3.la		\
	$(top_builddir)/gdk/libgdk-3.la		\
	$(GTK_DEP_LIBS)				\
	$(GDK_DEP_LIBS)				\
	-lm

bin_PROGRAMS = gtk3-demo gtk3-demo-application

desktopdir = $(datadir)/applications
dist_desktop_DATA = gtk3-demo.desktop

BUILT_SOURCES = demos.h demo_resources.c

EXTRA_DIST += 				\
	data/source.svg			\
	data/symbolic-source.svg	\
	demo.gresource.xml		\
	$(RESOURCES)			\
	org.gtk.Demo.gschema.xml

gsettings_SCHEMAS = \
	org.gtk.Demo.gschema.xml

@GSETTINGS_RULES@

demos.h: $(demos) geninclude.pl
	 $(AM_V_GEN) (here=`pwd` ; cd $(srcdir) && $(PERL) $$here/geninclude.pl $(demos)) > demos.h

gtk3_demo_SOURCES = 		\
	$(demos)		\
	demo_resources.c	\
	main.c			\
	demos.h

gtk3_demo_DEPENDENCIES = $(DEPS)
gtk3_demo_LDADD = $(LDADDS)
gtk3_demo_LDFLAGS = -export-dynamic

gtk3_demo_application_SOURCES = \
	application-standalone.c \
	demo_resources.c

gtk3_demo_application_LDADD = $(LDADDS)

demo_resources.c: demo.gresource.xml $(RESOURCES)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $(srcdir)/demo.gresource.xml

RESOURCES =				\
	$(demos)			\
	main.ui				\
	application.ui			\
	demo.ui				\
	menus.ui			\
	stack.ui			\
	revealer.ui			\
	theming.ui			\
	listbox.ui			\
	popover.ui			\
	alphatest.png			\
	apple-red.png			\
	brick.png			\
	brick2.png			\
	background.jpg			\
	floppybuddy.gif			\
	glarea-fragment.glsl		\
	glarea-vertex.glsl		\
	gnome-applets.png		\
	gnome-calendar.png		\
	gnome-foot.png			\
	gnome-fs-directory.png		\
	gnome-fs-regular.png		\
	gnome-gimp.png			\
	gnome-gmush.png			\
	gnome-gsame.png			\
	gnu-keys.png			\
	messages.txt			\
	css_accordion.css		\
	css_basics.css			\
	css_multiplebgs.css		\
	css_pixbufs.css			\
	css_shadows.css			\
	cssview.css			\
	reset.css			\
	alias_cursor.png		\
	all_scroll_cursor.png		\
	cell_cursor.png			\
	col_resize_cursor.png		\
	copy_cursor.png			\
	crosshair_cursor.png		\
	default_cursor.png		\
	e_resize_cursor.png		\
	ew_resize_cursor.png		\
	grabbing_cursor.png		\
	grab_cursor.png			\
	hand_cursor.png			\
	help_cursor.png			\
	move_cursor.png			\
	ne_resize_cursor.png		\
	nesw_resize_cursor.png		\
	none_cursor.png			\
	no_drop_cursor.png		\
	not_allowed_cursor.png		\
	n_resize_cursor.png		\
	ns_resize_cursor.png		\
	nw_resize_cursor.png		\
	nwse_resize_cursor.png		\
	pointer_cursor.png		\
	progress_cursor.png		\
	row_resize_cursor.png		\
	se_resize_cursor.png		\
	s_resize_cursor.png		\
	sw_resize_cursor.png		\
	text_cursor.png			\
	vertical_text_cursor.png	\
	wait_cursor.png			\
	w_resize_cursor.png		\
	zoom_in_cursor.png		\
	zoom_out_cursor.png


iconthemedir = $(datadir)/icons/hicolor

appsicon16dir = $(iconthemedir)/16x16/apps
appsicon22dir = $(iconthemedir)/22x22/apps
appsicon24dir = $(iconthemedir)/24x24/apps
appsicon32dir = $(iconthemedir)/32x32/apps
appsicon48dir = $(iconthemedir)/48x48/apps
appsicon256dir = $(iconthemedir)/256x256/apps
appsiconscalabledir = $(iconthemedir)/scalable/apps

dist_appsicon16_DATA = data/16x16/gtk3-demo.png data/16x16/gtk3-demo-symbolic.symbolic.png
dist_appsicon22_DATA = data/22x22/gtk3-demo.png data/22x22/gtk3-demo-symbolic.symbolic.png
dist_appsicon24_DATA = data/24x24/gtk3-demo.png data/24x24/gtk3-demo-symbolic.symbolic.png
dist_appsicon32_DATA = data/32x32/gtk3-demo.png data/32x32/gtk3-demo-symbolic.symbolic.png
dist_appsicon48_DATA = data/48x48/gtk3-demo.png data/48x48/gtk3-demo-symbolic.symbolic.png
dist_appsicon256_DATA = data/256x256/gtk3-demo.png data/256x256/gtk3-demo-symbolic.symbolic.png

update_icon_cache = gtk-update-icon-cache --ignore-theme-index --force

install-data-hook: install-update-icon-cache
uninstall-hook: uninstall-update-icon-cache

install-update-icon-cache:
	$(AM_V_at)$(POST_INSTALL)
	test -n "$(DESTDIR)" || $(update_icon_cache) "$(iconthemedir)"

uninstall-update-icon-cache:
	$(AM_V_at)$(POST_UNINSTALL)
	test -n "$(DESTDIR)" || $(update_icon_cache) "$(iconthemedir)"

DISTCLEANFILES = demos.h

-include $(top_srcdir)/git.mk
