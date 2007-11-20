INCLUDES =                                      \
	   -I$(top_srcdir)                      \
	   -I$(top_builddir)/gdk                \
	   -I$(top_srcdir)/gdk                  \
	   -DGDK_PIXBUF_DISABLE_DEPRECATED	\
	   -DGDK_DISABLE_DEPRECATED             \
	   -DGTK_DISABLE_DEPRECATED             \
	   $(GTK_DEBUG_FLAGS)                   \
	   $(GTK_DEP_CFLAGS)

DEPS =	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la  \
        $(top_builddir)/gdk/$(gdktargetlib)                             \
        $(top_builddir)/gtk/$(gtktargetlib)

progs_ldadd     = $(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la \
		  $(top_builddir)/gdk/$(gdktargetlib)                            \
		  $(top_builddir)/gtk/$(gtktargetlib)

GTESTER		 = gtester

TEST_PROGS	=
noinst_PROGRAMS = $(TEST_PROGS)


TEST_PROGS       += testing
testing_SOURCES	  = testing.c
testing_LDADD	  = $(progs_ldadd)





# Xvfb based test rules
XVFB = Xvfb
test-xvfb:
	${XVFB} -help 2>/dev/null 1>&2 \
	&& XID=`for id in \`seq 101 199\` ; do test -e /tmp/.X$$id-lock || { echo $$id; exit 0; }; done; exit 1` \
	&& { ${XVFB} :$$XID -nolisten tcp -auth /dev/null >/dev/null 2>&1 & \
	       trap "kill -15 $$! " 0 HUP INT QUIT TRAP USR1 PIPE TERM ; } \
	|| { echo Failed to start Xvfb environment for testing; false; } \
	&& set -e ; for prg in ${TEST_PROGS} ; do DISPLAY=:$$XID ./$$prg ; done

# exemplary unit test rules
test:
	set -e ; for prg in ${TEST_PROGS} ; do ./$$prg ; done
######### ${GTESTER} --verbose ${TEST_PROGS}
test-report:
	$(MAKE) test
######### ${GTESTER} --verbose -k -o testreport.xml ${TEST_PROGS}
.PHONY: test test-report
check-local: test

