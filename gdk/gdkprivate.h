#ifndef __GDK__PRIVATE_H__
#define __GDK__PRIVATE_H__

#include "gdk/gdktypes.h"

/* Private API for use in GTK+ */

void            gdk_pre_parse                   (void);

gboolean gdk_running_in_sandbox (void);
gboolean gdk_should_use_portal (void);

const char *   gdk_get_startup_notification_id (void);

PangoDirection gdk_unichar_direction (gunichar    ch) G_GNUC_CONST;
PangoDirection gdk_find_base_dir     (const char *text,
                                      int         len);

/* Backward compatibility shim, to avoid bumping up the minimum
 * required version of GLib; most of our uses of g_memdup() are
 * safe, and those that aren't have been fixed
 */
#if !GLIB_CHECK_VERSION (2, 67, 3)
# define g_memdup2(mem,size)    g_memdup((mem),(size))
#endif

void gdk_source_set_static_name_by_id (guint       tag,
                                       const char *name);

#if !GLIB_CHECK_VERSION(2, 69, 1)
#define g_source_set_static_name(source, name) g_source_set_name ((source), (name))
#endif

#ifndef I_
#define I_(string) g_intern_static_string (string)
#endif

#endif /* __GDK__PRIVATE_H__ */
