include $(top_srcdir)/Makefile.decl

bin_PROGRAMS = gtk4-widget-factory

desktopdir = $(datadir)/applications
dist_desktop_DATA = gtk4-widget-factory.desktop

gtk4_widget_factory_SOURCES = 		\
	widget-factory.c		\
	widget_factory_resources.c

BUILT_SOURCES = 			\
	widget_factory_resources.c

gtk4_widget_factory_DEPENDENCIES = 	\
	$(top_builddir)/gtk/libgtk-4.la

gtk4_widget_factory_CPPFLAGS = 		\
	-I$(top_srcdir)			\
	$(GTK_DEBUG_FLAGS)		\
	$(GTK_DEP_CFLAGS)		\
	$(NULL)

gtk4_widget_factory_LDADD = 		\
	$(top_builddir)/gtk/libgtk-4.la \
	$(GTK_DEP_LIBS)			\
	$(NULL)

widget_factory_resources.c: widget-factory.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/widget-factory.gresource.xml)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<


iconthemedir = $(datadir)/icons/hicolor

appsicon16dir = $(iconthemedir)/16x16/apps
appsicon22dir = $(iconthemedir)/22x22/apps
appsicon24dir = $(iconthemedir)/24x24/apps
appsicon32dir = $(iconthemedir)/32x32/apps
appsicon48dir = $(iconthemedir)/48x48/apps
appsicon256dir = $(iconthemedir)/256x256/apps

dist_appsicon16_DATA = data/16x16/gtk4-widget-factory.png data/16x16/gtk4-widget-factory-symbolic.symbolic.png
dist_appsicon22_DATA = data/22x22/gtk4-widget-factory.png data/22x22/gtk4-widget-factory-symbolic.symbolic.png
dist_appsicon24_DATA = data/24x24/gtk4-widget-factory.png data/24x24/gtk4-widget-factory-symbolic.symbolic.png
dist_appsicon32_DATA = data/32x32/gtk4-widget-factory.png data/32x32/gtk4-widget-factory-symbolic.symbolic.png
dist_appsicon48_DATA = data/48x48/gtk4-widget-factory.png data/48x48/gtk4-widget-factory-symbolic.symbolic.png
dist_appsicon256_DATA = data/256x256/gtk4-widget-factory.png data/256x256/gtk4-widget-factory-symbolic.symbolic.png

update_icon_cache = $(top_builddir)/gtk/gtk4-update-icon-cache$(EXEEXT) --ignore-theme-index --force

install-data-hook: install-update-icon-cache
uninstall-hook: uninstall-update-icon-cache

install-update-icon-cache:
	$(AM_V_at)$(POST_INSTALL)
	test -n "$(DESTDIR)" || $(update_icon_cache) "$(iconthemedir)"

uninstall-update-icon-cache:
	$(AM_V_at)$(POST_UNINSTALL)
	test -n "$(DESTDIR)" || $(update_icon_cache) "$(iconthemedir)"

EXTRA_DIST += 				\
	widget-factory.ui 		\
	widget-factory.css		\
	menus.ui 			\
	help-overlay.ui			\
	widget-factory.gresource.xml	\
	data/source.svg			\
	data/symbolic-source.svg

-include $(top_srcdir)/git.mk
