include $(top_srcdir)/Makefile.decl
NULL =

check_PROGRAMS = $(TEST_PROGS)

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

# work around random file systems showing up in the file chooser,
# breaking the 'pickers' test
TESTS_ENVIRONMENT = GIO_USE_VOLUME_MONITOR=unix

TEST_PROGS += accessibility-dump

TEST_PROGS += tree-performance

TEST_PROGS += text

TEST_PROGS += children

TEST_PROGS += derive

TEST_PROGS += value

# the focus test has no chance of working until
# all the idle handlers in gail are gone
#
# TEST_PROGS += focus

TEST_PROGS += tree-relationships

TEST_PROGS += util

noinst_PROGS = testfocus

testdata = \
	hello-world.ui hello-world.txt \
	mnemonic.ui mnemonic.txt \
	accessible-name.ui accessible-name.txt \
	notebook.ui notebook.txt \
	range.ui range.txt \
	link.ui link.txt \
	text.ui text.txt \
	buttons.ui buttons.txt \
	colorchooser.ui colorchooser.txt \
	about.ui about.txt \
	messagedialog.ui messagedialog.txt \
	expander.ui expander.txt \
	assistant.ui assistant.txt \
	pickers.ui pickers.txt \
	lockbutton.ui lockbutton.txt \
	spinner.ui spinner.txt \
	progress.ui progress.txt \
	infobar.ui infobar.txt \
	calendar.ui calendar.txt \
	statusbar.ui statusbar.txt \
	paned.ui paned.txt \
	iconview.ui iconview.txt \
	entries.ui entries.txt \
	scale-drawvalue.ui scale-drawvalue.txt \
	placeholder-text.ui placeholder-text.txt \
	menu.ui menu.txt \
	combos.ui combos.txt \
	listbox.ui listbox.txt \
	stack.ui stack.txt \
	headerbar.ui headerbar.txt \
	$(NULL)

test_in_files = \
	a11ytests.test.in \
	a11ychildren.test.in \
	a11ytree.test.in \
	a11yvalue.test.in \
	a11yderive.test.in \
	a11ytext.test.in \
	a11yutil.test.in \
	$(NULL)

EXTRA_DIST += $(test_in_files) $(testdata)

if BUILDOPT_INSTALL_TESTS
insttestdir=$(pkglibexecdir)/installed-tests
insttest_PROGRAMS = $(TEST_PROGS)

a11ytestdir = $(insttestdir)/a11ytests
a11ytest_DATA = $(testdata)

substitutions = \
	-e s,@pkglibexecdir\@,$(pkglibexecdir),g \
	$(NULL)

test_files = $(test_in_files:.test.in=.test)

$(test_files): %.test: %.test.in
	$(AM_V_GEN) sed $(substitutions) $< > $@.tmp && mv $@.tmp $@

DISTCLEANFILES = $(test_files)

testmetadir = $(datadir)/installed-tests/$(PACKAGE)
testmeta_DATA = $(test_files)
endif

-include $(top_srcdir)/git.mk
