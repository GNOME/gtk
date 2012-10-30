include $(top_srcdir)/Makefile.decl

noinst_LTLIBRARIES = libgtka11y.la

gtka11y_c_sources =			\
	gtkaccessibility.c		\
	gtkaccessibilityutil.c		\
	gtkaccessibilitymisc.c 		\
	gtkarrowaccessible.c		\
	gtkbooleancellaccessible.c	\
	gtkboxaccessible.c		\
	gtkbuttonaccessible.c		\
	gtkcellaccessible.c		\
	gtkcellaccessibleparent.c	\
	gtkcheckmenuitemaccessible.c	\
	gtkcolorswatchaccessible.c	\
	gtkcomboboxaccessible.c		\
	gtkcontaineraccessible.c	\
	gtkcontainercellaccessible.c	\
	gtkentryaccessible.c		\
	gtkexpanderaccessible.c		\
	gtkframeaccessible.c		\
	gtkiconviewaccessible.c		\
	gtkimageaccessible.c		\
	gtkimagecellaccessible.c	\
	gtklabelaccessible.c		\
	gtklinkbuttonaccessible.c	\
	gtklockbuttonaccessible.c	\
	gtkmenuaccessible.c		\
	gtkmenushellaccessible.c	\
	gtkmenuitemaccessible.c		\
	gtknotebookaccessible.c		\
	gtknotebookpageaccessible.c	\
	gtkpanedaccessible.c		\
	gtkprogressbaraccessible.c	\
	gtkradiobuttonaccessible.c	\
	gtkradiomenuitemaccessible.c	\
	gtkrangeaccessible.c		\
	gtkrenderercellaccessible.c	\
	gtkscaleaccessible.c		\
	gtkscalebuttonaccessible.c	\
	gtkscrollbaraccessible.c	\
	gtkscrolledwindowaccessible.c	\
	gtkspinbuttonaccessible.c	\
	gtkspinneraccessible.c		\
	gtkstatusbaraccessible.c	\
	gtkswitchaccessible.c		\
	gtktextcellaccessible.c		\
	gtktextviewaccessible.c		\
	gtktogglebuttonaccessible.c	\
	gtktoplevelaccessible.c		\
	gtktreeviewaccessible.c		\
	gtkwidgetaccessible.c		\
	gtkwindowaccessible.c

gtka11yincludedir = $(includedir)/gtk-3.0/gtk/a11y
gtka11yinclude_HEADERS =			\
	gtkarrowaccessible.h		\
	gtkbooleancellaccessible.h	\
	gtkboxaccessible.h		\
	gtkbuttonaccessible.h		\
	gtkcellaccessible.h		\
	gtkcellaccessibleparent.h	\
	gtkcheckmenuitemaccessible.h	\
	gtkcomboboxaccessible.h		\
	gtkcontaineraccessible.h	\
	gtkcontainercellaccessible.h	\
	gtkentryaccessible.h		\
	gtkexpanderaccessible.h		\
	gtkframeaccessible.h		\
	gtkiconviewaccessible.h		\
	gtkimageaccessible.h		\
	gtkimagecellaccessible.h	\
	gtklabelaccessible.h		\
	gtklinkbuttonaccessible.h	\
	gtklockbuttonaccessible.h	\
	gtkmenuaccessible.h		\
	gtkmenushellaccessible.h	\
	gtkmenuitemaccessible.h		\
	gtknotebookaccessible.h		\
	gtknotebookpageaccessible.h	\
	gtkpanedaccessible.h		\
	gtkprogressbaraccessible.h	\
	gtkradiobuttonaccessible.h	\
	gtkradiomenuitemaccessible.h	\
	gtkrangeaccessible.h		\
	gtkrenderercellaccessible.h	\
	gtkscaleaccessible.h		\
	gtkscalebuttonaccessible.h	\
	gtkscrollbaraccessible.h	\
	gtkscrolledwindowaccessible.h	\
	gtkspinbuttonaccessible.h	\
	gtkspinneraccessible.h		\
	gtkstatusbaraccessible.h	\
	gtkswitchaccessible.h		\
	gtktextcellaccessible.h		\
	gtktextviewaccessible.h		\
	gtktogglebuttonaccessible.h	\
	gtktoplevelaccessible.h		\
	gtktreeviewaccessible.h		\
	gtkwidgetaccessible.h		\
	gtkwindowaccessible.h

gtka11y_private_h_sources =		\
	gtkaccessibility.h		\
	gtkaccessibilityutil.h		\
	gtkaccessibilitymisc.h		\
	gtkcolorswatchaccessible.h	\
	gtkcellaccessibleprivate.h	\
	gtklockbuttonaccessibleprivate.h \
	gtkiconviewaccessibleprivate.h	\
	gtktreeviewaccessibleprivate.h	\
	gtktextviewaccessibleprivate.h  \
	gtkwidgetaccessibleprivate.h

libgtka11y_la_SOURCES = 		\
	$(gtka11y_c_sources)		\
	$(gtka11y_private_h_sources)

libgtka11y_la_CPPFLAGS = 		\
	-I$(top_srcdir)			\
	-I$(top_srcdir)/gdk		\
	-I$(top_builddir)/gdk		\
	-I$(top_srcdir)/gtk		\
	-I$(top_builddir)/gtk		\
	-DGTK_VERSION=\"$(GTK_VERSION)\"\
	-DGTK_COMPILATION		\
	$(AM_CPPFLAGS)

libgtka11y_la_CFLAGS = 			\
	$(GTK_DEP_CFLAGS)		\
	$(GTK_DEBUG_FLAGS)		\
	$(AM_CFLAGS)

libgtka11y_la_LIBADD =  		\
	$(GTK_DEP_LIBS)			\
	$(INTLLIBS)

libgtka11y_la_LDFLAGS =    		\
        $(LDFLAGS)

dist-hook: ../../build/win32/vs9/libgail.vcproj ../../build/win32/vs10/libgail.vcxproj ../../build/win32/vs10/libgail.vcxproj.filters

../../build/win32/vs9/libgail.vcproj: ../../build/win32/vs9/libgail.vcprojin
	for F in $(libgail_la_SOURCES); do \
		case $$F in \
		*.c) echo '   <File RelativePath="..\..\..\gtk\a11y\'$$F'" />' \
		     ;; \
		esac; \
	done >libgail.sourcefiles
	$(CPP) -P - <$(top_srcdir)/build/win32/vs9/libgail.vcprojin >$@
	rm libgail.sourcefiles

../../build/win32/vs10/libgail.vcxproj: ../../build/win32/vs10/libgail.vcxprojin
	for F in $(libgail_la_SOURCES); do \
		case $$F in \
		*.c) echo '   <ClCompile Include="..\..\..\gtk\a11y\'$$F'" />' \
		     ;; \
		esac; \
	done >libgail.vs10.sourcefiles
	$(CPP) -P - <$(top_srcdir)/build/win32/vs10/libgail.vcxprojin >$@
	rm libgail.vs10.sourcefiles

../../build/win32/vs10/libgail.vcxproj.filters: ../../build/win32/vs10/libgail.vcxproj.filtersin
	for F in $(libgail_la_SOURCES); do \
		case $$F in \
		*.c) echo '   <ClCompile Include="..\..\..\gtk\a11y\'$$F'"><Filter>Source Files</Filter></ClCompile>' \
		     ;; \
		esac; \
	done >libgail.vs10.sourcefiles.filters
	$(CPP) -P - <$(top_srcdir)/build/win32/vs10/libgail.vcxproj.filtersin >$@
	rm libgail.vs10.sourcefiles.filters


-include $(top_srcdir)/git.mk
