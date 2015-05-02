include $(top_srcdir)/Makefile.decl

NULL =

test_simplify = \
	simplify/test1.ui simplify/test1.expected \
	simplify/test2.ui simplify/test2.expected \
	simplify/test3.ui simplify/test3.expected \
	simplify/test4.ui simplify/test4.expected \
	simplify/test5.ui simplify/test5.expected \
	simplify/test6.ui simplify/test6.expected \
	simplify/test7.ui simplify/test7.expected \
	simplify/test8.ui simplify/test8.expected \
	$(NULL)

EXTRA_DIST += \
	$(test_simplify)

TESTS_ENVIRONMENT = \
	GTK_BUILDER_TOOL="$(top_builddir)/gtk/gtk-builder-tool"

TEST_PROGS += \
	test-simplify	\
	$(NULL)

-include $(top_srcdir)/git.mk
