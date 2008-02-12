
SUBDIRS = 	arrow \
		aspectframe \
		base \
		buttonbox \
		buttons \
		calendar \
		clist \
		entry \
		eventbox \
		filesel \
		fixed \
		frame \
		gtkdial \
		helloworld \
		helloworld2 \
		label \
		list \
		menu \
		notebook \
		packbox \
		paned \
		pixmap \
		progressbar \
		radiobuttons \
		rangewidgets \
		rulers \
		scribble-simple \
		scribble-xinput \
		scrolledwin \
		selection \
		spinbutton \
		statusbar \
		table \
		text \
		tictactoe \
		tree \
		wheelbarrow

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

