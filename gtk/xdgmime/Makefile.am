INCLUDES = -DXDG_PREFIX=_gtk_xdg

noinst_LTLIBRARIES = libxdgmime.la

libxdgmime_la_SOURCES = \
	xdgmime.c 	\
	xdgmimeglob.c 	\
	xdgmimeint.c 	\
	xdgmimemagic.c

noinst_PROGRAMS = test-mime

test_mime_LDADD = libxdgmime.la
test_mime_SOURCES = test-mime.c

