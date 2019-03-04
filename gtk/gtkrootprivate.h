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

enum {
  GTK_ROOT_PROP_FOCUS_WIDGET,
  GTK_ROOT_PROP_DEFAULT_WIDGET,
  GTK_ROOT_NUM_PROPERTIES
} GtkRootProperties;

guint gtk_root_install_properties (GObjectClass *object_class,
                                   guint         first_prop);

G_END_DECLS

#endif /* __GTK_ROOT_PRIVATE_H__ */
