include $(top_srcdir)/Makefile.decl

NULL=

noinst_PROGRAMS = $(TEST_PROGS)

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir)/gdk \
	-I$(top_srcdir)/gdk \
	-I$(top_builddir)/gsk \
	-I$(top_srcdir)/gsk \
	$(GTK_DEBUG_FLAGS) \
	$(GTK_DEP_CFLAGS)

LDADD = $(GTK_DEP_LIBS)

TEST_PROGS += \
	$(NULL)

if BUILDOPT_INSTALL_TESTS
insttestdir=$(pkglibexecdir)/installed-tests
insttest_PROGRAMS = $(TEST_PROGS)

%.test: %$(EXEEXT) Makefile
	$(AM_V_GEN) (echo '[Test]' > $@.tmp; \
	echo 'Type=session' >> $@.tmp; \
	echo 'Exec=$(insttestdir)/$<' >> $@.tmp; \
	mv $@.tmp $@)

test_files = $(TEST_PROGRS:=.test)

DISTCLEANFILES = $(test_files)

testmetadir = $(datadir)/installed-tests/$(PACKAGE)
testmeta_DATA = $(test_files)
endif

-include $(top_srcdir)/git.mk
