include $(top_srcdir)/Makefile.decl

libgdkincludedir = $(includedir)/gtk-3.0/gdk

INCLUDES = 				\
	-DG_LOG_DOMAIN=\"Gdk\"		\
	-DGDK_COMPILATION		\
	-I$(top_srcdir)			\
	-I$(top_srcdir)/gdk		\
	-I$(top_builddir)/gdk		\
	$(GTK_DEBUG_FLAGS) 		\
	$(GDK_DEP_CFLAGS)

LDADDS = $(GDK_DEP_LIBS)

noinst_LTLIBRARIES = libgdk-xcb.la

libgdk_xcb_la_SOURCES =    	\
	dragcontext.c		\
	visual.c		\
	screen.c		\
	display.c		\
	init.c			\
	window.c		\
	nothing.c

-include $(top_srcdir)/git.mk
