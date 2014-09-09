## Makefile.am for gtk+/modules/input
include $(top_srcdir)/Makefile.decl

AM_CPPFLAGS = \
	-I$(top_srcdir) 		\
	-I$(top_srcdir)/gdk		\
	-I$(top_builddir)/gdk		\
	$(GTK_DEBUG_FLAGS)		\
	$(GTK_XIM_FLAGS)		\
	$(GTK_DEP_CFLAGS)		\
	-DGTK_COMPILATION		\
	$(INCLUDED_IMMODULE_DEFINE)

DEPS = \
	$(top_builddir)/gtk/libgtk-3.la

LDADDS = \
	$(top_builddir)/gtk/libgtk-3.la	\
	$(GTK_DEP_LIBS)

moduledir = $(libdir)/gtk-3.0/$(GTK_BINARY_VERSION)/immodules

im_xim_la_LDFLAGS = -rpath $(moduledir) -avoid-version -module $(no_undefined)
im_xim_la_SOURCES = 		\
	gtkimcontextxim.c 	\
	gtkimcontextxim.h	\
	imxim.c
libstatic_im_xim_la_SOURCES = $(im_xim_la_SOURCES)
im_xim_la_LIBADD = $(LDADDS)
if USE_X11
if INCLUDE_IM_XIM
STATIC_XIM_MODULE = libstatic-im-xim.la
else
XIM_MODULE=im-xim.la
endif
endif

im_am_et_la_LDFLAGS = -rpath $(moduledir) -avoid-version -module $(no_undefined)
im_am_et_la_SOURCES = imam-et.c
libstatic_im_am_et_la_SOURCES = $(im_am_et_la_SOURCES)
im_am_et_la_LIBADD = $(LDADDS)
if INCLUDE_IM_AM_ET
STATIC_AM_ET_MODULE = libstatic-im-am-et.la
else
AM_ET_MODULE = im-am-et.la
endif

im_cedilla_la_LDFLAGS = -rpath $(moduledir) -avoid-version -module $(no_undefined)
im_cedilla_la_SOURCES = imcedilla.c
libstatic_im_cedilla_la_SOURCES = $(im_cedilla_la_SOURCES)
im_cedilla_la_LIBADD = $(LDADDS)
if INCLUDE_IM_CEDILLA
STATIC_CEDILLA_MODULE = libstatic-im-cedilla.la
else
CEDILLA_MODULE = im-cedilla.la
endif

im_cyrillic_translit_la_LDFLAGS = -rpath $(moduledir) -avoid-version -module $(no_undefined)
im_cyrillic_translit_la_SOURCES = imcyrillic-translit.c
libstatic_im_cyrillic_translit_la_SOURCES = $(im_cyrillic_translit_la_SOURCES)
im_cyrillic_translit_la_LIBADD = $(LDADDS)
if INCLUDE_IM_CYRILLIC_TRANSLIT
STATIC_CYRILLIC_TRANSLIT_MODULE = libstatic-im-cyrillic-translit.la
else
CYRILLIC_TRANSLIT_MODULE = im-cyrillic-translit.la
endif

im_ti_er_la_LDFLAGS = -rpath $(moduledir) -avoid-version -module $(no_undefined)
im_ti_er_la_SOURCES = imti-er.c
libstatic_im_ti_er_la_SOURCES = $(im_ti_er_la_SOURCES)
im_ti_er_la_LIBADD = $(LDADDS)
if INCLUDE_IM_TI_ER
STATIC_TI_ER_MODULE = libstatic-im-ti-er.la
else
TI_ER_MODULE = im-ti-er.la
endif

im_ti_et_la_LDFLAGS = -rpath $(moduledir) -avoid-version -module $(no_undefined)
im_ti_et_la_SOURCES = imti-et.c
libstatic_im_ti_et_la_SOURCES = $(im_ti_et_la_SOURCES)
im_ti_et_la_LIBADD = $(LDADDS)
if INCLUDE_IM_TI_ET
STATIC_TI_ET_MODULE = libstatic-im-ti-et.la
else
TI_ET_MODULE = im-ti-et.la
endif

im_thai_la_LDFLAGS = -rpath $(moduledir) -avoid-version -module $(no_undefined)
im_thai_la_SOURCES = 		\
	thai-charprop.c 	\
	thai-charprop.h 	\
	gtkimcontextthai.c 	\
	gtkimcontextthai.h	\
	imthai.c
libstatic_im_thai_la_SOURCES = $(im_thai_la_SOURCES)
im_thai_la_LIBADD = $(LDADDS)
if INCLUDE_IM_THAI
STATIC_THAI_MODULE = libstatic-im-thai.la
else
THAI_MODULE = im-thai.la
endif

im_viqr_la_LDFLAGS = -rpath $(moduledir) -avoid-version -module $(no_undefined)
im_viqr_la_SOURCES = imviqr.c
libstatic_im_viqr_la_SOURCES = $(im_viqr_la_SOURCES)
im_viqr_la_LIBADD = $(LDADDS)
if INCLUDE_IM_VIQR
STATIC_VIQR_MODULE = libstatic-im-viqr.la
else
VIQR_MODULE = im-viqr.la
endif

im_inuktitut_la_LDFLAGS = -rpath $(moduledir) -avoid-version -module $(no_undefined)
im_inuktitut_la_SOURCES = iminuktitut.c
libstatic_im_inuktitut_la_SOURCES = $(im_inuktitut_la_SOURCES)
im_inuktitut_la_LIBADD = $(LDADDS)
if INCLUDE_IM_INUKTITUT
STATIC_INUKTITUT_MODULE = libstatic-im-inuktitut.la
else
INUKTITUT_MODULE = im-inuktitut.la
endif

im_ipa_la_LDFLAGS = -rpath $(moduledir) -avoid-version -module $(no_undefined)
im_ipa_la_SOURCES = imipa.c
libstatic_im_ipa_la_SOURCES = $(im_ipa_la_SOURCES)
im_ipa_la_LIBADD = $(LDADDS)
if INCLUDE_IM_IPA
STATIC_IPA_MODULE = libstatic-im-ipa.la
else
IPA_MODULE = im-ipa.la
endif

im_ime_la_LDFLAGS = -rpath $(moduledir) -avoid-version -module $(no_undefined)
im_ime_la_SOURCES = \
	gtkimcontextime.c \
	gtkimcontextime.h \
	imime.c 	  \
	imm-extra.h
libstatic_im_ime_la_SOURCES = $(im_ime_la_SOURCES)
im_ime_la_LIBADD = -limm32 $(LDADDS)
libstatic_im_ime_la_LIBADD = -limm32
if USE_WIN32
if INCLUDE_IM_IME
STATIC_IME_MODULE = libstatic-im-ime.la
else
IME_MODULE=im-ime.la
endif
endif

im_quartz_la_CPPFLAGS = $(AM_CPPFLAGS) -xobjective-c
im_quartz_la_LDFLAGS = -rpath $(moduledir) -avoid-version -module $(no_undefined)
im_quartz_la_SOURCES = imquartz.c
libstatic_im_quartz_la_SOURCES = $(im_quartz_la_SOURCES)
im_quartz_la_LIBADD = $(LDADDS)
if INCLUDE_IM_QUARTZ
STATIC_QUARTZ_MODULE = libstatic-im-quartz.la
else
if USE_QUARTZ
QUARTZ_MODULE = im-quartz.la
endif
endif

im_broadway_la_CPPFLAGS = $(AM_CPPFLAGS)
im_broadway_la_LDFLAGS = -rpath $(moduledir) -avoid-version -module $(no_undefined)
im_broadway_la_SOURCES = imbroadway.c
libstatic_im_broadway_la_SOURCES = $(im_broadway_la_SOURCES)
im_broadway_la_LIBADD = $(LDADDS)
if INCLUDE_IM_BROADWAY
STATIC_BROADWAY_MODULE = libstatic-im-broadway.la
else
if USE_BROADWAY
BROADWAY_MODULE = im-broadway.la
endif
endif

multipress_defs = -DMULTIPRESS_LOCALEDIR=\""$(mplocaledir)"\" -DMULTIPRESS_CONFDIR=\""$(sysconfdir)/gtk-2.0"\"
im_multipress_la_CPPFLAGS = $(AM_CPPFLAGS) $(multipress_defs)
libstatic_im_multipress_la_CPPFLAGS = $(im_multipress_la_CPPFLAGS)
im_multipress_la_LDFLAGS = -rpath $(moduledir) -avoid-version -module $(no_undefined)
im_multipress_la_SOURCES =	\
	gtkimcontextmultipress.c	\
	gtkimcontextmultipress.h	\
	immultipress.c
libstatic_im_multipress_la_SOURCES = $(im_multipress_la_SOURCES)
im_multipress_la_LIBADD = $(LDADDS)
if INCLUDE_IM_MULTIPRESS
STATIC_MULTIPRESS_MODULE = libstatic-im-multipress.la
else
MULTIPRESS_MODULE = im-multipress.la
endif

imconffiledir = $(sysconfdir)/gtk-3.0
dist_imconffile_DATA = im-multipress.conf

if CROSS_COMPILING
RUN_QUERY_IMMODULES_TEST=false
else
RUN_QUERY_IMMODULES_TEST=test -z "$(DESTDIR)"
endif

# Running this if cross compiling or if DESTDIR is set is going to
# not work at all, so skip it.
# We use install-data-hook here to workaround a bug in automake and/or libtool
# that makes the install target for the loader libraries a dependency on
# install-data-am, and not install-exec-am. We need to ensure this gets run
# after the libraries are installed in their final locations.
install-data-hook:
	$(AM_V_GEN) if $(RUN_QUERY_IMMODULES_TEST) ; then \
	  echo $(mkinstalldirs) $(DESTDIR)$(libdir)/gtk-3.0/3.0.0 ; \
	  $(mkinstalldirs) $(DESTDIR)$(libdir)/gtk-3.0/3.0.0 ; \
	  echo "$(top_builddir)/gtk/gtk-query-immodules-3.0$(EXEEXT) > $(DESTDIR)$(libdir)/gtk-3.0/3.0.0/immodules.cache" ; \
	  $(top_builddir)/gtk/gtk-query-immodules-3.0$(EXEEXT) > $(DESTDIR)$(libdir)/gtk-3.0/3.0.0/immodules.cache ; \
	else \
	  echo "***" ; \
	  echo "*** Warning: immodules.cache not built" ; \
	  echo "***" ; \
	  echo "*** Generate this file manually on host" ; \
	  echo "*** system using gtk-query-immodules-3.0" ; \
	  echo "***" ; \
        fi

uninstall-local:
	rm -f $(DESTDIR)$(libdir)/gtk-3.0/3.0.0/immodules.cache

if BUILD_DYNAMIC_MODULES

module_LTLIBRARIES =			\
	$(AM_ET_MODULE)			\
	$(CEDILLA_MODULE)		\
	$(CYRILLIC_TRANSLIT_MODULE)	\
	$(IME_MODULE)			\
	$(INUKTITUT_MODULE)		\
	$(IPA_MODULE)			\
	$(MULTIPRESS_MODULE)		\
	$(QUARTZ_MODULE)		\
	$(BROADWAY_MODULE)		\
	$(THAI_MODULE)			\
	$(TI_ER_MODULE)			\
	$(TI_ET_MODULE)			\
	$(VIQR_MODULE)			\
	$(XIM_MODULE)

endif

noinst_LTLIBRARIES =				\
	$(STATIC_AM_ET_MODULE)			\
	$(STATIC_CEDILLA_MODULE)		\
	$(STATIC_CYRILLIC_TRANSLIT_MODULE)	\
	$(STATIC_IME_MODULE)			\
	$(STATIC_INUKTITUT_MODULE)		\
	$(STATIC_IPA_MODULE)			\
	$(STATIC_MULTIPRESS_MODULE)		\
	$(STATIC_QUARTZ_MODULE)			\
	$(STATIC_BROADWAY_MODULE)		\
	$(STATIC_THAI_MODULE)			\
	$(STATIC_TI_ER_MODULE)			\
	$(STATIC_TI_ET_MODULE)			\
	$(STATIC_VIQR_MODULE)			\
	$(STATIC_XIM_MODULE)

included-modules: $(noinst_LTLIBRARIES)

immodules.cache: Makefile.am $(module_LTLIBRARIES)
	$(AM_V_GEN) $(top_builddir)/gtk/gtk-query-immodules-3.0$(EXEEXT) $(module_LTLIBRARIES) > immodules.cache

EXTRA_DIST += README.multipress

CLEANFILES = immodules.cache

if CROSS_COMPILING
else
all-local: immodules.cache
endif

-include $(top_srcdir)/git.mk
