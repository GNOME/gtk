.PHONY: htmldir textdir faq

FAQ_FILES=html/gtkfaq.html		\
	html/gtkfaq-1.html       	\
	html/gtkfaq-2.html       	\
	html/gtkfaq-3.html       	\
	html/gtkfaq-4.html       	\
	html/gtkfaq-5.html       	\
	html/gtkfaq-6.html       	\
	html/gtkfaq-7.html		\
	text/gtkfaq.txt

$(FAQ_FILES): faq

htmldir:
	mkdir -p $(srcdir)/html

textdir:
	mkdir -p $(srcdir)/text

faq: htmldir textdir
	(cd $(srcdir); sgml2html gtkfaq.sgml;            \
	perl $(top_srcdir)/docs/gtkdocs_fix gtkfaq*html; \
	mv gtkfaq*html html/;                            \
	sgml2txt gtkfaq.sgml;                            \
	mv gtkfaq.txt text/)

EXTRA_DIST += gtkfaq.sgml gtk-faq.sgml

if HAVE_SGML2HTML
dist-hook: faq
	mkdir $(distdir)/html
	cp -p $(srcdir)/html/*.html $(distdir)/html
	mkdir $(distdir)/text
	cp -p $(srcdir)/text/*.txt $(distdir)/text
else
dist-hook:
	echo "***"
	echo "*** Warning: FAQ not built"
	echo "*** DISTRIBUTION IS INCOMPLETE"
	echo "***"
endif
