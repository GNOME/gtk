## Makefile.am for gtk+/demos

INCLUDES = @STRIP_BEGIN@ \
	-I$(top_srcdir) 				\
	-I$(top_srcdir)/gdk				\
	-I$(top_builddir)/gdk				\
	-DGTK_DISABLE_COMPAT_H				\
	-DGTK_LOCALEDIR=\"$(gtklocaledir)\"		\
	@GTK_DEBUG_FLAGS@				\
	@GTK_XIM_FLAGS@					\
	@GTK_LOCALE_FLAGS@				\
	@PANGO_CFLAGS@					\
	@GLIB_CFLAGS@					\
	@more_cflags@					\
@STRIP_END@

DEPS = \
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-1.3.la	\
	$(top_builddir)/gdk/@gdktargetlib@ 		\
	$(top_builddir)/gtk/@gtktargetlib@

LDADDS = @STRIP_BEGIN@ 					\
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-1.3.la	\
	$(top_builddir)/gdk/@gdktargetlib@        	\
	$(top_builddir)/gtk/@gtktargetlib@ 		\
	@more_ldflags@					\
	@more_libs@					\
	@GDK_WLIBS@					\
	@PANGO_LIBS@					\
	@GLIB_LIBS@					\
	@GTK_LIBS_EXTRA@				\
	-lm						\
@STRIP_END@

moduledir = $(libdir)/gtk-2.0/$(GTK_VERSION)/immodules

im_xim_la_LDFLAGS = -rpath $(moduledir) -avoid-version -module
im_xim_la_SOURCES = 		\
	gtkimcontextxim.c 	\
	gtkimcontextxim.h	\
	imxim.c
if USE_X11
IM_XIM_MODULE=im-xim.la 
endif

im_cyrillic_translit_la_LDFLAGS = -rpath $(moduledir) -avoid-version -module
im_cyrillic_translit_la_SOURCES = imcyrillic-translit.c

im_thai_broken_la_LDFLAGS = -rpath $(moduledir) -avoid-version -module
im_thai_broken_la_SOURCES = imthai-broken.c

im_viqr_la_LDFLAGS = -rpath $(moduledir) -avoid-version -module
im_viqr_la_SOURCES = imviqr.c

im_inuktitut_la_LDFLAGS = -rpath $(moduledir) -avoid-version -module
im_inuktitut_la_SOURCES = iminuktitut.c

im_ipa_la_LDFLAGS = -rpath $(moduledir) -avoid-version -module
im_ipa_la_SOURCES = imipa.c

module_LTLIBRARIES = im-cyrillic-translit.la $(IM_XIM_MODULE) im-viqr.la im-thai-broken.la \
                     im-inuktitut.la im-ipa.la
