include $(top_srcdir)/Makefile.decl

SUBDIRS = file lpr

if HAVE_CUPS
SUBDIRS += cups
endif

if TEST_PRINT_BACKEND
SUBDIRS += test
endif

DIST_SUBDIRS = cups file lpr test
