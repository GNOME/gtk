include $(top_srcdir)/Makefile.decl

noinst_LTLIBRARIES = libgtkactors.la

gtkactors_c_sources =			\
	gtkactor.c			\
	gtklayoutmanager.c

gtkactors_private_h_sources =		\
	gtkactorprivate.h		\
	gtklayoutmanagerprivate.h

libgtkactors_la_SOURCES = 		\
	$(gtkactors_c_sources)		\
	$(gtkactors_private_h_sources)

libgtkactors_la_CPPFLAGS = \
	-I$(top_srcdir)			\
	-I$(top_srcdir)/gdk		\
	-I$(top_builddir)/gdk		\
	-I$(top_srcdir)/gtk		\
	-I$(top_builddir)/gtk		\
	-DGTK_VERSION=\"$(GTK_VERSION)\"\
	-DGTK_COMPILATION		\
	$(AM_CPPFLAGS)

libgtkactors_la_CFLAGS = \
	$(GTK_DEP_CFLAGS)	\
	$(GTK_DEBUG_FLAGS)	\
	$(AM_CFLAGS)

libgtkactors_la_LIBADD =  \
	$(GTK_DEP_LIBS)		\
	$(INTLLIBS)

libgtkactors_la_LDFLAGS =    \
        $(LDFLAGS)
		
dist-hook: ../../build/win32/vs9/libgtkactors.vcproj ../../build/win32/vs10/libgtkactors.vcxproj ../../build/win32/vs10/libgtkactors.vcxproj.filters

../../build/win32/vs9/libgtkactors.vcproj: ../../build/win32/vs9/libgtkactors.vcprojin
	for F in $(libgtkactors_la_SOURCES); do \
		case $$F in \
		*.c) echo '   <File RelativePath="..\..\..\gtk\a11y\'$$F'" />' \
		     ;; \
		esac; \
	done >libgtkactors.sourcefiles
	$(CPP) -P - <$(top_srcdir)/build/win32/vs9/libgtkactors.vcprojin >$@
	rm libgtkactors.sourcefiles
	
../../build/win32/vs10/libgtkactors.vcxproj: ../../build/win32/vs10/libgtkactors.vcxprojin
	for F in $(libgtkactors_la_SOURCES); do \
		case $$F in \
		*.c) echo '   <ClCompile Include="..\..\..\gtk\a11y\'$$F'" />' \
		     ;; \
		esac; \
	done >libgtkactors.vs10.sourcefiles
	$(CPP) -P - <$(top_srcdir)/build/win32/vs10/libgtkactors.vcxprojin >$@
	rm libgtkactors.vs10.sourcefiles

../../build/win32/vs10/libgtkactors.vcxproj.filters: ../../build/win32/vs10/libgtkactors.vcxproj.filtersin
	for F in $(libgtkactors_la_SOURCES); do \
		case $$F in \
		*.c) echo '   <ClCompile Include="..\..\..\gtk\a11y\'$$F'"><Filter>Source Files</Filter></ClCompile>' \
		     ;; \
		esac; \
	done >libgtkactors.vs10.sourcefiles.filters
	$(CPP) -P - <$(top_srcdir)/build/win32/vs10/libgtkactors.vcxproj.filtersin >$@
	rm libgtkactors.vs10.sourcefiles.filters


-include $(top_srcdir)/git.mk
