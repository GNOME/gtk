SUBDIRS = pixmap_data

CFLAGS = $(GTK_CFLAGS)

enginedir = $(libdir)/gtk/themes/engines
engine_LTLIBRARIES = libpixmap.la libwin95.la libmotif.la libmetal.la

###### pixmap theme -> libpixmap.so

libpixmap_la_SOURCES = \
	pixmap_theme_draw.c	\
	pixmap_theme_main.c	\
	pixmap_theme.h


libpixmap_la_LIBADD = -lgdk_imlib -ljpeg -ltiff -lgif -lpng -lz 
libpixmap_la_LDFLAGS = -export-dynamic

###### win95 theme -> libwin95.so
libwin95_la_SOURCES = \
	win95_theme_draw.c	\
	win95_theme_main.c
libwin95_la_LDFLAGS = -export-dynamic

win95dir = $(datadir)/gtk/themes/win95
win95_DATA = win95_data/gtkrc

###### motif theme -> libmotif.so
libmotif_la_SOURCES = \
	motif_theme_draw.c	\
	motif_theme_main.c
libmotif_la_LDFLAGS = -export-dynamic

motifdir = $(datadir)/gtk/themes/win95
motif_DATA = motif_data/gtkrc

###### metal theme -> libmetal.so
libmetal_la_SOURCES = \
	metal_theme_draw.c	\
	metal_theme_main.c
libmetal_la_LDFLAGS = -export-dynamic

metaldir = $(datadir)/gtk/themes/win95
metal_DATA = metal_data/gtkrc

EXTRA_DIST = $(win95_DATA) $(motif_DATA) $(metal_DATA)
