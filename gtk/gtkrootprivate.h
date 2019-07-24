#ifndef __GTK_ROOT_PRIVATE_H__
#define __GTK_ROOT_PRIVATE_H__

#include "gtkroot.h"

#include "gtkanimationmanagerprivate.h"
#include "gtkconstraintsolverprivate.h"

G_BEGIN_DECLS

/*< private >
 * GtkRootIface:
 *
 * The list of functions that must be implemented for the #GtkRoot interface.
 */
struct _GtkRootInterface
{
  /*< private >*/
  GTypeInterface g_iface;

  /*< public >*/
  GdkDisplay * (* get_display)  (GtkRoot *self);

  GtkConstraintSolver * (* get_constraint_solver) (GtkRoot *self);
  GtkAnimationManager * (* get_animation_manager) (GtkRoot *self);
};

GtkConstraintSolver *   gtk_root_get_constraint_solver  (GtkRoot *self);
GtkAnimationManager *   gtk_root_get_animation_manager  (GtkRoot *self);

enum {
  GTK_ROOT_PROP_FOCUS_WIDGET,
  GTK_ROOT_NUM_PROPERTIES
} GtkRootProperties;

guint gtk_root_install_properties (GObjectClass *object_class,
                                   guint         first_prop);

G_END_DECLS

#endif /* __GTK_ROOT_PRIVATE_H__ */
