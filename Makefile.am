## Process this file with automake to produce Makefile.in

SRC_SUBDIRS = glib gdk gtk
SUBDIRS = $(SRC_SUBDIRS) docs

bin_SCRIPTS = gtk-config

EXTRA_DIST =	\
	gtk+.prj	\
        gtk.m4          \
	makecopyright	\
	TODO		\
	examples/gtkdial/Makefile \
	examples/gtkdial/dial_test.c \
	examples/gtkdial/gtkdial.h \
	examples/gtkdial/gtkdial.c \
	examples/helloworld/Makefile \
	examples/helloworld/helloworld.c \
	examples/helloworld2/Makefile \
	examples/helloworld2/helloworld2.c \
	examples/scribble-simple/Makefile \
	examples/scribble-simple/scribble-simple.c \
	examples/tictactoe/Makefile \
	examples/tictactoe/tictactoe.h \
	examples/tictactoe/tictactoe.c \
	examples/tictactoe/ttt_test.c \
	examples/buttons/Makefile \
	examples/buttons/info.xpm \
	examples/buttons/buttons.c \
	examples/packbox/Makefile \
	examples/packbox/packbox.c \
	examples/entry/Makefile \
	examples/entry/entry.c \
	examples/eventbox/Makefile \
	examples/eventbox/eventbox.c \
	examples/list/Makefile \
	examples/list/list.c \
	examples/menu/Makefile \
	examples/menu/menufactory.h \
	examples/menu/menu.c \
	examples/menu/mfmain.c \
	examples/menu/menufactory.c \
	examples/menu/mfmain.h \
	examples/notebook/Makefile \
	examples/notebook/notebook.c \
	examples/pixmap/Makefile \
	examples/pixmap/pixmap.c \
	examples/progressbar/Makefile \
	examples/progressbar/progressbar.c \
	examples/table/Makefile \
	examples/table/table.c \
	examples/radiobuttons/Makefile \
	examples/radiobuttons/radiobuttons.c \
	examples/rulers/Makefile \
	examples/rulers/rulers.c \
	examples/scrolledwin/Makefile \
	examples/scrolledwin/scrolledwin.c \
	examples/wheelbarrow/Makefile \
	examples/wheelbarrow/wheelbarrow.c \
	examples/statusbar/Makefile \
	examples/statusbar/statusbar.c \
	examples/selection/Makefile \
	examples/selection/gettargets.c \
	examples/selection/setselection.c \
	examples/filesel/Makefile \
	examples/filesel/filesel.c 


m4datadir = $(datadir)/aclocal
m4data_DATA = gtk.m4

.PHONY: files populate checkin release

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

populate:
	@echo "populating project"
	@files=`$(MAKE) files | grep -v "make\[[1-9]\]"`; prcs populate -d gtk+.prj $$files

checkin: populate
	@echo "checking in project"
	@prcs checkin

release:
	$(MAKE) dist distdir=$(PACKAGE)`date +"%y%m%d"`
