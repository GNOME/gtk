
# these have a handrolled Makefile that gets in the way of distchecking
# so they will not be included in tarballs until they get integrated
# in the new 'getting started' section of the docs
OLD_EXAMPLES = \
		arrow \
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

AM_CPPFLAGS = \
	-I$(top_srcdir)				\
	-I$(top_srcdir)/gdk			\
	-I$(top_builddir)/gdk			\
	-DGDK_DISABLE_DEPRECATED		\
	-DGTK_DISABLE_DEPRECATED		\
	$(GTK_DEBUG_FLAGS)			\
	$(GTK_DEP_CFLAGS)

LDADD = \
	$(top_builddir)/gtk/libgtk-3.la	\
	$(top_builddir)/gdk/libgdk-3.la \
	$(GTK_DEP_LIBS)

noinst_PROGRAMS = \
	hello-world				\
	window-default				\
	bloatpad				\
	plugman					\
	sunny					\
	action-namespace			\
	grid-packing				\
	drawing					\
	builder

EXTRA_DIST = builder.ui

-include $(top_srcdir)/git.mk
