## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.6

# The name of the module.
DOC_MODULE=gdk3

# The top-level SGML file.
DOC_MAIN_SGML_FILE=gdk-docs.sgml

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS=--deprecated-guards=GDK_DISABLE_DEPRECATED \
             --ignore-decorators=G_GNUC_WARN_UNUSED_RESULT

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=$(top_srcdir)/gdk $(top_srcdir)/gdk/x11

# Used for dependencies
HFILE_GLOB=$(top_srcdir)/gdk/*.h $(top_srcdir)/gdk/x11/gdkx.h
CFILE_GLOB=$(top_srcdir)/gdk/*.c

# Header files to ignore when scanning
IGNORE_HFILES= 			\
	gdkintl.h		\
	gdkmarshalers.h		\
	gdkkeysyms.h		\
	gdkinternals.h		\
	gdkprivate.h		\
	gdk*private.h		\
	keyname-table.h		\
	win32			\
	x11			\
	quartz

# Extra files to add when scanning
EXTRA_HFILES=			\
	$(top_srcdir)/gdk/x11/gdkx.h

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
AM_CPPFLAGS = 			\
	-I$(top_srcdir)		\
	-I$(top_builddir)	\
	-I$(top_builddir)/gdk	\
	$(GTK_DEBUG_FLAGS)	\
	$(GDK_DEP_CFLAGS)

GTKDOC_LIBS = $(top_builddir)/gdk/libgdk-3.la $(GDK_DEP_LIBS)

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=--output-format=xml --name-space=gdk

# Extra SGML files that are included by DOC_MAIN_SGML_FILE
content_files = 		\
	version.xml

# Images to copy into HTML directory
HTML_IMAGES = 				\
	images/rotated-text.png		\
	images/X_cursor.png		\
	images/arrow.png		\
	images/based_arrow_down.png	\
	images/based_arrow_up.png	\
	images/boat.png			\
	images/bogosity.png		\
	images/bottom_left_corner.png	\
	images/bottom_right_corner.png	\
	images/bottom_side.png		\
	images/bottom_tee.png		\
	images/box_spiral.png		\
	images/center_ptr.png		\
	images/circle.png		\
	images/clock.png		\
	images/coffee_mug.png		\
	images/cross.png		\
	images/cross_reverse.png	\
	images/crosshair.png		\
	images/diamond_cross.png	\
	images/dot.png			\
	images/dotbox.png		\
	images/double_arrow.png		\
	images/draft_large.png		\
	images/draft_small.png		\
	images/draped_box.png		\
	images/exchange.png		\
	images/fleur.png		\
	images/gobbler.png		\
	images/gumby.png		\
	images/hand1.png		\
	images/hand2.png		\
	images/heart.png		\
	images/icon.png			\
	images/iron_cross.png		\
	images/left_ptr.png		\
	images/left_side.png		\
	images/left_tee.png		\
	images/leftbutton.png		\
	images/ll_angle.png		\
	images/lr_angle.png		\
	images/man.png			\
	images/middlebutton.png		\
	images/mouse.png		\
	images/pencil.png		\
	images/pirate.png		\
	images/plus.png			\
	images/question_arrow.png	\
	images/right_ptr.png		\
	images/right_side.png		\
	images/right_tee.png		\
	images/rightbutton.png		\
	images/rtl_logo.png		\
	images/sailboat.png		\
	images/sb_down_arrow.png	\
	images/sb_h_double_arrow.png	\
	images/sb_left_arrow.png	\
	images/sb_right_arrow.png	\
	images/sb_up_arrow.png		\
	images/sb_v_double_arrow.png	\
	images/shuttle.png		\
	images/sizing.png		\
	images/spider.png		\
	images/spraycan.png		\
	images/star.png			\
	images/target.png		\
	images/tcross.png		\
	images/top_left_arrow.png	\
	images/top_left_corner.png	\
	images/top_right_corner.png	\
	images/top_side.png		\
	images/top_tee.png		\
	images/trek.png			\
	images/ul_angle.png		\
	images/umbrella.png		\
	images/ur_angle.png		\
	images/watch.png		\
	images/xterm.png

if ENABLE_DOC_CROSS_REFERENCES
# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS= \
	--extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/gobject \
	--extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/glib \
	--extra-dir=$(CAIRO_PREFIX)/share/gtk-doc/html/cairo
endif

include $(top_srcdir)/gtk-doc.make

# Other files to distribute
EXTRA_DIST += version.xml.in

if ENABLE_GTK_DOC
TESTS_ENVIRONMENT = cd $(srcdir) && \
  DOC_MODULE=$(DOC_MODULE) DOC_MAIN_SGML_FILE=$(DOC_MAIN_SGML_FILE) \
  SRCDIR=$(abs_srcdir) BUILDDIR=$(abs_builddir)
#TESTS = $(GTKDOC_CHECK)
endif

-include $(top_srcdir)/git.mk
