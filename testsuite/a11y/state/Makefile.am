include $(top_srcdir)/Makefile.decl

check_PROGRAMS = $(TEST_PROGS)

AM_CPPFLAGS =                           \
        -I$(top_srcdir)                 \
        -I$(top_builddir)/gdk           \
        -I$(top_srcdir)/gdk             \
        -DGDK_DISABLE_DEPRECATED        \
        -DGTK_DISABLE_DEPRECATED        \
        -DGTK_VERSION=\"$(GTK_VERSION)\"\
        $(GTK_DEBUG_FLAGS)              \
        $(GTK_DEP_CFLAGS)

LDADD = \
        $(top_builddir)/gdk/libgdk-3.la \
        $(top_builddir)/gtk/libgtk-3.la \
        $(GTK_DEP_LIBS)

TESTS_ENVIRONMENT =                     \
        GIO_USE_VOLUME_MONITOR=unix     \
        GSETTINGS_BACKEND=memory        \
        G_ENABLE_DIAGNOSTIC=0

TEST_PROGS += state-record

testdata = \
	focus1.ui focus1.in focus1.out \
	focus2.ui focus2.in focus2.out

EXTRA_DIST += $(testdata)

if BUILDOPT_INSTALL_TESTS
insttestdir = $(libexecdir)/installed-tests/$(PACKAGE)
insttest_PROGRAMS = $(TEST_PROGS)

statetestdir = $(insttestdir)/state
statetest_DATA = $(testdata)

a11ystate.test: Makefile
	$(AM_V_GEN) (echo '[Test]' > $@.tmp; \
	echo 'Type=session-exclusive' >> $@.tmp; \
	echo 'Exec=env G_ENABLE_DIAGNOSTIC=0 $(insttestdir)/state-record --directory $(statetestdir)' >> $@.tmp; \
	mv $@.tmp $@)

testfiles = a11ystate.test

DISTCLEANFILES = \
	$(testdata) \
	$(testfiles)

testmetadir = $(datadir)/installed-tests/$(PACKAGE)
testmeta_DATA = $(testfiles)
endif

-include $(top_srcdir)/git.mk
