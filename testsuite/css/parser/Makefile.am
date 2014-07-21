include $(top_srcdir)/Makefile.decl

NULL =

CLEANFILES =

TEST_PROGS += test-css-parser
test_in_files = test-css-parser.test.in

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

test_data = \
	animation-shorthand-crash.css \
	animation-shorthand-crash.ref.css \
	at-invalid-01.css \
	at-invalid-01.errors \
	at-invalid-01.ref.css \
	at-invalid-02.css \
	at-invalid-02.errors \
	at-invalid-02.ref.css \
	at-invalid-03.css \
	at-invalid-03.errors \
	at-invalid-03.ref.css \
	at-invalid-04.css \
	at-invalid-04.errors \
	at-invalid-04.ref.css \
	at-invalid-05.css \
	at-invalid-05.errors \
	at-invalid-05.ref.css \
	at-invalid-06.css \
	at-invalid-06.errors \
	at-invalid-06.ref.css \
	at-invalid-07.css \
	at-invalid-07.errors \
	at-invalid-07.ref.css \
	at-invalid-08.css \
	at-invalid-08.errors \
	at-invalid-08.ref.css \
	at-invalid-09.css \
	at-invalid-09.errors \
	at-invalid-09.ref.css \
	at-invalid-10.css \
	at-invalid-10.errors \
	at-invalid-10.ref.css \
	at-invalid-11.css \
	at-invalid-11.errors \
	at-invalid-11.ref.css \
	at-invalid-12.css \
	at-invalid-12.errors \
	at-invalid-12.ref.css \
	at-invalid-13.css \
	at-invalid-13.errors \
	at-invalid-13.ref.css \
	at-invalid-14.css \
	at-invalid-14.errors \
	at-invalid-14.ref.css \
	at-invalid-15.css \
	at-invalid-15.errors \
	at-invalid-15.ref.css \
	at-invalid-16.css \
	at-invalid-16.errors \
	at-invalid-16.ref.css \
	at-invalid-17.css \
	at-invalid-17.errors \
	at-invalid-17.ref.css \
	at-invalid-18.css \
	at-invalid-18.errors \
	at-invalid-18.ref.css \
	at-invalid-19.css \
	at-invalid-19.errors \
	at-invalid-19.ref.css \
	at-invalid-20.css \
	at-invalid-20.errors \
	at-invalid-20.ref.css \
	at-invalid-21.css \
	at-invalid-21.errors \
	at-invalid-21.ref.css \
	at-invalid-22.css \
	at-invalid-22.errors \
	at-invalid-22.ref.css \
	at-invalid-23.css \
	at-invalid-23.errors \
	at-invalid-23.ref.css \
	at-invalid-24.css \
	at-invalid-24.errors \
	at-invalid-24.ref.css \
	at-invalid-25.css \
	at-invalid-25.errors \
	at-invalid-25.ref.css \
	at-invalid-26.css \
	at-invalid-26.errors \
	at-invalid-26.ref.css \
	at-invalid-27.css \
	at-invalid-27.errors \
	at-invalid-27.ref.css \
	at-valid-01.css \
	at-valid-01.ref.css \
	at-valid-02.css \
	at-valid-02.ref.css \
	at-valid-03.css \
	at-valid-03.ref.css \
	at-valid-04.css \
	at-valid-04.ref.css \
	at-valid-05.css \
	at-valid-05.ref.css \
	at-valid-06.css \
	at-valid-06.ref.css \
	at-valid-07.css \
	at-valid-08.css \
	at-valid-08.ref.css \
	at-valid-09.css \
	at-valid-09.ref.css \
	at-valid-10.css \
	at-valid-10.ref.css \
	at-valid-11.css \
	at-valid-11.ref.css \
	at-valid-12.css \
	at-valid-12.ref.css \
	at-valid-13.css \
	at-valid-13.ref.css \
	at-valid-14.css \
	at-valid-14.ref.css \
	at-valid-15.css \
	at-valid-15.ref.css \
	at-valid-16.css \
	at-valid-16.ref.css \
	at-valid-17.css \
	at-valid-18.css \
	at-valid-18.ref.css \
	at-valid-19.css \
	at-valid-19.errors \
	at-valid-19.ref.css \
	at-valid-20.css \
	at-valid-20.errors \
	at-valid-20.ref.css \
	at-valid-21.css \
	at-valid-21.errors \
	at-valid-21.ref.css \
	background-position.css \
	background-position.errors \
	background-position.ref.css \
	background-position-errors.css \
	background-position-errors.errors \
	background-position-errors.ref.css \
	background-repeat.css \
	background-repeat.ref.css \
	background-shorthand.css \
	background-shorthand.ref.css \
	background-shorthand-single.css \
	background-shorthand-single.ref.css \
        background-size.css \
        background-size.ref.css \
	boolean.css \
	boolean.errors \
	boolean.ref.css \
	border.css \
	border.errors \
	border.ref.css \
	border-color-currentcolor.css \
	border-color-currentcolor.ref.css \
	border-radius.css \
	border-radius.errors \
	border-radius.ref.css \
	border-shorthand.css \
	border-shorthand.ref.css \
	close-at-end-of-file.css \
	close-at-end-of-file.errors \
	close-at-end-of-file.ref.css \
	colors-errors.css \
	colors-errors.errors \
	colors-errors.ref.css \
	colors-red.css \
	colors-red.ref.css \
	comment-detection.css \
	comment-detection.ref.css \
	cross-fade-basic.css \
	css-21-malformed-declarations.css \
	css-21-malformed-declarations.errors \
	css-21-malformed-declarations.ref.css \
	css-21-malformed-statements.css \
	css-21-malformed-statements.errors \
	css-21-malformed-statements.ref.css \
	currentcolor-everywhere.css \
	declarations.css \
	declarations.errors \
	declarations.ref.css \
	declarations-invalid-01.css \
	declarations-invalid-01.errors \
	declarations-invalid-01.ref.css \
	declarations-invalid-02.css \
	declarations-invalid-02.errors \
	declarations-invalid-02.ref.css \
	declarations-invalid-03.css \
	declarations-invalid-03.errors \
	declarations-invalid-03.ref.css \
	declarations-invalid-04.css \
	declarations-invalid-04.errors \
	declarations-invalid-04.ref.css \
	declarations-invalid-05.css \
	declarations-invalid-05.errors \
	declarations-invalid-05.ref.css \
	declarations-invalid-06.css \
	declarations-invalid-06.errors \
	declarations-invalid-06.ref.css \
	declarations-invalid-07.css \
	declarations-invalid-07.errors \
	declarations-invalid-07.ref.css \
	declarations-invalid-08.css \
	declarations-invalid-08.errors \
	declarations-invalid-08.ref.css \
	declarations-valid-01.css \
	declarations-valid-01.ref.css \
	declarations-valid-02.css \
	declarations-valid-02.ref.css \
	declarations-valid-03.css \
	declarations-valid-03.ref.css \
	declarations-valid-04.css \
	declarations-valid-04.ref.css \
	declarations-valid-05.css \
	declarations-valid-05.ref.css \
	declarations-valid-06.css \
	declarations-valid-06.ref.css \
	declarations-valid-07.css \
	declarations-valid-07.ref.css \
	declarations-valid-08.css \
	declarations-valid-08.ref.css \
	declarations-valid-09.css \
	declarations-valid-09.ref.css \
	declarations-valid-10.css \
	declarations-valid-10.ref.css \
	declarations-valid-11.css \
	declarations-valid-11.ref.css \
	declarations-valid-12.css \
	declarations-valid-12.ref.css \
	declarations-valid-13.css \
	declarations-valid-13.ref.css \
	declarations-valid-14.css \
	declarations-valid-14.ref.css \
	declarations-valid-15.css \
	declarations-valid-15.ref.css \
	declarations-valid-16.css \
	declarations-valid-16.ref.css \
	declarations-valid-17.css \
	declarations-valid-17.ref.css \
	declarations-valid-18.css \
	declarations-valid-18.errors \
	declarations-valid-18.ref.css \
	declarations-valid-19.css \
	declarations-valid-19.ref.css \
	declarations-valid-20.css \
	declarations-valid-20.ref.css \
	declarations-valid-21.css \
	declarations-valid-21.ref.css \
	declarations-valid-22.css \
	declarations-valid-22.ref.css \
	declarations-valid-23.css \
	declarations-valid-23.ref.css \
	declarations-valid-24.css \
	declarations-valid-24.ref.css \
	declarations-valid-25.css \
	declarations-valid-25.ref.css \
	does-not-exist.css \
	does-not-exist.errors \
	does-not-exist.ref.css \
	doubled.css \
	doubled.ref.css \
	empty.css \
	enum.css \
	enum.errors \
	enum.ref.css \
	font-family.css \
	font-family.errors \
	font-family.ref.css \
	freed-string-in-error-messages.css \
	freed-string-in-error-messages.errors \
	freed-string-in-error-messages.ref.css \
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
	keyframes-empty.css \
	keyframes-empty.ref.css \
	keyframes-ordering.css \
	keyframes-ordering.ref.css \
	linear-gradient.css \
	linear-gradient.ref.css \
	no-semicolon.css \
	no-semicolon.ref.css \
	nth-child.css \
	nth-child.ref.css \
	property-name-errors.css \
	property-name-errors.errors \
	property-name-errors.ref.css \
	pseudo-classes-known.css \
	pseudo-classes-unknown.css \
	pseudo-classes-unknown.errors \
	pseudo-classes-unknown.ref.css \
	selector.css \
	selector.ref.css \
	shadow.css \
	shadow.ref.css \
	shorthand.css \
	shorthand.ref.css \
	simple.css \
	single-slash.css \
	single-slash.errors\
	single-slash.ref.css \
	string-values.css \
	string-values.ref.css \
	test.png \
	text-shadow-invalid-but-worked-in-3.12.css \
	text-shadow-invalid-but-worked-in-3.12.errors \
	text-shadow-invalid-but-worked-in-3.12.ref.css \
	transition.css \
	transition.ref.css \
	value-inherit.css \
	value-inherit-shorthand.css \
	value-inherit-shorthand.ref.css \
	value-initial.css \
	value-initial-shorthand.css \
	value-initial-shorthand.ref.css \
	value-none.css \
	value-none.errors \
	value-none.ref.css \
	widget-style-property.css \
	$(NULL)

EXTRA_DIST += $(test_in_files) $(test_data)

if BUILDOPT_INSTALL_TESTS
insttestdir=$(libexecdir)/installed-tests/$(PACKAGE)/css/parser
insttest_PROGRAMS = $(TEST_PROGS)
insttest_DATA = $(test_data)

substitutions = \
	-e s,@libexecdir\@,$(libexecdir),g \
	$(NULL)

test_files = $(test_in_files:.test.in=.test)

$(test_files): %.test: %.test.in
	$(AM_V_GEN) sed $(substitutions) $< > $@.tmp && mv $@.tmp $@

EXTRA_DIST += $(test_in_files)

CLEANFILES += $(test_files)

testmetadir = $(datadir)/installed-tests/$(PACKAGE)/css/parser
testmeta_DATA = $(test_files)
endif
 
-include $(top_srcdir)/git.mk
