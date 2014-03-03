include $(top_srcdir)/Makefile.decl

SUBDIRS = file lpr

if HAVE_CLOUDPRINT
SUBDIRS += cloudprint
endif

if HAVE_CUPS
SUBDIRS += cups
endif

if TEST_PRINT_BACKEND
SUBDIRS += test
endif

if HAVE_PAPI
SUBDIRS += papi
endif

DIST_SUBDIRS = cloudprint cups file lpr test papi

-include $(top_srcdir)/git.mk
