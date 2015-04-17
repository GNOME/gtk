include $(top_srcdir)/Makefile.decl

GENERATED_ITEMS = \
	gdk-win32.vcxproj			\
	gdk-win32.vcxproj.filters		\
	gdk-broadway.vcxproj			\
	gdk-broadway.vcxproj.filters		\
	broadwayd.vcxproj			\
	broadwayd.vcxproj.filters		\
	gdk.vcxproj				\
	gdk.vcxproj.filters			\
	gtk.vcxproj				\
	gtk.vcxproj.filters			\
	gtk3-demo.vcxproj			\
	gtk3-demo.vcxproj.filters		\
	gtk3-demo-application.vcxproj		\
	gtk3-demo-application.vcxproj.filters	\
	gtk3-icon-browser.vcxproj		\
	gtk3-icon-browser.vcxproj.filters	\
	gailutil.vcxproj			\
	gailutil.vcxproj.filters		\
	gtk-install.props			\
	gtk-version-paths.props

EXTRA_DIST += \
	README.txt				\
	gtk+.sln				\
	gtk-prebuild.vcxproj			\
	gdk-win32.vcxprojin			\
	gdk-win32.vcxproj.filtersin		\
	gdk-broadway.vcxprojin			\
	gdk-broadway.vcxproj.filtersin		\
	broadwayd.vcxprojin			\
	broadwayd.vcxproj.filtersin		\
	gdk.vcxprojin				\
	gdk.vcxproj.filtersin			\
	gtk.vcxprojin				\
	gtk.vcxproj.filtersin			\
	gtk-encode-symbolic-svg.vcxproj		\
	gtk-encode-symbolic-svg.vcxproj.filters	\
	gtk3-demo.vcxprojin			\
	gtk3-demo.vcxproj.filtersin		\
	gtk3-demo-application.vcxprojin		\
	gtk3-demo-application.vcxproj.filtersin	\
	gtk3-icon-browser.vcxprojin		\
	gtk3-icon-browser.vcxproj.filtersin	\
	gailutil.vcxprojin			\
	gailutil.vcxproj.filtersin		\
	gtk-install.vcxproj			\
	gtk-build-defines.props			\
	gtk-copy-gdk-broadway.props		\
	gtk-gen-srcs.props			\
	gtk-ignore-broadway.props		\
	gtk-install.propsin			\
	gtk-version-paths.props.in		\
	$(GENERATED_ITEMS)

gtk-install.props: $(top_srcdir)/build/win32/vs10/gtk-install.propsin
	$(CPP) -P - <$(top_srcdir)/build/win32/vs10/gtk-install.propsin >$@
	rm gdk.vs10.headers
	rm gdk-win32.vs10.headers
	rm gdk-broadway.vs10.headers
	rm gtk.vs10.headers
	rm gailutil.vs10.headers

-include $(top_srcdir)/git.mk
