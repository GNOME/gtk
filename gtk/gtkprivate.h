#ifndef __GTK_PRIVATE_H__
#define __GTK_PRIVATE_H__

#include <glib.h>

G_BEGIN_DECLS

#ifdef G_OS_WIN32

const gchar *_gtk_get_datadir ();
const gchar *_gtk_get_libdir ();
const gchar *_gtk_get_sysconfdir ();
const gchar *_gtk_get_localedir ();
const gchar *_gtk_get_data_prefix ();

#undef GTK_DATADIR
#define GTK_DATADIR _gtk_get_datadir ()
#undef GTK_LIBDIR
#define GTK_LIBDIR _gtk_get_libdir ()
#undef GTK_LOCALEDIR
#define GTK_LOCALEDIR _gtk_get_localedir ()
#undef GTK_SYSCONFDIR
#define GTK_SYSCONFDIR _gtk_get_sysconfdir ()
#undef GTK_DATA_PREFIX
#define GTK_DATA_PREFIX _gtk_get_data_prefix ()

#endif /* G_OS_WIN32 */

#define GTK_PARAM_READABLE G_PARAM_READABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB
#define GTK_PARAM_WRITABLE G_PARAM_WRITABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB
#define GTK_PARAM_READWRITE G_PARAM_READWRITE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB

/* Many keyboard shortcuts for Mac are the same as for X
 * except they use Command key instead of Control (e.g. Cut,
 * Copy, Paste). This symbol is for those simple cases. */
#ifndef GDK_WINDOWING_QUARTZ
#define GTK_DEFAULT_ACCEL_MOD_MASK GDK_CONTROL_MASK
#else
#define GTK_DEFAULT_ACCEL_MOD_MASK GDK_META_MASK
#endif

gboolean _gtk_fnmatch (const char *pattern,
		       const char *string,
		       gboolean    no_leading_period);

G_END_DECLS

#endif /* __GTK_PRIVATE_H__ */
