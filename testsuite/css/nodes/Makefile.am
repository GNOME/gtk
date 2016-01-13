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
	$(top_builddir)/gdk/libgdk-3.la \
	$(top_builddir)/gtk/libgtk-3.la \
	$(GTK_DEP_LIBS)			\
	$(NULL)

test_css_nodes_SOURCES = \
	test-css-nodes.c		\
	$(NULL)

test_data = \
	box.ltr.ui	box.ltr.nodes		\
	box.rtl.ui	box.rtl.nodes		\
	buttons.ui 	buttons.nodes 		\
	entries.ui 	entries.nodes 		\
	levelbar.ltr.ui levelbar.ltr.nodes	\
	levelbar.rtl.ui levelbar.rtl.nodes	\
	notebook.ui 	notebook.nodes 		\
	paned.ltr.ui	paned.ltr.nodes		\
	paned.rtl.ui	paned.rtl.nodes		\
	progressbar.ui 	progressbar.nodes 	\
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
