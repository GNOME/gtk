include $(top_srcdir)/Makefile.decl

GENERATED_ITEMS = \
	gdk4-win32.vcxproj			\
	gdk4-win32.vcxproj.filters		\
	gdk4-broadway.vcxproj			\
	gdk4-broadway.vcxproj.filters		\
	broadwayd.vcxproj			\
	broadwayd.vcxproj.filters		\
	gdk-4.vcxproj				\
	gdk-4.vcxproj.filters			\
	gtk-4.vcxproj				\
	gtk-4.vcxproj.filters			\
	gtk4-demo.vcxproj			\
	gtk4-demo.vcxproj.filters		\
	gtk4-demo-application.vcxproj		\
	gtk4-demo-application.vcxproj.filters	\
	gtk4-icon-browser.vcxproj		\
	gtk4-icon-browser.vcxproj.filters	\
	gtk4-install.props			\
	gtk4-version-paths.props

MSVC10_HEADERS_LISTS =	\
	gdk4-win32.vs10.headers		\
	gdk4-broadway.vs10.headers	\
	gdk-4.vs10.headers		\
	gtk-4.vs10.headers

EXTRA_DIST += \
	README.txt				\
	gtk+-4.sln				\
	gtk4-prebuild.vcxproj			\
	gtk4-prebuild.vcxproj.filters		\
	gdk4-win32.vcxprojin			\
	gdk4-win32.vcxproj.filtersin		\
	gdk4-broadway.vcxprojin			\
	gdk4-broadway.vcxproj.filtersin		\
	broadwayd.vcxprojin			\
	broadwayd.vcxproj.filtersin		\
	gdk-4.vcxprojin				\
	gdk-4.vcxproj.filtersin			\
	gtk-4.vcxprojin				\
	gtk-4.vcxproj.filtersin			\
	gtk-builder-tool.vcxproj		\
	gtk-builder-tool.vcxproj.filters	\
	gtk-encode-symbolic-svg.vcxproj		\
	gtk-encode-symbolic-svg.vcxproj.filters	\
	gtk-query-settings.vcxproj		\
	gtk-query-settings.vcxproj.filters	\
	gtk-update-icon-cache.vcxproj		\
	gtk-update-icon-cache.vcxproj.filters	\
	gtk4-demo.vcxprojin			\
	gtk4-demo.vcxproj.filtersin		\
	gtk4-demo-application.vcxprojin		\
	gtk4-demo-application.vcxproj.filtersin	\
	gtk4-icon-browser.vcxprojin		\
	gtk4-icon-browser.vcxproj.filtersin	\
	gtk4-install.vcxproj			\
	gtk4-install.vcxproj.filters		\
	gtk4-build-defines.props		\
	gtk4-copy-gdk-broadway.props		\
	gtk4-gen-srcs.props			\
	gtk4-ignore-broadway.props		\
	gtk4-install.propsin			\
	gtk4-version-paths.props.in		\
	$(GENERATED_ITEMS)

DISTCLEANFILES = \
	$(GENERATED_ITEMS)

gtk4-install.props: $(top_srcdir)/build/win32/vs10/gtk4-install.propsin $(MSVC10_HEADERS_LISTS)
	$(CPP) -P - <$(top_srcdir)/build/win32/vs10/gtk4-install.propsin >$@
	rm $(MSVC10_HEADERS_LISTS)

-include $(top_srcdir)/git.mk
