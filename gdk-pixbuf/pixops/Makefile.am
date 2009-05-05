include $(top_srcdir)/Makefile.decl

noinst_LTLIBRARIES = libpixops.la

INCLUDES = \
	-I$(top_srcdir) -I$(top_builddir) 	\
	$(GTK_DEBUG_FLAGS)			\
	$(GDK_PIXBUF_DEP_CFLAGS)

noinst_PROGRAMS = timescale

timescale_SOURCES = timescale.c
timescale_LDADD = libpixops.la $(GLIB_LIBS) $(GDK_PIXBUF_DEP_LIBS)

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

EXTRA_DIST +=				\
	DETAILS				\
	pixbuf-transform-math.ltx	\
	makefile.msc

-include $(top_srcdir)/git.mk
