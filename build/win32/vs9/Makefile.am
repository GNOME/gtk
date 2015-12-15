include $(top_srcdir)/Makefile.decl

GENERATED_ITEMS = \
	gdk-win32.vcproj	\
	gdk.vcproj	\
	gtk.vcproj	\
	gailutil.vcproj	\
	gtk-demo.vcproj	\
	libwimp.vcproj	\
	gtk-install.vsprops

EXTRA_DIST +=	\
	README.txt	\
	gtk+.sln	\
	gtk-build-defines.vsprops	\
	gtk-gen-srcs.vsprops	\
	gtk-install.vspropsin	\
	gtk-version-paths.vsprops	\
	gdk-win32.vcprojin	\
	gdk.vcprojin	\
	gtk.vcprojin	\
	gtk-demo.vcprojin	\
	gailutil.vcprojin	\
	libwimp.vcprojin	\
	gtk-update-icon-cache.vcproj	\
	gtk-install.vcproj	\
	$(GENERATED_ITEMS)

MSVC_INTERMEDIATE_FILES = gdk-win32.headers gdk.headers gtk.headers gailutil.headers

gtk-install.vsprops: $(MSVC_INTERMEDIATE_FILES)
	$(CPP) -P - <$(top_srcdir)/build/win32/vs9/gtk-install.vspropsin >$@
	rm $(MSVC_INTERMEDIATE_FILES)

DISTCLEANFILES = $(GENERATED_ITEMS)

