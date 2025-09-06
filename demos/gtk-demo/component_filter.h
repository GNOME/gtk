#pragma once

#include <gtk/gtk.h>

G_DECLARE_FINAL_TYPE (ComponentFilter, component_filter, COMPONENT, FILTER, GtkWidget)

GtkWidget *            component_filter_new                    (void);

GskComponentTransfer * component_filter_get_component_transfer (ComponentFilter *self);
