## Makefile.am for GTK+

SRC_SUBDIRS = gdk-pixbuf gdk gtk modules demos tests contrib
SUBDIRS = po $(SRC_SUBDIRS) docs build m4macros

# require automake 1.4
AUTOMAKE_OPTIONS = 1.4

EXTRA_DIST =			\
	HACKING			\
	gtk+.spec.in		\
	makecopyright		\
	TODO			\
	TODO.xml		\
	NEWS.pre-1-0		\
	ChangeLog.pre-1-0	\
	ChangeLog.pre-1-2	\
	README.cvs-commits	\
	README.win32		\
	README.nanox		\
	config.h.win32		\
	gtk-zip.sh		\
	sanitize-la.sh		\
	po/README.tools		\
	po/README.translators	\
	po/desk.pl		\
	po/makefile.mingw	\
	po/update.pl		\
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
pkgconfig_DATA= gdk-pixbuf-2.0.pc gdk-$(GDKTARGET)-2.0.pc gtk+-$(GDKTARGET)-2.0.pc

DISTCLEANFILES =				\
	gtk+-$(GDKTARGET)-2.0.pc		\
	gdk-$(GDKTARGET)-2.0.pc			\
	gtk+-$(GDKTARGET)-2.0-uninstalled.pc	\
	gdk-$(GDKTARGET)-2.0-uninstalled.pc

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

dist-hook: gtk+.spec
	if test -f $(srcdir)/INSTALL.in && test -f $(srcdir)/README.in ; then \
	     CONFIG_FILES="INSTALL:$(srcdir)/INSTALL.in README:$(srcdir)/README.in" \
	     CONFIG_HEADERS= \
	     $(SHELL) config.status \
	     && cp INSTALL README $(distdir) ; \
	fi \
	&& cp gtk+.spec $(distdir)

.PHONY: files release sanity snapshot

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


# This is a version of the automake-1.4 distcheck rule modified
# to pass --enable-gtk-doc to ./configure
#
mydistcheck: dist
	-rm -rf $(distdir)
	GZIP=$(GZIP_ENV) $(TAR) zxf $(distdir).tar.gz
	mkdir $(distdir)/=build
	mkdir $(distdir)/=inst
	dc_install_base=`cd $(distdir)/=inst && pwd`; \
	cd $(distdir)/=build \
	  && ../configure --srcdir=.. --prefix=$$dc_install_base --enable-gtk-doc \
	  && $(MAKE) $(AM_MAKEFLAGS) \
	  && $(MAKE) $(AM_MAKEFLAGS) dvi \
	  && $(MAKE) $(AM_MAKEFLAGS) check \
	  && $(MAKE) $(AM_MAKEFLAGS) install \
	  && $(MAKE) $(AM_MAKEFLAGS) installcheck \
	  && $(MAKE) $(AM_MAKEFLAGS) dist
	-rm -rf $(distdir)
	@banner="$(distdir).tar.gz is ready for distribution"; \
	dashes=`echo "$$banner" | sed s/./=/g`; \
	echo "$$dashes"; \
	echo "$$banner"; \
	echo "$$dashes"
