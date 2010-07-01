include $(top_srcdir)/Makefile.decl

EXTRA_DIST += 			\
	gtk-faq.sgml

if HAVE_DOCBOOK
html:
	if test -w $(srcdir); then \
	  (cd $(srcdir); \
	   db2html gtk-faq.sgml; \
	   test -d html && rm -r html; \
	   mv gtk-faq html); \
	fi

pdf:
	if test -w $(srcdir); then \
	  (cd $(srcdir); db2pdf gtk-faq.sgml); \
	fi

all-local: html

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
	echo "*** Warning: FAQ not built"
	echo "*** DISTRIBUTION IS INCOMPLETE"
	echo "***"
endif

.PHONY: html

-include $(top_srcdir)/git.mk
