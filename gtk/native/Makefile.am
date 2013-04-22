CC = @CC_FOR_BUILD@
AM_CFLAGS = @CFLAGS_FOR_BUILD@
CPP = @CPP_FOR_BUILD@
AM_CPPFLAGS = @CPPFLAGS_FOR_BUILD@
AM_LDFLAGS = @LDFLAGS_FOR_BUILD@

if CROSS_COMPILING
if !USE_EXTERNAL_ICON_CACHE
noinst_PROGRAMS = native-update-icon-cache
native_update_icon_cache_CFLAGS = $(NATIVE_GDKPIXBUF_CFLAGS)
native_update_icon_cache_LDADD = $(NATIVE_GDKPIXBUF_LIBS)
native_update_icon_cache_SOURCES = $(srcdir)/../updateiconcache.c
endif
endif

-include $(top_srcdir)/git.mk
