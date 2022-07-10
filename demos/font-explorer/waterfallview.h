#pragma once

#include <gtk/gtk.h>


#define WATERFALL_VIEW_TYPE (waterfall_view_get_type ())
#define WATERFALL_VIEW(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), WATERFALL_VIEW_TYPE, WaterfallView))


typedef struct _WaterfallView         WaterfallView;
typedef struct _WaterfallViewClass    WaterfallViewClass;


GType           waterfall_view_get_type     (void);
