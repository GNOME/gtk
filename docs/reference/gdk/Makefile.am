## Process this file with automake to produce Makefile.in

# The name of the module.
DOC_MODULE=gdk

# The top-level SGML file.
DOC_MAIN_SGML_FILE=gdk-docs.sgml

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=../../../gdk

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS=--deprecated-guards="GDK_ENABLE_BROKEN|GDK_DISABLE_DEPRECATED"

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=--sgml-mode

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS=--extra-dir=../gdk-pixbuf/html

# Used for dependencies
HFILE_GLOB=$(top_srcdir)/gdk/*.h $(top_srcdir)/gdk/x11/gdkx.h
CFILE_GLOB=$(top_srcdir)/gdk/*.c

# Header files to ignore when scanning
IGNORE_HFILES=			\
	gdkkeysyms.h		\
	gdkinternals.h		\
	gdkprivate.h		\
	gdkpoly-generic.h	\
	gdkregion-generic.h	\
	linux-fb 		\
	nanox 			\
	win32 			\
	x11

# Extra files to add when scanning (relative to $srcdir)
EXTRA_HFILES=			\
	../../../gdk/x11/gdkx.h 

# Images to copy into HTML directory
HTML_IMAGES =				\
	images/X_cursor.png 		\
	images/arrow.png 		\
	images/based_arrow_down.png 	\
	images/based_arrow_up.png 	\
	images/boat.png 		\
	images/bogosity.png 		\
	images/bottom_left_corner.png 	\
	images/bottom_right_corner.png 	\
	images/bottom_side.png 		\
	images/bottom_tee.png 		\
	images/box_spiral.png 		\
	images/center_ptr.png 		\
	images/circle.png 		\
	images/clock.png 		\
	images/coffee_mug.png 		\
	images/cross.png 		\
	images/cross_reverse.png 	\
	images/crosshair.png 		\
	images/diamond_cross.png 	\
	images/dot.png 			\
	images/dotbox.png 		\
	images/double_arrow.png 	\
	images/draft_large.png 		\
	images/draft_small.png 		\
	images/draped_box.png 		\
	images/exchange.png 		\
	images/fleur.png 		\
	images/gobbler.png 		\
	images/gumby.png 		\
	images/hand1.png 		\
	images/hand2.png 		\
	images/heart.png 		\
	images/icon.png 		\
	images/iron_cross.png 		\
	images/left_ptr.png 		\
	images/left_side.png 		\
	images/left_tee.png 		\
	images/leftbutton.png 		\
	images/ll_angle.png 		\
	images/lr_angle.png 		\
	images/man.png 			\
	images/middlebutton.png 	\
	images/mouse.png 		\
	images/pencil.png 		\
	images/pirate.png 		\
	images/plus.png 		\
	images/question_arrow.png 	\
	images/right_ptr.png 		\
	images/right_side.png 		\
	images/right_tee.png 		\
	images/rightbutton.png 		\
	images/rtl_logo.png 		\
	images/sailboat.png 		\
	images/sb_down_arrow.png 	\
	images/sb_h_double_arrow.png 	\
	images/sb_left_arrow.png 	\
	images/sb_right_arrow.png 	\
	images/sb_up_arrow.png 		\
	images/sb_v_double_arrow.png 	\
	images/shuttle.png 		\
	images/sizing.png 		\
	images/spider.png 		\
	images/spraycan.png 		\
	images/star.png 		\
	images/target.png 		\
	images/tcross.png 		\
	images/top_left_arrow.png 	\
	images/top_left_corner.png 	\
	images/top_right_corner.png 	\
	images/top_side.png 		\
	images/top_tee.png 		\
	images/trek.png 		\
	images/ul_angle.png 		\
	images/umbrella.png 		\
	images/ur_angle.png 		\
	images/watch.png 		\
	images/xterm.png 

# Extra SGML files that are included by DOC_MAIN_SGML_FILE
content_files =

# Other files to distribute
extra_files =

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
GTKDOC_CFLAGS =
GTKDOC_LIBS =

# Commands for compiling and linking 
GTKDOC_CC=$(LIBTOOL) --mode=compile $(CC)
GTKDOC_LD=$(LIBTOOL) --mode=link $(CC)


####################################
# Everything below here is generic #
####################################

# We set GPATH here; this gives us semantics for GNU make
# which are more like other make's VPATH, when it comes to
# whether a source that is a target of one rule is then
# searched for in VPATH/GPATH.
#
GPATH = $(srcdir)

TARGET_DIR=$(HTML_DIR)/$(DOC_MODULE)

EXTRA_DIST = 				\
	$(content_files)		\
	$(extra_files)			\
	$(HTML_IMAGES)			\
	$(DOC_MAIN_SGML_FILE)		\
	$(DOC_MODULE).types		\
	$(DOC_MODULE)-sections.txt	\
	$(DOC_MODULE)-overrides.txt

DOC_STAMPS=scan-build.stamp tmpl-build.stamp sgml-build.stamp html-build.stamp \
	   $(srcdir)/tmpl.stamp $(srcdir)/sgml.stamp $(srcdir)/html.stamp

SCANOBJ_FILES = 		\
	$(DOC_MODULE).args 	\
	$(DOC_MODULE).hierarchy \
	$(DOC_MODULE).signals

if ENABLE_GTK_DOC
all-local: html-build.stamp

#### scan ####

scan-build.stamp: $(HFILE_GLOB)
	@echo '*** Scanning header files ***'
	if grep -l '^..*$$' $(srcdir)/$(DOC_MODULE).types > /dev/null ; then \
	    CC="$(GTKDOC_CC)" LD="$(GTKDOC_LD)" CFLAGS="$(GTKDOC_CFLAGS)" LDFLAGS="$(GTKDOC_LIBS)" gtkdoc-scangobj --module=$(DOC_MODULE) --output-dir=$(srcdir) ; \
	else \
	    cd $(srcdir) ; \
	    for i in $(SCANOBJ_FILES) ; do \
               test -f $$i || touch $$i ; \
	    done \
	fi
	cd $(srcdir) && \
	  gtkdoc-scan --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR) --ignore-headers="$(IGNORE_HFILES)" $(SCAN_OPTIONS) $(EXTRA_HFILES)
	touch scan-build.stamp

$(DOC_MODULE)-decl.txt $(SCANOBJ_FILES): scan-build.stamp
	@true

#### templates ####

tmpl-build.stamp: $(DOC_MODULE)-decl.txt $(SCANOBJ_FILES) $(DOC_MODULE)-sections.txt $(DOC_MODULE)-overrides.txt
	@echo '*** Rebuilding template files ***'
	cd $(srcdir) && gtkdoc-mktmpl --module=$(DOC_MODULE)
	touch tmpl-build.stamp

tmpl.stamp: tmpl-build.stamp
	@true

#### sgml ####

sgml-build.stamp: tmpl.stamp $(CFILE_GLOB) $(srcdir)/tmpl/*.sgml
	@echo '*** Building SGML ***'
	cd $(srcdir) && \
	gtkdoc-mkdb --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR) $(MKDB_OPTIONS)
	touch sgml-build.stamp

sgml.stamp: sgml-build.stamp
	@true

#### html ####

html-build.stamp: sgml.stamp $(DOC_MAIN_SGML_FILE) $(content_files)
	@echo '*** Building HTML ***'
	rm -rf $(srcdir)/html 
	mkdir $(srcdir)/html
	cd $(srcdir)/html && gtkdoc-mkhtml $(DOC_MODULE) ../$(DOC_MAIN_SGML_FILE)
	test "x$(HTML_IMAGES)" = "x" || ( cd $(srcdir) && cp $(HTML_IMAGES) html )
	@echo '-- Fixing Crossreferences' 
	cd $(srcdir) && gtkdoc-fixxref --module-dir=html --html-dir=$(HTML_DIR) $(FIXXREF_OPTIONS)
	touch html-build.stamp
endif

##############

clean-local:
	rm -f *~ *.bak $(SCANOBJ_FILES) *-unused.txt $(DOC_STAMPS)

maintainer-clean-local: clean
	cd $(srcdir) && rm -rf sgml html $(DOC_MODULE)-decl-list.txt $(DOC_MODULE)-decl.txt

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(TARGET_DIR)
	(installfiles=`echo $(srcdir)/html/*`; \
	if test "$$installfiles" = '$(srcdir)/html/*'; \
	then echo '-- Nothing to install' ; \
	else \
	  for i in $$installfiles; do \
	    echo '-- Installing '$$i ; \
	    $(INSTALL_DATA) $$i $(DESTDIR)$(TARGET_DIR); \
	  done; \
	  echo '-- Installing $(srcdir)/html/index.sgml' ; \
	  $(INSTALL_DATA) $(srcdir)/html/index.sgml $(DESTDIR)$(TARGET_DIR); \
	fi)

#
# Require gtk-doc when making dist
#
if ENABLE_GTK_DOC
dist-check-gtkdoc:
else
dist-check-gtkdoc:
	@echo "*** gtk-doc must be installed and enabled in order to make dist"
	@false
endif

dist-hook: dist-check-gtkdoc dist-hook-local
	mkdir $(distdir)/tmpl
	mkdir $(distdir)/sgml
	mkdir $(distdir)/html
	-cp $(srcdir)/tmpl/*.sgml $(distdir)/tmpl
	-cp $(srcdir)/sgml/*.sgml $(distdir)/sgml
	-cp $(srcdir)/html/* $(distdir)/html

.PHONY : dist-hook-local
