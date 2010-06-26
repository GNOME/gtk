include $(top_srcdir)/Makefile.decl

INCLUDES = \
	-I$(top_srcdir)			\
	-I$(top_builddir)/gdk		\
	-I$(top_srcdir)/gdk		\
	-DGDK_DISABLE_DEPRECATED	\
	-DGTK_DISABLE_DEPRECATED	\
	$(GTK_DEBUG_FLAGS)		\
	$(GTK_DEP_CFLAGS)

DEPS = \
	$(top_builddir)/gdk/$(gdktargetlib)				\
	$(top_builddir)/gtk/$(gtktargetlib)

progs_ldadd = \
	$(top_builddir)/gdk/$(gdktargetlib)				\
	$(top_builddir)/gtk/$(gtktargetlib)				\
	$(GTK_DEP_LIBS)

noinst_PROGRAMS = $(TEST_PROGS)


TEST_PROGS			+= testing
testing_SOURCES			 = testing.c
testing_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= liststore
liststore_SOURCES		 = liststore.c
liststore_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= treestore
treestore_SOURCES		 = treestore.c
treestore_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= treeview
treeview_SOURCES		 = treeview.c
treeview_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= treeview-scrolling
treeview_scrolling_SOURCES	 = treeview-scrolling.c
treeview_scrolling_LDADD	 = $(progs_ldadd)

TEST_PROGS			+= recentmanager
recentmanager_SOURCES 		 = recentmanager.c
recentmanager_LDADD   		 = $(progs_ldadd)

TEST_PROGS			+= floating
floating_SOURCES		 = floating.c
floating_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= object
object_SOURCES			 = object.c pixbuf-init.c
object_LDADD			 = $(progs_ldadd)

# this doesn't work in make distcheck, since running
# on a naked X server creates slightly different event
# sequences than running on a normal desktop
# TEST_PROGS			+= crossingevents
crossingevents_SOURCES		 = crossingevents.c
crossingevents_LDADD		 = $(progs_ldadd)

# this doesn't work in make distcheck, since it doesn't
# find file-chooser-test-dir 
# TEST_PROGS			+= filechooser
filechooser_SOURCES		 = filechooser.c pixbuf-init.c
filechooser_LDADD		 = $(progs_ldadd)

TEST_PROGS			+= builder
builder_SOURCES			 = builder.c
builder_LDADD			 = $(progs_ldadd)
builder_LDFLAGS			 = -export-dynamic

if OS_UNIX
TEST_PROGS			+= defaultvalue
endif
defaultvalue_SOURCES		 = defaultvalue.c pixbuf-init.c	
defaultvalue_LDADD 		 = $(progs_ldadd)

TEST_PROGS			+= textbuffer
textbuffer_SOURCES		 = textbuffer.c pixbuf-init.c
textbuffer_LDADD		 = $(progs_ldadd)

TEST_PROGS			+= filtermodel
filtermodel_SOURCES		 = filtermodel.c
filtermodel_LDADD		 = $(progs_ldadd)

TEST_PROGS			+= expander
expander_SOURCES		 = expander.c
expander_LDADD		 = $(progs_ldadd)

TEST_PROGS			+= action
action_SOURCES			 = action.c
action_LDADD			 = $(progs_ldadd)

-include $(top_srcdir)/git.mk
