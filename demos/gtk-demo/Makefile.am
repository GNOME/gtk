## Makefile.am for gtk+/demos
include $(top_srcdir)/Makefile.decl
include $(srcdir)/demos-sources.mak

demos_opt =

if BUILD_FONT_DEMO
demos_opt += $(font_features_demo)
endif

if OS_UNIX
demos_opt += $(page_setup_demo)
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
	$(resource_files)		\
	makefile.msc.in			\
	org.gtk.Demo.gschema.xml	\
	demos.h.win32	\
	meson.build	\
	geninclude.py

gsettings_SCHEMAS = \
	org.gtk.Demo.gschema.xml

@GSETTINGS_RULES@

demos.h: $(demos) geninclude.pl
	 $(AM_V_GEN) (here=`pwd` ; cd $(srcdir) && $(PERL) $$here/geninclude.pl $(demos)) > demos.h

demos.h.win32: $(demos_base) geninclude.pl
	 $(AM_V_GEN) (here=`pwd` ; cd $(srcdir) && $(PERL) $$here/geninclude.pl $(demos_base)) > demos.h.win32

nodist_gtk3_demo_SOURCES = demos.h demo_resources.c

gtk3_demo_SOURCES = 		\
	$(demos)		\
	gtkfishbowl.c		\
	gtkfishbowl.h		\
	main.c

gtk3_demo_DEPENDENCIES = $(DEPS)
gtk3_demo_LDADD = $(LDADDS)
gtk3_demo_LDFLAGS = -export-dynamic

nodist_gtk3_demo_application_SOURCES = demo_resources.c

gtk3_demo_application_SOURCES = application.c

gtk3_demo_application_LDADD = $(LDADDS)

resource_files = $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/demo.gresource.xml)

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

dist_appsicon16_DATA = data/16x16/gtk3-demo.png data/16x16/gtk3-demo-symbolic.symbolic.png
dist_appsicon22_DATA = data/22x22/gtk3-demo.png data/22x22/gtk3-demo-symbolic.symbolic.png
dist_appsicon24_DATA = data/24x24/gtk3-demo.png data/24x24/gtk3-demo-symbolic.symbolic.png
dist_appsicon32_DATA = data/32x32/gtk3-demo.png data/32x32/gtk3-demo-symbolic.symbolic.png
dist_appsicon48_DATA = data/48x48/gtk3-demo.png data/48x48/gtk3-demo-symbolic.symbolic.png
dist_appsicon256_DATA = data/256x256/gtk3-demo.png data/256x256/gtk3-demo-symbolic.symbolic.png

update_icon_cache = $(top_builddir)/gtk/gtk-update-icon-cache$(EXEEXT) --ignore-theme-index --force

install-data-hook: install-update-icon-cache
uninstall-hook: uninstall-update-icon-cache

install-update-icon-cache:
	$(AM_V_at)$(POST_INSTALL)
	test -n "$(DESTDIR)" || $(update_icon_cache) "$(iconthemedir)"

uninstall-update-icon-cache:
	$(AM_V_at)$(POST_UNINSTALL)
	test -n "$(DESTDIR)" || $(update_icon_cache) "$(iconthemedir)"

# ------------------- MSVC Build Items ----------------
MSVCPROJS = gtk3-demo gtk3-demo-application

gtk3_demo_FILES = $(gtk3_demo_SOURCES)
gtk3_demo_EXCLUDES = font_features.c|pagesetup.c

gtk3_demo_application_FILES = $(gtk3_demo_application_SOURCES)
gtk3_demo_application_EXCLUDES = dummy

include $(top_srcdir)/win32/Makefile.msvcproj

dist-hook: \
	$(top_builddir)/win32/vs9/gtk3-demo.vcproj	\
	$(top_builddir)/win32/vs9/gtk3-demo-application.vcproj

DISTCLEANFILES = demos.h demos.h.win32

-include $(top_srcdir)/git.mk
