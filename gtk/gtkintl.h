#ifndef __GTKINTL_H__
#define __GTKINTL_H__

#include <glib/gi18n-lib.h>

#define P_(String) g_dgettext (GETTEXT_PACKAGE "-properties", String)

/* not really I18N-related, but also a string marker macro */
#define I_(string) g_intern_static_string (string)

#endif
