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
	install.vcxproj

DISTCLEANFILES = $(EXTRA_DIST)

MSVC_SLN = gtk+

MSVC_VER = 11

MSVC_VER_LONG = 2012

include $(top_srcdir)/build/Makefile-newvs.am
