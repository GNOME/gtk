## Process this file with automake to produce Makefile.in

info_TEXINFOS = gdk.texi gtk.texi glib.texi

man_MANS = gtk-config.1

EXTRA_DIST =	\
	texinfo.tex 		\
	macros.texi		\
	Makefile.sgml		\
	gtkdocs_fix		\
	gtkfaq.sgml		\
        gtk-config.txt          \
	gtk_tut.sgml		\
	gtk_tut_it.sgml		\
	gtk_tut_fr.sgml		\
	debugging.txt		\
	developers.txt		\
	refcounting.txt		\
	styles.txt		\
	text_widget.txt 	\
	widget_system.txt 	\
	gtk_tut_packbox1.gif 	\
	gtk_tut_packbox2.gif 	\
	gtk_tut_table.gif	\
	gtk-config.1

	

files:
	@files=`ls $(DISTFILES) 2> /dev/null `; for p in $$files; do \
	  echo $$p; \
	done
