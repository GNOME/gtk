
SUBDIRS = aspectframe eventbox gtkdial notebook progressbar scrolledwin tictactoe \
	  clist filesel list paned rulers table \
          entry menu pixmap scribble-simple text \
	  base  helloworld packbox radiobuttons selection tree \
	  buttons helloworld2 packer rangewidgets  statusbar wheelbarrow 

all:
	list='$(SUBDIRS)'; \
	for subdir in $$list; do \
	  (cd $$subdir && $(MAKE)); \
	done

clean:
	list='$(SUBDIRS)'; \
	for subdir in $$list; do \
	  (cd $$subdir && $(MAKE) clean); \
	done

