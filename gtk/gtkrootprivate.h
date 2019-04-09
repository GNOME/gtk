#ifndef __GTK_ROOT_PRIVATE_H__
#define __GTK_ROOT_PRIVATE_H__

#include "gtkroot.h"

#include "gtkconstraintsolverprivate.h"

G_BEGIN_DECLS

/*
 * GtkRootIface:
 *
 * The list of functions that must be implemented for the #GtkRoot interface.
 */
struct _GtkRootInterface
{
  /*< private >*/
  GTypeInterface g_iface;

  /*< public >*/
  GdkDisplay *          (* get_display)                 (GtkRoot                *self);
  GskRenderer *         (* get_renderer)                (GtkRoot                *self);

  void                  (* get_surface_transform)       (GtkRoot                *self,
                                                         int                    *x,
                                                         int                    *y);

  GtkConstraintSolver * (* get_constraint_solver)       (GtkRoot                *self);
};

GdkDisplay *            gtk_root_get_display            (GtkRoot                *self);
GskRenderer *           gtk_root_get_renderer           (GtkRoot                *self);

void                    gtk_root_get_surface_transform  (GtkRoot                *self,
                                                         int                    *x,
                                                         int                    *y);

GtkConstraintSolver *   gtk_root_get_constraint_solver  (GtkRoot                *self);

enum {
  GTK_ROOT_PROP_FOCUS_WIDGET,
  GTK_ROOT_NUM_PROPERTIES
} GtkRootProperties;

guint gtk_root_install_properties (GObjectClass *object_class,
                                   guint         first_prop);

G_END_DECLS

#endif /* __GTK_ROOT_PRIVATE_H__ */
