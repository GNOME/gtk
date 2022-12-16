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

void gdk_source_set_static_name_by_id (guint       tag,
                                       const char *name);

#ifndef I_
#define I_(string) g_intern_static_string (string)
#endif

#if !GLIB_CHECK_VERSION (2, 76, 0)
static inline gboolean
g_set_str (char       **str_pointer,
           const char  *new_str)
{
  char *copy;

  if (*str_pointer == new_str ||
      (*str_pointer && new_str && strcmp (*str_pointer, new_str) == 0))
    return FALSE;

  copy = g_strdup (new_str);
  g_free (*str_pointer);
  *str_pointer = copy;

  return TRUE;
}
#endif

#endif /* __GDK__PRIVATE_H__ */
