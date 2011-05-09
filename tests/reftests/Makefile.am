include $(top_srcdir)/Makefile.decl

TEST_PROGS += gtk-reftest

check_PROGRAMS = $(TEST_PROGS)

gtk_reftest_CFLAGS = \
       	-I$(top_srcdir)                 \
       	-I$(top_builddir)/gdk           \
       	-I$(top_srcdir)/gdk             \
       	-DGDK_DISABLE_DEPRECATED        \
       	-DGTK_DISABLE_DEPRECATED        \
       	$(GTK_DEBUG_FLAGS)              \
       	$(GTK_DEP_CFLAGS)

gtk_reftest_LDADD = \
       	$(top_builddir)/gdk/libgdk-3.la \
      	$(top_builddir)/gtk/libgtk-3.la \
	$(GTK_DEP_LIBS)

gtk_reftest_SOURCES = \
	gtk-reftest.c

clean-local:
	rm -rf output/ || true

EXTRA_DIST += \
	box-packing.css \
	box-packing.ref.ui \
	box-packing.ui \
	entry-progress-coloring.css \
	entry-progress-coloring.ref.ui \
	entry-progress-coloring.ui \
	grid-expand.css \
	grid-expand.ref.ui \
	grid-expand.ui \
	grid-homogeneous.css \
	grid-homogeneous.ref.ui \
	grid-homogeneous.ui \
	label-sizing.css \
	label-sizing.ref.ui \
	label-sizing.ui \
	label-small-ellipsized.ref.ui \
	label-small-ellipsized.ui \
	label-wrap-justify.ref.ui \
	label-wrap-justify.ui \
	misc-alignment.css \
	misc-alignment.ui \
	misc-alignment.ref.ui \
	nonresizable-size.ref.ui \
	nonresizable-size.ui \
	quit-mnemonic.ref.ui \
	quit-mnemonic.ui \
	simple.ref.ui \
	simple.ui \
	toplevel-vs-popup.ref.ui \
	toplevel-vs-popup.ui
