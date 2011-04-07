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
	does-not-exist.css \
	does-not-exist.ref.css \
	no-semicolon.css \
	no-semicolon.ref.css \
	simple.css
