## Makefile.am for gtk+/tests

INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_builddir)/gdk			\
	-I$(top_srcdir)/gdk			\
	-DG_DISABLE_DEPRECATED			\
	-DGDK_PIXBUF_DISABLE_DEPRECATED		\
	-DGDK_DISABLE_DEPRECATED		\
	-DGTK_DISABLE_DEPRECATED		\
	$(GTK_DEBUG_FLAGS)			\
	$(GTK_DEP_CFLAGS)

DEPS =									\
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la	\
	$(top_builddir)/gdk/$(gdktargetlib)				\
	$(top_builddir)/gtk/$(gtktargetlib)

LDADDS =								\
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la	\
	$(top_builddir)/gdk/$(gdktargetlib)				\
	$(top_builddir)/gtk/$(gtktargetlib)

if USE_X11
testsocket_programs = testsocket testsocket_child
endif


noinst_PROGRAMS =			\
	simple				\
	testcalendar			\
	testdnd				\
	testgtk				\
	testicontheme			\
	testinput			\
	testmenus			\
	testmultidisplay		\
	testmultiscreen			\
	testrgb				\
	testselection			\
	$(testsocket_programs)		\
	testtext			\
        testtextbuffer			\
	testtoolbar			\
	stresstest-toolbar		\
	testtreeedit			\
	testtreeview			\
	testtreefocus			\
	testtreeflow			\
	testtreecolumns			\
	testtreesort			\
	treestoretest			\
	testxinerama			\
	pixbuf-read			\
	pixbuf-lowmem			\
	pixbuf-randomly-modified	\
	pixbuf-random			

simple_DEPENDENCIES = $(TEST_DEPS)
testicontheme_DEPENDENCIES = $(TEST_DEPS)
testcalendar_DEPENDENCIES = $(TEST_DEPS)
testdnd_DEPENDENCIES = $(TEST_DEPS)
testgtk_DEPENDENCIES = $(TEST_DEPS)
testinput_DEPENDENCIES = $(TEST_DEPS)
testmenus_DEPENDENCIES = $(TEST_DEPS)
testmultidisplay_DEPENDENCIES = $(TEST_DEPS)
testmultiscreen_DEPENDENCIES = $(TEST_DEPS)
testrgb_DEPENDENCIES = $(TEST_DEPS)
testselection_DEPENDENCIES = $(TEST_DEPS)
testsocket_DEPENDENCIES = $(DEPS)
testsocket_child_DEPENDENCIES = $(DEPS)
testtext_DEPENDENCIES = $(TEST_DEPS)
testtextbuffer_DEPENDENCIES = $(TEST_DEPS)
testtreeedit_DEPENDENCIES = $(DEPS)
testtreeview_DEPENDENCIES = $(DEPS)
testtreefocus_DEPENDENCIES = $(DEPS)
testtreeflow_DEPENDENCIES = $(DEPS)
testtreecolumns_DEPENDENCIES = $(DEPS)
testtreesort_DEPENDENCIES = $(DEPS)
treestoretest_DEPENDENCIES = $(TEST_DEPS)
testxinerama_DEPENDENCIES = $(TEST_DEPS)

simple_LDADD = $(LDADDS)
testcalendar_LDADD = $(LDADDS)
testdnd_LDADD = $(LDADDS)
testgtk_LDADD = $(LDADDS)
testicontheme_LDADD = $(LDADDS)
testinput_LDADD = $(LDADDS)
testmenus_LDADD = $(LDADDS)
testmultidisplay_LDADD = $(LDADDS)
testmultiscreen_LDADD = $(LDADDS)
testrgb_LDADD = $(LDADDS)
testselection_LDADD = $(LDADDS)
testsocket_LDADD = $(LDADDS)
testsocket_child_LDADD = $(LDADDS)
testtextbuffer_LDADD = $(LDADDS)
testtoolbar_LDADD = $(LDADDS)
stresstest_toolbar_LDADD = $(LDADDS)
testtreeedit_LDADD = $(LDADDS)
testtreeview_LDADD = $(LDADDS)
testtreefocus_LDADD = $(LDADDS)
testtreeflow_LDADD = $(LDADDS)
testtreecolumns_LDADD = $(LDADDS)
testtreesort_LDADD = $(LDADDS)
testtext_LDADD = $(LDADDS)
treestoretest_LDADD = $(LDADDS)
testxinerama_LDADD = $(LDADDS)
pixbuf_read_LDADD = $(LDADDS)
pixbuf_lowmem_LDADD = $(LDADDS)
pixbuf_randomly_modified_LDADD = $(LDADDS)
pixbuf_random_LDADD = $(LDADDS)

testgtk_SOURCES =	\
	prop-editor.c   \
	testgtk.c 	

testtreeedit_SOURCES = 	\
	testtreeedit.c

testtreeview_SOURCES =	\
	prop-editor.c	\
	testtreeview.c 

testtext_SOURCES =	\
	prop-editor.c	\
	testtext.c 

testtoolbar_SOURCES =	\
	testtoolbar.c	\
	prop-editor.c

testsocket_SOURCES =    	\
	testsocket.c		\
	testsocket_common.c

testsocket_child_SOURCES =    	\
	testsocket_child.c	\
	testsocket_common.c

EXTRA_DIST = 			\
	prop-editor.h		\
	testgtk.1 		\
	testgtkrc 		\
	testgtkrc2 		\
	circles.xbm		\
	3DRings.xpm		\
	FilesQueue.xpm		\
	Modeller.xpm		\
	check-y.xpm		\
	check-n.xpm		\
	marble.xpm		\
	test.xpm		\
	check-y.xpm		\
	check-n.xpm		\
	test.xpm
