include $(top_srcdir)/Makefile.decl

TEST_PROGS += accessibility-dump

check_PROGRAMS = $(TEST_PROGS)

accessibility_dump_CFLAGS = \
       	-I$(top_srcdir)                 \
       	-I$(top_builddir)/gdk           \
       	-I$(top_srcdir)/gdk             \
       	-DGDK_DISABLE_DEPRECATED        \
       	-DGTK_DISABLE_DEPRECATED        \
       	$(GTK_DEBUG_FLAGS)              \
       	$(GTK_DEP_CFLAGS)

accessibility_dump_LDADD = \
       	$(top_builddir)/gdk/libgdk-3.la \
      	$(top_builddir)/gtk/libgtk-3.la \
	$(GTK_DEP_LIBS)

accessibility_dump_SOURCES = \
	accessibility-dump.c

EXTRA_DIST += \
	hello-world.ui hello-world.txt \
	mnemonic.ui mnemonic.txt \
	accessible-name.ui accessible-name.txt \
	notebook.ui notebook.txt \
	range.ui range.txt \
	link.ui link.txt \
	text.ui text.txt \
	tree.ui tree.txt \
	buttons.ui buttions.txt \
	colorchooser.ui colorchooser.txt \
	about.ui about.txt \
	appchooser.ui appchooser.txt \
	messagedialog.ui messagedialog.txt \
	expander.ui expander.txt \
	assistant.ui assistant.txt \
	pickers.ui pickers.txt \
	lockbutton.ui lockbutton.txt \
	spinner.ui spinner.txt \
	progress.ui progress.txt \
	infobar.ui infobar.txt
