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
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf.la	\
	$(top_builddir)/gdk/@gdktargetlib@ 		\
	$(top_builddir)/gtk/@gtktargetlib@

LDADDS = @STRIP_BEGIN@ 					\
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf.la  	\
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
	testpixbuf-scale 	\
	pixbuf-demo

testpixbuf_DEPENDENCIES = $(DEPS)
testpixbuf_drawable_DEPENDENCIES = $(DEPS)
testpixbuf_scale_DEPENDENCIES = $(DEPS)
testanimation_DEPENDENCIES = $(DEPS)
pixbuf_demo_DEPENDENCIES = $(DEPS)

testpixbuf_LDADD = $(LDADDS) -lgmodule
testpixbuf_drawable_LDADD = $(LDADDS)
testpixbuf_scale_LDADD = $(LDADDS)
testanimation_LDADD = $(LDADDS) -lgmodule
pixbuf_demo_LDADD = $(LDADDS) -lgmodule

testpixbuf_SOURCES = testpixbuf.c pixbuf-init.c
testpixbuf_drawable_SOURCES = testpixbuf-drawable.c pixbuf-init.c
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
