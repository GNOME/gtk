include $(top_srcdir)/Makefile.decl

if USE_WIN32
wimp = ms-windows
endif

# the pixbuf engine needs to be ported to GtkThemingEngine
SUBDIRS = $(wimp) # pixbuf

DIST_SUBDIRS = ms-windows pixbuf


-include $(top_srcdir)/git.mk
