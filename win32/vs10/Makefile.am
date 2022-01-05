include $(top_srcdir)/Makefile.decl

GENERATED_ITEMS = \
	gdk3-win32.vcxproj			\
	gdk3-win32.vcxproj.filters		\
	gdk3-broadway.vcxproj			\
	gdk3-broadway.vcxproj.filters		\
	broadwayd.vcxproj			\
	broadwayd.vcxproj.filters		\
	gdk-3.vcxproj				\
	gdk-3.vcxproj.filters			\
	gtk-3.vcxproj				\
	gtk-3.vcxproj.filters			\
	gtk3-demo.vcxproj			\
	gtk3-demo.vcxproj.filters		\
	gtk3-demo-application.vcxproj		\
	gtk3-demo-application.vcxproj.filters	\
	gtk3-icon-browser.vcxproj		\
	gtk3-icon-browser.vcxproj.filters	\
	gailutil-3.vcxproj			\
	gailutil-3.vcxproj.filters		\
	gtk3-install.props			\
	gtk3-version-paths.props

MSVC10_HEADERS_LISTS =	\
	gdk3-win32.vs10.headers		\
	gdk3-broadway.vs10.headers	\
	gdk-3.vs10.headers		\
	gtk-3.vs10.headers		\
	gailutil-3.vs10.headers

EXTRA_DIST += \
	README.txt				\
	gtk+.sln				\
	gtk3-prebuild.vcxproj			\
	gdk3-win32.vcxprojin			\
	gdk3-win32.vcxproj.filtersin		\
	gdk3-broadway.vcxprojin			\
	gdk3-broadway.vcxproj.filtersin		\
	broadwayd.vcxprojin			\
	broadwayd.vcxproj.filtersin		\
	gdk-3.vcxprojin				\
	gdk-3.vcxproj.filtersin			\
	gtk-3.vcxprojin				\
	gtk-3.vcxproj.filtersin			\
	gtk-builder-tool.vcxproj		\
	gtk-builder-tool.vcxproj.filters	\
	gtk-encode-symbolic-svg.vcxproj		\
	gtk-encode-symbolic-svg.vcxproj.filters	\
	gtk-query-settings.vcxproj		\
	gtk-query-settings.vcxproj.filters	\
	gtk-update-icon-cache.vcxproj		\
	gtk-update-icon-cache.vcxproj.filters	\
	gtk3-demo.vcxprojin			\
	gtk3-demo.vcxproj.filtersin		\
	gtk3-demo-application.vcxprojin		\
	gtk3-demo-application.vcxproj.filtersin	\
	gtk3-icon-browser.vcxprojin		\
	gtk3-icon-browser.vcxproj.filtersin	\
	gailutil-3.vcxprojin			\
	gailutil-3.vcxproj.filtersin		\
	gtk3-install.vcxproj			\
	gtk3-install.vcxproj.filters		\
	gtk3-introspect.vcxproj			\
	gtk3-build-defines.props		\
	gtk3-copy-gdk-broadway.props		\
	gtk3-ignore-broadway.props		\
	gtk3-install.propsin			\
	gtk3-version-paths.props.in		\
	$(GENERATED_ITEMS)

DISTCLEANFILES = \
	$(GENERATED_ITEMS)

gtk3-install.props: $(top_srcdir)/win32/vs10/gtk3-install.propsin $(MSVC10_HEADERS_LISTS)
	-$(RM) $(top_builddir)/win32/vs11/gtk3-install.props
	-$(RM) $(top_builddir)/win32/vs12/gtk3-install.props
	-$(RM) $(top_builddir)/win32/vs14/gtk3-install.props
	-$(RM) $(top_builddir)/win32/vs15/gtk3-install.props
	-$(RM) $(top_builddir)/win32/vs16/gtk3-install.props
	$(CPP) -P - <$(top_srcdir)/win32/vs10/gtk3-install.propsin >$@
	rm $(MSVC10_HEADERS_LISTS)

-include $(top_srcdir)/git.mk
