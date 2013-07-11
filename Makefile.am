## Makefile.am for GTK+
include $(top_srcdir)/Makefile.decl

SRC_SUBDIRS = gdk gtk libgail-util modules demos tests testsuite examples
SUBDIRS = po po-properties $(SRC_SUBDIRS) docs m4macros build

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

EXTRA_DIST +=			\
	autogen.sh		\
	HACKING			\
	makecopyright		\
	NEWS.pre-1-0		\
	README.commits	\
	README.win32		\
	config.h.win32		\
	makefile.msc	\
	gtk-zip.sh.in		\
	sanitize-la.sh		\
	po/README.translators	\
	po/po2tbl.sed.in

MAINTAINERCLEANFILES = \
	$(GITIGNORE_MAINTAINERCLEANFILES_TOPLEVEL) \
	$(GITIGNORE_MAINTAINERCLEANFILES_M4_LIBTOOL) \
	$(GITIGNORE_MAINTAINERCLEANFILES_MAKEFILE_IN) \
	$(srcdir)/INSTALL \
	$(srcdir)/README \
	$(srcdir)/omf.make \
	$(srcdir)/xmldocs.make \
	$(srcdir)/gtk-doc.make \
	$(srcdir)/ChangeLog


## Copy .pc files to target-specific names
gtk+-x11-3.0.pc gtk+-win32-3.0.pc gtk+-quartz-3.0.pc gtk+-broadway-3.0.pc gtk+-wayland-3.0.pc: gtk+-3.0.pc
	rm -f $@ && \
	cp gtk+-3.0.pc $@

gdk-x11-3.0.pc gdk-win32-3.0.pc gdk-quartz-3.0.pc gdk-broadway-3.0.pc gdk-wayland-3.0.pc: gdk-3.0.pc
	rm -f $@ && \
	cp gdk-3.0.pc $@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gdk-3.0.pc gtk+-3.0.pc gail-3.0.pc

pkgconfig_DATA += ${GDK_BACKENDS:%=gtk+-%-3.0.pc}
pkgconfig_DATA += ${GDK_BACKENDS:%=gdk-%-3.0.pc}

if OS_UNIX
pkgconfig_DATA += gtk+-unix-print-3.0.pc
endif

DISTCLEANFILES = \
	gtk+-unix-print-3.0.pc		\
	gtk+-3.0.pc			\
	gtk+-x11-3.0.pc			\
	gtk+-win32-3.0.pc		\
	gtk+-quartz-3.0.pc		\
	gtk+-broadway-3.0.pc		\
	gtk+-wayland-3.0.pc		\
	gdk-3.0.pc			\
	gdk-x11-3.0.pc			\
	gdk-win32-3.0.pc		\
	gdk-quartz-3.0.pc		\
	gdk-broadway-3.0.pc		\
	gdk-wayland-3.0.pc		\
	gail-3.0.pc			\
	config.lt

distclean-local:
	if test "$(srcdir)" = "."; then :; else \
		rm -f ChangeLog; \
	fi

ChangeLog:
	$(AM_V_GEN) if test -d "$(srcdir)/.git"; then \
	  (GIT_DIR=$(top_srcdir)/.git ./missing --run git log GTK_2_16_0^^.. --stat) | fmt --split-only > $@.tmp \
	  && mv -f $@.tmp $@ \
	  || ($(RM) $@.tmp; \
	      echo Failed to generate ChangeLog, your ChangeLog may be outdated >&2; \
	      (test -f $@ || echo git-log is required to generate this file >> $@)); \
	else \
	  test -f $@ || \
	  (echo A git checkout and git-log is required to generate ChangeLog >&2 && \
	  echo A git checkout and git-log is required to generate this file >> $@); \
	fi

uninstall-local:
	rm -f $(DESTDIR)$(pkgconfigdir)/gtk+-3.0.pc

dist-hook:
	if test -f $(srcdir)/INSTALL.in && test -f $(srcdir)/README.in ; then \
	     CONFIG_FILES="INSTALL:$(srcdir)/INSTALL.in README:$(srcdir)/README.in" \
	     CONFIG_HEADERS= \
	     $(SHELL) config.status \
	     && cp INSTALL README $(distdir) ; \
	fi

.PHONY: files release sanity snapshot ChangeLog

files:
	@files=`ls $(DISTFILES) 2> /dev/null `; for p in $$files; do \
	  echo $$p; \
	done
	@for subdir in $(SUBDIRS); do \
	  files=`cd $$subdir; $(MAKE) files | grep -v "make\[[1-9]\]"`; \
	  for file in $$files; do \
	    echo $$subdir/$$file; \
	  done; \
	done

release:
	rm -rf .deps */.deps
	$(MAKE) distcheck

sanity:
	./sanity_check $(VERSION)


snapshot:
	$(MAKE) dist distdir=$(PACKAGE)-snap`date +"%Y%m%d"`

DISTCHECK_CONFIGURE_FLAGS =		\
		--enable-gtk-doc 	\
		--enable-man 		\
		--disable-rebuilds 	\
		--disable-maintainer-mode \
		--enable-introspection

GITIGNOREFILES = \
	po-properties/Makefile.in.in \
	po-properties/Makefile.in \
	po-properties/Makefile \
	po-properties/*.gmo \
	po-properties/*.mo \
	po-properties/POTFILES \
	po-properties/stamp-it \
	po-properties/.intltool-merge-cache

-include $(top_srcdir)/git.mk
