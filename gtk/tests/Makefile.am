include $(top_srcdir)/Makefile.decl

SUBDIRS =

AM_CPPFLAGS = \
	-I$(top_srcdir)			\
	-I$(top_builddir)/gdk		\
	-I$(top_srcdir)/gdk		\
	-DSRCDIR=\""$(abs_srcdir)"\"	\
	$(GTK_DEBUG_FLAGS)		\
	$(GTK_DEP_CFLAGS)

DEPS = \
	$(top_builddir)/gtk/libgtk-3.la

progs_ldadd = \
	$(top_builddir)/gtk/libgtk-3.la	\
	$(top_builddir)/gdk/libgdk-3.la	\
	$(GTK_DEP_LIBS)

noinst_PROGRAMS = $(TEST_PROGS)


TEST_PROGS			+= testing
testing_SOURCES			 = testing.c
testing_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= treemodel
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
treeview_SOURCES		 = treeview.c
treeview_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= treeview-scrolling
treeview_scrolling_SOURCES	 = treeview-scrolling.c
treeview_scrolling_LDADD	 = $(progs_ldadd) -lm

TEST_PROGS			+= recentmanager
recentmanager_SOURCES 		 = recentmanager.c
recentmanager_LDADD   		 = $(progs_ldadd)

TEST_PROGS			+= floating
floating_SOURCES		 = floating.c
floating_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= accel
accel_SOURCES			 = accel.c
accel_LDADD			 = $(progs_ldadd)

#TEST_PROGS			+= object
#object_SOURCES			 = object.c
#object_LDADD			 = $(progs_ldadd)

# this doesn't work in make distcheck, since running
# on a naked X server creates slightly different event
# sequences than running on a normal desktop
# TEST_PROGS			+= crossingevents
#crossingevents_SOURCES		 = crossingevents.c
#crossingevents_LDADD		 = $(progs_ldadd)

# Should be ported to new API's
#TEST_PROGS			+= filechooser
#filechooser_SOURCES		 = filechooser.c
#filechooser_LDADD		 = $(progs_ldadd)

TEST_PROGS			+= builder
builder_SOURCES			 = builder.c
builder_LDADD			 = $(progs_ldadd)
builder_LDFLAGS			 = -export-dynamic

if OS_UNIX
#TEST_PROGS			+= defaultvalue
#defaultvalue_SOURCES		 = defaultvalue.c
#defaultvalue_LDADD 		 = $(progs_ldadd)
endif

TEST_PROGS			+= textbuffer
textbuffer_SOURCES		 = textbuffer.c
textbuffer_LDADD		 = $(progs_ldadd)

TEST_PROGS			+= textiter
textiter_SOURCES		 = textiter.c
textiter_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= expander
expander_SOURCES		 = expander.c
expander_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= action
action_SOURCES			 = action.c
action_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= stylecontext
stylecontext_SOURCES		 = stylecontext.c
stylecontext_LDADD		 = $(progs_ldadd)

TEST_PROGS			+= papersize
papersize_SOURCES		 = papersize.c
papersize_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= cellarea
cellarea_SOURCES		 = cellarea.c
cellarea_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= treepath
treepath_SOURCES		 = treepath.c
treepath_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= accessible
accessible_SOURCES		 = accessible.c
accessible_LDADD		 = $(progs_ldadd)

TEST_PROGS			+= entry
entry_SOURCES			 = entry.c
entry_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= grid
grid_SOURCES			 = grid.c
grid_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= rbtree
rbtree_CFLAGS			 = -DGTK_COMPILATION -UG_ENABLE_DEBUG
rbtree_SOURCES			 = rbtree.c ../gtkrbtree.h ../gtkrbtree.c
rbtree_LDADD			 = $(GTK_DEP_LIBS)

TEST_PROGS			+= bitmask
bitmask_CFLAGS			 = -DGTK_COMPILATION -UG_ENABLE_DEBUG
bitmask_SOURCES			 = bitmask.c \
				   ../gtkbitmaskprivate.h \
				   ../gtkallocatedbitmaskprivate.h \
				   ../gtkallocatedbitmask.c
bitmask_LDADD			 = $(GTK_DEP_LIBS)

TEST_PROGS			+= regression-tests
regression_tests_SOURCES	 = regression-tests.c
regression_tests_LDADD		 = $(progs_ldadd)

TEST_PROGS			+= keyhash
keyhash_SOURCES			 = keyhash.c \
				   ../gtkkeyhash.c ../gtkkeyhash.h \
				   ../gtkprivate.c ../gtkprivate.h
keyhash_LDADD			 = $(progs_ldadd)
keyhash_CFLAGS			 = -DGTK_COMPILATION \
				   -DGTK_LIBDIR=\"$(libdir)\" \
				   -DGTK_DATADIR=\"$(datadir)\" \
				   -DGTK_DATA_PREFIX=\"$(prefix)\" \
				   -DGTK_SYSCONFDIR=\"$(sysconfdir)\"

EXTRA_DIST +=				\
	file-chooser-test-dir/empty     \
	file-chooser-test-dir/text.txt

-include $(top_srcdir)/git.mk
