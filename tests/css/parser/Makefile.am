include $(top_srcdir)/Makefile.decl

TEST_PROGS += test-css-parser

check_PROGRAMS = $(TEST_PROGS)

test_css_parser_CFLAGS = \
       	-I$(top_srcdir)                 \
       	-I$(top_builddir)/gdk           \
       	-I$(top_srcdir)/gdk             \
       	-DGDK_DISABLE_DEPRECATED        \
       	-DGTK_DISABLE_DEPRECATED        \
       	$(GTK_DEBUG_FLAGS)              \
       	$(GTK_DEP_CFLAGS)

test_css_parser_LDADD = \
       	$(top_builddir)/gdk/libgdk-3.la \
      	$(top_builddir)/gtk/libgtk-3.la \
	$(GTK_DEP_LIBS)

test_css_parser_SOURCES = \
	test-css-parser.c

clean-local:
	rm $(builddir)/*.out.css || true

EXTRA_DIST += \
	boolean.css \
	boolean.errors \
	boolean.ref.css \
	border.css \
	border.errors \
	border.ref.css \
	border-radius.css \
	border-radius.errors \
	border-radius.ref.css \
	close-at-end-of-file.css \
	close-at-end-of-file.errors \
	close-at-end-of-file.ref.css \
	colors-errors.css \
	colors-errors.errors \
	colors-errors.ref.css \
	colors-red.css \
	colors-red.ref.css \
	css-21-malformed-declarations.css \
	css-21-malformed-declarations.errors \
	css-21-malformed-declarations.ref.css \
	css-21-malformed-statements.css \
	css-21-malformed-statements.errors \
	css-21-malformed-statements.ref.css \
	declarations.css \
	declarations.errors \
	declarations.ref.css \
	does-not-exist.css \
	does-not-exist.errors \
	does-not-exist.ref.css \
	enum.css \
	enum.errors \
	enum.ref.css \
	font-family.css \
	font-family.errors \
	font-family.ref.css \
	import-cyclic-1.css \
	import-cyclic-1.errors \
	import-cyclic-1.ref.css \
	import-cyclic-2.css \
	import-cyclic-2.errors \
	import-cyclic-2.ref.css \
	import-cyclic-3.css \
	import-cyclic-3.errors \
	import-cyclic-3.ref.css \
	import-nonexistant.css \
	import-nonexistant.errors \
	import-nonexistant.ref.css \
	import-recursive.css \
	import-recursive.errors \
	import-recursive.ref.css \
	integer.css \
	integer.errors \
	integer.ref.css \
	no-semicolon.css \
	no-semicolon.ref.css \
	pseudo-classes-unknown.css \
	pseudo-classes-unknown.errors \
	pseudo-classes-unknown.ref.css \
	selector.css \
	selector.ref.css \
	shorthand.css \
	shorthand.ref.css \
	simple.css \
	single-slash.css \
	single-slash.errors\
	single-slash.ref.css \
	widget-style-property.css
