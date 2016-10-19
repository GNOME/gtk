include $(top_srcdir)/Makefile.decl

MSVC_HEADERS_LISTS =	\
	gdk4-win32.headers	\
	gdk4-broadway.headers	\
	gdk-4.headers		\
	gtk-4.headers

GENERATED_ITEMS = \
	gdk4-win32.vcproj		\
	gdk4-broadway.vcproj		\
	broadwayd.vcproj		\
	gdk-4.vcproj			\
	gtk-4.vcproj			\
	gtk4-demo.vcproj		\
	gtk4-demo-application.vcproj	\
	gtk4-icon-browser.vcproj	\
	gtk4-install.vsprops		\
	gtk4-version-paths.vsprops

EXTRA_DIST += \
	README.txt			\
	gtk+-4.sln			\
	gtk4-prebuild.vcproj		\
	gdk4-win32.vcprojin		\
	gdk-4.vcprojin			\
	gtk-4.vcprojin			\
	gtk-builder-tool.vcproj	\
	gtk-encode-symbolic-svg.vcproj	\
	gtk-query-settings.vcproj	\
	gtk-update-icon-cache.vcproj	\
	gtk4-demo.vcprojin		\
	gtk4-demo-application.vcprojin	\
	gtk4-icon-browser.vcprojin	\
	gtk4-install.vcproj		\
	broadwayd.vcprojin		\
	gdk4-broadway.vcprojin		\
	gtk4-build-defines.vsprops	\
	gtk4-copy-gdk-broadway.vsprops	\
	gtk4-gen-srcs.vsprops		\
	gtk4-ignore-broadway.vsprops	\
	gtk4-install.vspropsin		\
	gtk4-version-paths.vsprops.in	\
	$(GENERATED_ITEMS)

gtk4-install.vsprops: $(top_srcdir)/build/win32/vs9/gtk4-install.vspropsin $(MSVC_HEADERS_LISTS)
	$(CPP) -P - <$(top_srcdir)/build/win32/vs9/gtk4-install.vspropsin >$@
	rm $(MSVC_HEADERS_LISTS)

DISTCLEANFILES = $(GENERATED_ITEMS)

-include $(top_srcdir)/git.mk
