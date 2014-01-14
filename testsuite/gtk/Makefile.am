include $(top_srcdir)/Makefile.decl

NULL =

AM_CPPFLAGS = \
	-I$(top_srcdir)			\
	-I$(top_builddir)/gdk		\
	-I$(top_srcdir)/gdk		\
	-I$(top_builddir)/gtk		\
	-I$(top_srcdir)/gtk		\
	$(GTK_DEBUG_FLAGS)		\
	$(GTK_DEP_CFLAGS)

if OS_UNIX
AM_CPPFLAGS += -DHAVE_UNIX_PRINT_WIDGETS
endif

DEPS = \
	$(top_builddir)/gtk/libgtk-3.la

LDADD = \
	$(top_builddir)/gtk/libgtk-3.la	\
	$(top_builddir)/gdk/libgdk-3.la	\
	$(GTK_DEP_LIBS)

noinst_PROGRAMS = $(TEST_PROGS)

TEST_PROGS += 			\
	accel			\
	accessible		\
	action			\
	bitmask			\
	builder			\
	cellarea		\
	clipboard		\
	defaultvalue		\
	entry			\
	expander		\
	floating		\
	grid			\
	gtkmenu			\
	keyhash			\
	listbox			\
	no-gtk-init		\
	object			\
	objects-finalize	\
	papersize		\
	rbtree			\
	recentmanager		\
	regression-tests	\
	stylecontext		\
	templates		\
	textbuffer		\
	textiter		\
	treemodel		\
	treepath		\
	treeview		\
	window			\
	displayclose		\
	$(NULL)

if OS_UNIX
#TEST_PROGS			+= defaultvalue
endif

#TEST_PROGS			+= testing
#TEST_PROGS			+= treeview-scrolling
#TEST_PROGS			+= object
#TEST_PROGS			+= filechooser

# this doesn't work in make distcheck, since running
# on a naked X server creates slightly different event
# sequences than running on a normal desktop
# TEST_PROGS			+= crossingevents

treemodel_SOURCES = 		\
	treemodel.h 		\
	treemodel.c 		\
	liststore.c 		\
	treestore.c 		\
	filtermodel.c 		\
	sortmodel.c 		\
	modelrefcount.c 	\
	gtktreemodelrefcount.h 	\
	gtktreemodelrefcount.c	\
	$(NULL)

builder_LDFLAGS = -export-dynamic

rbtree_CFLAGS  = -DGTK_COMPILATION -UG_ENABLE_DEBUG
rbtree_LDADD = $(GTK_DEP_LIBS)
rbtree_SOURCES = 			\
	rbtree.c 			\
	$(top_srcdir)/gtk/gtkrbtree.h 	\
	$(top_srcdir)/gtk/gtkrbtree.c	\
	$(NULL)

bitmask_CFLAGS  = -DGTK_COMPILATION -UG_ENABLE_DEBUG
bitmask_LDADD = $(GTK_DEP_LIBS)
bitmask_SOURCES = 					\
	bitmask.c 					\
	$(top_srcdir)/gtk/gtkbitmaskprivate.h 		\
	$(top_srcdir)/gtk/gtkallocatedbitmaskprivate.h 	\
	$(top_srcdir)/gtk/gtkallocatedbitmask.c		\
	$(NULL)

keyhash_CFLAGS =					\
	-DGTK_COMPILATION 				\
	-DGTK_LIBDIR=\"$(libdir)\" 			\
	-DGTK_DATADIR=\"$(datadir)\" 			\
	-DGTK_DATA_PREFIX=\"$(prefix)\" 		\
	-DGTK_SYSCONFDIR=\"$(sysconfdir)\"		\
	$(NULL)

keyhash_SOURCES	=					\
	keyhash.c 					\
	$(top_srcdir)/gtk/gtkkeyhash.c 			\
	$(top_srcdir)/gtk/gtkkeyhash.h 			\
	$(top_builddir)/gtk/gtkresources.c 		\
	$(top_builddir)/gtk/gtkresources.h 		\
	$(top_srcdir)/gtk/gtkprivate.c 			\
	$(top_srcdir)/gtk/gtkprivate.h			\
	$(NULL)



EXTRA_DIST +=				\
	file-chooser-test-dir/empty     \
	file-chooser-test-dir/text.txt	\
	$(NULL)

GTK_GSETTINGS_SCHEMAS = \
	$(top_srcdir)/gtk/org.gtk.Settings.ColorChooser.gschema.xml \
	$(top_srcdir)/gtk/org.gtk.Settings.FileChooser.gschema.xml \
	$(NULL)

BUILT_SOURCES = gschemas.compiled

CLEANFILES = gschemas.compiled

gschemas.compiled: $(GTK_GSETTINGS_SCHEMAS)
	$(GLIB_COMPILE_SCHEMAS) \
		$(addprefix --schema-file=,$(GTK_GSETTINGS_SCHEMAS)) \
		--targetdir=$(builddir)

all-am: gschemas.compiled

if BUILDOPT_INSTALL_TESTS
insttestdir = $(pkglibexecdir)/installed-tests
insttest_PROGRAMS = $(TEST_PROGS)

%.test: %$(EXEEXT) Makefile
	$(AM_V_GEN) (echo '[Test]' > $@.tmp; \
	echo 'Type=session' >> $@.tmp; \
	echo 'Exec=$(insttestdir)/$<' >> $@.tmp; \
	mv $@.tmp $@)

test_files = $(TEST_PROGS:=.test)

DISTCLEANFILES = $(test_files)

testmetadir = $(datadir)/installed-tests/$(PACKAGE)
testmeta_DATA = $(test_files)
endif

-include $(top_srcdir)/git.mk
