
DIST_SUBDIRS = 	arrow \
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
		tictactoe
# wheelbarrow

INCLUDES = \
	-I$(top_srcdir)				\
	-I$(top_srcdir)/gdk			\
	-DGDK_DISABLE_DEPRECATED		\
	-DGTK_DISABLE_DEPRECATED		\
	$(GTK_DEBUG_FLAGS)			\
	$(GTK_DEP_CFLAGS)

LDADD = \
	$(top_builddir)/gdk/$(gdktargetlib)		\
	$(top_builddir)/gtk/$(gtktargetlib)		\
	$(GTK_DEP_LIBS)

bin_PROGRAMS = hello-world window-default
