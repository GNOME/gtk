/*
 * Copyright © 2022 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtktexttile.h"

#include "gtkcssstylechangeprivate.h"
#include "gtksnapshot.h"
#include "gtkwidgetprivate.h"

/**
 * GtkTextTile:
 *
 * `GtkTextTile` is a widget to show text in a predefined area.
 *
 * You likely want to use `GtkLabel` instead as this widget is for a small subset
 * of use cases. The main use case is usage inside lists such as `GtkColumnView`.
 *
 * While a `GtkLabel` sizes itself according to the text that is displayed,
 * `GtkTextTile` is given a size and fits the given text into that size as good
 * as it can.
 *
 * As it is a common occurence that text doesn't fit, users of this widget should
 * plan for that case.
 */

/* 3 chars are enough to display ellipsizing "..." */
#define DEFAULT_MIN_CHARS 3
/* This means we request no natural size and fall back to min size */
#define DEFAULT_NAT_CHARS 0
/* 1 line is what people want in 90% of cases */
#define DEFAULT_MIN_LINES 1
/* This means we request no natural size and fall back to min size */
#define DEFAULT_NAT_LINES 0

struct _GtkTextTile
{
  GtkWidget parent_instance;

  char *text;
  int min_chars;
  int nat_chars;
  int min_lines;
  int nat_lines;

  PangoLayout *layout;
};

enum
{
  PROP_0,
  PROP_TEXT,

  N_PROPS
};

G_DEFINE_TYPE (GtkTextTile, gtk_text_tile, GTK_TYPE_WIDGET)

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
gtk_text_tile_dispose (GObject *object)
{
  GtkTextTile *self = GTK_TEXT_TILE (object);

  g_clear_pointer (&self->text, g_free);

  G_OBJECT_CLASS (gtk_text_tile_parent_class)->dispose (object);
}

static void
gtk_text_tile_finalize (GObject *object)
{
  GtkTextTile *self = GTK_TEXT_TILE (object);

  g_clear_object (&self->layout);

  G_OBJECT_CLASS (gtk_text_tile_parent_class)->finalize (object);
}

static void
gtk_text_tile_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkTextTile *self = GTK_TEXT_TILE (object);

  switch (property_id)
    {
    case PROP_TEXT:
      g_value_set_string (value, self->text);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_text_tile_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkTextTile *self = GTK_TEXT_TILE (object);

  switch (property_id)
    {
    case PROP_TEXT:
      gtk_text_tile_set_text (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_text_tile_css_changed (GtkWidget         *widget,
                           GtkCssStyleChange *change)
{
  GtkTextTile *self = GTK_TEXT_TILE (widget);

  GTK_WIDGET_CLASS (gtk_text_tile_parent_class)->css_changed (widget, change);

  if (gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_TEXT_ATTRS))
    {
      PangoAttrList *new_attrs;

      new_attrs = gtk_css_style_get_pango_attributes (gtk_css_style_change_get_new_style (change));
      pango_layout_set_attributes (self->layout, new_attrs);
      pango_attr_list_unref (new_attrs);

      gtk_widget_queue_draw (widget);
    }
}

static int
get_char_pixels (PangoLayout *layout)
{
  PangoContext *context;
  PangoFontMetrics *metrics;
  int char_width, digit_width;

  context = pango_layout_get_context (layout);
  metrics = pango_context_get_metrics (context, NULL, NULL);
  char_width = pango_font_metrics_get_approximate_char_width (metrics);
  digit_width = pango_font_metrics_get_approximate_digit_width (metrics);
  pango_font_metrics_unref (metrics);

  return MAX (char_width, digit_width);
}

static void
gtk_text_tile_measure_width (GtkTextTile *self,
                             int         *minimum,
                             int         *natural)
{
  int char_pixels = get_char_pixels (self->layout);

  *minimum = self->min_chars * char_pixels;
  *natural = MAX (self->min_chars, self->nat_chars) * char_pixels;
}

static void
gtk_text_tile_measure_height (GtkTextTile *self,
                              int         *minimum,
                              int         *natural,
                              int         *minimum_baseline,
                              int         *natural_baseline)
{
  PangoContext *context;
  PangoFontMetrics *metrics;
  int ascent, descent;

  if (self->min_lines == 0 && self->nat_lines == 0)
    return;

  context = gtk_widget_get_pango_context (GTK_WIDGET (self));
  metrics = pango_context_get_metrics (context, NULL, NULL);

  ascent = pango_font_metrics_get_ascent (metrics);
  descent = pango_font_metrics_get_descent (metrics);

  *minimum = self->min_lines * (ascent + descent);
  *natural = MAX (self->min_lines, self->nat_lines) * (ascent + descent);

  if (minimum_baseline)
    *minimum_baseline = self->min_lines ? ascent : 0;
  if (natural_baseline)
    *natural_baseline = MAX (self->min_lines, self->nat_lines) ? ascent : 0;
}

static void
gtk_text_tile_measure (GtkWidget      *widget,
                       GtkOrientation  orientation,
                       int             for_size,
                       int            *minimum,
                       int            *natural,
                       int            *minimum_baseline,
                       int            *natural_baseline)
{
  GtkTextTile *self = GTK_TEXT_TILE (widget);

  /* We split this into 2 functions explicitly, so nobody gets the idea
   * of adding height-for-width to this.
   * This is meant to be fast, so that is a big no-no.
   */
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    gtk_text_tile_measure_width (self, minimum, natural);
  else
    gtk_text_tile_measure_height (self, minimum, natural, minimum_baseline, natural_baseline);

  *minimum = PANGO_PIXELS_CEIL (*minimum);
  *natural = PANGO_PIXELS_CEIL (*natural);
  if (*minimum_baseline > 0)
    *minimum_baseline = PANGO_PIXELS_CEIL (*minimum_baseline);
  if (*natural_baseline > 0)
    *natural_baseline = PANGO_PIXELS_CEIL (*natural_baseline);
}

static void
gtk_text_tile_snapshot (GtkWidget   *widget,
                        GtkSnapshot *snapshot)
{
  GtkTextTile *self = GTK_TEXT_TILE (widget);
  GtkStyleContext *context;

  if (!self->text || (*self->text == '\0'))
    return;

  context = _gtk_widget_get_style_context (widget);

  gtk_snapshot_render_layout (snapshot, context, 0, 0, self->layout);
}

static void
gtk_text_tile_class_init (GtkTextTileClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->dispose = gtk_text_tile_dispose;
  gobject_class->finalize = gtk_text_tile_finalize;
  gobject_class->get_property = gtk_text_tile_get_property;
  gobject_class->set_property = gtk_text_tile_set_property;

  widget_class->css_changed = gtk_text_tile_css_changed;
  widget_class->measure = gtk_text_tile_measure;
  widget_class->snapshot = gtk_text_tile_snapshot;

  /**
   * GtkTextTile:text: (attributes org.gtk.Property.get=gtk_text_tile_get_text org.gtk.Property.set=gtk_text_tile_set_text)
   *
   * The displayed text.
   *
   * Since: 4.8
   */
  properties[PROP_TEXT] =
    g_param_spec_string ("text", NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gtk_text_tile_init (GtkTextTile *self)
{
  self->min_chars = DEFAULT_MIN_CHARS;
  self->nat_chars = DEFAULT_NAT_CHARS;
  self->min_lines = DEFAULT_MIN_LINES;
  self->nat_lines = DEFAULT_NAT_LINES;

  self->layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), NULL);
}

/**
 * gtk_text_tile_new:
 * @text: (nullable): The text to display.
 *
 * Creates a new `GtkTextTile` with the given text.
 *
 * Returns: a new `GtkTextTile`
 *
 * Since: 4.8
 */
GtkWidget *
gtk_text_tile_new (const char *text)
{
  return g_object_new (GTK_TYPE_TEXT_TILE,
                       "text", text,
                       NULL);
}

/**
 * gtk_text_tile_set_text: (attributes org.gtk.Method.set_property=text)
 * @self: a `GtkTextTile`
 * @text: (nullable): The text to display
 *
 * Sets the text to be displayed.
 *
 * Since: 4.8
 */
void
gtk_text_tile_set_text (GtkTextTile *self,
                        const char  *text)
{
  g_return_if_fail (GTK_IS_TEXT_TILE (self));

  if (g_strcmp0 (self->text, text) == 0)
    return;

  g_free (self->text);
  self->text = g_strdup (text);

  pango_layout_set_text (self->layout, self->text, -1);

  /* This here not being a gtk_widget_queue_resize() is why this widget exists */
  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TEXT]);
}

/**
 * gtk_text_tile_get_text: (attributes org.gtk.Method.get_property=text)
 * @self: a `GtkTextTile`
 *
 * Gets the text that is displayed.
 *
 * Returns: (nullable) (transfer none): The displayed text
 *
 * Since: 4.8
 */
const char *
gtk_text_tile_get_text (GtkTextTile *self)
{
  g_return_val_if_fail (GTK_IS_TEXT_TILE (self), NULL);

  return self->text;
}

