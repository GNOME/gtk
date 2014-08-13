CC = @CC_FOR_BUILD@
AM_CFLAGS = @CFLAGS_FOR_BUILD@
CPP = @CPP_FOR_BUILD@
AM_CPPFLAGS = @CPPFLAGS_FOR_BUILD@
AM_LDFLAGS = @LDFLAGS_FOR_BUILD@

native-update-icon-cache$(BUILD_EXEEXT): $(srcdir)/../updateiconcache.c
	echo '#define GETTEXT_PACKAGE "gtk30"' > $(srcdir)/config.h
	$(AM_V_CCLD)$(CC_FOR_BUILD) $(CPPFLAGS_FOR_BUILD) -I. -I$(srcdir)/../.. $(NATIVE_GDKPIXBUF_CFLAGS) $(CFLAGS_FOR_BUILD) $^ $(LDFLAGS_FOR_BUILD) $(NATIVE_GDKPIXBUF_LIBS) $(LIBS_FOR_BUILD) -o $@
	rm -f $(srcdir)/config.h

if CROSS_COMPILING
if !USE_EXTERNAL_ICON_CACHE
all-local: native-update-icon-cache$(BUILD_EXEEXT)

native_update_icon_cache_CFLAGS = $(NATIVE_GDKPIXBUF_CFLAGS)
native_update_icon_cache_LDADD = $(NATIVE_GDKPIXBUF_LIBS)
native_update_icon_cache_SOURCES = $(srcdir)/../updateiconcache.c
endif
endif

-include $(top_srcdir)/git.mk
