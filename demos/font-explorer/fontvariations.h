#pragma once

#include <gtk/gtk.h>


#define FONT_VARIATIONS_TYPE (font_variations_get_type ())
#define FONT_VARIATIONS(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), FONT_VARIATIONS_TYPE, FontVariations))


typedef struct _FontVariations         FontVariations;
typedef struct _FontVariationsClass    FontVariationsClass;


GType            font_variations_get_type          (void);
FontVariations * font_variations_new               (void);
void             font_variations_set_font_map      (FontVariations *self,
                                                    Pango2FontMap  *map);
GAction *        font_variations_get_reset_action  (FontVariations *self);
