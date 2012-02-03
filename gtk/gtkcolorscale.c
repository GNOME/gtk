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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "gtkcolorscale.h"
#include "gtkhsv.h"
#include "gtkorientable.h"
#include "gtkstylecontext.h"
#include "gtkaccessible.h"
#include "gtkintl.h"

struct _GtkColorScalePrivate
{
  cairo_surface_t *surface;
  gint width, height;
  GdkRGBA color;
  GtkColorScaleType type;
};

G_DEFINE_TYPE (GtkColorScale, gtk_color_scale, GTK_TYPE_SCALE)

static cairo_pattern_t *
get_checkered_pattern (void)
{
  /* need to respect pixman's stride being a multiple of 4 */
  static unsigned char data[8] = { 0xFF, 0x00, 0x00, 0x00,
                                   0x00, 0xFF, 0x00, 0x00 };
  static cairo_surface_t *checkered = NULL;
  cairo_pattern_t *pattern;

  if (checkered == NULL)
    checkered = cairo_image_surface_create_for_data (data,
                                                     CAIRO_FORMAT_A8,
                                                     2, 2, 4);

  pattern = cairo_pattern_create_for_surface (checkered);
  cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);
  cairo_pattern_set_filter (pattern, CAIRO_FILTER_NEAREST);

  return pattern;
}

static void
create_h_surface (GtkColorScale *scale)
{
  GtkWidget *widget = GTK_WIDGET (scale);
  cairo_t *cr;
  cairo_surface_t *surface;
  gint width, height, stride;
  cairo_surface_t *tmp;
  guint red, green, blue;
  guint32 *data, *p;
  gdouble h;
  gdouble r, g, b;
  gdouble f;
  gint x, y;

  if (!gtk_widget_get_realized (widget))
    return;

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  if (width != scale->priv->width ||
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

static void
create_a_surface (GtkColorScale *scale)
{
  GtkWidget *widget = GTK_WIDGET (scale);
  cairo_t *cr;
  cairo_surface_t *surface;
  cairo_pattern_t *pattern;
  cairo_matrix_t matrix;
  GdkRGBA *color;
  gint width, height;

  if (!gtk_widget_get_realized (widget))
    return;

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

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
      scale->priv->height = height;
    }
  else
    return;

  if (width == 1 || height == 1)
    return;

  cr = cairo_create (surface);

  cairo_set_source_rgb (cr, 0.33, 0.33, 0.33);
  cairo_paint (cr);
  cairo_set_source_rgb (cr, 0.66, 0.66, 0.66);

  pattern = get_checkered_pattern ();
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

static void
create_surface (GtkColorScale *scale)
{
  switch (scale->priv->type)
    {
    case GTK_COLOR_SCALE_HUE:
      create_h_surface (scale);
      break;
    case GTK_COLOR_SCALE_ALPHA:
      create_a_surface (scale);
      break;
    }
}

static gboolean
scale_has_asymmetric_thumb (GtkWidget *widget)
{
  gchar *theme;
  gboolean res;

  g_object_get (gtk_widget_get_settings (widget),
                "gtk-theme-name", &theme,
                NULL);
  res = strcmp ("Adwaita", theme) == 0;
  g_free (theme);

  return res;
}

static gboolean
scale_draw (GtkWidget *widget,
            cairo_t   *cr)
{
  GtkColorScale *scale = GTK_COLOR_SCALE (widget);
  gint width, height;

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  create_surface (scale);

  cairo_save (cr);

  if (scale_has_asymmetric_thumb (widget))
    {
      if (gtk_orientable_get_orientation (GTK_ORIENTABLE (widget)) == GTK_ORIENTATION_VERTICAL)
        {
          if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
            cairo_rectangle (cr, 1, 1, width / 2 - 1, height - 2);
          else
            cairo_rectangle (cr, width / 2, 1, width / 2 - 1, height - 2);
        }
      else
        {
          cairo_rectangle (cr, 1, 1, width - 2, height / 2);
        }
    }
  else
    cairo_rectangle (cr, 1, 1, width - 2, height - 2);

  cairo_clip (cr);
  cairo_set_source_surface (cr, scale->priv->surface, 0, 0);
  cairo_paint (cr);

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

  scale->priv->type = GTK_COLOR_SCALE_HUE;
}

static void
scale_finalize (GObject *object)
{
  GtkColorScale *scale = GTK_COLOR_SCALE (object);

  if (scale->priv->surface)
    cairo_surface_destroy (scale->priv->surface);

  G_OBJECT_CLASS (gtk_color_scale_parent_class)->finalize (object);
}

static void
gtk_color_scale_class_init (GtkColorScaleClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->finalize = scale_finalize;

  widget_class->draw = scale_draw;

  g_type_class_add_private (class, sizeof (GtkColorScalePrivate));
}

void
gtk_color_scale_set_color (GtkColorScale *scale,
                           const GdkRGBA *color)
{
  scale->priv->color.red = color->red;
  scale->priv->color.green = color->green;
  scale->priv->color.blue = color->blue;
  scale->priv->color.alpha = color->alpha;
  if (scale->priv->surface)
    {
      cairo_surface_destroy (scale->priv->surface);
      scale->priv->surface = NULL;
    }
  create_surface (scale);
  gtk_widget_queue_draw (GTK_WIDGET (scale));
}

static void
gtk_color_scale_set_type (GtkColorScale     *scale,
                          GtkColorScaleType  type)
{
  AtkObject *atk_obj;

  scale->priv->type = type;
  cairo_surface_destroy (scale->priv->surface);
  scale->priv->surface = NULL;
  create_surface (scale);
  gtk_widget_queue_draw (GTK_WIDGET (scale));

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

GtkWidget *
gtk_color_scale_new (GtkAdjustment     *adjustment,
                     GtkColorScaleType  type)
{
  GtkWidget *scale;

  scale = (GtkWidget *) g_object_new (GTK_TYPE_COLOR_SCALE,
                                      "adjustment", adjustment,
                                      "draw-value", FALSE,
                                      NULL);
  gtk_color_scale_set_type (GTK_COLOR_SCALE (scale), type);

  return scale;
}
