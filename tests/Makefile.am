## Makefile.am for gtk+/tests
include $(top_srcdir)/Makefile.decl

INCLUDES =				\
	-I$(top_srcdir)			\
	-I$(top_builddir)/gdk		\
	-I$(top_srcdir)/gdk		\
	-DGDK_DISABLE_DEPRECATED	\
	-DGTK_DISABLE_DEPRECATED	\
	$(GTK_DEBUG_FLAGS)		\
	$(GTK_DEP_CFLAGS)		\
	$(GDK_DEP_CFLAGS)

DEPS = \
	$(top_builddir)/gtk/libgtk-3.la

LDADDS = \
	$(top_builddir)/gtk/libgtk-3.la	\
	$(top_builddir)/gdk/libgdk-3.la	\
	$(GTK_DEP_LIBS)			\
	-lm

if USE_X11
testsocket_programs = testsocket testsocket_child
endif

noinst_PROGRAMS =  $(TEST_PROGS)	\
	simple				\
	flicker				\
	print-editor			\
	testaccel			\
	testadjustsize			\
	testappchooser			\
	testappchooserbutton		\
	testassistant			\
	testbbox			\
	testbuttons			\
	testcairo			\
	testcalendar			\
	testcombo			\
	testcombochange			\
	testcellrenderertext		\
	testdnd				\
	testellipsise			\
	testentrycompletion 		\
	testentryicons			\
	testfilechooser			\
	testfilechooserbutton		\
	testframe			\
	testgeometry			\
	testgiconpixbuf			\
	testgrid			\
	testgtk				\
	testheightforwidth		\
	testiconview			\
	testiconview-keynav		\
	testicontheme			\
	testimage			\
	testinput			\
	testmenubars			\
	testmountoperation		\
	testmultidisplay		\
	testmultiscreen			\
	testnotebookdnd			\
	testnumerableicon		\
	testnouiprint			\
	testoffscreen			\
	testoffscreenwindow		\
	testorientable			\
	testprint			\
	testrecentchooser 		\
	testrecentchoosermenu		\
	testrichtext			\
	testscale			\
	testselection			\
	$(testsocket_programs)		\
	testspinbutton			\
	teststatusicon			\
	testtoolbar			\
	stresstest-toolbar		\
	testtreeedit			\
	testtreemodel			\
	testtreeview			\
	testtreefocus			\
	testtreeflow			\
	testtreecolumns			\
	testtreecolumnsizing		\
	testtreesort			\
	testverticalcells			\
	treestoretest			\
	testxinerama			\
	testwindows			\
	testmerge			\
	testactions			\
	testgrouping			\
	testtooltips			\
	testexpand			\
	testexpander			\
	testvolumebutton		\
	testscrolledwindow		\
	testswitch			\
	testcellarea			\
	testswitch			\
	styleexamples			\
	testtreemenu			\
	testtoplevelembed		\
	testnoscreen			\
	testtreepos			\
	testsensitive

if USE_X11
noinst_PROGRAMS += testerrors
endif

if HAVE_CXX
noinst_PROGRAMS += autotestkeywords
TESTS = autotestkeywords

autotestkeywords_SOURCES = autotestkeywords.cc
autotestkeywords_CPPFLAGS = -I$(srcdir)/dummy-headers

if HAVE_OBJC
autotestkeywords_CPPFLAGS += -DHAVE_OBJC=1 -x objective-c++
endif

endif

flicker_DEPENDENCIES = $(TEST_DEPS)
simple_DEPENDENCIES = $(TEST_DEPS)
print_editor_DEPENDENCIES = $(TEST_DEPS)
testheightforwidth_DEPENDENCIES = $(TEST_DEPS)
testicontheme_DEPENDENCIES = $(TEST_DEPS)
testiconview_DEPENDENCIES = $(TEST_DEPS)
testaccel_DEPENDENCIES = $(TEST_DEPS)
testadjustsize_DEPENDENCIES = $(TEST_DEPS)
testassistant_DEPENDENCIES = $(TEST_DEPS)
testbbox_DEPENDENCIES = $(TEST_DEPS)
testbuttons_DEPENDENCIES = $(TEST_DEPS)
testcairo_DEPENDENCIES = $(TEST_DEPS)
testcalendar_DEPENDENCIES = $(TEST_DEPS)
testcombo_DEPENDENCIES = $(TEST_DEPS)
testcombochange_DEPENDENCIES = $(TEST_DEPS)
testcellrenderertext_DEPENDENCIES = $(TEST_DEPS)
testdnd_DEPENDENCIES = $(TEST_DEPS)
testellipsise_DEPENDENCIES = $(TEST_DEPS)
testentrycompletion_DEPENDENCIES = $(TEST_DEPS)
testentryicons_DEPENDENCIES = $(TEST_DEPS)
testerrors_DEPENDENCIES = $(TEST_DEPS)
testfilechooser_DEPENDENCIES = $(TEST_DEPS)
testfilechooserbutton_DEPENDENCIES = $(TEST_DEPS)
testframe_DEPENDENCIES = $(TEST_DEPS)
testgeometry_DEPENDENCIES = $(TEST_DEPS)
testgiconpixbuf = $(TEST_DEPS)
testgrid_DEPENDENCIES = $(TEST_DEPS)
testgtk_DEPENDENCIES = $(TEST_DEPS)
testinput_DEPENDENCIES = $(TEST_DEPS)
testimage_DEPENDENCIES = $(TEST_DEPS)
testmenubars_DEPENDENCIES = $(TEST_DEPS)
testmountoperation_DEPENDENCIES = $(TEST_DEPS)
testmultidisplay_DEPENDENCIES = $(TEST_DEPS)
testmultiscreen_DEPENDENCIES = $(TEST_DEPS)
testnotebookdnd_DEPENDENCIES = $(TEST_DEPS)
testnouiprint_DEPENDENCIES = $(TEST_DEPS)
testnumerableicon_DEPENDENCIES = $(TEST_DEPS)
testoffscreen_DEPENDENCIES = $(TEST_DEPS)
testoffscreenwindow_DEPENDENCIES = $(TEST_DEPS)
testappchooser_DEPENDENCIES = $(TEST_DEPS)
testappchooserbutton_DEPENDENCIES = $(TEST_DEPS)
testorientable_DEPENDENCIES = $(TEST_DEPS)
testprint_DEPENDENCIES = $(TEST_DEPS)
testrecentchooser_DEPENDENCIES = $(TEST_DEPS)
testrecentchoosermenu_DEPENDENCIES = $(TEST_DEPS)
testrichtext_DEPENDENCIES = $(TEST_DEPS)
testscale_DEPENDENCIES = $(TEST_DEPS)
testselection_DEPENDENCIES = $(TEST_DEPS)
testsocket_DEPENDENCIES = $(DEPS)
testsocket_child_DEPENDENCIES = $(DEPS)
testspinbutton_DEPENDENCIES = $(TEST_DEPS)
teststatusicon_DEPENDENCIES = $(TEST_DEPS)
testtreeedit_DEPENDENCIES = $(DEPS)
testtreemodel_DEPENDENCIES = $(DEPS)
testtreeview_DEPENDENCIES = $(DEPS)
testtreefocus_DEPENDENCIES = $(DEPS)
testtreeflow_DEPENDENCIES = $(DEPS)
testtreecolumns_DEPENDENCIES = $(DEPS)
testtreecolumnsizing_DEPENDENCIES = $(DEPS)
testtreesort_DEPENDENCIES = $(DEPS)
testverticalcells_DEPENDENCIES = $(DEPS)
treestoretest_DEPENDENCIES = $(TEST_DEPS)
testxinerama_DEPENDENCIES = $(TEST_DEPS)
testmerge_DEPENDENCIES = $(TEST_DEPS)
testactions_DEPENDENCIES = $(TEST_DEPS)
testgrouping_DEPENDENCIES = $(TEST_DEPS)
testtooltips_DEPENDENCIES = $(TEST_DEPS)
testvolumebutton_DEPENDENCIES = $(TEST_DEPS)
testscrolledwindow_DEPENDENCIES = $(TEST_DEPS)
testcellarea_DEPENDENCIES = $(TEST_DEPS)
testtreemenu_DEPENDENCIES = $(TEST_DEPS)
testwindows_DEPENDENCIES = $(TEST_DEPS)
testexpand_DEPENDENCIES = $(TEST_DEPS)
testexpander_DEPENDENCIES = $(TEST_DEPS)
testswitch_DEPENDENCIES = $(TEST_DEPS)
styleexamples_DEPENDENCIES = $(TEST_DEPS)
testtoplevelembed_DEPENDENCIES = $(TEST_DEPS)
testnoscreen_DEPENDENCIES = $(TEST_DEPS)
testtreepos_DEPENDENCIES = $(TEST_DEPS)
testsensitive_DEPENDENCIES = $(TEST_DEPS)

flicker_LDADD = $(LDADDS)
simple_LDADD = $(LDADDS)
print_editor_LDADD = $(LDADDS)
testaccel_LDADD = $(LDADDS)
testadjustsize_LDADD = $(LDADDS)
testassistant_LDADD = $(LDADDS)
testbbox_LDADD = $(LDADDS)
testbuttons_LDADD = $(LDADDS)
testcairo_LDADD = $(LDADDS)
testcalendar_LDADD = $(LDADDS)
testcombo_LDADD = $(LDADDS)
testcombochange_LDADD = $(LDADDS)
testcellrenderertext_LDADD = $(LDADDS)
testdnd_LDADD = $(LDADDS)
testellipsise_LDADD = $(LDADDS)
testentrycompletion_LDADD = $(LDADDS)
testentryicons_LDADD = $(LDADDS)
testerrors_LDADD = $(LDADDS)
testfilechooser_LDADD = $(LDADDS)
testfilechooserbutton_LDADD = $(LDADDS)
testframe_LDADD = $(LDADDS)
testgeometry_LDADD = $(LDADDS)
testgiconpixbuf_LDADD = $(LDADDS)
testgrid_LDADD = $(LDADDS)
testgtk_LDADD = $(LDADDS)
testheightforwidth_LDADD = $(LDADDS)
testicontheme_LDADD = $(LDADDS)
testiconview_LDADD = $(LDADDS)
testiconview_keynav_LDADD = $(LDADDS)
testinput_LDADD = $(LDADDS)
testimage_LDADD = $(LDADDS)
testmenubars_LDADD = $(LDADDS)
testmountoperation_LDADD = $(LDADDS)
testmultidisplay_LDADD = $(LDADDS)
testmultiscreen_LDADD = $(LDADDS)
testnotebookdnd_LDADD = $(LDADDS)
testnouiprint_LDADD = $(LDADDS)
testnumerableicon_LDADD = $(LDADDS)
testoffscreen_LDADD = $(LDADDS)
testoffscreenwindow_LDADD = $(LDADDS)
testappchooser_LDADD = $(LDADDS)
testappchooserbutton_LDADD = $(LDADDS)
testorientable_LDADD = $(LDADDS)
testprint_LDADD = $(LDADDS)
testrecentchooser_LDADD = $(LDADDS)
testrecentchoosermenu_LDADD = $(LDADDS)
testrichtext_LDADD = $(LDADDS)
testscale_LDADD = $(LDADDS)
testselection_LDADD = $(LDADDS)
testsocket_LDADD = $(LDADDS)
testsocket_child_LDADD = $(LDADDS)
testspinbutton_LDADD = $(LDADDS)
teststatusicon_LDADD = $(LDADDS)
testtoolbar_LDADD = $(LDADDS)
stresstest_toolbar_LDADD = $(LDADDS)
testtreeedit_LDADD = $(LDADDS)
testtreemodel_LDADD = $(LDADDS)
testtreeview_LDADD = $(LDADDS)
testtreefocus_LDADD = $(LDADDS)
testtreeflow_LDADD = $(LDADDS)
testtreecolumns_LDADD = $(LDADDS)
testtreecolumnsizing_LDADD = $(LDADDS)
testtreesort_LDADD = $(LDADDS)
testverticalcells_LDADD = $(LDADDS)
treestoretest_LDADD = $(LDADDS)
testxinerama_LDADD = $(LDADDS)
testmerge_LDADD = $(LDADDS)
testactions_LDADD = $(LDADDS)
testgrouping_LDADD = $(LDADDS)
testtooltips_LDADD = $(LDADDS)
testvolumebutton_LDADD = $(LDADDS)
testscrolledwindow_LDADD = $(LDADDS)
testcellarea_LDADD = $(LDADDS)
testtreemenu_LDADD = $(LDADDS)
testwindows_LDADD = $(LDADDS)
testexpand_LDADD = $(LDADDS)
testexpander_LDADD = $(LDADDS)
testswitch_LDADD = $(LDADDS)
styleexamples_LDADD = $(LDADDS)
testtoplevelembed_LDADD = $(LDADDS)
testnoscreen_LDADD = $(LDADDS)
testtreepos_LDADD = $(LDADDS)
testsensitive_LDADD = $(LDADDS)


testentrycompletion_SOURCES = 	\
	prop-editor.c		\
	testentrycompletion.c

testentryicons_SOURCES =	\
	prop-editor.c		\
	testentryicons.c

testfilechooser_SOURCES = 	\
	prop-editor.c		\
	testfilechooser.c

testfilechooserbutton_SOURCES =	\
	prop-editor.c		\
	testfilechooserbutton.c

testgrid_SOURCES =	\
	testgrid.c

testgtk_SOURCES =	\
	prop-editor.c   \
	testgtk.c

testtreeedit_SOURCES = 	\
	testtreeedit.c

testtreemodel_SOURCES = \
	testtreemodel.c

testtreeview_SOURCES =	\
	prop-editor.c	\
	testtreeview.c

testtoolbar_SOURCES =	\
	testtoolbar.c	\
	prop-editor.c

testprint_SOURCES =    	\
	testprint.c		\
	testprintfileoperation.h \
	testprintfileoperation.c

testsocket_SOURCES =    	\
	testsocket.c		\
	testsocket_common.c

testsocket_child_SOURCES =    	\
	testsocket_child.c	\
	testsocket_common.c

testspinbutton_SOURCES =    	\
	testspinbutton.c

teststatusicon_SOURCES =    	\
	prop-editor.c		\
	teststatusicon.c

testmerge_SOURCES = 		\
	testmerge.c

testactions_SOURCES = 		\
	testactions.c

testbbox_SOURCES = 		\
	testbbox.c

testbuttons_SOURCES = 		\
	testbuttons.c

testframe_SOURCES = 		\
	testframe.c

testgeometry_SOURCES = 		\
	testgeometry.c

testgiconpixbuf_SOURCES =	\
	testgiconpixbuf.c

testiconview_SOURCES = 		\
	testiconview.c		\
	prop-editor.c

testiconview_keynav_SOURCES = 	\
	testiconview-keynav.c

testnumerableicon_SOURCES =	\
	testnumerableicon.c	\
	prop-editor.c

testrecentchooser_SOURCES = 	\
	prop-editor.c 		\
	testrecentchooser.c

testgrouping_SOURCES =		\
	testgrouping.c

testtooltips_SOURCES =		\
	testtooltips.c

testrecentchoosermenu_SOURCES =	\
	testrecentchoosermenu.c

testvolumebutton_SOURCES =	\
	testvolumebutton.c

testscrolledwindow_SOURCES =	\
	testscrolledwindow.c

testcellarea_SOURCES =		\
	testcellarea.c		\
	cellareascaffold.c	\
	cellareascaffold.h

testtreemenu_SOURCES =		\
	testtreemenu.c

testoffscreen_SOURCES = 	\
	gtkoffscreenbox.c	\
	gtkoffscreenbox.h	\
	testoffscreen.c

testoffscreenwindow_SOURCES =	\
	testoffscreenwindow.c

testappchooser_SOURCES = \
	testappchooser.c

testappchooserbutton_SOURCES = \
	testappchooserbutton.c

testwindows_SOURCES = 	\
	testwindows.c

testexpand_SOURCES = testexpand.c

testexpander_SOURCES = testexpander.c

testswitch_SOURCES = testswitch.c

styleexamples_SOURCES = styleexamples.c

testtoplevelembed_SOURCES = testtoplevelembed.c

EXTRA_DIST += 			\
	gradient1.png		\
	prop-editor.h		\
	testgtk.1 		\
	testgtk.css		\
	testgtk2.css		\
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
	merge-1.ui		\
	merge-2.ui		\
	merge-3.ui		\
	gnome-textfile.png	\
	makefile.msc


-include $(top_srcdir)/git.mk
