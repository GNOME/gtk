## Makefile.am for gtk+/demos

SUBDIRS = gtk-demo

INCLUDES = @STRIP_BEGIN@ \
	-I$(top_srcdir) 				\
	-I$(top_builddir)/gdk				\
	-DGTK_DISABLE_COMPAT_H				\
	-DGTK_DISABLE_DEPRECATED			\
	@GTK_DEBUG_FLAGS@				\
	@GTK_DEP_CFLAGS@				\
@STRIP_END@

DEPS = \
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-1.3.la	\
	$(top_builddir)/gdk/@gdktargetlib@ 		\
	$(top_builddir)/gtk/@gtktargetlib@

LDADDS = @STRIP_BEGIN@ 					\
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-1.3.la	\
	$(top_builddir)/gdk/@gdktargetlib@        	\
	$(top_builddir)/gtk/@gtktargetlib@ 		\
@STRIP_END@

noinst_PROGRAMS = 		\
	testpixbuf 		\
	testpixbuf-drawable 	\
	testanimation 		\
	testpixbuf-save		\
	testpixbuf-scale 	\
	pixbuf-demo

BUILT_SOURCES=test-inline-pixbufs.h

test-inline-pixbufs.h: $(top_builddir)/gdk-pixbuf/make-inline-pixbuf apple-red.png gnome-foot.png
	(topdir=`cd $(top_builddir) && pwd` ; curdir=`pwd` ; \
         cd $(srcdir) &&				     \
            GDK_PIXBUF_MODULEDIR=$$topdir/gdk-pixbuf/.libs $$topdir/gdk-pixbuf/make-inline-pixbuf $$curdir/test-inline-pixbufs.h  apple_red apple-red.png gnome_foot gnome-foot.png)

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
