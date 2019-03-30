include $(top_srcdir)/Makefile.decl

SUBDIRS = input

if OS_UNIX
SUBDIRS += printbackends
endif

EXTRA_DIST += \
	meson.build

-include $(top_srcdir)/git.mk
