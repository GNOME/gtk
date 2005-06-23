#ifndef __GDKPIXBUFINTL_H__
#define __GDKPIXBUFINTL_H__

#include "config.h"
#include <glib/gi18n-lib.h>

#ifdef ENABLE_NLS
#define P_(String) dgettext(GETTEXT_PACKAGE "-properties",String)
#else
#define P_(String) (String)
#endif

#endif
