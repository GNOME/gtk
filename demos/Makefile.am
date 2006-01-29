## Makefile.am for gtk+/demos

SUBDIRS = gtk-demo

INCLUDES = 						\
	-I$(top_srcdir) 				\
	-I$(top_builddir)/gdk				\
	-DGDK_PIXBUF_DISABLE_DEPRECATED			\
	-DGDK_DISABLE_DEPRECATED			\
	-DGTK_DISABLE_DEPRECATED			\
	$(GTK_DEBUG_FLAGS)				\
	$(GTK_DEP_CFLAGS)

DEPS = 									\
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la	\
	$(top_builddir)/gdk/$(gdktargetlib) 				\
	$(top_builddir)/gtk/$(gtktargetlib)

LDADDS = 								\
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la	\
	$(top_builddir)/gdk/$(gdktargetlib)        			\
	$(top_builddir)/gtk/$(gtktargetlib)

noinst_PROGRAMS = 		\
	testpixbuf-drawable 	\
	testanimation 		\
	testpixbuf-save		\
	testpixbuf-scale 	\
	pixbuf-demo

# Need to build test-inline-pixbufs.h for testpixbuf
if HAVE_PNG
noinst_PROGRAMS += testpixbuf
BUILT_SOURCES = test-inline-pixbufs.h
endif

if CROSS_COMPILING
pixbuf_csource=$(GDK_PIXBUF_CSOURCE)
pixbuf_csource_deps=
else
pixbuf_csource=GDK_PIXBUF_MODULE_FILE=$(top_builddir)/gdk-pixbuf/gdk-pixbuf.loaders $(top_builddir)/gdk-pixbuf/gdk-pixbuf-csource
pixbuf_csource_deps=$(top_builddir)/gdk-pixbuf/gdk-pixbuf-csource $(top_builddir)/gdk-pixbuf/gdk-pixbuf.loaders 
endif

test-inline-pixbufs.h: $(pixbuf_csource_deps) apple-red.png gnome-foot.png
	$(pixbuf_csource) --raw --build-list		\
	        apple_red  $(srcdir)/apple-red.png	\
                gnome_foot $(srcdir)/gnome-foot.png	\
        > test-inline-pixbufs.h				\
	|| (rm -f test-inline-pixbufs.h && false)

testpixbuf_DEPENDENCIES = $(DEPS)
testpixbuf_drawable_DEPENDENCIES = $(DEPS)
testpixbuf_save_DEPENDENCIES = $(DEPS)
testpixbuf_scale_DEPENDENCIES = $(DEPS)
testanimation_DEPENDENCIES = $(DEPS)
pixbuf_demo_DEPENDENCIES = $(DEPS)

testpixbuf_LDADD = $(LDADDS)
testpixbuf_drawable_LDADD = $(LDADDS)
testpixbuf_save_LDADD = $(LDADDS)
testpixbuf_scale_LDADD = $(LDADDS)
testanimation_LDADD = $(LDADDS)
pixbuf_demo_LDADD = $(LDADDS)

testpixbuf_SOURCES = testpixbuf.c pixbuf-init.c
testpixbuf_drawable_SOURCES = testpixbuf-drawable.c pixbuf-init.c
testpixbuf_save_SOURCES = testpixbuf-save.c
testpixbuf_scale_SOURCES = testpixbuf-scale.c pixbuf-init.c
testanimation_SOURCES = testanimation.c pixbuf-init.c
pixbuf_demo_SOURCES = pixbuf-demo.c pixbuf-init.c

EXTRA_DIST =			\
	apple-red.png		\
	background.jpg		\
	gnome-applets.png	\
	gnome-calendar.png	\
	gnome-foot.png		\
	gnome-gimp.png		\
	gnome-gmush.png		\
	gnome-gsame.png		\
	gnu-keys.png

DISTCLEANFILES = test-inline-pixbufs.h
