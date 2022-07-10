#pragma once

#include <gtk/gtk.h>


#define PLAIN_VIEW_TYPE (plain_view_get_type ())
#define PLAIN_VIEW(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), PLAIN_VIEW_TYPE, PlainView))


typedef struct _PlainView         PlainView;
typedef struct _PlainViewClass    PlainViewClass;


GType           plain_view_get_type     (void);
