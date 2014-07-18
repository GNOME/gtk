/* GTK - The GIMP Toolkit
 * Copyright © 2012 Red Hat, Inc.
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
 *
 * Author: Cosimo Cecchi <cosimoc@gnome.org>
 *
 */

/**
 * SECTION:gtklevelbar
 * @Title: GtkLevelBar
 * @Short_description: A bar that can used as a level indicator
 *
 * The #GtkLevelBar is a bar widget that can be used
 * as a level indicator. Typical use cases are displaying the strength
 * of a password, or showing the charge level of a battery.
 *
 * Use gtk_level_bar_set_value() to set the current value, and
 * gtk_level_bar_add_offset_value() to set the value offsets at which
 * the bar will be considered in a different state. GTK will add two offsets
 * by default on the level bar: #GTK_LEVEL_BAR_OFFSET_LOW and
 * #GTK_LEVEL_BAR_OFFSET_HIGH, with values 0.25 and 0.75 respectively.
 *
 * ## Adding a custom offset on the bar
 *
 * |[<!-- language="C" -->
 *
 * static GtkWidget *
 * create_level_bar (void)
 * {
 *   GtkWidget *widget;
 *   GtkLevelBar *bar;
 *
 *   widget = gtk_level_bar_new ();
 *   bar = GTK_LEVEL_BAR (widget);
 *
 *   /<!---->* This changes the value of the default low offset
 *   *<!---->/
 *
 *   gtk_level_bar_add_offset_value (bar,
 *                                   GTK_LEVEL_BAR_OFFSET_LOW,
 *                                   0.10);
 *
 *   /<!---->* This adds a new offset to the bar; the application will
 *    be able to change its color by using the following selector,
 *    either by adding it to its CSS file or using
 *    gtk_css_provider_load_from_data() and
 *    gtk_style_context_add_provider()
 *
 *    * .level-bar.fill-block.level-my-offset {
 *    *   background-color: green;
 *    *   border-style: solid;
 *    *   border-color: black;
 *    *   border-style: 1px;
 *    * }
 *    *<!---->/
 *
 *   gtk_level_bar_add_offset_value (bar, "my-offset", 0.60);
 *
 *   return widget;
 * }
 * ]|
 *
 * The default interval of values is between zero and one, but it’s possible to
 * modify the interval using gtk_level_bar_set_min_value() and
 * gtk_level_bar_set_max_value(). The value will be always drawn in proportion to
 * the admissible interval, i.e. a value of 15 with a specified interval between
 * 10 and 20 is equivalent to a value of 0.5 with an interval between 0 and 1.
 * When #GTK_LEVEL_BAR_MODE_DISCRETE is used, the bar level is rendered
 * as a finite and number of separated blocks instead of a single one. The number
 * of blocks that will be rendered is equal to the number of units specified by
 * the admissible interval.
 * For instance, to build a bar rendered with five blocks, it’s sufficient to
 * set the minimum value to 0 and the maximum value to 5 after changing the indicator
 * mode to discrete.
 *
 * Since: 3.6
 */
#include "config.h"

#include "gtkbuildable.h"
#include "gtkintl.h"
#include "gtkorientableprivate.h"
#include "gtklevelbar.h"
#include "gtkmarshalers.h"
#include "gtkstylecontext.h"
#include "gtktypebuiltins.h"
#include "gtkwidget.h"
#include "gtkwidgetprivate.h"

#include <math.h>
#include <stdlib.h>

#include "a11y/gtklevelbaraccessible.h"

#include "fallback-c89.c"

#define DEFAULT_BLOCK_SIZE 3

/* these don't make sense outside of GtkLevelBar, so we don't add
 * global defines */
#define STYLE_CLASS_INDICATOR_CONTINUOUS "indicator-continuous"
#define STYLE_CLASS_INDICATOR_DISCRETE   "indicator-discrete"
#define STYLE_CLASS_FILL_BLOCK           "fill-block"
#define STYLE_CLASS_EMPTY_FILL_BLOCK     "empty-fill-block"

enum {
  PROP_VALUE = 1,
  PROP_MIN_VALUE,
  PROP_MAX_VALUE,
  PROP_MODE,
  PROP_INVERTED,
  LAST_PROPERTY,
  PROP_ORIENTATION /* overridden */
};

enum {
  SIGNAL_OFFSET_CHANGED,
  NUM_SIGNALS
};

static GParamSpec *properties[LAST_PROPERTY] = { NULL, };
static guint signals[NUM_SIGNALS] = { 0, };

typedef struct {
  gchar *name;
  gdouble value;
} GtkLevelBarOffset;

struct _GtkLevelBarPrivate {
  GtkOrientation orientation;

  GtkLevelBarMode bar_mode;

  gdouble min_value;
  gdouble max_value;
  gdouble cur_value;

  GList *offsets;

  guint inverted : 1;
};

static void gtk_level_bar_set_value_internal (GtkLevelBar *self,
                                              gdouble      value);

static void gtk_level_bar_buildable_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkLevelBar, gtk_level_bar, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GtkLevelBar)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_level_bar_buildable_init))

static GtkLevelBarOffset *
gtk_level_bar_offset_new (const gchar *name,
                          gdouble      value)
{
  GtkLevelBarOffset *offset = g_slice_new0 (GtkLevelBarOffset);

  offset->name = g_strdup (name);
  offset->value = value;

  return offset;
}

static void
gtk_level_bar_offset_free (GtkLevelBarOffset *offset)
{
  g_free (offset->name);
  g_slice_free (GtkLevelBarOffset, offset);
}

static gint
offset_find_func (gconstpointer data,
                  gconstpointer user_data)
{
  const GtkLevelBarOffset *offset = data;
  const gchar *name = user_data;

  return g_strcmp0 (name, offset->name);
}

static gint
offset_sort_func (gconstpointer a,
                  gconstpointer b)
{
  const GtkLevelBarOffset *offset_a = a;
  const GtkLevelBarOffset *offset_b = b;

  return (offset_a->value > offset_b->value);
}

static gboolean
gtk_level_bar_ensure_offset (GtkLevelBar *self,
                             const gchar *name,
                             gdouble      value)
{
  GList *existing;
  GtkLevelBarOffset *offset = NULL;

  existing = g_list_find_custom (self->priv->offsets, name, offset_find_func);
  if (existing)
    offset = existing->data;

  if (offset && (offset->value == value))
    return FALSE;

  if (offset)
    {
      gtk_level_bar_offset_free (offset);
      self->priv->offsets = g_list_delete_link (self->priv->offsets, existing);
    }

  offset = gtk_level_bar_offset_new (name, value);
  self->priv->offsets = g_list_insert_sorted (self->priv->offsets, offset, offset_sort_func);

  return TRUE;
}

static gboolean
gtk_level_bar_value_in_interval (GtkLevelBar *self,
                                 gdouble      value)
{
  return ((value >= self->priv->min_value) &&
          (value <= self->priv->max_value));
}

static void
gtk_level_bar_get_min_block_size (GtkLevelBar *self,
                                  gint        *block_width,
                                  gint        *block_height)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GtkStyleContext *context = gtk_widget_get_style_context (widget);
  GtkStateFlags flags = gtk_widget_get_state_flags (widget);
  GtkBorder border, tmp, tmp2;
  gint min_width, min_height;

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, STYLE_CLASS_FILL_BLOCK);
  gtk_style_context_get_border (context, flags, &border);
  gtk_style_context_get_padding (context, flags, &tmp);
  gtk_style_context_get_margin (context, flags, &tmp2);
  gtk_style_context_restore (context);

  gtk_style_context_get_style (context,
                               "min-block-width", &min_width,
                               "min-block-height", &min_height,
                               NULL);

  border.top += tmp.top;
  border.right += tmp.right;
  border.bottom += tmp.bottom;
  border.left += tmp.left;

  border.top += tmp2.top;
  border.right += tmp2.right;
  border.bottom += tmp2.bottom;
  border.left += tmp2.left;

  if (block_width)
    *block_width = MAX (border.left + border.right, min_width);
  if (block_height)
    *block_height = MAX (border.top + border.bottom, min_height);
}

static gint
gtk_level_bar_get_num_blocks (GtkLevelBar *self)
{
  if (self->priv->bar_mode == GTK_LEVEL_BAR_MODE_CONTINUOUS)
    return 1;
  else if (self->priv->bar_mode == GTK_LEVEL_BAR_MODE_DISCRETE)
    return MAX (1, (gint) (round (self->priv->max_value) - round (self->priv->min_value)));

  return 0;
}

static void
gtk_level_bar_get_borders (GtkLevelBar *self,
                           GtkBorder   *borders_out)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GtkStyleContext *context = gtk_widget_get_style_context (widget);
  GtkStateFlags flags = gtk_widget_get_state_flags (widget);
  GtkBorder border, tmp;

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_TROUGH);
  gtk_style_context_get_border (context, flags, &border);
  gtk_style_context_get_padding (context, flags, &tmp);
  gtk_style_context_restore (context);

  border.top += tmp.top;
  border.right += tmp.right;
  border.bottom += tmp.bottom;
  border.left += tmp.left;

  if (borders_out)
    *borders_out = border;
}

static void
gtk_level_bar_draw_fill_continuous (GtkLevelBar           *self,
                                    cairo_t               *cr,
                                    gboolean               inverted,
                                    cairo_rectangle_int_t *fill_area)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GtkStyleContext *context = gtk_widget_get_style_context (widget);
  GtkStateFlags flags = gtk_widget_get_state_flags (widget);
  cairo_rectangle_int_t base_area, block_area;
  GtkBorder block_margin;
  gdouble fill_percentage;

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, STYLE_CLASS_FILL_BLOCK);
  gtk_style_context_get_margin (context, flags, &block_margin);

  /* render the empty (unfilled) part */
  base_area = *fill_area;
  base_area.x += block_margin.left;
  base_area.y += block_margin.top;
  base_area.width -= block_margin.left + block_margin.right;
  base_area.height -= block_margin.top + block_margin.bottom;

  gtk_style_context_add_class (context, STYLE_CLASS_EMPTY_FILL_BLOCK);

  gtk_render_background (context, cr, base_area.x, base_area.y,
                         base_area.width, base_area.height);
  gtk_render_frame (context, cr, base_area.x, base_area.y,
                    base_area.width, base_area.height);

  gtk_style_context_remove_class (context, STYLE_CLASS_EMPTY_FILL_BLOCK);

  /* now render the filled part on top of it */
  block_area = base_area;

  fill_percentage = (self->priv->cur_value - self->priv->min_value) /
    (self->priv->max_value - self->priv->min_value);

  if (self->priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      block_area.width = (gint) floor (block_area.width * fill_percentage);

      if (inverted)
        block_area.x += base_area.width - block_area.width;
    }
  else
    {
      block_area.height = (gint) floor (block_area.height * fill_percentage);

      if (inverted)
        block_area.y += base_area.height - block_area.height;
    }

  gtk_render_background (context, cr, block_area.x, block_area.y,
                         block_area.width, block_area.height);
  gtk_render_frame (context, cr, block_area.x, block_area.y,
                    block_area.width, block_area.height);

  gtk_style_context_restore (context);
}

static void
gtk_level_bar_draw_fill_discrete (GtkLevelBar           *self,
                                  cairo_t               *cr,
                                  gboolean               inverted,
                                  cairo_rectangle_int_t *fill_area)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GtkStyleContext *context = gtk_widget_get_style_context (widget);
  GtkStateFlags flags = gtk_widget_get_state_flags (widget);
  gint num_blocks, num_filled, idx;
  gint block_width, block_height;
  gint block_draw_width, block_draw_height;
  GtkBorder block_margin;
  cairo_rectangle_int_t block_area;

  gtk_level_bar_get_min_block_size (self, &block_width, &block_height);

  block_area = *fill_area;
  num_blocks = gtk_level_bar_get_num_blocks (self);
  num_filled = (gint) round (self->priv->cur_value) - (gint) round (self->priv->min_value);

  if (self->priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    block_width = MAX (block_width, (gint) floor (block_area.width / num_blocks));
  else
    block_height = MAX (block_height, (gint) floor (block_area.height / num_blocks));

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, STYLE_CLASS_FILL_BLOCK);
  gtk_style_context_get_margin (context, flags, &block_margin);

  block_draw_width = block_width - block_margin.left - block_margin.right;
  block_draw_height = block_height - block_margin.top - block_margin.bottom;

  if (self->priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      block_draw_height = MAX (block_draw_height, block_area.height - block_margin.top - block_margin.bottom);
      block_area.y += block_margin.top;

      if (inverted)
        block_area.x += block_area.width - block_draw_width;
    }
  else
    {
      block_draw_width = MAX (block_draw_width, block_area.width - block_margin.left - block_margin.right);
      block_area.x += block_margin.left;

      if (inverted)
        block_area.y += block_area.height - block_draw_height;
    }

  for (idx = 0; idx < num_blocks; idx++)
    {
      if (self->priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          if (inverted)
            block_area.x -= block_margin.right;
          else
            block_area.x += block_margin.left;
        }
      else
        {
          if (inverted)
            block_area.y -= block_margin.bottom;
          else
            block_area.y += block_margin.top;
        }

      if (idx > num_filled - 1)
        gtk_style_context_add_class (context, STYLE_CLASS_EMPTY_FILL_BLOCK);

      gtk_render_background (context, cr,
                             block_area.x, block_area.y,
                             block_draw_width, block_draw_height);
      gtk_render_frame (context, cr,
                        block_area.x, block_area.y,
                        block_draw_width, block_draw_height);

      if (self->priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          if (inverted)
            block_area.x -= block_draw_width + block_margin.left;
          else
            block_area.x += block_draw_width + block_margin.right;
        }
      else
        {
          if (inverted)
            block_area.y -= block_draw_height + block_margin.top;
          else
            block_area.y += block_draw_height + block_margin.bottom;
        }
    }

  gtk_style_context_restore (context);
}

static void
gtk_level_bar_draw_fill (GtkLevelBar *self,
                         cairo_t     *cr)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GtkBorder trough_borders;
  gboolean inverted;
  cairo_rectangle_int_t fill_area;

  gtk_level_bar_get_borders (self, &trough_borders);

  fill_area.x = trough_borders.left;
  fill_area.y = trough_borders.top;
  fill_area.width = gtk_widget_get_allocated_width (widget) -
    trough_borders.left - trough_borders.right;
  fill_area.height = gtk_widget_get_allocated_height (widget) -
    trough_borders.top - trough_borders.bottom;

  inverted = self->priv->inverted;
  if (gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL)
    {
      if (self->priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        inverted = !inverted;
    }

  if (self->priv->bar_mode == GTK_LEVEL_BAR_MODE_CONTINUOUS)
    gtk_level_bar_draw_fill_continuous (self, cr, inverted, &fill_area);
  else if (self->priv->bar_mode == GTK_LEVEL_BAR_MODE_DISCRETE)
    gtk_level_bar_draw_fill_discrete (self, cr, inverted, &fill_area);
}

static gboolean
gtk_level_bar_draw (GtkWidget *widget,
                    cairo_t   *cr)
{
  GtkLevelBar *self = GTK_LEVEL_BAR (widget);
  GtkStyleContext *context = gtk_widget_get_style_context (widget);
  gint width, height;

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_TROUGH);

  gtk_render_background (context, cr, 0, 0, width, height);
  gtk_render_frame (context, cr, 0, 0, width, height);

  gtk_style_context_restore (context);

  gtk_level_bar_draw_fill (self, cr);

  return FALSE;
}

static void
gtk_level_bar_get_preferred_width (GtkWidget *widget,
                                   gint      *minimum,
                                   gint      *natural)
{
  GtkLevelBar *self = GTK_LEVEL_BAR (widget);
  GtkBorder borders;
  gint num_blocks;
  gint width, block_width;

  num_blocks = gtk_level_bar_get_num_blocks (self);
  gtk_level_bar_get_min_block_size (self, &block_width, NULL);

  gtk_level_bar_get_borders (self, &borders);
  width = borders.left + borders.right;

  if (self->priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    width += num_blocks * block_width;
  else
    width += block_width;

  *minimum = width;
  *natural = width;
}

static void
gtk_level_bar_get_preferred_height (GtkWidget *widget,
                                    gint      *minimum,
                                    gint      *natural)
{
  GtkLevelBar *self = GTK_LEVEL_BAR (widget);
  GtkBorder borders;
  gint num_blocks;
  gint height, block_height;

  num_blocks = gtk_level_bar_get_num_blocks (self);
  gtk_level_bar_get_min_block_size (self, NULL, &block_height);

  gtk_level_bar_get_borders (self, &borders);
  height = borders.top + borders.bottom;

  if (self->priv->orientation == GTK_ORIENTATION_VERTICAL)
    height += num_blocks * block_height;
  else
    height += block_height;

  *minimum = height;
  *natural = height;
}

static void
gtk_level_bar_size_allocate (GtkWidget     *widget,
                             GtkAllocation *allocation)
{
  GTK_WIDGET_CLASS (gtk_level_bar_parent_class)->size_allocate (widget, allocation);

  _gtk_widget_set_simple_clip (widget);
}

static void
gtk_level_bar_update_mode_style_classes (GtkLevelBar *self)
{
  GtkStyleContext *context = gtk_widget_get_style_context (GTK_WIDGET (self));

  if (self->priv->bar_mode == GTK_LEVEL_BAR_MODE_CONTINUOUS)
    {
      gtk_style_context_remove_class (context, STYLE_CLASS_INDICATOR_DISCRETE);
      gtk_style_context_add_class (context, STYLE_CLASS_INDICATOR_CONTINUOUS);
    }
  else if (self->priv->bar_mode == GTK_LEVEL_BAR_MODE_DISCRETE)
    {
      gtk_style_context_remove_class (context, STYLE_CLASS_INDICATOR_CONTINUOUS);
      gtk_style_context_add_class (context, STYLE_CLASS_INDICATOR_DISCRETE);
    }
}

static void
gtk_level_bar_update_level_style_classes (GtkLevelBar *self)
{
  GtkStyleContext *context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gdouble value = gtk_level_bar_get_value (self);
  gchar *offset_style_class, *value_class = NULL;
  GtkLevelBarOffset *offset, *prev_offset;
  GList *l;

  for (l = self->priv->offsets; l != NULL; l = l->next)
    {
      offset = l->data;

      offset_style_class = g_strconcat ("level-", offset->name, NULL);
      gtk_style_context_remove_class (context, offset_style_class);

      /* find the right offset for our style class */
      if (((l->prev == NULL) && (value <= offset->value)) ||
          ((l->next == NULL) && (value >= offset->value)))
        {
          value_class = offset_style_class;
        }
      else if ((l->next != NULL) && (l->prev != NULL))
        {
          prev_offset = l->prev->data;
          if ((prev_offset->value <= value) && (value < offset->value))
            value_class = offset_style_class;
        }
      else
        {
          g_free (offset_style_class);
        }
    }

  if (value_class != NULL)
    {
      gtk_style_context_add_class (context, value_class);
      g_free (value_class);
    }
}

static void
gtk_level_bar_ensure_offsets_in_range (GtkLevelBar *self)
{
  GtkLevelBarOffset *offset;
  GList *l = self->priv->offsets;

  while (l != NULL)
    {
      offset = l->data;
      l = l->next;

      if (offset->value < self->priv->min_value)
        gtk_level_bar_ensure_offset (self, offset->name, self->priv->min_value);
      else if (offset->value > self->priv->max_value)
        gtk_level_bar_ensure_offset (self, offset->name, self->priv->max_value);
    }
}

typedef struct {
  GtkLevelBar *self;
  GList *offsets;
} OffsetsParserData;

static void
offset_start_element (GMarkupParseContext  *context,
                      const gchar          *element_name,
                      const gchar         **names,
                      const gchar         **values,
                      gpointer              user_data,
                      GError              **error)
{
  OffsetsParserData *parser_data = user_data;
  const gchar *name = NULL;
  const gchar *value_str = NULL;
  GtkLevelBarOffset *offset;
  gint line_number, char_number;
  gint idx;

  if (strcmp (element_name, "offsets") == 0)
    ;
  else if (strcmp (element_name, "offset") == 0)
    {
      for (idx = 0; names[idx] != NULL; idx++)
        {
          if (strcmp (names[idx], "name") == 0)
            {
              name = values[idx];
            }
          else if (strcmp (names[idx], "value") == 0)
            {
              value_str = values[idx];
            }
          else
            {
              g_markup_parse_context_get_position (context,
                                                   &line_number,
                                                   &char_number);
              g_set_error (error,
                           GTK_BUILDER_ERROR,
                           GTK_BUILDER_ERROR_INVALID_ATTRIBUTE,
                           "%s:%d:%d '%s' is not a valid attribute of <%s>",
                           "<input>",
                           line_number, char_number, names[idx], "offset");

              return;
            }
        }

      if (name && value_str)
        {
          offset = gtk_level_bar_offset_new (name, g_ascii_strtod (value_str, NULL));
          parser_data->offsets = g_list_prepend (parser_data->offsets, offset);
        }
    }
  else
    {
      g_markup_parse_context_get_position (context,
                                           &line_number,
                                           &char_number);
      g_set_error (error,
                   GTK_BUILDER_ERROR,
                   GTK_BUILDER_ERROR_UNHANDLED_TAG,
                   "%s:%d:%d unsupported tag for GtkLevelBar: \"%s\"",
                   "<input>",
                   line_number, char_number, element_name);
    }
}

static const GMarkupParser offset_parser =
{
  offset_start_element
};

static gboolean
gtk_level_bar_buildable_custom_tag_start (GtkBuildable  *buildable,
                                          GtkBuilder    *builder,
                                          GObject       *child,
                                          const gchar   *tagname,
                                          GMarkupParser *parser,
                                          gpointer      *data)
{
  OffsetsParserData *parser_data;

  if (child)
    return FALSE;

  if (strcmp (tagname, "offsets") != 0)
    return FALSE;

  parser_data = g_slice_new0 (OffsetsParserData);
  parser_data->self = GTK_LEVEL_BAR (buildable);
  parser_data->offsets = NULL;

  *parser = offset_parser;
  *data = parser_data;

  return TRUE;
}

static void
gtk_level_bar_buildable_custom_finished (GtkBuildable *buildable,
                                         GtkBuilder   *builder,
                                         GObject      *child,
                                         const gchar  *tagname,
                                         gpointer      user_data)
{
  OffsetsParserData *parser_data;
  GtkLevelBar *self;
  GtkLevelBarOffset *offset;
  GList *l;

  parser_data = user_data;
  self = parser_data->self;

  if (strcmp (tagname, "offsets") != 0)
    goto out;

  for (l = parser_data->offsets; l != NULL; l = l->next)
    {
      offset = l->data;
      gtk_level_bar_add_offset_value (self, offset->name, offset->value);
    }

 out:
  g_list_free_full (parser_data->offsets, (GDestroyNotify) gtk_level_bar_offset_free);
  g_slice_free (OffsetsParserData, parser_data);
}

static void
gtk_level_bar_buildable_init (GtkBuildableIface *iface)
{
  iface->custom_tag_start = gtk_level_bar_buildable_custom_tag_start;
  iface->custom_finished = gtk_level_bar_buildable_custom_finished;
}

static void
gtk_level_bar_set_orientation (GtkLevelBar    *self,
                               GtkOrientation  orientation)
{
  if (self->priv->orientation != orientation)
    {
      self->priv->orientation = orientation;
      _gtk_orientable_set_style_classes (GTK_ORIENTABLE (self));
      gtk_widget_queue_resize (GTK_WIDGET (self));
      g_object_notify (G_OBJECT (self), "orientation");
    }
}

static void
gtk_level_bar_get_property (GObject    *obj,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkLevelBar *self = GTK_LEVEL_BAR (obj);

  switch (property_id)
    {
    case PROP_VALUE:
      g_value_set_double (value, gtk_level_bar_get_value (self));
      break;
    case PROP_MIN_VALUE:
      g_value_set_double (value, gtk_level_bar_get_min_value (self));
      break;
    case PROP_MAX_VALUE:
      g_value_set_double (value, gtk_level_bar_get_max_value (self));
      break;
    case PROP_MODE:
      g_value_set_enum (value, gtk_level_bar_get_mode (self));
      break;
    case PROP_INVERTED:
      g_value_set_boolean (value, gtk_level_bar_get_inverted (self));
      break;
    case PROP_ORIENTATION:
      g_value_set_enum (value, self->priv->orientation);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
      break;
    }
}

static void
gtk_level_bar_set_property (GObject      *obj,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkLevelBar *self = GTK_LEVEL_BAR (obj);

  switch (property_id)
    {
    case PROP_VALUE:
      gtk_level_bar_set_value (self, g_value_get_double (value));
      break;
    case PROP_MIN_VALUE:
      gtk_level_bar_set_min_value (self, g_value_get_double (value));
      break;
    case PROP_MAX_VALUE:
      gtk_level_bar_set_max_value (self, g_value_get_double (value));
      break;
    case PROP_MODE:
      gtk_level_bar_set_mode (self, g_value_get_enum (value));
      break;
    case PROP_INVERTED:
      gtk_level_bar_set_inverted (self, g_value_get_boolean (value));
      break;
    case PROP_ORIENTATION:
      gtk_level_bar_set_orientation (self, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
      break;
    }
}

static void
gtk_level_bar_finalize (GObject *obj)
{
  GtkLevelBar *self = GTK_LEVEL_BAR (obj);

  g_list_free_full (self->priv->offsets, (GDestroyNotify) gtk_level_bar_offset_free);

  G_OBJECT_CLASS (gtk_level_bar_parent_class)->finalize (obj);
}

static void
gtk_level_bar_class_init (GtkLevelBarClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GtkWidgetClass *wclass = GTK_WIDGET_CLASS (klass);

  oclass->get_property = gtk_level_bar_get_property;
  oclass->set_property = gtk_level_bar_set_property;
  oclass->finalize = gtk_level_bar_finalize;

  wclass->draw = gtk_level_bar_draw;
  wclass->size_allocate = gtk_level_bar_size_allocate;
  wclass->get_preferred_width = gtk_level_bar_get_preferred_width;
  wclass->get_preferred_height = gtk_level_bar_get_preferred_height;

  g_object_class_override_property (oclass, PROP_ORIENTATION, "orientation");

  /**
   * GtkLevelBar::offset-changed:
   * @self: a #GtkLevelBar
   * @name: the name of the offset that changed value
   *
   * Emitted when an offset specified on the bar changes value as an
   * effect to gtk_level_bar_add_offset_value() being called.
   *
   * The signal supports detailed connections; you can connect to the
   * detailed signal "changed::x" in order to only receive callbacks when
   * the value of offset "x" changes.
   *
   * Since: 3.6
   */
  signals[SIGNAL_OFFSET_CHANGED] =
    g_signal_new ("offset-changed",
                  GTK_TYPE_LEVEL_BAR,
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_DETAILED,
                  G_STRUCT_OFFSET (GtkLevelBarClass, offset_changed),
                  NULL, NULL,
                  _gtk_marshal_VOID__STRING,
                  G_TYPE_NONE,
                  1, G_TYPE_STRING);

  /**
   * GtkLevelBar:value:
   *
   * The #GtkLevelBar:value property determines the currently
   * filled value of the level bar.
   *
   * Since: 3.6
   */
  properties[PROP_VALUE] =
    g_param_spec_double ("value",
                         P_("Currently filled value level"),
                         P_("Currently filled value level of the level bar"),
                         0.0, G_MAXDOUBLE, 0.0,
                         G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkLevelBar:min-value:
   *
   * The #GtkLevelBar:min-value property determines the minimum value of
   * the interval that can be displayed by the bar.
   *
   * Since: 3.6
   */
  properties[PROP_MIN_VALUE] =
    g_param_spec_double ("min-value",
                         P_("Minimum value level for the bar"),
                         P_("Minimum value level that can be displayed by the bar"),
                         0.0, G_MAXDOUBLE, 0.0,
                         G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkLevelBar:max-value:
   *
   * The #GtkLevelBar:max-value property determaxes the maximum value of
   * the interval that can be displayed by the bar.
   *
   * Since: 3.6
   */
  properties[PROP_MAX_VALUE] =
    g_param_spec_double ("max-value",
                         P_("Maximum value level for the bar"),
                         P_("Maximum value level that can be displayed by the bar"),
                         0.0, G_MAXDOUBLE, 1.0,
                         G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkLevelBar:mode:
   *
   * The #GtkLevelBar:mode property determines the way #GtkLevelBar
   * interprets the value properties to draw the level fill area.
   * Specifically, when the value is #GTK_LEVEL_BAR_MODE_CONTINUOUS,
   * #GtkLevelBar will draw a single block representing the current value in
   * that area; when the value is #GTK_LEVEL_BAR_MODE_DISCRETE,
   * the widget will draw a succession of separate blocks filling the
   * draw area, with the number of blocks being equal to the units separating
   * the integral roundings of #GtkLevelBar:min-value and #GtkLevelBar:max-value.
   *
   * Since: 3.6
   */
  properties[PROP_MODE] =
    g_param_spec_enum ("mode",
                       P_("The mode of the value indicator"),
                       P_("The mode of the value indicator displayed by the bar"),
                       GTK_TYPE_LEVEL_BAR_MODE,
                       GTK_LEVEL_BAR_MODE_CONTINUOUS,
                       G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkLevelBar:inverted:
   *
   * Level bars normally grow from top to bottom or left to right.
   * Inverted level bars grow in the opposite direction.
   *
   * Since: 3.8
   */
  properties[PROP_INVERTED] =
    g_param_spec_boolean ("inverted",
                          P_("Inverted"),
                          P_("Invert the direction in which the level bar grows"),
                          FALSE,
                          G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkLevelBar:min-block-height:
   *
   * The min-block-height style property determines the minimum
   * height for blocks filling the #GtkLevelBar widget.
   *
   * Since: 3.6
   */
  gtk_widget_class_install_style_property
    (wclass, g_param_spec_int ("min-block-height",
                               P_("Minimum height for filling blocks"),
                               P_("Minimum height for blocks that fill the bar"),
                               1, G_MAXINT, DEFAULT_BLOCK_SIZE,
                               G_PARAM_READWRITE));
  /**
   * GtkLevelBar:min-block-width:
   *
   * The min-block-width style property determines the minimum
   * width for blocks filling the #GtkLevelBar widget.
   *
   * Since: 3.6
   */
  gtk_widget_class_install_style_property
    (wclass, g_param_spec_int ("min-block-width",
                               P_("Minimum width for filling blocks"),
                               P_("Minimum width for blocks that fill the bar"),
                               1, G_MAXINT, DEFAULT_BLOCK_SIZE,
                               G_PARAM_READWRITE));

  g_object_class_install_properties (oclass, LAST_PROPERTY, properties);

  gtk_widget_class_set_accessible_type (wclass, GTK_TYPE_LEVEL_BAR_ACCESSIBLE);
}

static void
gtk_level_bar_init (GtkLevelBar *self)
{
  GtkStyleContext *context;

  self->priv = gtk_level_bar_get_instance_private (self);

  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_LEVEL_BAR);

  self->priv->cur_value = 0.0;
  self->priv->min_value = 0.0;
  self->priv->max_value = 1.0;

  gtk_level_bar_ensure_offset (self, GTK_LEVEL_BAR_OFFSET_LOW, 0.25);
  gtk_level_bar_ensure_offset (self, GTK_LEVEL_BAR_OFFSET_HIGH, 0.75);
  gtk_level_bar_update_level_style_classes (self);

  self->priv->bar_mode = GTK_LEVEL_BAR_MODE_CONTINUOUS;
  gtk_level_bar_update_mode_style_classes (self);

  /* set initial orientation and style classes */
  self->priv->orientation = GTK_ORIENTATION_HORIZONTAL;
  _gtk_orientable_set_style_classes (GTK_ORIENTABLE (self));

  self->priv->inverted = FALSE;

  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
}

/**
 * gtk_level_bar_new:
 *
 * Creates a new #GtkLevelBar.
 *
 * Returns: a #GtkLevelBar.
 *
 * Since: 3.6
 */
GtkWidget *
gtk_level_bar_new (void)
{
  return g_object_new (GTK_TYPE_LEVEL_BAR, NULL);
}

/**
 * gtk_level_bar_new_for_interval:
 * @min_value: a positive value
 * @max_value: a positive value
 *
 * Utility constructor that creates a new #GtkLevelBar for the specified
 * interval.
 *
 * Returns: a #GtkLevelBar
 *
 * Since: 3.6
 */
GtkWidget *
gtk_level_bar_new_for_interval (gdouble min_value,
                                gdouble max_value)
{
  return g_object_new (GTK_TYPE_LEVEL_BAR,
                       "min-value", min_value,
                       "max-value", max_value,
                       NULL);
}

/**
 * gtk_level_bar_get_min_value:
 * @self: a #GtkLevelBar
 *
 * Returns the value of the #GtkLevelBar:min-value property.
 *
 * Returns: a positive value
 *
 * Since: 3.6
 */
gdouble
gtk_level_bar_get_min_value (GtkLevelBar *self)
{
  g_return_val_if_fail (GTK_IS_LEVEL_BAR (self), 0.0);

  return self->priv->min_value;
}

/**
 * gtk_level_bar_get_max_value:
 * @self: a #GtkLevelBar
 *
 * Returns the value of the #GtkLevelBar:max-value property.
 *
 * Returns: a positive value
 *
 * Since: 3.6
 */
gdouble
gtk_level_bar_get_max_value (GtkLevelBar *self)
{
  g_return_val_if_fail (GTK_IS_LEVEL_BAR (self), 0.0);

  return self->priv->max_value;
}

/**
 * gtk_level_bar_get_value:
 * @self: a #GtkLevelBar
 *
 * Returns the value of the #GtkLevelBar:value property.
 *
 * Returns: a value in the interval between
 *     #GtkLevelBar:min-value and #GtkLevelBar:max-value
 *
 * Since: 3.6
 */
gdouble
gtk_level_bar_get_value (GtkLevelBar *self)
{
  g_return_val_if_fail (GTK_IS_LEVEL_BAR (self), 0.0);

  return self->priv->cur_value;
}

static void
gtk_level_bar_set_value_internal (GtkLevelBar *self,
                                  gdouble      value)
{
  self->priv->cur_value = value;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VALUE]);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

/**
 * gtk_level_bar_set_min_value:
 * @self: a #GtkLevelBar
 * @value: a positive value
 *
 * Sets the value of the #GtkLevelBar:min-value property.
 *
 * Since: 3.6
 */
void
gtk_level_bar_set_min_value (GtkLevelBar *self,
                             gdouble      value)
{
  g_return_if_fail (GTK_IS_LEVEL_BAR (self));
  g_return_if_fail (value >= 0.0);

  if (value != self->priv->min_value)
    {
      self->priv->min_value = value;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MIN_VALUE]);

      if (self->priv->min_value > self->priv->cur_value)
        gtk_level_bar_set_value_internal (self, self->priv->min_value);

      gtk_level_bar_update_level_style_classes (self);
    }
}

/**
 * gtk_level_bar_set_max_value:
 * @self: a #GtkLevelBar
 * @value: a positive value
 *
 * Sets the value of the #GtkLevelBar:max-value property.
 *
 * Since: 3.6
 */
void
gtk_level_bar_set_max_value (GtkLevelBar *self,
                             gdouble      value)
{
  g_return_if_fail (GTK_IS_LEVEL_BAR (self));
  g_return_if_fail (value >= 0.0);

  if (value != self->priv->max_value)
    {
      self->priv->max_value = value;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MAX_VALUE]);

      if (self->priv->max_value < self->priv->cur_value)
        gtk_level_bar_set_value_internal (self, self->priv->max_value);

      gtk_level_bar_ensure_offsets_in_range (self);
      gtk_level_bar_update_level_style_classes (self);
    }
}

/**
 * gtk_level_bar_set_value:
 * @self: a #GtkLevelBar
 * @value: a value in the interval between
 *     #GtkLevelBar:min-value and #GtkLevelBar:max-value
 *
 * Sets the value of the #GtkLevelBar:value property.
 *
 * Since: 3.6
 */
void
gtk_level_bar_set_value (GtkLevelBar *self,
                         gdouble      value)
{
  g_return_if_fail (GTK_IS_LEVEL_BAR (self));

  if (value != self->priv->cur_value)
    {
      gtk_level_bar_set_value_internal (self, value);
      gtk_level_bar_update_level_style_classes (self);
    }
}

/**
 * gtk_level_bar_get_mode:
 * @self: a #GtkLevelBar
 *
 * Returns the value of the #GtkLevelBar:mode property.
 *
 * Returns: a #GtkLevelBarMode
 *
 * Since: 3.6
 */
GtkLevelBarMode
gtk_level_bar_get_mode (GtkLevelBar *self)
{
  g_return_val_if_fail (GTK_IS_LEVEL_BAR (self), 0);

  return self->priv->bar_mode;
}

/**
 * gtk_level_bar_set_mode:
 * @self: a #GtkLevelBar
 * @mode: a #GtkLevelBarMode
 *
 * Sets the value of the #GtkLevelBar:mode property.
 *
 * Since: 3.6
 */
void
gtk_level_bar_set_mode (GtkLevelBar     *self,
                        GtkLevelBarMode  mode)
{
  g_return_if_fail (GTK_IS_LEVEL_BAR (self));

  if (self->priv->bar_mode != mode)
    {
      self->priv->bar_mode = mode;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODE]);

      gtk_level_bar_update_mode_style_classes (self);
      gtk_widget_queue_resize (GTK_WIDGET (self));
    }
}

/**
 * gtk_level_bar_get_inverted:
 * @self: a #GtkLevelBar
 *
 * Return the value of the #GtkLevelBar:inverted property.
 *
 * Returns: %TRUE if the level bar is inverted
 *
 * Since: 3.8
 */
gboolean
gtk_level_bar_get_inverted (GtkLevelBar *self)
{
  g_return_val_if_fail (GTK_IS_LEVEL_BAR (self), FALSE);

  return self->priv->inverted;
}

/**
 * gtk_level_bar_set_inverted:
 * @self: a #GtkLevelBar
 * @inverted: %TRUE to invert the level bar
 *
 * Sets the value of the #GtkLevelBar:inverted property.
 *
 * Since: 3.8
 */
void
gtk_level_bar_set_inverted (GtkLevelBar *self,
                            gboolean     inverted)
{
  g_return_if_fail (GTK_IS_LEVEL_BAR (self));

  if (self->priv->inverted != inverted)
    {
      self->priv->inverted = inverted;
      gtk_widget_queue_resize (GTK_WIDGET (self));
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INVERTED]);
    }
}

/**
 * gtk_level_bar_remove_offset_value:
 * @self: a #GtkLevelBar
 * @name: (allow-none): the name of an offset in the bar
 *
 * Removes an offset marker previously added with
 * gtk_level_bar_add_offset_value().
 *
 * Since: 3.6
 */
void
gtk_level_bar_remove_offset_value (GtkLevelBar *self,
                                   const gchar *name)
{
  GList *existing;

  g_return_if_fail (GTK_IS_LEVEL_BAR (self));

  existing = g_list_find_custom (self->priv->offsets, name, offset_find_func);
  if (existing)
    {
      gtk_level_bar_offset_free (existing->data);
      self->priv->offsets = g_list_delete_link (self->priv->offsets, existing);

      gtk_level_bar_update_level_style_classes (self);
    }
}

/**
 * gtk_level_bar_add_offset_value:
 * @self: a #GtkLevelBar
 * @name: the name of the new offset
 * @value: the value for the new offset
 *
 * Adds a new offset marker on @self at the position specified by @value.
 * When the bar value is in the interval topped by @value (or between @value
 * and #GtkLevelBar:max-value in case the offset is the last one on the bar)
 * a style class named `level-`@name will be applied
 * when rendering the level bar fill.
 * If another offset marker named @name exists, its value will be
 * replaced by @value.
 *
 * Since: 3.6
 */
void
gtk_level_bar_add_offset_value (GtkLevelBar *self,
                                const gchar *name,
                                gdouble      value)
{
  GQuark name_quark;

  g_return_if_fail (GTK_IS_LEVEL_BAR (self));
  g_return_if_fail (gtk_level_bar_value_in_interval (self, value));

  if (!gtk_level_bar_ensure_offset (self, name, value))
    return;

  gtk_level_bar_update_level_style_classes (self);
  name_quark = g_quark_from_string (name);
  g_signal_emit (self, signals[SIGNAL_OFFSET_CHANGED], name_quark, name);
}

/**
 * gtk_level_bar_get_offset_value:
 * @self: a #GtkLevelBar
 * @name: (allow-none): the name of an offset in the bar
 * @value: (out): location where to store the value
 *
 * Fetches the value specified for the offset marker @name in @self,
 * returning %TRUE in case an offset named @name was found.
 *
 * Returns: %TRUE if the specified offset is found
 *
 * Since: 3.6
 */
gboolean
gtk_level_bar_get_offset_value (GtkLevelBar *self,
                                const gchar *name,
                                gdouble     *value)
{
  GList *existing;
  GtkLevelBarOffset *offset = NULL;

  g_return_val_if_fail (GTK_IS_LEVEL_BAR (self), 0.0);

  existing = g_list_find_custom (self->priv->offsets, name, offset_find_func);
  if (existing)
    offset = existing->data;

  if (!offset)
    return FALSE;

  if (value)
    *value = offset->value;

  return TRUE;
}
