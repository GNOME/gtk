include $(top_srcdir)/Makefile.decl

NULL=

noinst_PROGRAMS = $(TEST_PROGS)

AM_CPPFLAGS = 				\
	$(GTK_DEP_CFLAGS) 		\
	-I$(top_srcdir) 		\
	-I$(top_builddir)/gdk 		\
	$(NULL)

LDADD = 				\
	$(GTK_DEP_LIBS) 		\
	$(top_builddir)/gtk/libgtk-4.la \
	$(NULL)

#TEST_PROGS              += check-gdk-cairo

TEST_PROGS += 				\
	cairo				\
	cursor				\
	display				\
	encoding			\
	keysyms				\
	rectangle			\
	rgba				\
	seat				\
	$(NULL)

CLEANFILES = 			\
	cairosurface.png	\
	gdksurface.png		\
	$(NULL)

if BUILDOPT_INSTALL_TESTS
insttestdir=$(libexecdir)/installed-tests/$(PACKAGE)
insttest_PROGRAMS = $(TEST_PROGS)

%.test: %$(EXEEXT) Makefile
	$(AM_V_GEN) (echo '[Test]' > $@.tmp; \
	echo 'Type=session' >> $@.tmp; \
	echo 'Output=TAP' >> $@.tmp; \
	echo 'Exec=$(insttestdir)/$< --tap' >> $@.tmp; \
	mv $@.tmp $@)

test_files = $(TEST_PROGRS:=.test)

DISTCLEANFILES = $(test_files)

testmetadir = $(datadir)/installed-tests/$(PACKAGE)
testmeta_DATA = $(test_files)
endif

-include $(top_srcdir)/git.mk
