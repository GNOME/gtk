TUTORIAL_FR_FILES=html/gtk_tut_fr-1.html   	\
html/gtk_tut_fr-2.html   	\
html/gtk_tut_fr-3.html   	\
html/gtk_tut_fr-4.html   	\
html/gtk_tut_fr-5.html   	\
html/gtk_tut_fr-6.html   	\
html/gtk_tut_fr-7.html   	\
html/gtk_tut_fr-8.html   	\
html/gtk_tut_fr-9.html   	\
html/gtk_tut_fr-10.html  	\
html/gtk_tut_fr-11.html  	\
html/gtk_tut_fr-12.html  	\
html/gtk_tut_fr-13.html  	\
html/gtk_tut_fr-14.html  	\
html/gtk_tut_fr-15.html  	\
html/gtk_tut_fr-16.html  	\
html/gtk_tut_fr-17.html  	\
html/gtk_tut_fr-18.html  	\
html/gtk_tut_fr-19.html  	\
html/gtk_tut_fr-20.html  	\
html/gtk_tut_fr-21.html  	\
html/gtk_tut_fr-22.html  	\
html/gtk_tut_fr-23.html  	\
html/gtk_tut_fr-24.html  	\
html/gtk_tut_fr.html text/gtk_tut_fr.txt

TUTORIAL_FILES=text/gtk_tut.txt html/gtk_tut.html \
html/gtk_tut.html        	\
html/gtk_tut-1.html      	\
html/gtk_tut-2.html      	\
html/gtk_tut-3.html      	\
html/gtk_tut-4.html      	\
html/gtk_tut-5.html      	\
html/gtk_tut-6.html      	\
html/gtk_tut-7.html      	\
html/gtk_tut-8.html      	\
html/gtk_tut-9.html      	\
html/gtk_tut-10.html     	\
html/gtk_tut-11.html     	\
html/gtk_tut-12.html     	\
html/gtk_tut-13.html     	\
html/gtk_tut-14.html     	\
html/gtk_tut-15.html     	\
html/gtk_tut-16.html     	\
html/gtk_tut-17.html     	\
html/gtk_tut-18.html     	\
html/gtk_tut-19.html     	\
html/gtk_tut-20.html     	\
html/gtk_tut-21.html     	\
html/gtk_tut-22.html     	\
html/gtk_tut-23.html     	\
html/gtk_tut-24.html     	\
html/gtk_tut-25.html     	\
html/gtk_tut-26.html     	\
html/gtk_tut-27.html     	\
html/gtk_tut-28.html     	\
html/gtk_tut-29.html     	\
html/gtk_tut-30.html     	\
html/gtk_tut-31.html

TUTORIAL_IT_FILES=	html/gtk_tut_it.html     	\
	html/gtk_tut_it-1.html   	\
	html/gtk_tut_it-2.html   	\
	html/gtk_tut_it-3.html   	\
	html/gtk_tut_it-4.html   	\
	html/gtk_tut_it-5.html   	\
	html/gtk_tut_it-6.html   	\
	html/gtk_tut_it-7.html   	\
	html/gtk_tut_it-8.html   	\
	html/gtk_tut_it-9.html   	\
	html/gtk_tut_it-10.html  	\
	html/gtk_tut_it-11.html  	\
	html/gtk_tut_it-12.html  	\
	html/gtk_tut_it-13.html  	\
	html/gtk_tut_it-14.html  	\
	html/gtk_tut_it-15.html  	\
	html/gtk_tut_it-16.html  	\
	html/gtk_tut_it-17.html  	\
	html/gtk_tut_it-18.html  	\
	html/gtk_tut_it-19.html  	\
	html/gtk_tut_it-20.html  	\
	html/gtk_tut_it-21.html  	\
	html/gtk_tut_it-22.html  	\
	html/gtk_tut_it-23.html  	\
	html/gtk_tut_it-24.html		\
	text/gtk_tut_it.txt

GTKDOCS_FIX=$(srcdir)/../gtkdocs_fix

.PHONY: htmldir textdir tutorial tutorial_it tutorial_fr

htmldir:
	(cd $(srcdir); mkdir -p html; cp gtk_tut*gif html)

textdir:
	mkdir -p $(srcdir)/text

$(TUTORIAL_FILES): tutorial

$(TUTORIAL_FR_FILES): tutorial_fr

$(TUTORIAL_IT_FILES): tutorial_it

tutorial: htmldir textdir
	(cd $(srcdir); sgml2html gtk_tut.sgml; \
	perl $(GTKDOCS_FIX) gtk_tut*html; \
	mv gtk_tut*html html/; \
	sgml2txt gtk_tut.sgml; \
	mv gtk_tut.txt text/)

tutorial_it: htmldir textdir
	(cd $(srcdir); sgml2html --language=it gtk_tut_it.sgml; \
	perl $(GTKDOCS_FIX) gtk_tut_it*html; \
	mv gtk_tut_it*html html/; \
	sgml2txt --language=it gtk_tut_it.sgml; \
	mv gtk_tut_it.txt text/)

tutorial_fr: htmldir textdir
	(cd $(srcdir); sgml2html --language=fr gtk_tut_fr.sgml; \
	perl $(GTKDOCS_FIX) gtk_tut_fr*html; \
	mv gtk_tut_fr*html html/; \
	sgml2txt --language=fr gtk_tut_fr.sgml; \
	mv gtk_tut_fr.txt text/)


EXTRA_DIST += \
	gtk_tut.sgml			\
	gtk_tut_12.es.sgml		\
	gtk_tut_it.sgml			\
	gtk_tut_fr.sgml			\
	gtk_tut_packbox1.gif 		\
	gtk_tut_packbox1.eps 		\
	gtk_tut_packbox1.jpg 		\
	gtk_tut_packbox2.gif 		\
	gtk_tut_packbox2.eps 		\
	gtk_tut_packbox2.jpg 		\
	gtk_tut_table.gif		\
	gtk_tut_table.eps		\
	gtk_tut_table.jpg		\
	package_tutorial.sh		\
	package-db-tutorial.sh

if HAVE_SGML2HTML
dist-hook: tutorial tutorial_it tutorial_fr
	mkdir $(distdir)/html
	cp -p $(srcdir)/html/*.html $(distdir)/html
	cp -p $(srcdir)/html/*.gif $(distdir)/html
	mkdir $(distdir)/text
	cp -p $(srcdir)/text/*.txt $(distdir)/text
else
dist-hook:
	echo "***"
	echo "*** Warning: Tutorial not built"
	echo "*** DISTRIBUTION IS INCOMPLETE"
	echo "***"
endif
