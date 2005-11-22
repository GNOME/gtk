INCLUDES = 						\
	-I$(top_srcdir) 				\
	-I$(top_builddir)/gdk				\
	-I$(top_srcdir)/gdk				\
	-I$(top_srcdir)/gdk/x11				\
	$(GTK_DEBUG_FLAGS)				\
	$(GTK_DEP_CFLAGS)

DEPS = 									\
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la	\
	$(top_builddir)/gdk/$(gdktargetlib) 				\
	$(top_builddir)/gtk/$(gtktargetlib)

LDADDS = 								\
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la	\
	$(top_builddir)/gdk/$(gdktargetlib)        			\
	$(top_builddir)/gtk/$(gtktargetlib)				\
	$(GTK_DEP_LIBS)

if USE_X11
noinst_PROGRAMS = 		\
	doc-shooter
endif

doc_shooter_DEPENDENCIES = $(DEPS)
doc_shooter_LDADD = $(LDADDS)
doc_shooter_SOURCES=	\
	shadow.c	\
	shadow.h	\
	shooter.c	\
	widgets.c	\
	widgets.h

clean-local:
	rm -f *.png
