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
	gsk-4.vcxproj	\
	gsk-4.vcxproj.filters	\
	gtk-4.vcxproj	\
	gtk-4.vcxproj.filters	\
	gtk4-builder-tool.vcxproj		\
	gtk4-builder-tool.vcxproj.filters	\
	gtk4-encode-symbolic-svg.vcxproj	\
	gtk4-encode-symbolic-svg.vcxproj.filters	\
	gtk4-query-settings.vcxproj		\
	gtk4-query-settings.vcxproj.filters	\
	gtk4-update-icon-cache.vcxproj		\
	gtk4-update-icon-cache.vcxproj.filters	\
	gtk4-demo.vcxproj	\
	gtk4-demo.vcxproj.filters	\
	gtk4-demo-application.vcxproj	\
	gtk4-demo-application.vcxproj.filters	\
	gtk4-icon-browser.vcxproj	\
	gtk4-icon-browser.vcxproj.filters	\
	gtk4-install.vcxproj	\
	gtk4-install.vcxproj.filters	\
	gtk4-build-defines.props	\
	gtk4-gen-srcs.props	\
	gtk4-install.props	\
	gtk4-version-paths.props

DISTCLEANFILES = $(EXTRA_DIST)

MSVC_BASE_VER = 12
MSVC_BASE_VER_LONG = 2013
MSVC_VER = 15
MSVC_VER_LONG = 15
MSVC_TOOLSET = 141

include $(top_srcdir)/win32/Makefile-newvs.am

-include $(top_srcdir)/git.mk
