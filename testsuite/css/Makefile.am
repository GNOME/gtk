include $(top_srcdir)/Makefile.decl

NULL =

SUBDIRS = parser

check_PROGRAMS = $(TEST_PROGS)
test_in_files =

AM_CPPFLAGS =				\
	-I$(top_srcdir)                 \
	-I$(top_builddir)/gdk           \
	-I$(top_srcdir)/gdk             \
	-DGDK_DISABLE_DEPRECATED        \
	-DGTK_DISABLE_DEPRECATED        \
	-DGTK_VERSION=\"$(GTK_VERSION)\"\
	$(GTK_DEBUG_FLAGS)              \
	$(GTK_DEP_CFLAGS)

LDADD = \
	$(top_builddir)/gdk/libgdk-3.la \
	$(top_builddir)/gtk/libgtk-3.la \
	$(GTK_DEP_LIBS)

TEST_PROGS += api
test_in_files += api.test.in

EXTRA_DIST += $(test_in_files)

if BUILDOPT_INSTALL_TESTS
insttestdir=$(pkglibexecdir)/installed-tests/css
insttest_PROGRAMS = $(TEST_PROGS)

substitutions = \
	-e s,@pkglibexecdir\@,$(pkglibexecdir),g \
	$(NULL)

test_files = $(test_in_files:.test.in=.test)

$(test_files): %.test: %.test.in
	$(AM_V_GEN) sed $(substitutions) $< > $@.tmp && mv $@.tmp $@

DISTCLEANFILES = $(test_files)

testmetadir = $(datadir)/installed-tests/$(PACKAGE)/css
testmeta_DATA = $(test_files)
endif

-include $(top_srcdir)/git.mk
