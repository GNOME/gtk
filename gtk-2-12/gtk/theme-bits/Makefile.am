INCLUDES = -I$(top_srcdir) -I$(top_builddir) 	\
	-DGDK_PIXBUF_DISABLE_DEPRECATED		\
	$(GDK_PIXBUF_DEP_CFLAGS)

noinst_PROGRAMS = decompose-bits

decompose_bits_SOURCES = decompose-bits.c
decompose_bits_LDADD = $(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la

EXTRA_DIST = check-13.png check-13-inconsistent.png radio-13.png radio-13-inconsistent.png
