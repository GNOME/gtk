
SUBDIRS = 	arrow \
		aspectframe \
		base \
		buttonbox \
		buttons \
		calendar \
		entry \
		eventbox \
		fixed \
		frame \
		gtkdial \
		helloworld \
		helloworld2 \
		label \
		menu \
		notebook \
		packbox \
		paned \
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
		tictactoe \
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

