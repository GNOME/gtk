include $(top_srcdir)/Makefile.decl

GENERATED_ITEMS = gen-enums.bat

if HAVE_INTROSPECTION
GENERATED_ITEMS += \
	introspection.body.mak	\
	Gdk_4_0_gir_list	\
	GdkWin32_4_0_gir_list	\
	Gsk_4_0_gir_list	\
	Gtk_4_0_gir_list

MSVC_INTROSPECTION_INTERMEDIATE_FILES =	\
	Gdk-4.0.gir.msvc.introspect		\
	GdkWin32-4.0.gir.msvc.introspect	\
	Gsk-4.0.gir.msvc.introspect		\
	Gtk-4.0.gir.msvc.introspect

introspection.body.mak: $(MSVC_INTROSPECTION_INTERMEDIATE_FILES)
	-$(RM) introspection.body.mak
	for F in `ls *.msvc.introspect`; do \
		case $$F in \
			*)	cat $(top_builddir)/win32/$$F >>introspection.body.mak \
			;; \
		esac; \
	done
	$(RM) $(MSVC_INTROSPECTION_INTERMEDIATE_FILES)
endif

DISTCLEANFILES = $(GENERATED_ITEMS)

gen-enums.bat: $(srcdir)/gen-enums.batin gsk.enum.headers
	$(CPP) -P - <$(srcdir)/gen-enums.batin >$@
	$(RM) gsk.enum.headers

SUBDIRS =	\
	vs12	\
	vs14	\
	vs15

EXTRA_DIST +=	\
	detectenv-msvc.mak		\
	introspection-msvc.mak		\
	gtk-introspection-msvc.mak	\
	replace.py			\
	pc_base.py			\
	gtkpc.py			\
	gen-enums.batin			\
	$(GENERATED_ITEMS)

-include $(top_srcdir)/git.mk
