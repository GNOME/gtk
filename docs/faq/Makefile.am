if HAVE_DOCBOOK
html: 
	(cd $(srcdir); \
	 db2html gtk-faq.sgml; \
	 mv gtk-faq html)

pdf:
	(cd $(srcdir); db2pdf gtk-faq.sgml)

dist-hook: html
	cp -Rp $(srcdir)/html $(distdir)
else
html:
	echo "***"
	echo "*** Warning: FAQ not built"
	echo "***"

pdf:
	echo "***"
	echo "*** Warning: FAQ not built"
	echo "***"

dist-hook:
	echo "***"
	echo "*** Warning: Tutorial not built"
	echo "*** DISTRIBUTION IS INCOMPLETE"
	echo "***"
endif

.PHONY: html
