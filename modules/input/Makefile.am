## Makefile.am for gtk+/modules/input

if PLATFORM_WIN32
no_undefined = -no-undefined
endif

INCLUDES = @STRIP_BEGIN@ \
	-I$(top_srcdir) 				\
	-I$(top_srcdir)/gdk				\
	-I$(top_builddir)/gdk				\
	-DGTK_LOCALEDIR=\"$(gtklocaledir)\"		\
	@GTK_DEBUG_FLAGS@				\
	@GTK_XIM_FLAGS@					\
	@GTK_DEP_CFLAGS@				\
@STRIP_END@

DEPS = \
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-1.3.la	\
	$(top_builddir)/gdk/@gdktargetlib@ 		\
	$(top_builddir)/gtk/@gtktargetlib@

LDADDS = @STRIP_BEGIN@ 					\
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-1.3.la	\
	$(top_builddir)/gdk/@gdktargetlib@        	\
	$(top_builddir)/gtk/@gtktargetlib@ 		\
	@GTK_DEP_LIBS@					\
@STRIP_END@

moduledir = $(libdir)/gtk-2.0/immodules/$(GTK_BINARY_VERSION)

im_xim_la_LDFLAGS = -rpath $(moduledir) -avoid-version -module $(no_undefined)
im_xim_la_SOURCES = 		\
	gtkimcontextxim.c 	\
	gtkimcontextxim.h	\
	imxim.c
im_xim_la_LIBADD = $(LDADDS)
if USE_X11
IM_XIM_MODULE=im-xim.la 
endif

im_am_et_la_LDFLAGS = -rpath $(moduledir) -avoid-version -module $(no_undefined)
im_am_et_la_SOURCES = imam-et.c
im_am_et_la_LIBADD = $(LDADDS)

im_cyrillic_translit_la_LDFLAGS = -rpath $(moduledir) -avoid-version -module $(no_undefined)
im_cyrillic_translit_la_SOURCES = imcyrillic-translit.c
im_cyrillic_translit_la_LIBADD = $(LDADDS)

im_ti_er_la_LDFLAGS = -rpath $(moduledir) -avoid-version -module $(no_undefined)
im_ti_er_la_SOURCES = imti-er.c
im_ti_er_la_LIBADD = $(LDADDS)

im_ti_et_la_LDFLAGS = -rpath $(moduledir) -avoid-version -module $(no_undefined)
im_ti_et_la_SOURCES = imti-et.c
im_ti_et_la_LIBADD = $(LDADDS)

im_thai_broken_la_LDFLAGS = -rpath $(moduledir) -avoid-version -module $(no_undefined)
im_thai_broken_la_SOURCES = imthai-broken.c
im_thai_broken_la_LIBADD = $(LDADDS)

im_viqr_la_LDFLAGS = -rpath $(moduledir) -avoid-version -module $(no_undefined)
im_viqr_la_SOURCES = imviqr.c
im_viqr_la_LIBADD = $(LDADDS)

im_inuktitut_la_LDFLAGS = -rpath $(moduledir) -avoid-version -module $(no_undefined)
im_inuktitut_la_SOURCES = iminuktitut.c
im_inuktitut_la_LIBADD = $(LDADDS)

im_ipa_la_LDFLAGS = -rpath $(moduledir) -avoid-version -module $(no_undefined)
im_ipa_la_SOURCES = imipa.c
im_ipa_la_LIBADD = $(LDADDS)

im_hangul_la_LDFLAGS = -rpath $(moduledir) -avoid-version -module $(no_undefined)
im_hangul_la_SOURCES = imhangul.c imhangul-defs.h
im_hangul_la_LIBADD = $(LDADDS)

# Running this if DESTDIR is set is going to not work at all, so
# skip it.
install-data-local:
	if [ -z "$(DESTDIR)" ] ; then \
	  $(mkinstalldirs) $(DESTDIR)$(sysconfdir)/gtk-2.0 ; \
	  $(top_builddir)/gtk/gtk-query-immodules-2.0 > $(DESTDIR)$(sysconfdir)/gtk-2.0/gtk.immodules ; \
        fi

module_LTLIBRARIES =				\
	$(IM_XIM_MODULE)			\
	im-am-et.la				\
	im-cyrillic-translit.la			\
	im-hangul.la				\
	im-inuktitut.la				\
	im-ipa.la				\
	im-thai-broken.la			\
	im-ti-er.la				\
	im-ti-et.la				\
	im-viqr.la

gtk.immodules: Makefile.am $(module_LTLIBRARIES)
	$(top_builddir)/gtk/gtk-query-immodules-2.0 *.la > gtk.immodules

all-local: gtk.immodules
