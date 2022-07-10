#pragma once

#include <gtk/gtk.h>


#define FONT_FEATURES_TYPE (font_features_get_type ())
#define FONT_FEATURES(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), FONT_FEATURES_TYPE, FontFeatures))


typedef struct _FontFeatures         FontFeatures;
typedef struct _FontFeaturesClass    FontFeaturesClass;


GType          font_features_get_type          (void);
GAction *      font_features_get_reset_action  (FontFeatures  *self);
