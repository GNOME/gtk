## Makefile.am for gtk+/demos

INCLUDES = @STRIP_BEGIN@ \
	-I$(top_srcdir) 				\
	-I$(top_builddir)/gdk				\
	-DGTK_DISABLE_COMPAT_H				\
	@GTK_DEBUG_FLAGS@				\
	@GTK_XIM_FLAGS@					\
	@GTK_LOCALE_FLAGS@				\
	@PANGO_CFLAGS@					\
	@GLIB_CFLAGS@					\
	@more_cflags@					\
@STRIP_END@

DEPS = \
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-1.3.la	\
	$(top_builddir)/gdk/@gdktargetlib@ 		\
	$(top_builddir)/gtk/@gtktargetlib@

LDADDS = @STRIP_BEGIN@ 					\
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-1.3.la	\
	$(top_builddir)/gdk/@gdktargetlib@        	\
	$(top_builddir)/gtk/@gtktargetlib@ 		\
	@more_ldflags@					\
	@more_libs@					\
	@GDK_WLIBS@					\
	@PANGO_LIBS@					\
	@GLIB_LIBS@					\
	@GTK_LIBS_EXTRA@				\
	-lm						\
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
	GDK_PIXBUF_MODULEDIR=$(top_builddir)/gdk-pixbuf/.libs $(top_builddir)/gdk-pixbuf/make-inline-pixbuf test-inline-pixbufs.h apple_red apple-red.png gnome_foot gnome-foot.png 

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
