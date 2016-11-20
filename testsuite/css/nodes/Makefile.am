include $(top_srcdir)/Makefile.decl

NULL =

CLEANFILES =

TEST_PROGS += test-css-nodes
test_in_files = test-css-nodes.test.in

check_PROGRAMS = $(TEST_PROGS)

test_css_nodes_CFLAGS = \
	-I$(top_srcdir)                 \
	-I$(top_builddir)/gdk           \
	-I$(top_srcdir)/gdk             \
	$(GTK_DEBUG_FLAGS)              \
	$(GTK_DEP_CFLAGS)		\
	$(NULL)

test_css_nodes_LDADD = \
	$(top_builddir)/gtk/libgtk-4.la \
	$(GTK_DEP_LIBS)			\
	$(NULL)

test_css_nodes_SOURCES = \
	test-css-nodes.c		\
	$(NULL)

test_data = \
	box.ltr.ui		box.ltr.nodes			\
	box.rtl.ui		box.rtl.nodes			\
	box-packing.ltr.ui	box-packing.ltr.nodes		\
	box-packing.rtl.ui	box-packing.rtl.nodes		\
	buttons.ui 		buttons.nodes 			\
	checkbutton.ltr.ui	checkbutton.ltr.nodes		\
	checkbutton.rtl.ui	checkbutton.rtl.nodes		\
	combobox.ui		combobox.nodes			\
	entries.ui 		entries.nodes 			\
	expander.ltr.ui		expander.ltr.nodes		\
	expander.rtl.ui		expander.rtl.nodes		\
	levelbar.ltr.ui 	levelbar.ltr.nodes		\
	levelbar.rtl.ui 	levelbar.rtl.nodes		\
	notebook-arrows.ui 	notebook-arrows.nodes 		\
	notebook-arrows2.ui 	notebook-arrows2.nodes 		\
	notebook.top.ltr.ui 	notebook.top.ltr.nodes 		\
	notebook.top.rtl.ui 	notebook.top.rtl.nodes 		\
	notebook.left.ltr.ui 	notebook.left.ltr.nodes 	\
	notebook.left.rtl.ui 	notebook.left.rtl.nodes 	\
	notebook.right.ltr.ui 	notebook.right.ltr.nodes 	\
	notebook.right.rtl.ui 	notebook.right.rtl.nodes 	\
	notebook.bottom.ltr.ui 	notebook.bottom.ltr.nodes 	\
	notebook.bottom.rtl.ui 	notebook.bottom.rtl.nodes 	\
	paned.ltr.ui		paned.ltr.nodes			\
	paned.rtl.ui		paned.rtl.nodes			\
	progressbar.ui 		progressbar.nodes 		\
	scale.ui		scale.nodes			\
	$(NULL)

EXTRA_DIST += $(test_in_files) $(test_data)

if BUILDOPT_INSTALL_TESTS
insttestdir=$(libexecdir)/installed-tests/$(PACKAGE)/css/nodes
insttest_PROGRAMS = $(TEST_PROGS)
insttest_DATA = $(test_data)

substitutions = \
	-e s,@libexecdir\@,$(libexecdir),g \
	$(NULL)

test_files = $(test_in_files:.test.in=.test)

$(test_files): %.test: %.test.in
	$(AM_V_GEN) sed $(substitutions) $< > $@.tmp && mv $@.tmp $@

EXTRA_DIST += $(test_files)

CLEANFILES += $(test_files)

testmetadir = $(datadir)/installed-tests/$(PACKAGE)/css/nodes
testmeta_DATA = $(test_files)
endif


-include $(top_srcdir)/git.mk
