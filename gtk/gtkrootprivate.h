#pragma once

#include "gtkroot.h"

#include "gtkconstraintsolverprivate.h"

G_BEGIN_DECLS

/**
 * GtkRootIface:
 *
 * The list of functions that must be implemented for the `GtkRoot` interface.
 */
struct _GtkRootInterface
{
  /*< private >*/
  GTypeInterface g_iface;

  /*< public >*/
  GdkDisplay * (* get_display)  (GtkRoot *self);

  GtkConstraintSolver * (* get_constraint_solver) (GtkRoot *self);

  GtkWidget *  (* get_focus)    (GtkRoot   *self);
  void         (* set_focus)    (GtkRoot   *self,
                                 GtkWidget *focus);

  gboolean     (* set_popout)   (GtkRoot   *self,
                                 GtkPopout *poput);

};

GtkConstraintSolver *   gtk_root_get_constraint_solver  (GtkRoot *self);

void             gtk_root_start_layout  (GtkRoot *self);
void             gtk_root_stop_layout   (GtkRoot *self);
void             gtk_root_queue_restyle (GtkRoot *self);
gboolean         gtk_root_set_popout    (GtkRoot *self,
                                         GtkPopout *popout);

G_END_DECLS

