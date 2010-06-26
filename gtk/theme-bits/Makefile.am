include $(top_srcdir)/Makefile.decl

INCLUDES = \
	-I$(top_srcdir)		\
	 -I$(top_builddir) 	\
	$(BASE_DEPENDENCIES_CFLAGS)

noinst_PROGRAMS = decompose-bits

decompose_bits_SOURCES = decompose-bits.c
decompose_bits_LDADD = $(BASE_DEPENDENCIES_LIBS)

EXTRA_DIST += check-13.png check-13-inconsistent.png radio-13.png radio-13-inconsistent.png

-include $(top_srcdir)/git.mk
