#pragma once

#include <gtk/gtk.h>


#define INFO_VIEW_TYPE (info_view_get_type ())
#define INFO_VIEW(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), INFO_VIEW_TYPE, InfoView))


typedef struct _InfoView         InfoView;
typedef struct _InfoViewClass    InfoViewClass;


GType           info_view_get_type     (void);
