include $(top_srcdir)/Makefile.decl

EXTRA_DIST +=	\
	README.txt	\
	gtk+.sln	\
	gtk3-prebuild.vcxproj	\
	gdk3-win32.vcxproj	\
	gdk3-win32.vcxproj.filters	\
	gdk-3.vcxproj	\
	gdk-3.vcxproj.filters	\
	gtk-3.vcxproj	\
	gtk-3.vcxproj.filters	\
	gtk-builder-tool.vcxproj		\
	gtk-builder-tool.vcxproj.filters	\
	gtk-encode-symbolic-svg.vcxproj	\
	gtk-encode-symbolic-svg.vcxproj.filters	\
	gtk-query-settings.vcxproj		\
	gtk-query-settings.vcxproj.filters	\
	gtk-update-icon-cache.vcxproj		\
	gtk-update-icon-cache.vcxproj.filters	\
	gtk3-demo.vcxproj	\
	gtk3-demo.vcxproj.filters	\
	gtk3-demo-application.vcxproj	\
	gtk3-demo-application.vcxproj.filters	\
	gtk3-icon-browser.vcxproj	\
	gtk3-icon-browser.vcxproj.filters	\
	gailutil-3.vcxproj	\
	gailutil-3.vcxproj.filters	\
	gtk3-install.vcxproj	\
	gtk3-install.vcxproj.filters	\
	gtk3-introspect.vcxproj		\
	broadwayd.vcxproj	\
	broadwayd.vcxproj.filters	\
	gdk3-broadway.vcxproj	\
	gdk3-broadway.vcxproj.filters	\
	gtk3-build-defines.props	\
	gtk3-copy-gdk-broadway.props	\
	gtk3-ignore-broadway.props	\
	gtk3-install.props	\
	gtk3-version-paths.props

DISTCLEANFILES = $(EXTRA_DIST)

MSVC_VER = 12
MSVC_FORMAT_VER = 12
MSVC_VER_LONG = 2013

include $(top_srcdir)/win32/Makefile-newvs.am

-include $(top_srcdir)/git.mk
