include $(top_srcdir)/Makefile.decl

GENERATED_ITEMS = \
	gdk4-win32.vcxproj			\
	gdk4-win32.vcxproj.filters		\
	gdk-4.vcxproj				\
	gdk-4.vcxproj.filters			\
	gsk-4.vcxproj				\
	gsk-4.vcxproj.filters			\
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

MSVC12_HEADERS_LISTS =	\
	gdk4-win32.vs12.headers		\
	gdk-4.vs12.headers		\
	gsk-4.vs12.headers		\
	gtk-4.vs12.headers

EXTRA_DIST += \
	README.txt				\
	gtk+-4.sln				\
	gtk4-prebuild.vcxproj			\
	gtk4-prebuild.vcxproj.filters		\
	gdk4-win32.vcxprojin			\
	gdk4-win32.vcxproj.filtersin		\
	gdk-4.vcxprojin				\
	gdk-4.vcxproj.filtersin			\
	gsk-4.vcxprojin				\
	gsk-4.vcxproj.filtersin			\
	gtk-4.vcxprojin				\
	gtk-4.vcxproj.filtersin			\
	gtk4-builder-tool.vcxproj		\
	gtk4-builder-tool.vcxproj.filters	\
	gtk4-encode-symbolic-svg.vcxproj	\
	gtk4-encode-symbolic-svg.vcxproj.filters	\
	gtk4-query-settings.vcxproj		\
	gtk4-query-settings.vcxproj.filters	\
	gtk4-update-icon-cache.vcxproj		\
	gtk4-update-icon-cache.vcxproj.filters	\
	gtk4-demo.vcxprojin			\
	gtk4-demo.vcxproj.filtersin		\
	gtk4-demo-application.vcxprojin		\
	gtk4-demo-application.vcxproj.filtersin	\
	gtk4-icon-browser.vcxprojin		\
	gtk4-icon-browser.vcxproj.filtersin	\
	gtk4-install.vcxproj			\
	gtk4-install.vcxproj.filters		\
	gtk4-build-defines.props		\
	gtk4-gen-srcs.props			\
	gtk4-install.propsin			\
	gtk4-version-paths.props.in		\
	$(GENERATED_ITEMS)

DISTCLEANFILES = \
	$(GENERATED_ITEMS)

gtk4-install.props: $(top_srcdir)/win32/vs12/gtk4-install.propsin $(MSVC12_HEADERS_LISTS)
	-$(RM) $(top_builddir)/win32/vs14/gtk4-install.props
	-$(RM) $(top_builddir)/win32/vs15/gtk4-install.props
	$(CPP) -P - <$(top_srcdir)/win32/vs12/gtk4-install.propsin >$@
	rm $(MSVC12_HEADERS_LISTS)

-include $(top_srcdir)/git.mk
