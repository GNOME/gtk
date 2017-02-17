include $(top_srcdir)/Makefile.decl

GENERATED_ITEMS = \
	gdk-win32.vcxproj	\
	gdk.vcxproj		\
	gtk.vcxproj		\
	gailutil.vcxproj	\
	libwimp.vcxproj		\
	gtk-demo.vcxproj	\
	gdk-win32.vcxproj.filters	\
	gdk.vcxproj.filters	\
	gtk.vcxproj.filters	\
	gailutil.vcxproj.filters	\
	libwimp.vcxproj.filters	\
	gtk-demo.vcxproj.filters	\
	gtk-install.props

EXTRA_DIST +=	\
	README.txt	\
	gtk+.sln	\
	gtk-build-defines.props	\
	gtk-gen-srcs.props	\
	gtk-install.propsin	\
	gtk-version-paths.props	\
	gdk-win32.vcxprojin	\
	gdk-win32.vcxproj.filtersin	\
	gdk.vcxprojin	\
	gdk.vcxproj.filtersin	\
	gtk.vcxprojin	\
	gtk.vcxproj.filtersin	\
	gtk-demo.vcxprojin	\
	gtk-demo.vcxproj.filtersin	\
	gailutil.vcxprojin	\
	gailutil.vcxproj.filtersin	\
	libwimp.vcxprojin	\
	libwimp.vcxproj.filtersin	\
	gtk-update-icon-cache.vcxproj	\
	gtk-update-icon-cache.vcxproj.filters	\
	gtk-install.vcxproj	\
	gtk-install.vcxproj.filters	\
	$(GENERATED_ITEMS)

MSVC_INTERMEDIATE_FILES = gdk-win32.vs10.headers gdk.vs10.headers gtk.vs10.headers gailutil.vs10.headers

gtk-install.props: $(MSVC_INTERMEDIATE_FILES)
	-$(RM) $(top_builddir)/build/win32/vs11/gtk-install.props
	-$(RM) $(top_builddir)/build/win32/vs12/gtk-install.props
	-$(RM) $(top_builddir)/build/win32/vs14/gtk-install.props
	-$(RM) $(top_builddir)/build/win32/vs15/gtk-install.props
	$(CPP) -P - <$(top_srcdir)/build/win32/vs10/gtk-install.propsin >$@
	rm $(MSVC_INTERMEDIATE_FILES)

DISTCLEANFILES = $(GENERATED_ITEMS)
