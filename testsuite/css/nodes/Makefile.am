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
        $(GTK_DEP_CFLAGS)

test_css_nodes_LDADD = \
        $(top_builddir)/gdk/libgdk-3.la \
        $(top_builddir)/gtk/libgtk-3.la \
        $(GTK_DEP_LIBS)

test_css_nodes_SOURCES = \
        test-css-nodes.c

test_data = \
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
