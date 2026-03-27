#pragma once

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_SIZE_CLAMP (gtk_size_clamp_get_type())

GDK_AVAILABLE_IN_4_24
G_DECLARE_FINAL_TYPE (GtkSizeClamp, gtk_size_clamp, GTK, SIZE_CLAMP, GtkWidget)

GDK_AVAILABLE_IN_4_24
GtkWidget *gtk_size_clamp_new                        (int max_width,
                                                      int max_height);

GDK_AVAILABLE_IN_4_24
void       gtk_size_clamp_set_child                  (GtkSizeClamp *self,
                                                      GtkWidget    *child);
GDK_AVAILABLE_IN_4_24
GtkWidget *gtk_size_clamp_get_child                  (GtkSizeClamp *self);

GDK_AVAILABLE_IN_4_24
int        gtk_size_clamp_get_max_width              (GtkSizeClamp *self);
GDK_AVAILABLE_IN_4_24
void       gtk_size_clamp_set_max_width              (GtkSizeClamp *self,
                                                      int           max_width);
GDK_AVAILABLE_IN_4_24
int        gtk_size_clamp_get_max_height             (GtkSizeClamp *self);
GDK_AVAILABLE_IN_4_24
void       gtk_size_clamp_set_max_height             (GtkSizeClamp *self,
                                                      int           max_height);

GDK_AVAILABLE_IN_4_24
gboolean   gtk_size_clamp_get_constant_size          (GtkSizeClamp *self);
GDK_AVAILABLE_IN_4_24
void       gtk_size_clamp_set_constant_size          (GtkSizeClamp *self,
                                                      gboolean      constant_size);

GDK_AVAILABLE_IN_4_24
gboolean   gtk_size_clamp_get_request_natural_width  (GtkSizeClamp *self);
GDK_AVAILABLE_IN_4_24
void       gtk_size_clamp_set_request_natural_width  (GtkSizeClamp *self,
                                                      gboolean      request_natural_width);

GDK_AVAILABLE_IN_4_24
gboolean   gtk_size_clamp_get_request_natural_height (GtkSizeClamp *self);
GDK_AVAILABLE_IN_4_24
void       gtk_size_clamp_set_request_natural_height (GtkSizeClamp *self,
                                                      gboolean      request_natural_height);

G_END_DECLS
