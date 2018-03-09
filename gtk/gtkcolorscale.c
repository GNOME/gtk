/* GTK - The GIMP Toolkit
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkcolorscaleprivate.h"

#include "gtkcolorchooserprivate.h"
#include "gtkgesturelongpress.h"
#include "gtkcolorutils.h"
#include "gtkorientable.h"
#include "gtkrangeprivate.h"
#include "gtkstylecontext.h"
#include "gtkaccessible.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtksnapshot.h"

#include <math.h>

struct _GtkColorScalePrivate
{
  GdkRGBA color;
  GtkColorScaleType type;
};

enum
{
  PROP_ZERO,
  PROP_SCALE_TYPE
};

static void hold_action (GtkGestureLongPress *gesture,
                         gdouble              x,
                         gdouble              y,
                         GtkColorScale       *scale);

G_DEFINE_TYPE_WITH_PRIVATE (GtkColorScale, gtk_color_scale, GTK_TYPE_SCALE)

void
gtk_color_scale_snapshot_trough (GtkColorScale  *scale,
                                 GtkSnapshot    *snapshot,
                                 int             x,
                                 int             y,
                                 int             width,
                                 int             height)
{
  GtkWidget *widget = GTK_WIDGET (scale);
  cairo_t *cr;

  if (width <= 1 || height <= 1)
    return;

  cr = gtk_snapshot_append_cairo (snapshot,
                                  &GRAPHENE_RECT_INIT(x, y, width, height),
                                  "ColorScaleTrough");
  cairo_translate (cr, x, y);

  if (gtk_orientable_get_orientation (GTK_ORIENTABLE (widget)) == GTK_ORIENTATION_HORIZONTAL &&
      gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    {
      cairo_translate (cr, width, 0);
      cairo_scale (cr, -1, 1);
    }

  if (scale->priv->type == GTK_COLOR_SCALE_HUE)
    {
      gint stride;
      cairo_surface_t *tmp;
      guint red, green, blue;
      guint32 *data, *p;
      gdouble h;
      gdouble r, g, b;
      gdouble f;
      int hue_x, hue_y;

      stride = cairo_format_stride_for_width (CAIRO_FORMAT_RGB24, width);

      data = g_malloc (height * stride);

      f = 1.0 / (height - 1);
      for (hue_y = 0; hue_y < height; hue_y++)
        {
          h = CLAMP (hue_y * f, 0.0, 1.0);
          p = data + hue_y * (stride / 4);
          for (hue_x = 0; hue_x < width; hue_x++)
            {
              gtk_hsv_to_rgb (h, 1, 1, &r, &g, &b);
              red = CLAMP (r * 255, 0, 255);
              green = CLAMP (g * 255, 0, 255);
              blue = CLAMP (b * 255, 0, 255);
              p[hue_x] = (red << 16) | (green << 8) | blue;
            }
        }

      tmp = cairo_image_surface_create_for_data ((guchar *)data, CAIRO_FORMAT_RGB24,
                                                 width, height, stride);

      cairo_set_source_surface (cr, tmp, 0, 0);
      cairo_paint (cr);

      cairo_surface_destroy (tmp);
      g_free (data);
    }
  else if (scale->priv->type == GTK_COLOR_SCALE_ALPHA)
    {
      cairo_pattern_t *pattern;
      cairo_matrix_t matrix;
      GdkRGBA *color;

      cairo_set_source_rgb (cr, 0.33, 0.33, 0.33);
      cairo_paint (cr);
      cairo_set_source_rgb (cr, 0.66, 0.66, 0.66);

      pattern = _gtk_color_chooser_get_checkered_pattern ();
      cairo_matrix_init_scale (&matrix, 0.125, 0.125);
      cairo_pattern_set_matrix (pattern, &matrix);
      cairo_mask (cr, pattern);
      cairo_pattern_destroy (pattern);

      color = &scale->priv->color;

      pattern = cairo_pattern_create_linear (0, 0, width, 0);
      cairo_pattern_add_color_stop_rgba (pattern, 0, color->red, color->green, color->blue, 0);
      cairo_pattern_add_color_stop_rgba (pattern, width, color->red, color->green, color->blue, 1);
      cairo_set_source (cr, pattern);
      cairo_paint (cr);
      cairo_pattern_destroy (pattern);
    }

  cairo_destroy (cr);
}

static void
gtk_color_scale_init (GtkColorScale *scale)
{
  GtkStyleContext *context;
  GtkGesture *gesture;

  scale->priv = gtk_color_scale_get_instance_private (scale);

  gesture = gtk_gesture_long_press_new ();
  g_signal_connect (gesture, "pressed",
                    G_CALLBACK (hold_action), scale);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture),
                                              GTK_PHASE_TARGET);
  gtk_widget_add_controller (GTK_WIDGET (scale), GTK_EVENT_CONTROLLER (gesture));

  context = gtk_widget_get_style_context (GTK_WIDGET (scale));
  gtk_style_context_add_class (context, "color");
}

static void
scale_get_property (GObject    *object,
                    guint       prop_id,
                    GValue     *value,
                    GParamSpec *pspec)
{
  GtkColorScale *scale = GTK_COLOR_SCALE (object);

  switch (prop_id)
    {
    case PROP_SCALE_TYPE:
      g_value_set_int (value, scale->priv->type);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
scale_set_type (GtkColorScale     *scale,
                GtkColorScaleType  type)
{
  AtkObject *atk_obj;

  scale->priv->type = type;

  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (scale));
  if (GTK_IS_ACCESSIBLE (atk_obj))
    {
      if (type == GTK_COLOR_SCALE_HUE)
        atk_object_set_name (atk_obj, C_("Color channel", "Hue"));
      else if (type == GTK_COLOR_SCALE_ALPHA)
        atk_object_set_name (atk_obj, C_("Color channel", "Alpha"));
      atk_object_set_role (atk_obj, ATK_ROLE_COLOR_CHOOSER);
    }
}

static void
scale_set_property (GObject      *object,
                    guint         prop_id,
                    const GValue *value,
                    GParamSpec   *pspec)
{
  GtkColorScale *scale = GTK_COLOR_SCALE (object);

  switch (prop_id)
    {
    case PROP_SCALE_TYPE:
      scale_set_type (scale, (GtkColorScaleType)g_value_get_int (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hold_action (GtkGestureLongPress *gesture,
             gdouble              x,
             gdouble              y,
             GtkColorScale       *scale)
{
  gboolean handled;

  g_signal_emit_by_name (scale, "popup-menu", &handled);
}

static void
gtk_color_scale_class_init (GtkColorScaleClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->get_property = scale_get_property;
  object_class->set_property = scale_set_property;

  g_object_class_install_property (object_class, PROP_SCALE_TYPE,
      g_param_spec_int ("scale-type", P_("Scale type"), P_("Scale type"),
                        0, 1, 0,
                        GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

void
gtk_color_scale_set_rgba (GtkColorScale *scale,
                          const GdkRGBA *color)
{
  scale->priv->color = *color;
  gtk_widget_queue_draw (GTK_WIDGET (scale));
}

GtkWidget *
gtk_color_scale_new (GtkAdjustment     *adjustment,
                     GtkColorScaleType  type)
{
  return g_object_new (GTK_TYPE_COLOR_SCALE,
                       "adjustment", adjustment,
                       "draw-value", FALSE,
                       "scale-type", type,
                       NULL);
}
