include $(top_srcdir)/Makefile.decl

EXTRA_DIST +=	\
	README.txt	\
	gtk+.sln	\
	gtk-build-defines.props	\
	gtk-gen-srcs.props	\
	gtk-install.props	\
	gtk-version-paths.props	\
	gdk-win32.vcxproj	\
	gdk-win32.vcxproj.filters	\
	gdk.vcxproj	\
	gdk.vcxproj.filters	\
	gtk.vcxproj	\
	gtk.vcxproj.filters	\
	gtk-demo.vcxproj	\
	gtk-demo.vcxproj.filters	\
	gailutil.vcxproj	\
	gailutil.vcxproj.filters	\
	libwimp.vcxproj	\
	libwimp.vcxproj.filters	\
	gtk-update-icon-cache.vcxproj	\
	gtk-update-icon-cache.vcxproj.filters	\
	gtk-install.vcxproj	\
	gtk-install.vcxproj.filters

DISTCLEANFILES = $(EXTRA_DIST)

MSVC_VER = 16
MSVC_VER_LONG = 16
MSVC_TOOLSET = 142
MSVC_FORMAT_VER = 12

include $(top_srcdir)/build/Makefile-newvs.am
