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
	$(test_simplify)	\
	$(NULL)

TESTS_ENVIRONMENT = \
	GTK_BUILDER_TOOL="$(top_builddir)/gtk/gtk-builder-tool"

TEST_PROGS += \
	test-simplify	\
	$(NULL)

if BUILDOPT_INSTALL_TESTS
insttestdir = $(libexecdir)/installed-tests/$(PACKAGE)
insttest_SCRIPTS = $(TEST_PROGS)
nobase_insttest_DATA = $(test_simplify)

test-simplify.test: test-simplify Makefile
	$(AM_V_GEN) (echo '[Test]' > $@.tmp; \
	echo 'Type=session' >> $@.tmp; \
	echo 'Exec env G_ENABLE_DIAGNOSTIC=0 TEST_DATA_DIR="$(insttestdir)/simplify" $(insttestdir)/$<' >> $@.tmp; \
	mv $@.tmp $@)

test_files = $(TEST_PROGS:=.test)

DISTCLEANFILES = \
	$(test_files)		\
	$(NULL)

testmetadir = #(datadir)/installed-tests/$(PACKAGE)
testmeta_DATA = $(test_files)
endif

-include $(top_srcdir)/git.mk
