## Makefile.am for gtk+/demos
include $(top_srcdir)/Makefile.decl

SUBDIRS = gtk-demo

INCLUDES = 				\
	-I$(top_srcdir) 		\
	-I$(top_builddir)/gdk		\
	-DGDK_DISABLE_DEPRECATED	\
	-DGTK_DISABLE_DEPRECATED	\
	$(GTK_DEBUG_FLAGS)		\
	$(GTK_DEP_CFLAGS)

DEPS = \
	$(top_builddir)/gtk/libgtk-3.la

LDADDS = \
	$(top_builddir)/gtk/libgtk-3.la	\
	$(top_builddir)/gdk/libgdk-3.la	\
	$(GTK_DEP_LIBS)				\
	$(MATH_LIB)

noinst_PROGRAMS = 		\
	testanimation 		\
	testpixbuf-color	\
	testpixbuf-save		\
	testpixbuf-scale 	\
	pixbuf-demo

testpixbuf_save_DEPENDENCIES = $(DEPS)
testpixbuf_color_DEPENDENCIES = $(DEPS)
testpixbuf_scale_DEPENDENCIES = $(DEPS)
testanimation_DEPENDENCIES = $(DEPS)
pixbuf_demo_DEPENDENCIES = $(DEPS)

testpixbuf_save_LDADD = $(LDADDS)
testpixbuf_color_LDADD = $(LDADDS)
testpixbuf_scale_LDADD = $(LDADDS)
testanimation_LDADD = $(LDADDS)
pixbuf_demo_LDADD = $(LDADDS)

testpixbuf_save_SOURCES = testpixbuf-save.c
testpixbuf_color_SOURCES = testpixbuf-color.c
testpixbuf_scale_SOURCES = testpixbuf-scale.c pixbuf-init.c
testanimation_SOURCES = testanimation.c pixbuf-init.c
pixbuf_demo_SOURCES = pixbuf-demo.c pixbuf-init.c

EXTRA_DIST +=			\
	apple-red.png		\
	background.jpg		\
	gnome-applets.png	\
	gnome-calendar.png	\
	gnome-foot.png		\
	gnome-gimp.png		\
	gnome-gmush.png		\
	gnome-gsame.png		\
	gnu-keys.png

-include $(top_srcdir)/git.mk
