#pragma once

#include <gtk/gtk.h>


#define FONT_VIEW_TYPE (font_view_get_type ())
#define FONT_VIEW(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), FONT_VIEW_TYPE, FontView))


typedef struct _FontView         FontView;
typedef struct _FontViewClass    FontViewClass;


GType           font_view_get_type     (void);
FontView *      font_view_new          (void);
void            font_view_set_font_map (FontView      *self,
                                        Pango2FontMap *map);
