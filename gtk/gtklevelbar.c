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
 * the bar will be considered in a different state. GTK will add a few
 * offsets by default on the level bar: #GTK_LEVEL_BAR_OFFSET_LOW,
 * #GTK_LEVEL_BAR_OFFSET_HIGH and #GTK_LEVEL_BAR_OFFSET_FULL, with
 * values 0.25, 0.75 and 1.0 respectively.
 *
 * Note that it is your responsibility to update preexisting offsets
 * when changing the minimum or maximum value. GTK+ will simply clamp
 * them to the new range.
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
 *   // This changes the value of the default low offset
 *
 *   gtk_level_bar_add_offset_value (bar,
 *                                   GTK_LEVEL_BAR_OFFSET_LOW,
 *                                   0.10);
 *
 *   // This adds a new offset to the bar; the application will
 *   // be able to change its color CSS like this:
 *   //
 *   // levelbar block.my-offset {
 *   //   background-color: magenta;
 *   //   border-style: solid;
 *   //   border-color: black;
 *   //   border-style: 1px;
 *   // }
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
 * as a finite number of separated blocks instead of a single one. The number
 * of blocks that will be rendered is equal to the number of units specified by
 * the admissible interval.
 *
 * For instance, to build a bar rendered with five blocks, it’s sufficient to
 * set the minimum value to 0 and the maximum value to 5 after changing the indicator
 * mode to discrete.
 *
 * GtkLevelBar was introduced in GTK+ 3.6.
 *
 * # GtkLevelBar as GtkBuildable
 *
 * The GtkLevelBar implementation of the GtkBuildable interface supports a
 * custom <offsets> element, which can contain any number of <offset> elements,
 * each of which must have name and value attributes.
 *
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * levelbar[.discrete]
 * ╰── trough
 *     ├── block.filled.level-name
 *     ┊
 *     ├── block.empty
 *     ┊
 * ]|
 *
 * GtkLevelBar has a main CSS node with name levelbar and one of the style
 * classes .discrete or .continuous and a subnode with name trough. Below the
 * trough node are a number of nodes with name block and style class .filled
 * or .empty. In continuous mode, there is exactly one node of each, in discrete
 * mode, the number of filled and unfilled nodes corresponds to blocks that are
 * drawn. The block.filled nodes also get a style class .level-name corresponding
 * to the level for the current value.
 *
 * In horizontal orientation, the nodes are always arranged from left to right,
 * regardless of text direction.
 */
#include "config.h"

#include "gtkbuildable.h"
#include "gtkbuilderprivate.h"
#include "gtkintl.h"
#include "gtkorientableprivate.h"
#include "gtklevelbar.h"
#include "gtkmarshalers.h"
#include "gtkstylecontext.h"
#include "gtktypebuiltins.h"
#include "gtkwidget.h"
#include "gtkwidgetprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkgizmoprivate.h"

#include <math.h>
#include <stdlib.h>

#include "a11y/gtklevelbaraccessible.h"

#include "fallback-c89.c"

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

struct _GtkLevelBar {
  GtkWidget parent_instance;
};

typedef struct _GtkLevelBarClass   GtkLevelBarClass;
struct _GtkLevelBarClass {
  GtkWidgetClass parent_class;

  void (* offset_changed) (GtkLevelBar *self,
                           const gchar *name);
};

typedef struct _GtkLevelBarPrivate GtkLevelBarPrivate;
struct _GtkLevelBarPrivate {
  GtkOrientation orientation;

  GtkLevelBarMode bar_mode;

  gdouble min_value;
  gdouble max_value;
  gdouble cur_value;

  GList *offsets;

  GtkWidget *trough_widget;
  GtkWidget **block_widget;
  guint n_blocks;

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
  GtkLevelBarPrivate *priv = gtk_level_bar_get_instance_private (self);
  GList *existing;
  GtkLevelBarOffset *offset = NULL;
  GtkLevelBarOffset *new_offset;

  existing = g_list_find_custom (priv->offsets, name, offset_find_func);
  if (existing)
    offset = existing->data;

  if (offset && (offset->value == value))
    return FALSE;

  new_offset = gtk_level_bar_offset_new (name, value);

  if (offset)
    {
      gtk_level_bar_offset_free (offset);
      priv->offsets = g_list_delete_link (priv->offsets, existing);
    }

  priv->offsets = g_list_insert_sorted (priv->offsets, new_offset, offset_sort_func);

  return TRUE;
}

static gboolean
gtk_level_bar_value_in_interval (GtkLevelBar *self,
                                 gdouble      value)
{
  GtkLevelBarPrivate *priv = gtk_level_bar_get_instance_private (self);

  return ((value >= priv->min_value) &&
          (value <= priv->max_value));
}

static gint
gtk_level_bar_get_num_blocks (GtkLevelBar *self)
{
  GtkLevelBarPrivate *priv = gtk_level_bar_get_instance_private (self);

  if (priv->bar_mode == GTK_LEVEL_BAR_MODE_CONTINUOUS)
    return 1;
  else if (priv->bar_mode == GTK_LEVEL_BAR_MODE_DISCRETE)
    return MAX (1, (gint) (round (priv->max_value) - round (priv->min_value)));

  return 0;
}

static gint
gtk_level_bar_get_num_block_nodes (GtkLevelBar *self)
{
  GtkLevelBarPrivate *priv = gtk_level_bar_get_instance_private (self);
  if (priv->bar_mode == GTK_LEVEL_BAR_MODE_CONTINUOUS)
    return 2;
  else
    return gtk_level_bar_get_num_blocks (self);
}

static void
gtk_level_bar_get_min_block_size (GtkLevelBar *self,
                                  gint        *block_width,
                                  gint        *block_height)
{
  GtkLevelBarPrivate *priv = gtk_level_bar_get_instance_private (self);
  guint i, n_blocks;
  gint width, height;

  *block_width = *block_height = 0;
  n_blocks = gtk_level_bar_get_num_block_nodes (self);

  for (i = 0; i < n_blocks; i++)
    {
      gtk_widget_measure (priv->block_widget[i],
                          GTK_ORIENTATION_HORIZONTAL,
                          -1,
                          &width, NULL,
                          NULL, NULL);
      gtk_widget_measure (priv->block_widget[i],
                          GTK_ORIENTATION_VERTICAL,
                          -1,
                          &height, NULL,
                          NULL, NULL);

      *block_width = MAX (width, *block_width);
      *block_height = MAX (height, *block_height);
    }
}

static gboolean
gtk_level_bar_get_real_inverted (GtkLevelBar *self)
{
  GtkLevelBarPrivate *priv = gtk_level_bar_get_instance_private (self);
  if (gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL &&
      priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    return !priv->inverted;

  return priv->inverted;
}

static void
gtk_level_bar_render_trough (GtkGizmo    *gizmo,
                             GtkSnapshot *snapshot)
{
  GtkWidget *widget = GTK_WIDGET (gizmo);
  GtkLevelBar *self = GTK_LEVEL_BAR (gtk_widget_get_parent (GTK_WIDGET (gizmo)));
  GtkLevelBarPrivate *priv = gtk_level_bar_get_instance_private (self);

  if (priv->bar_mode == GTK_LEVEL_BAR_MODE_CONTINUOUS)
    {
      gboolean inverted;

      inverted = gtk_level_bar_get_real_inverted (self);

      /* render the empty (unfilled) part */
      gtk_widget_snapshot_child (widget, priv->block_widget[inverted ? 0 : 1], snapshot);

      /* now render the filled part on top of it */
      if (priv->cur_value != 0)
        gtk_widget_snapshot_child (widget, priv->block_widget[inverted ? 1 : 0], snapshot);
    }
  else
    {
      gint num_blocks, i;

      num_blocks = gtk_level_bar_get_num_blocks (self);

      for (i = 0; i < num_blocks; i++)
        gtk_widget_snapshot_child (widget, priv->block_widget[i], snapshot);
    }
}

static void
gtk_level_bar_measure_trough (GtkGizmo       *gizmo,
                              GtkOrientation  orientation,
                              int             for_size,
                              int            *minimum,
                              int            *natural,
                              int            *minimum_baseline,
                              int            *natural_baseline)
{
  GtkWidget *widget = GTK_WIDGET (gizmo);
  GtkLevelBar *self = GTK_LEVEL_BAR (gtk_widget_get_parent (widget));
  GtkLevelBarPrivate *priv = gtk_level_bar_get_instance_private (self);
  gint num_blocks, size;
  gint block_width, block_height;

  num_blocks = gtk_level_bar_get_num_blocks (self);
  gtk_level_bar_get_min_block_size (self, &block_width, &block_height);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        size = num_blocks * block_width;
      else
        size = block_width;
    }
  else
    {
      if (priv->orientation == GTK_ORIENTATION_VERTICAL)
        size = num_blocks * block_height;
      else
        size = block_height;
    }

  *minimum = size;
  *natural = size;
}

static void
gtk_level_bar_measure (GtkWidget      *widget,
                       GtkOrientation  orientation,
                       int             for_size,
                       int            *minimum,
                       int            *natural,
                       int            *minimum_baseline,
                       int            *natural_baseline)
{
  GtkLevelBarPrivate *priv = gtk_level_bar_get_instance_private (GTK_LEVEL_BAR (widget));

  gtk_widget_measure (priv->trough_widget,
                      orientation,
                      for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);
}

static void
gtk_level_bar_allocate_trough_continuous (GtkLevelBar *self,
                                          int          width,
                                          int          height,
                                          int          baseline)
{
  GtkLevelBarPrivate *priv = gtk_level_bar_get_instance_private (self);
  GtkAllocation block_area;
  gdouble fill_percentage;
  gboolean inverted;
  int block_min;

  inverted = gtk_level_bar_get_real_inverted (self);

  /* allocate the empty (unfilled) part */
  gtk_widget_size_allocate (priv->block_widget[inverted ? 0 : 1],
                            &(GtkAllocation) {0, 0, width, height},
                            baseline);

  if (priv->cur_value == 0)
    return;

  /* now allocate the filled part */
  block_area = (GtkAllocation) {0, 0, width, height};
  fill_percentage = (priv->cur_value - priv->min_value) /
    (priv->max_value - priv->min_value);

  gtk_widget_measure (priv->block_widget[inverted ? 1 : 0],
                      priv->orientation, -1,
                      &block_min, NULL,
                      NULL, NULL);

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      block_area.width = (gint) floor (block_area.width * fill_percentage);
      block_area.width = MAX (block_area.width, block_min);

      if (inverted)
        block_area.x += width - block_area.width;
    }
  else
    {
      block_area.height = (gint) floor (block_area.height * fill_percentage);
      block_area.height = MAX (block_area.height, block_min);

      if (inverted)
        block_area.y += height - block_area.height;
    }

  gtk_widget_size_allocate (priv->block_widget[inverted ? 1 : 0],
                            &block_area,
                            baseline);
}

static void
gtk_level_bar_allocate_trough_discrete (GtkLevelBar *self,
                                        int          width,
                                        int          height,
                                        int          baseline)
{
  GtkLevelBarPrivate *priv = gtk_level_bar_get_instance_private (self);
  GtkAllocation block_area;
  gint num_blocks, i;
  gint block_width, block_height;

  gtk_level_bar_get_min_block_size (self, &block_width, &block_height);
  num_blocks = gtk_level_bar_get_num_blocks (self);

  if (num_blocks == 0)
    return;

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      block_width = MAX (block_width, (gint) floor (width / num_blocks));
      block_height = height;
    }
  else
    {
      block_width = width;
      block_height = MAX (block_height, (gint) floor (height / num_blocks));
    }

  block_area.x = 0;
  block_area.y = 0;
  block_area.width = block_width;
  block_area.height = block_height;

  for (i = 0; i < num_blocks; i++)
    {
      gtk_widget_size_allocate (priv->block_widget[i],
                                &block_area,
                                baseline);

      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        block_area.x += block_area.width;
      else
        block_area.y += block_area.height;
    }
}

static void
gtk_level_bar_allocate_trough (GtkGizmo *gizmo,
                               int       width,
                               int       height,
                               int       baseline)
{
  GtkWidget *widget = GTK_WIDGET (gizmo);
  GtkLevelBar *self = GTK_LEVEL_BAR (gtk_widget_get_parent (widget));
  GtkLevelBarPrivate *priv = gtk_level_bar_get_instance_private (self);

  if (priv->bar_mode == GTK_LEVEL_BAR_MODE_CONTINUOUS)
    gtk_level_bar_allocate_trough_continuous (self, width, height, baseline);
  else
    gtk_level_bar_allocate_trough_discrete (self, width, height, baseline);
}

static void
gtk_level_bar_size_allocate (GtkWidget *widget,
                             int        width,
                             int        height,
                             int        baseline)
{
  GtkLevelBarPrivate *priv = gtk_level_bar_get_instance_private (GTK_LEVEL_BAR (widget));

  gtk_widget_size_allocate (priv->trough_widget,
                            &(GtkAllocation) {
                              0, 0,
                              width, height
                            }, baseline);
}

static void
update_block_nodes (GtkLevelBar *self)
{
  GtkLevelBarPrivate *priv = gtk_level_bar_get_instance_private (self);
  guint n_blocks;
  guint i;

  n_blocks = gtk_level_bar_get_num_block_nodes (self);

  if (priv->n_blocks == n_blocks)
    return;
  else if (n_blocks < priv->n_blocks)
    {
      for (i = n_blocks; i < priv->n_blocks; i++)
        {
          gtk_widget_unparent (priv->block_widget[i]);
        }
      priv->block_widget = g_renew (GtkWidget*, priv->block_widget, n_blocks);
      priv->n_blocks = n_blocks;
    }
  else
    {
      priv->block_widget = g_renew (GtkWidget*, priv->block_widget, n_blocks);
      for (i = priv->n_blocks; i < n_blocks; i++)
        {
          priv->block_widget[i] = gtk_gizmo_new ("block", NULL, NULL, NULL, NULL);
          gtk_widget_insert_before (priv->block_widget[i], GTK_WIDGET (priv->trough_widget), NULL);
        }
      priv->n_blocks = n_blocks;
    }
}

static void
update_mode_style_classes (GtkLevelBar *self)
{
  GtkLevelBarPrivate *priv = gtk_level_bar_get_instance_private (self);
  GtkCssNode *widget_node;

  widget_node = gtk_widget_get_css_node (GTK_WIDGET (self));
  if (priv->bar_mode == GTK_LEVEL_BAR_MODE_CONTINUOUS)
    {
      gtk_css_node_remove_class (widget_node, g_quark_from_static_string ("discrete"));
      gtk_css_node_add_class (widget_node, g_quark_from_static_string ("continuous"));
    }
  else if (priv->bar_mode == GTK_LEVEL_BAR_MODE_DISCRETE)
    {
      gtk_css_node_add_class (widget_node, g_quark_from_static_string ("discrete"));
      gtk_css_node_remove_class (widget_node, g_quark_from_static_string ("continuous"));
    }
}

static void
update_level_style_classes (GtkLevelBar *self)
{
  GtkLevelBarPrivate *priv = gtk_level_bar_get_instance_private (self);
  gdouble value;
  const gchar *value_class = NULL;
  GtkLevelBarOffset *offset, *prev_offset;
  GList *l;
  gint num_filled, num_blocks, i;
  gboolean inverted;

  value = gtk_level_bar_get_value (self);

  for (l = priv->offsets; l != NULL; l = l->next)
    {
      offset = l->data;

      /* find the right offset for our style class */
      if (value <= offset->value)
        {
          if (l->prev == NULL)
            {
              value_class = offset->name;
            }
          else
            {
              prev_offset = l->prev->data;
              if (prev_offset->value < value)
                value_class = offset->name;
            }
        }

      if (value_class)
        break;
    }

  inverted = gtk_level_bar_get_real_inverted (self);
  num_blocks = gtk_level_bar_get_num_block_nodes (self);

  if (priv->bar_mode == GTK_LEVEL_BAR_MODE_CONTINUOUS)
    num_filled = 1;
  else
    num_filled = MIN (num_blocks, (gint) round (priv->cur_value) - (gint) round (priv->min_value));

  for (i = 0; i < num_filled; i++)
    {
      GtkStyleContext *context = gtk_widget_get_style_context (priv->block_widget[inverted ? num_blocks - 1 - i : i]);
      GList *classes = gtk_style_context_list_classes (context);

      for (l = classes; l; l = l->next)
        gtk_style_context_remove_class (context, l->data);

      g_list_free (classes);

      gtk_style_context_add_class (context, "filled");
      if (value_class)
        gtk_style_context_add_class (context, value_class);
    }

  for (; i < num_blocks; i++)
    {
      GtkStyleContext *context = gtk_widget_get_style_context (priv->block_widget[inverted ? num_blocks - 1 - i : i]);
      GList *classes = gtk_style_context_list_classes (context);

      for (l = classes; l; l = l->next)
        gtk_style_context_remove_class (context, l->data);

      g_list_free (classes);

      gtk_style_context_add_class (context, "empty");
    }
}

static void
gtk_level_bar_direction_changed (GtkWidget        *widget,
                                 GtkTextDirection  previous_dir)
{
  GtkLevelBar *self = GTK_LEVEL_BAR (widget);

  update_level_style_classes (self);

  GTK_WIDGET_CLASS (gtk_level_bar_parent_class)->direction_changed (widget, previous_dir);
}

static void
gtk_level_bar_ensure_offsets_in_range (GtkLevelBar *self)
{
  GtkLevelBarPrivate *priv = gtk_level_bar_get_instance_private (self);
  GtkLevelBarOffset *offset;
  GList *l = priv->offsets;

  while (l != NULL)
    {
      offset = l->data;
      l = l->next;

      if (offset->value < priv->min_value)
        gtk_level_bar_ensure_offset (self, offset->name, priv->min_value);
      else if (offset->value > priv->max_value)
        gtk_level_bar_ensure_offset (self, offset->name, priv->max_value);
    }
}

typedef struct {
  GtkLevelBar *self;
  GtkBuilder *builder;
  GList *offsets;
} OffsetsParserData;

static void
offset_start_element (GtkBuildableParseContext  *context,
                      const gchar          *element_name,
                      const gchar         **names,
                      const gchar         **values,
                      gpointer              user_data,
                      GError              **error)
{
  OffsetsParserData *data = user_data;

  if (strcmp (element_name, "offsets") == 0)
    {
      if (!_gtk_builder_check_parent (data->builder, context, "object", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_INVALID, NULL, NULL,
                                        G_MARKUP_COLLECT_INVALID))
        _gtk_builder_prefix_error (data->builder, context, error);
    }
  else if (strcmp (element_name, "offset") == 0)
    {
      const gchar *name;
      const gchar *value;
      GValue gvalue = G_VALUE_INIT;
      GtkLevelBarOffset *offset;

      if (!_gtk_builder_check_parent (data->builder, context, "offsets", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_STRING, "name", &name,
                                        G_MARKUP_COLLECT_STRING, "value", &value,
                                        G_MARKUP_COLLECT_INVALID))
        {
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      if (!gtk_builder_value_from_string_type (data->builder, G_TYPE_DOUBLE, value, &gvalue, error))
        {
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      offset = gtk_level_bar_offset_new (name, g_value_get_double (&gvalue));
      data->offsets = g_list_prepend (data->offsets, offset);
    }
  else
    {
      _gtk_builder_error_unhandled_tag (data->builder, context,
                                        "GtkLevelBar", element_name,
                                        error);
    }
}

static const GtkBuildableParser offset_parser =
{
  offset_start_element
};

static gboolean
gtk_level_bar_buildable_custom_tag_start (GtkBuildable  *buildable,
                                          GtkBuilder    *builder,
                                          GObject       *child,
                                          const gchar   *tagname,
                                          GtkBuildableParser *parser,
                                          gpointer      *parser_data)
{
  OffsetsParserData *data;

  if (child)
    return FALSE;

  if (strcmp (tagname, "offsets") != 0)
    return FALSE;

  data = g_slice_new0 (OffsetsParserData);
  data->self = GTK_LEVEL_BAR (buildable);
  data->builder = builder;
  data->offsets = NULL;

  *parser = offset_parser;
  *parser_data = data;

  return TRUE;
}

static void
gtk_level_bar_buildable_custom_finished (GtkBuildable *buildable,
                                         GtkBuilder   *builder,
                                         GObject      *child,
                                         const gchar  *tagname,
                                         gpointer      user_data)
{
  OffsetsParserData *data = user_data;
  GtkLevelBar *self;
  GtkLevelBarOffset *offset;
  GList *l;

  self = data->self;

  if (strcmp (tagname, "offsets") != 0)
    goto out;

  for (l = data->offsets; l != NULL; l = l->next)
    {
      offset = l->data;
      gtk_level_bar_add_offset_value (self, offset->name, offset->value);
    }

 out:
  g_list_free_full (data->offsets, (GDestroyNotify) gtk_level_bar_offset_free);
  g_slice_free (OffsetsParserData, data);
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
  GtkLevelBarPrivate *priv = gtk_level_bar_get_instance_private (self);
  if (priv->orientation != orientation)
    {
      priv->orientation = orientation;
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
  GtkLevelBarPrivate *priv = gtk_level_bar_get_instance_private (self);

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
      g_value_set_enum (value, priv->orientation);
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
  GtkLevelBarPrivate *priv = gtk_level_bar_get_instance_private (self);
  gint i;

  g_list_free_full (priv->offsets, (GDestroyNotify) gtk_level_bar_offset_free);

  for (i = 0; i < priv->n_blocks; i++)
    gtk_widget_unparent (priv->block_widget[i]);

  g_free (priv->block_widget);

  gtk_widget_unparent (priv->trough_widget);

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

  wclass->size_allocate = gtk_level_bar_size_allocate;
  wclass->measure = gtk_level_bar_measure;
  wclass->direction_changed = gtk_level_bar_direction_changed;

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
   */
  signals[SIGNAL_OFFSET_CHANGED] =
    g_signal_new (I_("offset-changed"),
                  GTK_TYPE_LEVEL_BAR,
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_DETAILED,
                  G_STRUCT_OFFSET (GtkLevelBarClass, offset_changed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  1, G_TYPE_STRING);

  /**
   * GtkLevelBar:value:
   *
   * The #GtkLevelBar:value property determines the currently
   * filled value of the level bar.
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
   */
  properties[PROP_INVERTED] =
    g_param_spec_boolean ("inverted",
                          P_("Inverted"),
                          P_("Invert the direction in which the level bar grows"),
                          FALSE,
                          G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (oclass, LAST_PROPERTY, properties);

  gtk_widget_class_set_accessible_type (wclass, GTK_TYPE_LEVEL_BAR_ACCESSIBLE);
  gtk_widget_class_set_css_name (wclass, I_("levelbar"));
}

static void
gtk_level_bar_init (GtkLevelBar *self)
{
  GtkLevelBarPrivate *priv = gtk_level_bar_get_instance_private (self);

  priv->cur_value = 0.0;
  priv->min_value = 0.0;
  priv->max_value = 1.0;

  /* set initial orientation and style classes */
  priv->orientation = GTK_ORIENTATION_HORIZONTAL;
  _gtk_orientable_set_style_classes (GTK_ORIENTABLE (self));

  priv->inverted = FALSE;

  priv->trough_widget = gtk_gizmo_new ("trough",
                                       gtk_level_bar_measure_trough,
                                       gtk_level_bar_allocate_trough,
                                       gtk_level_bar_render_trough,
                                       NULL);
  gtk_widget_set_parent (priv->trough_widget, GTK_WIDGET (self));

  gtk_level_bar_ensure_offset (self, GTK_LEVEL_BAR_OFFSET_LOW, 0.25);
  gtk_level_bar_ensure_offset (self, GTK_LEVEL_BAR_OFFSET_HIGH, 0.75);
  gtk_level_bar_ensure_offset (self, GTK_LEVEL_BAR_OFFSET_FULL, 1.0);

  priv->block_widget = NULL;
  priv->n_blocks = 0;

  priv->bar_mode = GTK_LEVEL_BAR_MODE_CONTINUOUS;
  update_mode_style_classes (self);
  update_block_nodes (self);
  update_level_style_classes (self);
}

/**
 * gtk_level_bar_new:
 *
 * Creates a new #GtkLevelBar.
 *
 * Returns: a #GtkLevelBar.
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
 */
gdouble
gtk_level_bar_get_min_value (GtkLevelBar *self)
{
  GtkLevelBarPrivate *priv = gtk_level_bar_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_LEVEL_BAR (self), 0.0);

  return priv->min_value;
}

/**
 * gtk_level_bar_get_max_value:
 * @self: a #GtkLevelBar
 *
 * Returns the value of the #GtkLevelBar:max-value property.
 *
 * Returns: a positive value
 */
gdouble
gtk_level_bar_get_max_value (GtkLevelBar *self)
{
  GtkLevelBarPrivate *priv = gtk_level_bar_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_LEVEL_BAR (self), 0.0);

  return priv->max_value;
}

/**
 * gtk_level_bar_get_value:
 * @self: a #GtkLevelBar
 *
 * Returns the value of the #GtkLevelBar:value property.
 *
 * Returns: a value in the interval between
 *     #GtkLevelBar:min-value and #GtkLevelBar:max-value
 */
gdouble
gtk_level_bar_get_value (GtkLevelBar *self)
{
  GtkLevelBarPrivate *priv = gtk_level_bar_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_LEVEL_BAR (self), 0.0);

  return priv->cur_value;
}

static void
gtk_level_bar_set_value_internal (GtkLevelBar *self,
                                  gdouble      value)
{
  GtkLevelBarPrivate *priv = gtk_level_bar_get_instance_private (self);

  priv->cur_value = value;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VALUE]);
  gtk_widget_queue_allocate (GTK_WIDGET (priv->trough_widget));
}

/**
 * gtk_level_bar_set_min_value:
 * @self: a #GtkLevelBar
 * @value: a positive value
 *
 * Sets the value of the #GtkLevelBar:min-value property.
 *
 * You probably want to update preexisting level offsets after calling
 * this function.
 */
void
gtk_level_bar_set_min_value (GtkLevelBar *self,
                             gdouble      value)
{
  GtkLevelBarPrivate *priv = gtk_level_bar_get_instance_private (self);

  g_return_if_fail (GTK_IS_LEVEL_BAR (self));
  g_return_if_fail (value >= 0.0);

  if (value == priv->min_value)
    return;

  priv->min_value = value;

  if (priv->min_value > priv->cur_value)
    gtk_level_bar_set_value_internal (self, priv->min_value);

  update_block_nodes (self);
  update_level_style_classes (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MIN_VALUE]);
}

/**
 * gtk_level_bar_set_max_value:
 * @self: a #GtkLevelBar
 * @value: a positive value
 *
 * Sets the value of the #GtkLevelBar:max-value property.
 *
 * You probably want to update preexisting level offsets after calling
 * this function.
 */
void
gtk_level_bar_set_max_value (GtkLevelBar *self,
                             gdouble      value)
{
  GtkLevelBarPrivate *priv = gtk_level_bar_get_instance_private (self);

  g_return_if_fail (GTK_IS_LEVEL_BAR (self));
  g_return_if_fail (value >= 0.0);

  if (value == priv->max_value)
    return;

  priv->max_value = value;

  if (priv->max_value < priv->cur_value)
    gtk_level_bar_set_value_internal (self, priv->max_value);

  gtk_level_bar_ensure_offsets_in_range (self);
  update_block_nodes (self);
  update_level_style_classes (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MAX_VALUE]);
}

/**
 * gtk_level_bar_set_value:
 * @self: a #GtkLevelBar
 * @value: a value in the interval between
 *     #GtkLevelBar:min-value and #GtkLevelBar:max-value
 *
 * Sets the value of the #GtkLevelBar:value property.
 */
void
gtk_level_bar_set_value (GtkLevelBar *self,
                         gdouble      value)
{
  GtkLevelBarPrivate *priv = gtk_level_bar_get_instance_private (self);

  g_return_if_fail (GTK_IS_LEVEL_BAR (self));

  if (value == priv->cur_value)
    return;

  gtk_level_bar_set_value_internal (self, value);
  update_level_style_classes (self);
}

/**
 * gtk_level_bar_get_mode:
 * @self: a #GtkLevelBar
 *
 * Returns the value of the #GtkLevelBar:mode property.
 *
 * Returns: a #GtkLevelBarMode
 */
GtkLevelBarMode
gtk_level_bar_get_mode (GtkLevelBar *self)
{
  GtkLevelBarPrivate *priv = gtk_level_bar_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_LEVEL_BAR (self), 0);

  return priv->bar_mode;
}

/**
 * gtk_level_bar_set_mode:
 * @self: a #GtkLevelBar
 * @mode: a #GtkLevelBarMode
 *
 * Sets the value of the #GtkLevelBar:mode property.
 */
void
gtk_level_bar_set_mode (GtkLevelBar     *self,
                        GtkLevelBarMode  mode)
{
  GtkLevelBarPrivate *priv = gtk_level_bar_get_instance_private (self);

  g_return_if_fail (GTK_IS_LEVEL_BAR (self));

  if (priv->bar_mode == mode)
    return;

  priv->bar_mode = mode;

  update_mode_style_classes (self);
  update_block_nodes (self);
  update_level_style_classes (self);
  gtk_widget_queue_resize (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODE]);

}

/**
 * gtk_level_bar_get_inverted:
 * @self: a #GtkLevelBar
 *
 * Return the value of the #GtkLevelBar:inverted property.
 *
 * Returns: %TRUE if the level bar is inverted
 */
gboolean
gtk_level_bar_get_inverted (GtkLevelBar *self)
{
  GtkLevelBarPrivate *priv = gtk_level_bar_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_LEVEL_BAR (self), FALSE);

  return priv->inverted;
}

/**
 * gtk_level_bar_set_inverted:
 * @self: a #GtkLevelBar
 * @inverted: %TRUE to invert the level bar
 *
 * Sets the value of the #GtkLevelBar:inverted property.
 */
void
gtk_level_bar_set_inverted (GtkLevelBar *self,
                            gboolean     inverted)
{
  GtkLevelBarPrivate *priv = gtk_level_bar_get_instance_private (self);

  g_return_if_fail (GTK_IS_LEVEL_BAR (self));

  if (priv->inverted == inverted)
    return;

  priv->inverted = inverted;
  gtk_widget_queue_resize (GTK_WIDGET (self));
  update_level_style_classes (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INVERTED]);
}

/**
 * gtk_level_bar_remove_offset_value:
 * @self: a #GtkLevelBar
 * @name: (allow-none): the name of an offset in the bar
 *
 * Removes an offset marker previously added with
 * gtk_level_bar_add_offset_value().
 */
void
gtk_level_bar_remove_offset_value (GtkLevelBar *self,
                                   const gchar *name)
{
  GtkLevelBarPrivate *priv = gtk_level_bar_get_instance_private (self);
  GList *existing;

  g_return_if_fail (GTK_IS_LEVEL_BAR (self));

  existing = g_list_find_custom (priv->offsets, name, offset_find_func);
  if (existing)
    {
      gtk_level_bar_offset_free (existing->data);
      priv->offsets = g_list_delete_link (priv->offsets, existing);

      update_level_style_classes (self);
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

  update_level_style_classes (self);
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
 */
gboolean
gtk_level_bar_get_offset_value (GtkLevelBar *self,
                                const gchar *name,
                                gdouble     *value)
{
  GtkLevelBarPrivate *priv = gtk_level_bar_get_instance_private (self);
  GList *existing;
  GtkLevelBarOffset *offset = NULL;

  g_return_val_if_fail (GTK_IS_LEVEL_BAR (self), FALSE);

  existing = g_list_find_custom (priv->offsets, name, offset_find_func);
  if (existing)
    offset = existing->data;

  if (!offset)
    return FALSE;

  if (value)
    *value = offset->value;

  return TRUE;
}
