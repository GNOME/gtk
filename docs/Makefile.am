## Process this file with automake to produce Makefile.in

info_TEXINFOS = gdk.texi gtk.texi glib.texi

EXTRA_DIST =	\
	texinfo.tex	\
	macros.texi	\
	Makefile.sgml	\
	gtkdocs_fix	\
	gtkfaq.sgml	\
	gtk_tut.sgml	\
	gtk_tut_it.sgml	\
	debugging.txt	\
	developers.txt	\
	refcounting.txt	\
	styles.txt	\
	text_widget.txt	\
	widget_system.txt
	

files:
	@files=`ls $(DISTFILES) 2> /dev/null `; for p in $$files; do \
	  echo $$p; \
	done
