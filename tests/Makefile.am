## Makefile.am for gtk+/tests

INCLUDES = @STRIP_BEGIN@ \
	-I$(top_srcdir) 				\
	-I$(top_builddir)/gdk				\
	-I$(top_srcdir)/gdk				\
	@GTK_DEBUG_FLAGS@				\
	@GTK_DEP_CFLAGS@				\
@STRIP_END@

DEPS = \
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-1.3.la	\
	$(top_builddir)/gdk/@gdktargetlib@ 		\
	$(top_builddir)/gtk/@gtktargetlib@

LDADDS = @STRIP_BEGIN@ 					\
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-1.3.la	\
	$(top_builddir)/gdk/@gdktargetlib@        	\
	$(top_builddir)/gtk/@gtktargetlib@ 		\
@STRIP_END@

if USE_X11
testsocket_programs = testsocket testsocket_child
endif


noinst_PROGRAMS = 		\
	simple			\
	testcalendar		\
	testdnd			\
	testgtk 		\
	testinput		\
	testrgb			\
	testselection 		\
	$(testsocket_programs)	\
	testtext		\
        testtextbuffer		\
	testtreeedit		\
	testtreeview		\
	testtreefocus		\
	testtreeflow		\
	testtreecolumns		\
	testtreesort		\
	treestoretest

simple_DEPENDENCIES = $(TEST_DEPS)
testcalendar_DEPENDENCIES = $(TEST_DEPS)
testdnd_DEPENDENCIES = $(TEST_DEPS)
testgtk_DEPENDENCIES = $(TEST_DEPS)
testinput_DEPENDENCIES = $(TEST_DEPS)
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

simple_LDADD = $(LDADDS)
testcalendar_LDADD = $(LDADDS)
testdnd_LDADD = $(LDADDS)
testgtk_LDADD = $(LDADDS)
testinput_LDADD = $(LDADDS)
testrgb_LDADD = $(LDADDS)
testselection_LDADD = $(LDADDS)
testsocket_LDADD = $(LDADDS)
testsocket_child_LDADD = $(LDADDS)
testtextbuffer_LDADD = $(LDADDS)
testtreeedit_LDADD = $(LDADDS)
testtreeview_LDADD = $(LDADDS)
testtreefocus_LDADD = $(LDADDS)
testtreeflow_LDADD = $(LDADDS)
testtreecolumns_LDADD = $(LDADDS)
testtreesort_LDADD = $(LDADDS)
testtext_LDADD = $(LDADDS)
treestoretest_LDADD = $(LDADDS)

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

testsocket_SOURCES =    	\
	testsocket.c		\
	testsocket_common.c

testsocket_child_SOURCES =    	\
	testsocket_child.c	\
	testsocket_common.c

EXTRA_DIST = @STRIP_BEGIN@ 	\
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
	test.xpm		\
@STRIP_END@


