#pragma once

#include <gtk/gtkwidget.h>
#include <gtk/gtkstack.h>

#define GTK_TYPE_TAB_WIDGET (gtk_tab_widget_get_type ())
G_DECLARE_FINAL_TYPE (GtkTabWidget, gtk_tab_widget, GTK, TAB_WIDGET, GtkWidget)

GtkWidget *  gtk_tab_widget_new (GtkStackPage *page,
                                 unsigned int  position);
