include $(top_srcdir)/Makefile.decl
NULL =

TESTS_ENVIRONMENT = 				\
	REFTEST_MODULE_DIR=$(abs_builddir)	\
	GTK_IM_MODULE=gtk-im-context-simple 	\
	GSETTINGS_BACKEND=memory		\
	G_ENABLE_DIAGNOSTIC=0

TEST_PROGS += gtk-reftest

check_PROGRAMS = $(TEST_PROGS)

gtk_reftest_CFLAGS = \
	-I$(top_srcdir)                 \
	-I$(top_builddir)/gdk           \
	-I$(top_srcdir)/gdk             \
	-DGDK_DISABLE_DEPRECATED        \
	-DGTK_DISABLE_DEPRECATED        \
	$(GTK_DEBUG_FLAGS)              \
	$(GTK_DEP_CFLAGS)		\
	$(GMODULE_CFLAGS)		\
	$(NULL)

gtk_reftest_LDADD = \
	libgtkreftestprivate.la		\
	$(top_builddir)/gdk/libgdk-3.la \
	$(top_builddir)/gtk/libgtk-3.la \
	$(GTK_DEP_LIBS)			\
	$(GMODULE_LIBS)			\
	$(NULL)

if PLATFORM_WIN32
noinst_LTLIBRARIES =
lib_LTLIBRARIES =		\
	libgtkreftestprivate.la	\
	$(NULL)
else
noinst_LTLIBRARIES =		\
	libgtkreftestprivate.la	\
	$(NULL)
endif

libgtkreftestprivate_la_CFLAGS = $(gtk_reftest_CFLAGS)
libgtkreftestprivate_la_LDFLAGS = $(no_undefined)
libgtkreftestprivate_la_LIBADD =	\
	$(top_builddir)/gdk/libgdk-3.la \
	$(top_builddir)/gtk/libgtk-3.la \
	$(GTK_DEP_LIBS)			\
	$(GMODULE_LIBS)			\
	$(NULL)

libgtkreftestprivate_la_SOURCES =	\
	reftest-compare.c		\
	reftest-compare.h		\
	reftest-module.c		\
	reftest-module.h		\
	reftest-snapshot.c		\
	reftest-snapshot.h		\
	$(NULL)

gtk_reftest_SOURCES = \
	gtk-reftest.c			\
	gtk-reftest.h

clean-local:
	rm -rf output/ || true

EXTRA_DIST += \
	align-expand.sh \
	$(NULL)

testdata = \
	721800-0px-dotted-border.css \
	721800-0px-dotted-border.ref.ui \
	721800-0px-dotted-border.ui \
	actionbar.css \
	actionbar.ref.ui \
	actionbar.ui \
	alignment-props.css \
	alignment-props.ref.ui \
	alignment-props.ui \
	animation-direction.css \
	animation-direction.ref.ui \
	animation-direction.ui \
	background-area.css \
	background-area.ref.ui \
	background-area.ui \
	background-color-transparent.css \
	background-color-transparent.ref.ui \
	background-color-transparent.ui \
	background-image-multiple.css \
	background-image-multiple.ref.ui \
	background-image-multiple.ui \
	background-origin.css \
	background-origin.ref.ui \
	background-origin.ui \
	background-positioning-area-empty.ref.ui \
	background-positioning-area-empty.ui \
	background-position.css \
	background-position.ref.ui \
	background-position.ui \
	background-position-repeat.css \
	background-position-repeat.ref.ui \
	background-position-repeat.ui \
	background-position-repeat-bg.png \
	background-position-simple.css \
	background-position-simple.ref.ui \
	background-position-simple.ui \
	background-size.css \
	background-size.ref.ui \
	background-size.ui \
	background-size-keywords.css \
	background-size-keywords.ref.ui \
	background-size-keywords.ui \
	background-size-zero.css \
	background-size-zero.ref.ui \
	background-size-zero.ui \
	background-window-transparent.css \
	background-window-transparent.ref.ui \
	background-window-transparent.ui \
	bad-color-names.css \
	bad-color-names.ref.ui \
	bad-color-names.ui \
	reference-background-origin.png	\
	border-color-default.css \
	border-color-default.ref.ui \
	border-color-default.ui \
	border-half-pixel.css \
	border-half-pixel.ref.ui \
	border-half-pixel.ui \
	border-image-ball-green.png \
	border-image-ball-red.png \
	border-image-ball-yellow.png \
	border-image-balls.png \
	border-image-3-balls-green.png \
	border-image-3-balls-green-squished.png \
	border-image-3-balls-yellow.png \
	border-image-3-balls-yellow-squished.png \
	border-image-excess-size.css \
	border-image-excess-size.ref.ui \
	border-image-excess-size.ui \
	border-image-gradient.css \
	border-image-gradient.ref.ui \
	border-image-gradient.ui \
	border-image-url-scaled.css \
	border-image-url-scaled.ref.ui \
	border-image-url-scaled.ui \
	border-image-url.css \
	border-image-url.ref.ui \
	border-image-url.ui \
	border-radius-clamp.css \
	border-radius-clamp.ref.ui \
	border-radius-clamp.ui \
	border-rgb.png \
	border-rgb@2.png \
	border-style-none.css \
	border-style-none.ref.ui \
	border-style-none.ui \
	border-style.css \
	border-style.ref.ui \
	border-style.ui \
	box-child-expand.ref.ui \
	box-child-expand.ui \
	box-packing.css \
	box-packing.ref.ui \
	box-packing.ui \
	box-pseudo-classes.css \
	box-pseudo-classes.ref.ui \
	box-pseudo-classes.ui \
	box-shadow-spec-inset.css \
	box-shadow-spec-inset.ref.ui \
	box-shadow-spec-inset.ui \
	box-shadow-spread.css \
	box-shadow-spread.ref.ui \
	box-shadow-spread.ui \
	button-wrapping.ui \
	button-wrapping.ref.ui \
	cellrenderer-pixbuf-stock-rtl.css \
	cellrenderer-pixbuf-stock-rtl.ui \
	cellrenderer-pixbuf-stock-rtl.ref.ui \
	css-currentcolor-alpha.css \
	css-currentcolor-alpha.ui \
	css-currentcolor-alpha.ref.ui \
	css-match-class.css \
	css-match-class.ref.ui \
	css-match-class.ui \
	css-match-descendant-later.css \
	css-match-descendant-later.ref.ui \
	css-match-descendant-later.ui \
	css-match-exact.css \
	css-match-exact.ref.ui \
	css-match-exact.ui \
	css-match-import.css \
	css-match-import-import.css \
	css-match-import.ui \
	css-match-import.ref.ui \
	css-match-inherit.css \
	css-match-inherit.ref.ui \
	css-match-inherit.ui \
	css-match-inherit-different-state.css \
	css-match-inherit-different-state.ref.ui \
	css-match-inherit-different-state.ui \
	css-match-name.css \
	css-match-name.ref.ui \
	css-match-name.ui \
	css-match-region-matches-star.css \
	css-match-region-matches-star.ui \
	css-match-region-matches-star.ref.ui \
	css-match-regions.css \
	css-match-regions.ref.ui \
	css-match-regions.ui \
	css-match-siblings.css \
	css-match-siblings.ref.ui \
	css-match-siblings.ui \
	css-match-subtype.css \
	css-match-subtype.ref.ui \
	css-match-subtype.ui \
	css-match-type.css \
	css-match-type.ref.ui \
	css-match-type.ui \
	css-multi-state.css \
	css-multi-state.ref.ui \
	css-multi-state.ui \
	entry-progress-coloring.css \
	entry-progress-coloring.ref.ui \
	entry-progress-coloring.ui \
	fixed-widget-stacking.ref.ui \
	fixed-widget-stacking.ui \
	flipping-icons.ref.ui \
	flipping-icons.ui \
	font-sizes-names.css \
	font-sizes-names.ref.ui \
	font-sizes-names.ui \
	gtk-image-effect-inherit.css \
	gtk-image-effect-inherit.ref.ui \
	gtk-image-effect-inherit.ui \
	green-20x20.png \
	grid-empty-with-spacing.ref.ui \
	grid-empty-with-spacing.ui \
	grid-expand.css \
	grid-expand.ref.ui \
	grid-expand.ui \
	grid-wfh.ui \
	grid-wfh.ref.ui \
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
	gtk-icontheme-sizing.css \
	gtk-icontheme-sizing.ref.ui \
	gtk-icontheme-sizing.ui \
	icon-shadow-no-transform.css \
	icon-shadow-no-transform.ref.ui \
	icon-shadow-no-transform.ui \
	icon-style-basics.css \
	icon-style-basics.ref.ui \
	icon-style-basics.ui \
	icon-vfuncs.css \
	icon-vfuncs.ref.ui \
	icon-vfuncs.ui \
	iconview-empty.css \
	iconview-empty.ui \
	iconview-empty.ref.ui \
	image-load-from-file.css \
	image-load-from-file.ref.ui \
	image-load-from-file.ui \
	info-bar-message-types.css \
	info-bar-message-types.ref.ui \
	info-bar-message-types.ui \
	inherit-and-initial.css \
	inherit-and-initial.ref.ui \
	inherit-and-initial.ui \
	label-attribute-preference.css \
	label-attribute-preference.ref.ui \
	label-attribute-preference.ui \
	label-background.css \
	label-background.ref.ui \
	label-background.ui \
	label-box-shadow-clip.css \
	label-box-shadow-clip.ref.ui \
	label-box-shadow-clip.ui \
	label-clipping.css \
	label-clipping.ref.ui \
	label-clipping.ui \
	label-ellipsize-small.ref.ui \
	label-ellipsize-small.ui \
	label-ellipsize-with-big.ref.ui \
	label-ellipsize-with-big.ui \
	label-shadows.css \
	label-shadows.ref.ui \
	label-shadows.ui \
	label-sizing.css \
	label-sizing.ref.ui \
	label-sizing.ui \
	label-small-ellipsized.ref.ui \
	label-small-ellipsized.ui \
	label-text-shadow-clipping.ref.ui \
	label-text-shadow-clipping.ui \
	label-width-chars-dont-shrink.ref.ui \
	label-width-chars-dont-shrink.ui \
	label-wrap-justify.ref.ui \
	label-wrap-justify.ui \
	linear-gradient.css \
	linear-gradient.ref.ui \
	linear-gradient.ui \
	linear-gradient-transition-to-other.css \
	linear-gradient-transition-to-other.ref.ui \
	linear-gradient-transition-to-other.ui \
	link-coloring.css \
	link-coloring.ref.ui \
	link-coloring.ui \
	marble.xpm \
	messagedialog-secondarytext.ui \
	messagedialog-secondarytext.ref.ui \
	misc-alignment.css \
	misc-alignment.ref.ui \
	misc-alignment.ui \
	named-colors.css \
	named-colors.ref.ui \
	named-colors.ui \
	no-colors.css \
	no-colors.ref.ui \
	no-colors.ui \
	nonresizable-size.ref.ui \
	nonresizable-size.ui \
	notebook-childproperties.css \
	notebook-childproperties.ui \
	notebook-childproperties.ref.ui \
	notebook-tab-position.css \
	notebook-tab-position.ui \
	notebook-tab-position.ref.ui \
	nth-child.css \
	nth-child.ref.ui \
	nth-child.ui \
	opacity.css \
	opacity.ui \
	opacity.ref.ui \
	opacity-initial.css \
	opacity-initial.ref.ui \
	opacity-initial.ui \
	overlay-no-main-widget.ref.ui \
	overlay-no-main-widget.ui \
	paned-undersized.css \
	paned-undersized.ref.ui \
	paned-undersized.ui \
	pseudoclass-on-box.css \
	pseudoclass-on-box.ref.ui \
	pseudoclass-on-box.ui \
	pseudoclass-on-parent.css \
	pseudoclass-on-parent.ref.ui \
	pseudoclass-on-parent.ui \
	quit-mnemonic.css \
	quit-mnemonic.ref.ui \
	quit-mnemonic.ui \
	reset-to-defaults.css \
	revealer-extra-size.ref.ui \
	revealer-extra-size.ui \
	revealer-wrappable-contents.ref.ui \
	revealer-wrappable-contents.ui \
	rotated-layout.ref.ui \
	rotated-layout.ui \
	separator-size.ref.ui \
	separator-size.ui \
	set-default-direction.ui \
	set-default-direction.ref.ui \
	shadow-clip-rounding.css \
	shadow-clip-rounding.ref.ui \
	shadow-clip-rounding.ui \
	shorthand-entry-border.css \
	shorthand-entry-border.ref.ui \
	shorthand-entry-border.ui \
	sibling-pseudoclasses.css \
	sibling-pseudoclasses.ref.ui \
	sibling-pseudoclasses.ui \
	simple.ref.ui \
	simple.ui \
	sizegroups-basics.css \
	sizegroups-basics.ui \
	sizegroups-basics.ref.ui \
	sizegroups-evolution-identity-page.ui \
	sizegroups-evolution-identity-page.ref.ui \
	sizegroups-get-preferred-null.ui \
	sizegroups-get-preferred-null.ref.ui \
	statusbar-remove-all.ref.ui \
	statusbar-remove-all.ui \
	style-properties-nth-child.css \
	style-properties-nth-child.ref.ui \
	style-properties-nth-child.ui \
	style-properties-only-child.css \
	style-properties-only-child.ref.ui \
	style-properties-only-child.ui \
	symbolic-icon-translucent-color.css \
	symbolic-icon-translucent-color.ref.ui \
	symbolic-icon-translucent-color.ui \
	textview-border-windows.css \
	textview-border-windows.ref.ui \
	textview-border-windows.ui \
	textview-margins.css \
	textview-margins.ref.ui \
	textview-margins.ui \
	toplevel-vs-popup.ref.ui \
	toplevel-vs-popup.ui \
	treeview-fixed-height.css \
	treeview-fixed-height.ref.ui \
	treeview-fixed-height.ui \
	treeview-headers-hidden.ref.ui \
	treeview-headers-hidden.ui \
	unresolvable.css \
	unresolvable.ref.ui \
	unresolvable.ui \
	window-border-width.ref.ui \
	window-border-width.ui \
	window-default-size.ref.ui \
	window-default-size.ui \
	window-height-for-width.ref.ui \
	window-height-for-width.ui \
	window-show-contents-on-map.ref.ui \
	window-show-contents-on-map.ui \
	$(NULL)

EXTRA_DIST += 			\
	$(testdata)		\
	reftests-dark.test.in	\
	reftests-hc.test.in	\
	reftests.test.in	\
	$(NULL)

if BUILDOPT_INSTALL_TESTS
insttestdir=$(libexecdir)/installed-tests/$(PACKAGE)
insttest_PROGRAMS = gtk-reftest

reftestdir = $(insttestdir)/reftests
reftest_DATA = $(testdata)
reftest_LTLIBRARIES =			\
	libreftest.la			\
	$(NULL)

libreftest_la_LDFLAGS = -rpath $(reftestdir)

substitutions = \
	-e s,@libexecdir\@,$(libexecdir),g \
	$(NULL)

%.test: %.test.in
	$(AM_V_GEN) sed $(substitutions) $< > $@.tmp && mv $@.tmp $@

built_tests = 			\
	reftests.test		\
	reftests-dark.test	\
	reftests-hc.test

DISTCLEANFILES = $(built_tests)

testmetadir = $(datadir)/installed-tests/$(PACKAGE)
testmeta_DATA = $(built_tests)
else
noinst_LTLIBRARIES +=			\
	libreftest.la			\
	$(NULL)

libreftest_la_LDFLAGS = -rpath /iHaveNoIdeaWhatImDoing

endif

libreftest_la_LDFLAGS += -avoid-version -module $(no_undefined)
libreftest_la_CFLAGS = $(gtk_reftest_CFLAGS)
libreftest_la_LIBADD = $(gtk_reftest_LDADD)
libreftest_la_SOURCES =			\
	expand-expander.c		\
	set-default-direction.c		\
	statusbar-remove-all.c		\
	textview-border-windows.c	\
	$(NULL)

-include $(top_srcdir)/git.mk
