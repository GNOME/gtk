noinst_LTLIBRARIES = libpixops.la

INCLUDES = $(GLIB_CFLAGS) $(GTK_CFLAGS) -I$(top_srcdir)/gdk-pixbuf

noinst_PROGRAMS = timescale

timescale_SOURCES = timescale.c
timescale_LDADD = libpixops.la $(GLIB_LIBS) -lm

if USE_MMX
mmx_sources =				\
	have_mmx.S			\
	scale_line_22_33_mmx.S 		\
	composite_line_22_4a4_mmx.S 	\
	composite_line_color_22_4a4_mmx.S
endif

libpixops_la_SOURCES =  		\
	pixops.c			\
	pixops.h			\
	pixops-internal.h		\
	$(mmx_sources)

EXTRA_DIST =				\
	DETAILS