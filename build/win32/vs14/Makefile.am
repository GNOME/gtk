include $(top_srcdir)/Makefile.decl

EXTRA_DIST +=	\
	README.txt	\
	gtk+-4.sln	\
	gtk4-prebuild.vcxproj	\
	gtk4-prebuild.vcxproj.filters	\
	gdk4-win32.vcxproj	\
	gdk4-win32.vcxproj.filters	\
	gdk-4.vcxproj	\
	gdk-4.vcxproj.filters	\
	gtk-4.vcxproj	\
	gtk-4.vcxproj.filters	\
	gtk-builder-tool.vcxproj		\
	gtk-builder-tool.vcxproj.filters	\
	gtk-encode-symbolic-svg.vcxproj	\
	gtk-encode-symbolic-svg.vcxproj.filters	\
	gtk-query-settings.vcxproj		\
	gtk-query-settings.vcxproj.filters	\
	gtk-update-icon-cache.vcxproj		\
	gtk-update-icon-cache.vcxproj.filters	\
	gtk4-demo.vcxproj	\
	gtk4-demo.vcxproj.filters	\
	gtk4-demo-application.vcxproj	\
	gtk4-demo-application.vcxproj.filters	\
	gtk4-icon-browser.vcxproj	\
	gtk4-icon-browser.vcxproj.filters	\
	gtk4-install.vcxproj	\
	gtk4-install.vcxproj.filters	\
	broadwayd.vcxproj	\
	broadwayd.vcxproj.filters	\
	gdk4-broadway.vcxproj	\
	gdk4-broadway.vcxproj.filters	\
	gtk4-build-defines.props	\
	gtk4-copy-gdk-broadway.props	\
	gtk4-gen-srcs.props	\
	gtk4-ignore-broadway.props	\
	gtk4-install.props	\
	gtk4-version-paths.props

DISTCLEANFILES = $(EXTRA_DIST)

MSVC_VER = 14
MSVC_FORMAT_VER = 12
MSVC_VER_LONG = 14

include $(top_srcdir)/build/Makefile-newvs.am

-include $(top_srcdir)/git.mk
