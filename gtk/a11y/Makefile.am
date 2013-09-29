include $(top_srcdir)/Makefile.decl

noinst_LTLIBRARIES = libgtka11y.la

gtka11y_c_sources =			\
	gtkaccessibility.c		\
	gtkaccessibilitymisc.c 		\
	gtkaccessibilityutil.c		\
	gtkarrowaccessible.c		\
	gtkbooleancellaccessible.c	\
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
	gtkflowboxaccessible.c		\
	gtkflowboxchildaccessible.c	\
	gtkframeaccessible.c		\
	gtkiconviewaccessible.c		\
	gtkimageaccessible.c		\
	gtkimagecellaccessible.c	\
	gtklabelaccessible.c		\
	gtklevelbaraccessible.c 	\
	gtklinkbuttonaccessible.c	\
	gtklistboxaccessible.c		\
	gtklistboxrowaccessible.c	\
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
gtka11yinclude_HEADERS =		\
	gtkarrowaccessible.h		\
	gtkbooleancellaccessible.h	\
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
	gtklevelbaraccessible.h 	\
	gtklinkbuttonaccessible.h	\
	gtklistboxaccessible.h		\
	gtklistboxrowaccessible.h	\
	gtklockbuttonaccessible.h	\
	gtkmenuaccessible.h		\
	gtkmenuitemaccessible.h		\
	gtkmenushellaccessible.h	\
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
	gtkaccessibilitymisc.h		\
	gtkaccessibilityutil.h		\
	gtkcellaccessibleprivate.h	\
	gtkcolorswatchaccessibleprivate.h \
	gtkiconviewaccessibleprivate.h	\
	gtklabelaccessibleprivate.h 	\
	gtklistboxaccessibleprivate.h	\
	gtklockbuttonaccessibleprivate.h \
	gtktextviewaccessibleprivate.h  \
	gtktreeviewaccessibleprivate.h	\
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
	$(GDK_HIDDEN_VISIBILITY_CFLAGS) \
	$(GTK_DEP_CFLAGS)		\
	$(GTK_DEBUG_FLAGS)		\
	$(AM_CFLAGS)

libgtka11y_la_LIBADD =  		\
	$(GTK_DEP_LIBS)			\
	$(INTLLIBS)

libgtka11y_la_LDFLAGS =    		\
        $(LDFLAGS)

dist-hook: $(top_builddir)/build/win32/vs9/gtka11y.vcproj $(top_builddir)/build/win32/vs10/gtka11y.vcxproj $(top_builddir)/build/win32/vs10/gtka11y.vcxproj.filters

$(top_builddir)/build/win32/vs9/gtka11y.vcproj: $(top_srcdir)/build/win32/vs9/gtka11y.vcprojin
	for F in $(gtka11y_c_sources); do \
		case $$F in \
		*.c) echo '   <File RelativePath="..\..\..\gtk\a11y\'$$F'" />' \
		     ;; \
		esac; \
	done >gtka11y.sourcefiles
	$(CPP) -P - <$(top_srcdir)/build/win32/vs9/gtka11y.vcprojin >$@
	rm gtka11y.sourcefiles

$(top_builddir)/build/win32/vs10/gtka11y.vcxproj: $(top_srcdir)/build/win32/vs10/gtka11y.vcxprojin
	for F in $(gtka11y_c_sources); do \
		case $$F in \
		*.c) echo '   <ClCompile Include="..\..\..\gtk\a11y\'$$F'" />' \
		     ;; \
		esac; \
	done >gtka11y.vs10.sourcefiles
	$(CPP) -P - <$(top_srcdir)/build/win32/vs10/gtka11y.vcxprojin >$@
	rm gtka11y.vs10.sourcefiles

$(top_builddir)/build/win32/vs10/gtka11y.vcxproj.filters: $(top_srcdir)/build/win32/vs10/gtka11y.vcxproj.filtersin
	for F in $(gtka11y_c_sources); do \
		case $$F in \
		*.c) echo '   <ClCompile Include="..\..\..\gtk\a11y\'$$F'"><Filter>Source Files</Filter></ClCompile>' \
		     ;; \
		esac; \
	done >gtka11y.vs10.sourcefiles.filters
	$(CPP) -P - <$(top_srcdir)/build/win32/vs10/gtka11y.vcxproj.filtersin >$@
	rm gtka11y.vs10.sourcefiles.filters


-include $(top_srcdir)/git.mk
