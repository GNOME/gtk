## Makefile.am for gtk+/tests
include $(top_srcdir)/Makefile.decl

SUBDIRS = visuals

AM_CPPFLAGS =				\
	-I$(top_srcdir)			\
	-I$(top_builddir)/gdk		\
	-I$(top_srcdir)/gdk		\
	$(GTK_DEBUG_FLAGS)		\
	$(GTK_DEP_CFLAGS)		\
	$(GDK_DEP_CFLAGS)

DEPS = \
	$(top_builddir)/gtk/libgtk-3.la

LDADD = \
	$(top_builddir)/gtk/libgtk-3.la	\
	$(top_builddir)/gdk/libgdk-3.la	\
	$(GTK_DEP_LIBS)			\
	$(GDK_DEP_LIBS)			\
	-lm


if USE_X11
testsocket_programs = testsocket testsocket_child
endif

noinst_PROGRAMS =  $(TEST_PROGS)	\
	overlayscroll			\
	syncscroll			\
	subsurface			\
	animated-resizing		\
	animated-revealing		\
	motion-compression		\
	scrolling-performance		\
	simple				\
	flicker				\
	print-editor			\
	video-timer			\
	testaccel			\
	testadjustsize			\
	testappchooser			\
	testappchooserbutton		\
	testassistant			\
	testbaseline			\
	testbbox			\
	testbox                         \
	testboxcss                      \
	testbuttons			\
	testcairo			\
	testcalendar			\
	testclipboard			\
	testcolorchooser		\
	testcombo			\
	testcombochange			\
	testcellrenderertext		\
	testdialog			\
	testdnd				\
	testellipsise			\
	testemblems			\
	testentrycompletion 		\
	testentryicons			\
	testfilechooser			\
	testfilechooserbutton		\
	testflowbox			\
	testfontselection		\
	testfontselectiondialog		\
	testfontchooser			\
	testfontchooserdialog 		\
	testframe			\
	testfullscreen			\
	testgeometry			\
	testgiconpixbuf			\
	testglarea			\
	testglblending			\
	testgrid			\
	testgtk				\
	testheaderbar			\
	testheightforwidth		\
	testiconview			\
	testiconview-keynav		\
	testicontheme			\
	testimage			\
	testinput			\
	testkineticscrolling		\
	testlist			\
	testlist2			\
	testlist3			\
	testlevelbar			\
	testlockbutton			\
	testmenubutton			\
	testmountoperation		\
	testmultidisplay		\
	testnotebookdnd			\
	testnumerableicon		\
	testnouiprint			\
	testoffscreen			\
	testoffscreenwindow		\
	testorientable			\
	testoverlay			\
	testoverlaystyleclass		\
	testprint			\
	testrecentchooser 		\
	testrecentchoosermenu		\
	testrichtext			\
	testscale			\
	testselection			\
	testselectionmode		\
	$(testsocket_programs)		\
	testspinbutton			\
	teststatusicon			\
	teststockbuttonmenu		\
	testtoolbar			\
	testtoolbar2			\
	stresstest-toolbar		\
	testtreechanging                \
	testtreeedit			\
	testtreemodel			\
	testtreeview			\
	testtreefocus			\
	testtreeflow			\
	testtreecolumns			\
	testtreecolumnsizing		\
	testtreesort			\
	testverticalcells		\
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
	testscrolledge			\
	testswitch			\
	testcellarea			\
	testswitch			\
	styleexamples			\
	testtreemenu			\
	testtoplevelembed		\
	testnoscreen			\
	testtreepos			\
	testsensitive			\
	testtextview			\
	testanimation			\
	testpixbuf-save			\
	testpixbuf-color		\
	testpixbuf-scale		\
	testgmenu			\
	testlogout			\
	teststack			\
	testrevealer			\
	testtitlebar			\
	testsplitheaders		\
	testactionbar			\
	testwindowsize			\
	testpopover			\
	gdkgears			\
	$(NULL)

if USE_X11
noinst_PROGRAMS += testerrors
endif

if HAVE_CXX

AM_CXXFLAGS = $(AM_CPPFLAGS)

noinst_PROGRAMS += autotestkeywords
TESTS = autotestkeywords

autotestkeywords_SOURCES = autotestkeywords.cc
autotestkeywords_CPPFLAGS = -I$(srcdir)/dummy-headers

if HAVE_OBJC
autotestkeywords_CPPFLAGS += -DHAVE_OBJC=1 -x objective-c++
endif

endif

animated_resizing_DEPENDENCIES = $(TEST_DEPS)
animated_revealing_DEPENDENCIES = $(TEST_DEPS)
flicker_DEPENDENCIES = $(TEST_DEPS)
motion_compression_DEPENDENCIES = $(TEST_DEPS)
scrolling_performance_DEPENDENCIES = $(TEST_DEPS)
simple_DEPENDENCIES = $(TEST_DEPS)
print_editor_DEPENDENCIES = $(TEST_DEPS)
video_timer_DEPENDENCIES = $(TEST_DEPS)
testheightforwidth_DEPENDENCIES = $(TEST_DEPS)
testicontheme_DEPENDENCIES = $(TEST_DEPS)
testiconview_DEPENDENCIES = $(TEST_DEPS)
testaccel_DEPENDENCIES = $(TEST_DEPS)
testadjustsize_DEPENDENCIES = $(TEST_DEPS)
testassistant_DEPENDENCIES = $(TEST_DEPS)
testbaseline_DEPENDENCIES = $(TEST_DEPS)
testbbox_DEPENDENCIES = $(TEST_DEPS)
testbuttons_DEPENDENCIES = $(TEST_DEPS)
testcairo_DEPENDENCIES = $(TEST_DEPS)
testcalendar_DEPENDENCIES = $(TEST_DEPS)
testclipboard_DEPENDENCIES = $(TEST_DEPS)
testcolorchooser_DEPENDENCIES = $(TEST_DEPS)
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
testflowbox_DEPENDENCIES = $(TEST_DEPS)
testfontselection_DEPENDENCIES = $(TEST_DEPS)
testfontselectiondialog_DEPENDENCIES = $(TEST_DEPS)
testfontchooser_DEPENDENCIES = $(TEST_DEPS)
testfontchooserdialog_DEPENDENCIES = $(TEST_DEPS)
testframe_DEPENDENCIES = $(TEST_DEPS)
testgeometry_DEPENDENCIES = $(TEST_DEPS)
testgiconpixbuf = $(TEST_DEPS)
testgrid_DEPENDENCIES = $(TEST_DEPS)
testgtk_DEPENDENCIES = $(TEST_DEPS)
testheaderbar_DEPENDENCIES = $(TEST_DEPS)
testinput_DEPENDENCIES = $(TEST_DEPS)
testimage_DEPENDENCIES = $(TEST_DEPS)
testkineticscrolling_DEPENDENCIES = $(TEST_DEPS)
testlist_DEPENDENCIES = $(TEST_DEPS)
testlist2_DEPENDENCIES = $(TEST_DEPS)
testlevelbar_DEPENDENCIES = $(TEST_DEPS)
testlockbutton_DEPENDENCIES = $(TEST_DEPS)
testmenubutton_DEPENDENCIES = $(TEST_DEPS)
testmountoperation_DEPENDENCIES = $(TEST_DEPS)
testmultidisplay_DEPENDENCIES = $(TEST_DEPS)
testnotebookdnd_DEPENDENCIES = $(TEST_DEPS)
testnouiprint_DEPENDENCIES = $(TEST_DEPS)
testnumerableicon_DEPENDENCIES = $(TEST_DEPS)
testoffscreen_DEPENDENCIES = $(TEST_DEPS)
testoffscreenwindow_DEPENDENCIES = $(TEST_DEPS)
testappchooser_DEPENDENCIES = $(TEST_DEPS)
testappchooserbutton_DEPENDENCIES = $(TEST_DEPS)
testorientable_DEPENDENCIES = $(TEST_DEPS)
testoverlay_DEPENDENCIES = $(TEST_DEPS)
testoverlaystyleclass_DEPENDENCIES = $(TEST_DEPS)
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
teststockbuttonmenu_DEPENDENCIES = $(TEST_DEPS)
testtreechanging_DEPENDENCIES =	$(DEPS)
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
testscrolledge_DEPENDENCIES = $(TEST_DEPS)
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
testtextview_DEPENDENCIES = $(TEST_DEPS)
testanimation_DEPENDENCIES = $(TEST_DEPS)
testpixbuf_save_DEPENDENCIES = $(TEST_DEPS)
testpixbuf_color_DEPENDENCIES = $(TEST_DEPS)
testpixbuf_scale_DEPENDENCIES = $(TEST_DEPS)
testgmenu_DEPENDENCIES = $(TEST_DEPS)
testlogout_DEPENDENCIES = $(TEST_DEPS)
teststack_DEPENDENCIES = $(TEST_DEPS)
testrevealer_DEPENDENCIES = $(TEST_DEPS)
testtitlebar_DEPENDENCIES = $(TEST_DEPS)
testwindowsize_DEPENDENCIES = $(TEST_DEPS)

animated_resizing_SOURCES = 	\
	animated-resizing.c	\
	frame-stats.c		\
	frame-stats.h		\
	variable.c		\
	variable.h

animated_revealing_SOURCES = 	\
	animated-revealing.c	\
	frame-stats.c		\
	frame-stats.h		\
	variable.c		\
	variable.h

scrolling_performance_SOURCES = \
	scrolling-performance.c	\
	frame-stats.c		\
	frame-stats.h		\
	variable.c		\
	variable.h

video_timer_SOURCES = 	\
	video-timer.c	\
	variable.c	\
	variable.h

testboxcss_SOURCES =	\
	testboxcss.c

testentrycompletion_SOURCES = 	\
	testentrycompletion.c

testentryicons_SOURCES =	\
	testentryicons.c

testfilechooser_SOURCES = 	\
	testfilechooser.c

testfilechooserbutton_SOURCES =	\
	testfilechooserbutton.c

testflowbox_SOURCES = \
	testflowbox.c

testfontselection_SOURCES = \
	testfontselection.c

testfontselectiondialog_SOURCES = \
	testfontselectiondialog.c

testfontchooser_SOURCES = \
	testfontchooser.c

testfontchooserdialog_SOURCES = \
	testfontchooserdialog.c

testgrid_SOURCES =	\
	testgrid.c

testgtk_SOURCES =	\
	testgtk.c

testtreechanging_SOURCES =	\
	testtreechanging.c

testtreeedit_SOURCES = 	\
	testtreeedit.c

testtreemodel_SOURCES = \
	testtreemodel.c

testtreeview_SOURCES =	\
	testtreeview.c

testtoolbar_SOURCES =	\
	testtoolbar.c

testmenubutton_SOURCES =	\
	testmenubutton.c

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
	teststatusicon.c

testmerge_SOURCES = 		\
	testmerge.c

testactions_SOURCES = 		\
	testactions.c

testbaseline_SOURCES = 		\
	testbaseline.c

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
	testiconview.c

testiconview_keynav_SOURCES = 	\
	testiconview-keynav.c

testnumerableicon_SOURCES =	\
	testnumerableicon.c

testrecentchooser_SOURCES = 	\
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
	testcellarea.c

testtreemenu_SOURCES =		\
	testtreemenu.c

testoffscreen_SOURCES = 	\
	gtkoffscreenbox.c	\
	gtkoffscreenbox.h	\
	testoffscreen.c

testoffscreenwindow_SOURCES =	\
	testoffscreenwindow.c

testoverlay_SOURCES =	\
	testoverlay.c

testoverlaystyleclass_SOURCES =	\
	testoverlaystyleclass.c

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

testtextview_SOURCES = testtextview.c

testanimation_SOURCES = testanimation.c

testpixbuf_scale_SOURCES = testpixbuf-scale.c

testpixbuf_color_SOURCES = testpixbuf-color.c

testpixbuf_save_SOURCES = testpixbuf-save.c

testcolorchooser_SOURCES = testcolorchooser.c

testkineticscrolling_SOURCES = testkineticscrolling.c

teststack_SOURCES = teststack.c

testrevealer_SOURCES = testrevealer.c

testtitlebar_SOURCES = testtitlebar.c

testwindowsize_SOURCES = testwindowsize.c

gdkgears_SOURCES =	\
	gdkgears.c	\
	gtkgears.c	\
	gtkgears.h

testglblending_SOURCES =	\
	testglblending.c	\
	gtkgears.c	\
	gtkgears.h

EXTRA_DIST += 			\
	gradient1.png		\
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
	testsplitheaders.ui	\
	makefile.msc		\
	mydialog.ui		\
	mydialog2.ui		\
	popover.ui		\
	selectionmode.ui

CLEANFILES = 			\
	autotestkeywords.log	\
	autotestkeywords.trs	\
	test-suite.log

-include $(top_srcdir)/git.mk
