SUBDIRS = pixmap_data win95_data motif_data metal_data

CFLAGS = $(GTK_CFLAGS) $(GDK_IMLIB_CFLAGS)
LIBS = $(GTK_LIBS) $(GDK_IMLIB_LIBS)

enginedir = $(libdir)/gtk/themes/engines
engine_LTLIBRARIES = libpixmap.la libwin95.la libmotif.la libmetal.la

###### pixmap theme -> libpixmap.so

libpixmap_la_SOURCES = \
	pixmap_theme_draw.c	\
	pixmap_theme_main.c	\
	pixmap_theme.h

libpixmap_la_LDFLAGS = -export-dynamic

###### win95 theme -> libwin95.so
libwin95_la_SOURCES = \
	win95_theme_draw.c	\
	win95_theme_main.c
libwin95_la_LDFLAGS = -export-dynamic

###### motif theme -> libmotif.so
libmotif_la_SOURCES = \
	motif_theme_draw.c	\
	motif_theme_main.c
libmotif_la_LDFLAGS = -export-dynamic

###### metal theme -> libmetal.so
libmetal_la_SOURCES = \
	metal_theme_draw.c	\
	metal_theme_main.c
libmetal_la_LDFLAGS = -export-dynamic

