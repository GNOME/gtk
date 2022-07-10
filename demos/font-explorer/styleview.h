#pragma once

#include <gtk/gtk.h>


#define STYLE_VIEW_TYPE (style_view_get_type ())
#define STYLE_VIEW(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), STYLE_VIEW_TYPE, StyleView))


typedef struct _StyleView         StyleView;
typedef struct _StyleViewClass    StyleViewClass;


GType           style_view_get_type     (void);
