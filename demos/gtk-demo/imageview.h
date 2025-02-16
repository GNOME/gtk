#pragma once

#include <gtk/gtk.h>

#define IMAGE_TYPE_VIEW (image_view_get_type ())
G_DECLARE_FINAL_TYPE (ImageView, image_view, IMAGE, VIEW, GtkWidget)

GtkWidget * image_view_new (const char *resource);
