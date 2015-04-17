include $(top_srcdir)/Makefile.decl

GENERATED_ITEMS = \
	gdk-win32.vcproj		\
	gdk-broadway.vcproj		\
	broadwayd.vcproj		\
	gdk.vcproj			\
	gtk.vcproj			\
	gtk3-demo.vcproj		\
	gtk3-demo-application.vcproj	\
	gtk3-icon-browser.vcproj	\
	gailutil.vcproj			\
	gtk-install.vsprops		\
	gtk-version-paths.vsprops

EXTRA_DIST += \
	README.txt			\
	gtk+.sln			\
	gtk-prebuild.vcproj		\
	gdk-win32.vcprojin		\
	gdk.vcprojin			\
	gtk.vcprojin			\
	gtk-encode-symbolic-svg.vcproj	\
	gtk3-demo.vcprojin		\
	gtk3-demo-application.vcprojin	\
	gtk3-icon-browser.vcprojin	\
	gailutil.vcprojin		\
	gtk-install.vcproj		\
	broadwayd.vcprojin		\
	gdk-broadway.vcprojin		\
	gtk-build-defines.vsprops	\
	gtk-copy-gdk-broadway.vsprops	\
	gtk-gen-srcs.vsprops		\
	gtk-ignore-broadway.vsprops	\
	gtk-install.vspropsin		\
	gtk-version-paths.vsprops.in	\
	$(GENERATED_ITEMS)

gtk-install.vsprops: $(top_srcdir)/build/win32/vs9/gtk-install.vspropsin
	$(CPP) -P - <$(top_srcdir)/build/win32/vs9/gtk-install.vspropsin >$@
	rm gdk.headers
	rm gdk-win32.headers
	rm gdk-broadway.headers
	rm gtk.headers
	rm gailutil.headers

DISTCLEANFILES = $(GENERATED_ITEMS)

-include $(top_srcdir)/git.mk
