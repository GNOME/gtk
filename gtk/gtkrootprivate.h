#ifndef __GTK_ROOT_PRIVATE_H__
#define __GTK_ROOT_PRIVATE_H__

#include "gtkroot.h"

G_BEGIN_DECLS

GdkDisplay *            gtk_root_get_display            (GtkRoot                *self);
GskRenderer *           gtk_root_get_renderer           (GtkRoot                *self);

void                    gtk_root_get_surface_transform  (GtkRoot                *self,
                                                         int                    *x,
                                                         int                    *y);

void                    gtk_root_queue_restyle          (GtkRoot                *self);
void                    gtk_root_start_layout_phase     (GtkRoot                *self);
void                    gtk_root_stop_layout_phase      (GtkRoot                *self);

gboolean                gtk_root_activate_key           (GtkRoot                *self,
                                                         GdkEventKey            *event);

void                    gtk_root_update_pointer_focus   (GtkRoot                *root,
                                                         GdkDevice              *device,
                                                         GdkEventSequence       *sequence,
                                                         GtkWidget              *target,
                                                         double                  x,
                                                         double                  y);
void                    gtk_root_update_pointer_focus_on_state_change (GtkRoot   *root,
                                                                       GtkWidget *widget);
GtkWidget *             gtk_root_lookup_pointer_focus   (GtkRoot                 *root,
                                                         GdkDevice              *device,
                                                         GdkEventSequence       *sequence);
GtkWidget *             gtk_root_lookup_pointer_focus_implicit_grab (GtkRoot          *root,
                                                                     GdkDevice        *device,
                                                                     GdkEventSequence *sequence);
GtkWidget *             gtk_root_lookup_effective_pointer_focus (GtkRoot          *root,
                                                                 GdkDevice        *device,
                                                                 GdkEventSequence *sequence);
void                    gtk_root_set_pointer_focus_grab  (GtkRoot                *root,
                                                          GdkDevice              *device,
                                                          GdkEventSequence       *sequence,
                                                          GtkWidget              *target);
void                    gtk_root_maybe_update_cursor     (GtkRoot                *root,
                                                          GtkWidget              *widget,
                                                          GdkDevice              *device);

enum {
  GTK_ROOT_PROP_FOCUS_WIDGET,
  GTK_ROOT_PROP_DEFAULT_WIDGET,
  GTK_ROOT_NUM_PROPERTIES
} GtkRootProperties;

guint gtk_root_install_properties (GObjectClass *object_class,
                                   guint         first_prop);

G_END_DECLS

#endif /* __GTK_ROOT_PRIVATE_H__ */
