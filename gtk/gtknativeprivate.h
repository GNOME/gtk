#ifndef __GTK_NATIVE_PRIVATE_H__
#define __GTK_NATIVE_PRIVATE_H__

#include "gtknative.h"

G_BEGIN_DECLS

/**
 * GtkNativeIface:
 *
 * The list of functions that must be implemented for the #GtkNative interface.
 */
struct _GtkNativeInterface
{
  /*< private >*/
  GTypeInterface g_iface;

  /*< public >*/
  GdkSurface *  (* get_surface)           (GtkNative    *self);
  GskRenderer * (* get_renderer)          (GtkNative    *self);

  void          (* get_surface_transform) (GtkNative    *self,
                                           double       *x,
                                           double       *y);

  void          (* check_resize)          (GtkNative    *self);
};

G_END_DECLS

#endif /* __GTK_NATIVE_PRIVATE_H__ */
