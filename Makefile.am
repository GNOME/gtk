## Makefile.am for GTK+

SRC_SUBDIRS = gdk gtk
SUBDIRS = po $(SRC_SUBDIRS) docs

bin_SCRIPTS = gtk-config

# require automake 1.4
AUTOMAKE_OPTIONS = 1.4

EXTRA_DIST =	\
	HACKING		\
	gtk+.spec.in	\
        gtk.m4          \
	makecopyright	\
	TODO		\
	NEWS.pre-1-0	\
	ChangeLog.pre-1-0	\
	README.cvs-commits	\
	intl/libgettext.h	\
	intl/po2tbl.sed.in	\
	examples/aspectframe/Makefile  \
	examples/aspectframe/aspectframe.c  \
	examples/Makefile  \
	examples/README.1ST  \
	examples/extract.awk  \
	examples/extract.sh  \
	examples/base/Makefile  \
	examples/base/base.c  \
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
	examples/list/Makefile  \
	examples/list/list.c  \
	examples/menu/Makefile  \
	examples/menu/menu.c  \
	examples/menu/itemfactory.c  \
	examples/notebook/Makefile  \
	examples/notebook/notebook.c  \
	examples/packbox/Makefile  \
	examples/packbox/packbox.c  \
	examples/packer/Makefile  \
	examples/packer/pack.c  \
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
	examples/find-examples.sh \
	gdk.pc.in gtk+.pc.in

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA= gdk.pc gtk+.pc

dist-hook: gtk+.spec
	if test -e $(srcdir)/INSTALL.in && test -e $(srcdir)/README.in ; then \
	     CONFIG_FILES="INSTALL:$(srcdir)/INSTALL.in README:$(srcdir)/README.in" \
	     CONFIG_HEADERS= \
	     $(SHELL) config.status \
	     && cp INSTALL README $(distdir) ; \
	fi \
	&& cp gtk+.spec $(distdir)

m4datadir = $(datadir)/aclocal
m4data_DATA = gtk.m4

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
	$(MAKE) dist distdir=$(PACKAGE)`date +"%y%m%d"`
