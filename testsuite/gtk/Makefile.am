include $(top_srcdir)/Makefile.decl

NULL=

SUBDIRS =

AM_CPPFLAGS = \
	-I$(top_srcdir)			\
	-I$(top_builddir)/gdk		\
	-I$(top_srcdir)/gdk		\
	-I$(top_builddir)/gtk		\
	-I$(top_srcdir)/gtk		\
	-DSRCDIR=\""$(abs_srcdir)"\"	\
	$(GTK_DEBUG_FLAGS)		\
	$(GTK_DEP_CFLAGS)

if OS_UNIX
AM_CPPFLAGS += -DHAVE_UNIX_PRINT_WIDGETS
endif

DEPS = \
	$(top_builddir)/gtk/libgtk-3.la

progs_ldadd = \
	$(top_builddir)/gtk/libgtk-3.la	\
	$(top_builddir)/gdk/libgdk-3.la	\
	$(GTK_DEP_LIBS)

test_in_files =

noinst_PROGRAMS = $(TEST_PROGS)


#TEST_PROGS			+= testing
#test_in_files			+= testing.test.in
testing_SOURCES			 = testing.c
testing_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= treemodel
test_in_files			+= treemodel.test.in
treemodel_SOURCES		 = \
				treemodel.h \
				treemodel.c \
				liststore.c \
				treestore.c \
				filtermodel.c \
				sortmodel.c \
				modelrefcount.c \
				gtktreemodelrefcount.h \
				gtktreemodelrefcount.c
treemodel_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= treeview
test_in_files			+= treeview.test.in
treeview_SOURCES		 = treeview.c
treeview_LDADD			 = $(progs_ldadd)

#TEST_PROGS			+= treeview-scrolling
#test_in_files			+= treeview-scrolling.test.in
treeview_scrolling_SOURCES	 = treeview-scrolling.c
treeview_scrolling_LDADD	 = $(progs_ldadd) -lm

TEST_PROGS			+= recentmanager
test_in_files			+= recentmanager.test.in
recentmanager_SOURCES 		 = recentmanager.c
recentmanager_LDADD   		 = $(progs_ldadd)

TEST_PROGS			+= floating
test_in_files			+= floating.test.in
floating_SOURCES		 = floating.c
floating_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= accel
test_in_files			+= accel.test.in
accel_SOURCES			 = accel.c
accel_LDADD			 = $(progs_ldadd)

#TEST_PROGS			+= object
#test_in_files			+= object.test.in
#object_SOURCES			 = object.c
#object_LDADD			 = $(progs_ldadd)

# this doesn't work in make distcheck, since running
# on a naked X server creates slightly different event
# sequences than running on a normal desktop
# TEST_PROGS			+= crossingevents
#test_in_files			+= crossingevents.test.in
#crossingevents_SOURCES		 = crossingevents.c
#crossingevents_LDADD		 = $(progs_ldadd)

#TEST_PROGS			+= filechooser
#test_in_files			+= filechooser.test.in
filechooser_SOURCES		 = filechooser.c
filechooser_LDADD		 = $(progs_ldadd)

TEST_PROGS			+= builder
test_in_files			+= builder.test.in
builder_SOURCES			 = builder.c
builder_LDADD			 = $(progs_ldadd)
builder_LDFLAGS			 = -export-dynamic

TEST_PROGS			+= templates
test_in_files			+= templates.test.in
templates_SOURCES		 = templates.c
templates_LDADD			 = $(progs_ldadd)

if OS_UNIX
#TEST_PROGS			+= defaultvalue
#test_in_files			+= defaultvalue.test.in
#defaultvalue_SOURCES		 = defaultvalue.c
#defaultvalue_LDADD 		 = $(progs_ldadd)
endif

TEST_PROGS			+= textbuffer
test_in_files			+= textbuffer.test.in
textbuffer_SOURCES		 = textbuffer.c
textbuffer_LDADD		 = $(progs_ldadd)

TEST_PROGS			+= textiter
test_in_files			+= textiter.test.in
textiter_SOURCES		 = textiter.c
textiter_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= expander
test_in_files			+= expander.test.in
expander_SOURCES		 = expander.c
expander_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= action
test_in_files			+= action.test.in
action_SOURCES			 = action.c
action_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= stylecontext
test_in_files			+= stylecontext.test.in
stylecontext_SOURCES		 = stylecontext.c
stylecontext_LDADD		 = $(progs_ldadd)

TEST_PROGS			+= papersize
test_in_files			+= papersize.test.in
papersize_SOURCES		 = papersize.c
papersize_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= cellarea
test_in_files			+= cellarea.test.in
cellarea_SOURCES		 = cellarea.c
cellarea_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= treepath
test_in_files			+= treepath.test.in
treepath_SOURCES		 = treepath.c
treepath_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= accessible
test_in_files			+= accessible.test.in
accessible_SOURCES		 = accessible.c
accessible_LDADD		 = $(progs_ldadd)

TEST_PROGS			+= entry
test_in_files			+= entry.test.in
entry_SOURCES			 = entry.c
entry_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= grid
test_in_files			+= grid.test.in
grid_SOURCES			 = grid.c
grid_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= rbtree
test_in_files			+= rbtree.test.in
rbtree_CFLAGS			 = -DGTK_COMPILATION -UG_ENABLE_DEBUG
rbtree_SOURCES			 = rbtree.c \
				   $(top_srcdir)/gtk/gtkrbtree.h \
				   $(top_srcdir)/gtk/gtkrbtree.c
rbtree_LDADD			 = $(GTK_DEP_LIBS)

TEST_PROGS			+= bitmask
test_in_files			+= bitmask.test.in
bitmask_CFLAGS			 = -DGTK_COMPILATION -UG_ENABLE_DEBUG
bitmask_SOURCES			 = bitmask.c \
				   $(top_srcdir)/gtk/gtkbitmaskprivate.h \
				   $(top_srcdir)/gtk/gtkallocatedbitmaskprivate.h \
				   $(top_srcdir)/gtk/gtkallocatedbitmask.c
bitmask_LDADD			 = $(GTK_DEP_LIBS)

TEST_PROGS			+= regression-tests
test_in_files			+= regression-tests.test.in
regression_tests_SOURCES	 = regression-tests.c
regression_tests_LDADD		 = $(progs_ldadd)

TEST_PROGS			+= keyhash
test_in_files			+= keyhash.test.in
keyhash_SOURCES			 = keyhash.c \
				   $(top_srcdir)/gtk/gtkkeyhash.c \
				   $(top_srcdir)/gtk/gtkkeyhash.h \
				   $(top_builddir)/gtk/gtkresources.c \
				   $(top_builddir)/gtk/gtkresources.h \
				   $(top_srcdir)/gtk/gtkprivate.c \
				   $(top_srcdir)/gtk/gtkprivate.h
keyhash_LDADD			 = $(progs_ldadd)
keyhash_CFLAGS			 = -DGTK_COMPILATION \
				   -DGTK_LIBDIR=\"$(libdir)\" \
				   -DGTK_DATADIR=\"$(datadir)\" \
				   -DGTK_DATA_PREFIX=\"$(prefix)\" \
				   -DGTK_SYSCONFDIR=\"$(sysconfdir)\"

TEST_PROGS			+= gtkmenu
test_in_files			+= gtkmenu.test.in
gtkmenu_SOURCES			 = gtkmenu.c
gtkmenu_LDADD			 = $(progs_ldadd)


TEST_PROGS			+= objects-finalize
test_in_files			+= objects-finalize.test.in
objects_finalize_SOURCES         = objects-finalize.c
objects_finalize_LDADD	         = $(progs_ldadd)

TEST_PROGS			+= window
test_in_files			+= window.test.in
window_SOURCES			 = window.c
window_LDADD			 = $(progs_ldadd)

EXTRA_DIST +=				\
	file-chooser-test-dir/empty     \
	file-chooser-test-dir/text.txt

if BUILDOPT_INSTALL_TESTS
insttestdir=$(pkglibexecdir)/installed-tests
insttest_PROGRAMS = $(TEST_PROGS)

substitutions = \
	-e s,@pkglibexecdir\@,$(pkglibexecdir),g \
	$(NULL)

test_files = $(test_in_files:.test.in=.test)

$(test_files): %.test: %.test.in
	$(AM_V_GEN) sed $(substitutions) $< > $@.tmp && mv $@.tmp $@

EXTRA_DIST += $(test_in_files)

testmetadir = $(datadir)/installed-tests/$(PACKAGE)
testmeta_DATA = $(test_files)
endif

-include $(top_srcdir)/git.mk
