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
#include "gtkcolorutils.h"
#include "gtkorientable.h"
#include "gtkstylecontext.h"
#include "gtkaccessible.h"
#include "gtkpressandholdprivate.h"
#include "gtkprivate.h"
#include "gtkintl.h"

#include <math.h>

struct _GtkColorScalePrivate
{
  cairo_surface_t *surface;
  gint width, height;
  GdkRGBA color;
  GtkColorScaleType type;

  GtkPressAndHold *press_and_hold;
};

enum
{
  PROP_ZERO,
  PROP_SCALE_TYPE
};

G_DEFINE_TYPE (GtkColorScale, gtk_color_scale, GTK_TYPE_SCALE)

static void
gtk_color_scale_get_trough_size (GtkColorScale *scale,
                                 gint *x_offset_out,
                                 gint *y_offset_out,
                                 gint *width_out,
                                 gint *height_out)
{
  GtkWidget *widget = GTK_WIDGET (scale);
  gint width, height, focus_line_width, focus_padding;
  gint x_offset, y_offset;
  gint slider_width, slider_height;

  gtk_widget_style_get (widget,
                        "focus-line-width", &focus_line_width,
                        "focus-padding", &focus_padding,
                        "slider-width", &slider_width,
                        "slider-length", &slider_height,
                        NULL);

  width = gtk_widget_get_allocated_width (widget) - 2 * (focus_line_width + focus_padding);
  height = gtk_widget_get_allocated_height (widget) - 2 * (focus_line_width + focus_padding);

  x_offset = focus_line_width + focus_padding;
  y_offset = focus_line_width + focus_padding;

  /* if the slider has a vertical shape, draw the trough asymmetric */
  if (slider_width > slider_height)
    {
      if (gtk_orientable_get_orientation (GTK_ORIENTABLE (widget)) == GTK_ORIENTATION_VERTICAL)
        {
          if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
            x_offset += (gint) floor (slider_width / 2.0);

          width = (gint) floor (slider_width / 2.0);
        }
      else
        {
          height = (gint) floor (slider_width / 2.0);
        }
    }

  if (width_out)
    *width_out = width;
  if (height_out)
    *height_out = height;
  if (x_offset_out)
    *x_offset_out = x_offset;
  if (y_offset_out)
    *y_offset_out = y_offset;
}

static void
create_surface (GtkColorScale *scale)
{
  GtkWidget *widget = GTK_WIDGET (scale);
  cairo_surface_t *surface;
  gint width, height;

  if (!gtk_widget_get_realized (widget))
    return;

  gtk_color_scale_get_trough_size (scale,
                                   NULL, NULL,
                                   &width, &height);

  if (!scale->priv->surface ||
      width != scale->priv->width ||
      height != scale->priv->height)
    {
      surface = gdk_window_create_similar_surface (gtk_widget_get_window (widget),
                                                   CAIRO_CONTENT_COLOR,
                                                   width, height);
      if (scale->priv->surface)
        cairo_surface_destroy (scale->priv->surface);
      scale->priv->surface = surface;
      scale->priv->width = width;
      scale->priv->height= height;
    }
  else
    surface = scale->priv->surface;

  if (width == 1 || height == 1)
    return;

  if (scale->priv->type == GTK_COLOR_SCALE_HUE)
    {
      cairo_t *cr;
      gint stride;
      cairo_surface_t *tmp;
      guint red, green, blue;
      guint32 *data, *p;
      gdouble h;
      gdouble r, g, b;
      gdouble f;
      gint x, y;

      stride = cairo_format_stride_for_width (CAIRO_FORMAT_RGB24, width);

      data = g_malloc (height * stride);

      f = 1.0 / (height - 1);
      for (y = 0; y < height; y++)
        {
          h = CLAMP (y * f, 0.0, 1.0);
          p = data + y * (stride / 4);
          for (x = 0; x < width; x++)
            {
              gtk_hsv_to_rgb (h, 1, 1, &r, &g, &b);
              red = CLAMP (r * 255, 0, 255);
              green = CLAMP (g * 255, 0, 255);
              blue = CLAMP (b * 255, 0, 255);
              p[x] = (red << 16) | (green << 8) | blue;
            }
        }

      tmp = cairo_image_surface_create_for_data ((guchar *)data, CAIRO_FORMAT_RGB24,
                                                 width, height, stride);
      cr = cairo_create (surface);

      cairo_set_source_surface (cr, tmp, 0, 0);
      cairo_paint (cr);

      cairo_destroy (cr);
      cairo_surface_destroy (tmp);
      g_free (data);
    }
  else if (scale->priv->type == GTK_COLOR_SCALE_ALPHA)
    {
      cairo_t *cr;
      cairo_pattern_t *pattern;
      cairo_matrix_t matrix;
      GdkRGBA *color;

      cr = cairo_create (surface);

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

      cairo_destroy (cr);
    }
}

static gboolean
scale_draw (GtkWidget *widget,
            cairo_t   *cr)
{
  GtkColorScale *scale = GTK_COLOR_SCALE (widget);
  gint width, height, x_offset, y_offset;
  cairo_pattern_t *pattern;

  create_surface (scale);
  gtk_color_scale_get_trough_size (scale,
                                   &x_offset, &y_offset,
                                   &width, &height);

  cairo_save (cr);
  cairo_translate (cr, x_offset, y_offset);
  cairo_rectangle (cr, 0, 0, width, height);

  pattern = cairo_pattern_create_for_surface (scale->priv->surface);
  if (gtk_orientable_get_orientation (GTK_ORIENTABLE (widget)) == GTK_ORIENTATION_HORIZONTAL &&
      gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    {
      cairo_matrix_t matrix;

      cairo_matrix_init_scale (&matrix, -1, 1);
      cairo_matrix_translate (&matrix, -width, 0);
      cairo_pattern_set_matrix (pattern, &matrix);
    }
  cairo_set_source (cr, pattern);
  cairo_fill (cr);

  cairo_pattern_destroy (pattern);

  cairo_restore (cr);

  GTK_WIDGET_CLASS (gtk_color_scale_parent_class)->draw (widget, cr);

  return FALSE;
}

static void
gtk_color_scale_init (GtkColorScale *scale)
{
  scale->priv = G_TYPE_INSTANCE_GET_PRIVATE (scale,
                                             GTK_TYPE_COLOR_SCALE,
                                             GtkColorScalePrivate);
  gtk_widget_add_events (GTK_WIDGET (scale), GDK_TOUCH_MASK);
}

static void
scale_finalize (GObject *object)
{
  GtkColorScale *scale = GTK_COLOR_SCALE (object);

  if (scale->priv->surface)
    cairo_surface_destroy (scale->priv->surface);

  g_clear_object (&scale->priv->press_and_hold);

  G_OBJECT_CLASS (gtk_color_scale_parent_class)->finalize (object);
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
      atk_object_set_role (gtk_widget_get_accessible (GTK_WIDGET (scale)), ATK_ROLE_COLOR_CHOOSER);
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
hold_action (GtkPressAndHold *pah,
             gint             x,
             gint             y,
             GtkColorScale   *scale)
{
  gboolean handled;

  g_signal_emit_by_name (scale, "popup-menu", &handled);
}

static gboolean
scale_touch (GtkWidget     *widget,
             GdkEventTouch *event)
{
  GtkColorScale *scale = GTK_COLOR_SCALE (widget);

  if (!scale->priv->press_and_hold)
    {
      gint drag_threshold;

      g_object_get (gtk_widget_get_settings (widget),
                    "gtk-dnd-drag-threshold", &drag_threshold,
                    NULL);

      scale->priv->press_and_hold = gtk_press_and_hold_new ();

      g_object_set (scale->priv->press_and_hold,
                    "drag-threshold", drag_threshold,
                    "hold-time", 1000,
                    NULL);

      g_signal_connect (scale->priv->press_and_hold, "hold",
                        G_CALLBACK (hold_action), scale);
    }

  gtk_press_and_hold_process_event (scale->priv->press_and_hold, (GdkEvent *)event);

  return GTK_WIDGET_CLASS (gtk_color_scale_parent_class)->touch_event (widget, event);
}


static void
gtk_color_scale_class_init (GtkColorScaleClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->finalize = scale_finalize;
  object_class->get_property = scale_get_property;
  object_class->set_property = scale_set_property;

  widget_class->draw = scale_draw;
  widget_class->touch_event = scale_touch;

  g_object_class_install_property (object_class, PROP_SCALE_TYPE,
      g_param_spec_int ("scale-type", P_("Scale type"), P_("Scale type"),
                        0, 1, 0,
                        GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (class, sizeof (GtkColorScalePrivate));
}

void
gtk_color_scale_set_rgba (GtkColorScale *scale,
                          const GdkRGBA *color)
{
  scale->priv->color = *color;
  scale->priv->width = -1; /* force surface refresh */
  create_surface (scale);
  gtk_widget_queue_draw (GTK_WIDGET (scale));
}

GtkWidget *
gtk_color_scale_new (GtkAdjustment     *adjustment,
                     GtkColorScaleType  type)
{
  return (GtkWidget *) g_object_new (GTK_TYPE_COLOR_SCALE,
                                     "adjustment", adjustment,
                                     "draw-value", FALSE,
                                     "scale-type", type,
                                     NULL);
}
