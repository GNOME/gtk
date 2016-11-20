include $(top_srcdir)/Makefile.decl

NULL =

CLEANFILES =

TEST_PROGS += test-css-style
test_in_files = test-css-style.test.in

check_PROGRAMS = $(TEST_PROGS)

test_css_style_CFLAGS = \
        -I$(top_srcdir)                 \
        -I$(top_builddir)/gdk           \
        -I$(top_srcdir)/gdk             \
        $(GTK_DEBUG_FLAGS)              \
        $(GTK_DEP_CFLAGS)		\
	$(NULL)

test_css_style_LDADD = \
        $(top_builddir)/gtk/libgtk-4.la \
        $(GTK_DEP_LIBS)			\
	$(NULL)

test_css_style_SOURCES = \
        test-css-style.c		\
	resources.c			\
	$(NULL)

test_data = \
	adjacent-states.ui 	adjacent-states.css	adjacent-states.nodes 	\
	colornames.ui		colornames.css		colornames.nodes	\
        currentcolor.ui 	currentcolor.css        currentcolor.nodes      \
	font.ui			font.css		font.nodes		\
        gradient.ui    	 	gradient.css            gradient.nodes          \
        inherit.ui      	inherit.css             inherit.nodes           \
	label.ui		label.css	        label.nodes             \
	nth-child.ui		nth-child.css	        nth-child.nodes         \
	$(NULL)

BUILT_SOURCES = resources.c

resource_files = $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(builddir)/test-css-style.gresource.xml)

resources.c: test-css-style.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $(srcdir)/test-css-style.gresource.xml

EXTRA_DIST += \
	test-css-style.gresource.xml	\
	$(resource_files)		\
	$(test_in_files) 		\
	$(test_data)			\
	$(NULL)

if BUILDOPT_INSTALL_TESTS
insttestdir=$(libexecdir)/installed-tests/$(PACKAGE)/css/style
insttest_PROGRAMS = $(TEST_PROGS)
insttest_DATA = $(test_data)

substitutions = \
        -e s,@libexecdir\@,$(libexecdir),g \
        $(NULL)

test_files = $(test_in_files:.test.in=.test)

$(test_files): %.test: %.test.in
	$(AM_V_GEN) sed $(substitutions) $< > $@.tmp && mv $@.tmp $@

EXTRA_DIST += \
	$(test_files) 		\
	$(NULL)

CLEANFILES += $(test_files)

testmetadir = $(datadir)/installed-tests/$(PACKAGE)/css/style
testmeta_DATA = $(test_files)
endif


-include $(top_srcdir)/git.mk
