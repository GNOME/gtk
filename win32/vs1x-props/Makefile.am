include $(top_srcdir)/Makefile.decl

GENERATED_ITEMS = gtk3-version-paths.props

EXTRA_DIST += \
	gtk3-build-defines.props		\
	gtk3-version-paths.props.in		\
	$(GENERATED_ITEMS)

DISTCLEANFILES = \
	$(GENERATED_ITEMS)

-include $(top_srcdir)/git.mk
