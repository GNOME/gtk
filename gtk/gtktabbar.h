#pragma once

#include <gtk/gtkstack.h>
#include <gtk/gtkwidget.h>

#define GTK_TYPE_TAB_BAR (gtk_tab_bar_get_type ())
G_DECLARE_FINAL_TYPE (GtkTabBar, gtk_tab_bar, GTK, TAB_BAR, GtkWidget)

GDK_AVAILABLE_IN_4_12
GtkTabBar *     gtk_tab_bar_new          (void);

GDK_AVAILABLE_IN_4_12
void            gtk_tab_bar_set_stack    (GtkTabBar       *self,
                                          GtkStack        *stack);

GDK_AVAILABLE_IN_4_12
GtkStack *      gtk_tab_bar_get_stack    (GtkTabBar       *self);

GDK_AVAILABLE_IN_4_12
void            gtk_tab_bar_set_position (GtkTabBar       *self,
                                          GtkPositionType  position);
GDK_AVAILABLE_IN_4_12
GtkPositionType gtk_tab_bar_get_position (GtkTabBar       *self);

