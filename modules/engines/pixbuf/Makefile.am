INCLUDES = $(GTK_CFLAGS)

enginedir = $(libdir)/gtk/themes/engines

engine_LTLIBRARIES = libpixmap.la

libpixmap_la_SOURCES = 		\
	pixbuf-draw.c		\
	pixbuf-main.c		\
	pixbuf-render.c		\
	pixbuf.h

libpixmap_la_LDFLAGS = -module -avoid-version
libpixmap_la_LIBADD = -lgdk_pixbuf
