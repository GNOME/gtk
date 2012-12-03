include $(top_srcdir)/Makefile.decl

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

EXTRA_DIST += \
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
	combos.ui combos.txt

-include $(top_srcdir)/git.mk
