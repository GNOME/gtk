include $(top_srcdir)/Makefile.decl

MSVC_HEADERS_LISTS =	\
	gdk3-win32.headers	\
	gdk3-broadway.headers	\
	gdk-3.headers		\
	gtk-3.headers		\
	gailutil-3.headers

GENERATED_ITEMS = \
	gdk3-win32.vcproj		\
	gdk3-broadway.vcproj		\
	broadwayd.vcproj		\
	gdk-3.vcproj			\
	gtk-3.vcproj			\
	gtk3-demo.vcproj		\
	gtk3-demo-application.vcproj	\
	gtk3-icon-browser.vcproj	\
	gailutil-3.vcproj		\
	gtk3-version-paths.vsprops

EXTRA_DIST += \
	README.txt			\
	gtk+.sln			\
	gtk3-prebuild.vcproj		\
	gdk3-win32.vcprojin		\
	gdk-3.vcprojin			\
	gtk-3.vcprojin			\
	gtk-builder-tool.vcproj	\
	gtk-encode-symbolic-svg.vcproj	\
	gtk-query-settings.vcproj	\
	gtk-update-icon-cache.vcproj	\
	gtk3-demo.vcprojin		\
	gtk3-demo-application.vcprojin	\
	gtk3-icon-browser.vcprojin	\
	gailutil-3.vcprojin		\
	gtk3-install.vcproj		\
	gtk3-introspect.vcproj		\
	broadwayd.vcprojin		\
	gdk3-broadway.vcprojin		\
	gtk3-build-defines.vsprops	\
	gtk3-copy-gdk-broadway.vsprops	\
	gtk3-ignore-broadway.vsprops	\
	gtk3-version-paths.vsprops.in	\
	$(GENERATED_ITEMS)

gtk3-install.vsprops: $(top_srcdir)/win32/vs9/gtk3-install.vspropsin $(MSVC_HEADERS_LISTS)
	$(CPP) -P - <$(top_srcdir)/win32/vs9/gtk3-install.vspropsin >$@
	rm $(MSVC_HEADERS_LISTS)

DISTCLEANFILES = $(GENERATED_ITEMS)

-include $(top_srcdir)/git.mk
