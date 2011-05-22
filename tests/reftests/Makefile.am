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
	align-expand.sh \
	box-packing.css \
	box-packing.ref.ui \
	box-packing.ui \
	css-match-class.css \
	css-match-class.ref.ui \
	css-match-class.ui \
	css-match-descendant-later.css \
	css-match-descendant-later.ref.ui \
	css-match-descendant-later.ui \
	css-match-exact.css \
	css-match-exact.ref.ui \
	css-match-exact.ui \
	css-match-inherit.css \
	css-match-inherit.ref.ui \
	css-match-inherit.ui \
	css-match-name.css \
	css-match-name.ref.ui \
	css-match-name.ui \
	css-match-region-matches-star.css \
	css-match-region-matches-star.ref.ui \
	css-match-region-matches-star.ui \
	css-match-regions.css \
	css-match-regions.ref.ui \
	css-match-regions.ui \
	css-match-style-property-order.css \
	css-match-style-property-order.ref.ui \
	css-match-style-property-order.ui \
	css-match-subtype.css \
	css-match-subtype.ref.ui \
	css-match-subtype.ui \
	css-match-type.css \
	css-match-type.ref.ui \
	css-match-type.ui \
	entry-progress-coloring.css \
	entry-progress-coloring.ref.ui \
	entry-progress-coloring.ui \
	green-20x20.png \
	grid-expand.css \
	grid-expand.ref.ui \
	grid-expand.ui \
	grid-homogeneous.css \
	grid-homogeneous.ref.ui \
	grid-homogeneous.ui \
	grid-spacing1.ref.ui \
	grid-spacing1.ui \
	grid-spacing2.ref.ui \
	grid-spacing2.ui \
	grid-spacing3.css \
	grid-spacing3.ref.ui \
	grid-spacing3.ui \
	label-shadows.css \
	label-shadows.ref.ui \
	label-shadows.ui \
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
	paned-undersized.css \
	paned-undersized.ref.ui \
	paned-undersized.ui \
	quit-mnemonic.ref.ui \
	quit-mnemonic.ui \
	rotated-layout.ref.ui \
	rotated-layout.ui \
	shorthand-entry-border.css \
	shorthand-entry-border.ref.ui \
	shorthand-entry-border.ui \
	simple.ref.ui \
	simple.ui \
	toplevel-vs-popup.ref.ui \
	toplevel-vs-popup.ui
