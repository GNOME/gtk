## Makefile.am for GTK+
include $(top_srcdir)/Makefile.decl

SRC_SUBDIRS = gdk gtk modules demos tests perf
SUBDIRS = po po-properties $(SRC_SUBDIRS) docs m4macros build

# require automake 1.4
AUTOMAKE_OPTIONS = 1.7
ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST +=			\
	HACKING			\
	makecopyright		\
	NEWS.pre-1-0		\
	ChangeLog.pre-1-0	\
	ChangeLog.pre-1-2	\
	ChangeLog.pre-2-0	\
	ChangeLog.pre-2-2	\
	ChangeLog.pre-2-4	\
	ChangeLog.pre-2-6	\
	ChangeLog.pre-2-8	\
	ChangeLog.pre-2-10	\
	ChangeLog.pre-2-12	\
	ChangeLog.pre-2-14	\
	ChangeLog.pre-2-16	\
	ChangeLog.gtk-async-file-chooser	\
	ChangeLog.gtk-printing	\
	README.commits	\
	README.win32		\
	config.h.win32		\
	makefile.msc	\
	gtk-zip.sh.in		\
	sanitize-la.sh		\
	po/README.translators	\
	po/po2tbl.sed.in	\
	examples/aspectframe/Makefile  \
	examples/aspectframe/aspectframe.c  \
	examples/Makefile  \
	examples/README.1ST  \
	examples/extract.awk  \
	examples/extract.sh  \
	examples/arrow/Makefile	\
	examples/arrow/arrow.c	\
	examples/base/Makefile  \
	examples/base/base.c  \
	examples/buttonbox/Makefile \
	examples/buttonbox/buttonbox.c	\
	examples/buttons/Makefile  \
	examples/buttons/buttons.c  \
	examples/buttons/info.xpm  \
	examples/calendar/Makefile  \
	examples/calendar/calendar.c  \
	examples/clist/Makefile  \
	examples/clist/clist.c  \
	examples/entry/Makefile  \
	examples/entry/entry.c  \
	examples/eventbox/Makefile  \
	examples/eventbox/eventbox.c  \
	examples/filesel/Makefile  \
	examples/filesel/filesel.c  \
	examples/gtkdial/Makefile  \
	examples/gtkdial/dial_test.c  \
	examples/gtkdial/gtkdial.c  \
	examples/gtkdial/gtkdial.h  \
	examples/helloworld/Makefile  \
	examples/helloworld/helloworld.c  \
	examples/helloworld2/Makefile  \
	examples/helloworld2/helloworld2.c  \
	examples/label/Makefile	\
	examples/label/label.c	\
	examples/list/Makefile  \
	examples/list/list.c  \
	examples/menu/Makefile  \
	examples/menu/menu.c  \
	examples/menu/itemfactory.c  \
	examples/notebook/Makefile  \
	examples/notebook/notebook.c  \
	examples/packbox/Makefile  \
	examples/packbox/packbox.c  \
	examples/paned/Makefile  \
	examples/paned/paned.c  \
	examples/pixmap/Makefile  \
	examples/pixmap/pixmap.c  \
	examples/progressbar/Makefile  \
	examples/progressbar/progressbar.c  \
	examples/radiobuttons/Makefile  \
	examples/radiobuttons/radiobuttons.c  \
	examples/rangewidgets/Makefile  \
	examples/rangewidgets/rangewidgets.c  \
	examples/rulers/Makefile  \
	examples/rulers/rulers.c  \
	examples/scribble-simple/Makefile  \
	examples/scribble-simple/scribble-simple.c  \
	examples/scribble-xinput/Makefile	\
	examples/scribble-xinput/scribble-xinput.c	\
	examples/scrolledwin/Makefile  \
	examples/scrolledwin/scrolledwin.c  \
	examples/selection/Makefile  \
	examples/selection/gettargets.c  \
	examples/selection/setselection.c  \
	examples/statusbar/Makefile  \
	examples/statusbar/statusbar.c  \
	examples/table/Makefile  \
	examples/table/table.c  \
	examples/text/Makefile  \
	examples/text/text.c  \
	examples/tictactoe/Makefile  \
	examples/tictactoe/tictactoe.c  \
	examples/tictactoe/tictactoe.h  \
	examples/tictactoe/ttt_test.c  \
	examples/tree/Makefile  \
	examples/tree/tree.c  \
	examples/wheelbarrow/Makefile  \
	examples/wheelbarrow/wheelbarrow.c  \
	examples/fixed/fixed.c  \
	examples/fixed/Makefile  \
	examples/frame/frame.c  \
	examples/frame/Makefile  \
	examples/spinbutton/spinbutton.c  \
	examples/spinbutton/Makefile  \
	examples/find-examples.sh
MAINTAINERCLEANFILES = \
	$(srcdir)/INSTALL \
	$(srcdir)/README \
	$(srcdir)/aclocal.m4 \
	$(srcdir)/autoscan.log \
	$(srcdir)/compile \
	$(srcdir)/config.guess \
	$(srcdir)/config.h.in \
	$(srcdir)/config.sub \
	$(srcdir)/configure.scan \
	$(srcdir)/depcomp \
	$(srcdir)/install-sh \
	$(srcdir)/ltmain.sh \
	$(srcdir)/missing \
	$(srcdir)/mkinstalldirs \
	$(srcdir)/omf.make \
	$(srcdir)/xmldocs.make \
	$(srcdir)/gtk-doc.make \
	$(srcdir)/ChangeLog \
	`find "$(srcdir)" -type f -name Makefile.in -print`

GDKTARGET=@gdktarget@

## Copy .pc files to target-specific names
gtk+-$(GDKTARGET)-2.0.pc: gtk+-2.0.pc
	rm -f gtk+-$(GDKTARGET)-2.0.pc && \
	cp gtk+-2.0.pc gtk+-$(GDKTARGET)-2.0.pc

gdk-$(GDKTARGET)-2.0.pc: gdk-2.0.pc
	rm -f gdk-$(GDKTARGET)-2.0.pc && \
	cp gdk-2.0.pc gdk-$(GDKTARGET)-2.0.pc

gtk+-$(GDKTARGET)-2.0-uninstalled.pc: gtk+-2.0-uninstalled.pc
	rm -f gtk+-$(GDKTARGET)-2.0-uninstalled.pc && \
	cp gtk+-2.0-uninstalled.pc gtk+-$(GDKTARGET)-2.0-uninstalled.pc

gdk-$(GDKTARGET)-2.0-uninstalled.pc: gdk-2.0-uninstalled.pc
	rm -f gdk-$(GDKTARGET)-2.0-uninstalled.pc && \
	cp gdk-2.0-uninstalled.pc gdk-$(GDKTARGET)-2.0-uninstalled.pc

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA= gdk-$(GDKTARGET)-2.0.pc gtk+-$(GDKTARGET)-2.0.pc gail.pc

if OS_UNIX
pkgconfig_DATA += gtk+-unix-print-2.0.pc
endif

DISTCLEANFILES =				\
	gtk+-unix-print-2.0.pc			\
	gtk+-$(GDKTARGET)-2.0.pc		\
	gdk-$(GDKTARGET)-2.0.pc			\
	gail.pc					\
	gtk+-$(GDKTARGET)-2.0-uninstalled.pc	\
	gdk-$(GDKTARGET)-2.0-uninstalled.pc	\
	gail-uninstalled.pc 			\
	config.lt

distclean-local:
	if test "$(srcdir)" = "."; then :; else \
		rm -f ChangeLog; \
	fi

ChangeLog:
	@echo Creating $@
	@if test -d "$(srcdir)/.git"; then \
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

## copy the default target for this platform to gdk-2.0.pc and gtk+-2.0.pc
DEFAULT_GDKTARGET=x11
install-data-hook:
	(cd $(DESTDIR)$(pkgconfigdir) && \
	 test -f gdk-$(DEFAULT_GDKTARGET)-2.0.pc && \
	 test -f gtk+-$(DEFAULT_GDKTARGET)-2.0.pc && \
	 rm -f gdk-2.0.pc && cp -f gdk-$(DEFAULT_GDKTARGET)-2.0.pc gdk-2.0.pc && \
	 rm -f gtk+-2.0.pc && cp -f gtk+-$(DEFAULT_GDKTARGET)-2.0.pc gtk+-2.0.pc) || \
	(cd $(DESTDIR)$(pkgconfigdir) && \
	 rm -f gdk-2.0.pc && cp -f gdk-$(GDKTARGET)-2.0.pc gdk-2.0.pc && \
	 rm -f gtk+-2.0.pc && cp -f gtk+-$(GDKTARGET)-2.0.pc gtk+-2.0.pc)

uninstall-local:
	rm -f $(DESTDIR)$(pkgconfigdir)/gdk-2.0.pc
	rm -f $(DESTDIR)$(pkgconfigdir)/gtk+-2.0.pc

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

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc --enable-man --disable-rebuilds --enable-introspection

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
