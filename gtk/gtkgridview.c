/*
 * Copyright © 2018 Benjamin Otte
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

#include "gtkgridview.h"

#include "gtkadjustment.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkscrollable.h"

#define DEFAULT_MAX_COLUMNS (7)

/**
 * SECTION:gtkgridview
 * @title: GtkGridView
 * @short_description: A widget for displaying lists
 * @see_also: #GListModel
 *
 * GtkGridView is a widget to present a view into a large dynamic list of items.
 */

struct _GtkGridView
{
  GtkWidget parent_instance;

  GListModel *model;
  GtkAdjustment *adjustment[2];
  GtkScrollablePolicy scroll_policy[2];
  guint min_columns;
  guint max_columns;
};

enum
{
  PROP_0,
  PROP_HADJUSTMENT,
  PROP_HSCROLL_POLICY,
  PROP_MAX_COLUMNS,
  PROP_MIN_COLUMNS,
  PROP_MODEL,
  PROP_VADJUSTMENT,
  PROP_VSCROLL_POLICY,

  N_PROPS
};

G_DEFINE_TYPE_WITH_CODE (GtkGridView, gtk_grid_view, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL))

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
gtk_grid_view_adjustment_value_changed_cb (GtkAdjustment *adjustment,
                                           GtkGridView   *self)
{
  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

static void
gtk_grid_view_update_adjustments (GtkGridView    *self,
                                  GtkOrientation  orientation)
{
  g_signal_handlers_block_by_func (self->adjustment[orientation],
                                   gtk_grid_view_adjustment_value_changed_cb,
                                   self);
  gtk_adjustment_configure (self->adjustment[orientation],
                            0,
                            0,
                            0,
                            0,
                            0,
                            0);
  g_signal_handlers_unblock_by_func (self->adjustment[orientation],
                                     gtk_grid_view_adjustment_value_changed_cb,
                                     self);
}

static gboolean
gtk_grid_view_is_empty (GtkGridView *self)
{
  return self->model == NULL;
}

static void
gtk_grid_view_measure (GtkWidget      *widget,
                       GtkOrientation  orientation,
                       int             for_size,
                       int            *minimum,
                       int            *natural,
                       int            *minimum_baseline,
                       int            *natural_baseline)
{
  GtkGridView *self = GTK_GRID_VIEW (widget);

  if (gtk_grid_view_is_empty (self))
    {
      *minimum = 0;
      *natural = 0;
      return;
    }

  *minimum = 0;
  *natural = 0;
  return;
}

static void
gtk_grid_view_size_allocate (GtkWidget *widget,
                             int        width,
                             int        height,
                             int        baseline)
{
  //GtkGridView *self = GTK_GRID_VIEW (widget);
}

static void
gtk_grid_view_model_items_changed_cb (GListModel  *model,
                                      guint        position,
                                      guint        removed,
                                      guint        added,
                                      GtkGridView *self)
{
}

static void
gtk_grid_view_clear_model (GtkGridView *self)
{
  if (self->model == NULL)
    return;

  g_signal_handlers_disconnect_by_func (self->model, gtk_grid_view_model_items_changed_cb, self);
  g_clear_object (&self->model);
}

static void
gtk_grid_view_clear_adjustment (GtkGridView    *self,
                                GtkOrientation  orientation)
{
  if (self->adjustment[orientation] == NULL)
    return;

  g_signal_handlers_disconnect_by_func (self->adjustment[orientation],
                                        gtk_grid_view_adjustment_value_changed_cb,
                                        self);
  g_clear_object (&self->adjustment[orientation]);
}

static void
gtk_grid_view_dispose (GObject *object)
{
  GtkGridView *self = GTK_GRID_VIEW (object);

  gtk_grid_view_clear_model (self);

  gtk_grid_view_clear_adjustment (self, GTK_ORIENTATION_HORIZONTAL);
  gtk_grid_view_clear_adjustment (self, GTK_ORIENTATION_VERTICAL);

  G_OBJECT_CLASS (gtk_grid_view_parent_class)->dispose (object);
}

static void
gtk_grid_view_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkGridView *self = GTK_GRID_VIEW (object);

  switch (property_id)
    {
    case PROP_HADJUSTMENT:
      g_value_set_object (value, self->adjustment[GTK_ORIENTATION_HORIZONTAL]);
      break;

    case PROP_HSCROLL_POLICY:
      g_value_set_enum (value, self->scroll_policy[GTK_ORIENTATION_HORIZONTAL]);
      break;

    case PROP_MAX_COLUMNS:
      g_value_set_uint (value, self->max_columns);
      break;

    case PROP_MIN_COLUMNS:
      g_value_set_uint (value, self->min_columns);
      break;

    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;

    case PROP_VADJUSTMENT:
      g_value_set_object (value, self->adjustment[GTK_ORIENTATION_VERTICAL]);
      break;

    case PROP_VSCROLL_POLICY:
      g_value_set_enum (value, self->scroll_policy[GTK_ORIENTATION_VERTICAL]);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_grid_view_set_adjustment (GtkGridView    *self,
                              GtkOrientation  orientation,
                              GtkAdjustment  *adjustment)
{
  if (self->adjustment[orientation] == adjustment)
    return;

  if (adjustment == NULL)
    adjustment = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
  g_object_ref_sink (adjustment);

  gtk_grid_view_clear_adjustment (self, orientation);

  self->adjustment[orientation] = adjustment;
  gtk_grid_view_update_adjustments (self, orientation);

  g_signal_connect (adjustment, "value-changed",
		    G_CALLBACK (gtk_grid_view_adjustment_value_changed_cb),
		    self);
}

static void
gtk_grid_view_set_scroll_policy (GtkGridView         *self,
                                 GtkOrientation       orientation,
                                 GtkScrollablePolicy  scroll_policy)
{
  if (self->scroll_policy[orientation] == scroll_policy)
    return;

  self->scroll_policy[orientation] = scroll_policy;

  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self),
                            orientation == GTK_ORIENTATION_HORIZONTAL
                            ? properties[PROP_HSCROLL_POLICY]
                            : properties[PROP_VSCROLL_POLICY]);
}

static void
gtk_grid_view_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkGridView *self = GTK_GRID_VIEW (object);

  switch (property_id)
    {
    case PROP_HADJUSTMENT:
      gtk_grid_view_set_adjustment (self, GTK_ORIENTATION_HORIZONTAL, g_value_get_object (value));
      break;

    case PROP_HSCROLL_POLICY:
      gtk_grid_view_set_scroll_policy (self, GTK_ORIENTATION_HORIZONTAL, g_value_get_enum (value));
      break;

    case PROP_MAX_COLUMNS:
      gtk_grid_view_set_max_columns (self, g_value_get_uint (value));
      break;

    case PROP_MIN_COLUMNS:
      gtk_grid_view_set_min_columns (self, g_value_get_uint (value));
      break;

    case PROP_MODEL:
      gtk_grid_view_set_model (self, g_value_get_object (value));
      break;

    case PROP_VADJUSTMENT:
      gtk_grid_view_set_adjustment (self, GTK_ORIENTATION_VERTICAL, g_value_get_object (value));
      break;

    case PROP_VSCROLL_POLICY:
      gtk_grid_view_set_scroll_policy (self, GTK_ORIENTATION_VERTICAL, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_grid_view_class_init (GtkGridViewClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  gpointer iface;

  widget_class->measure = gtk_grid_view_measure;
  widget_class->size_allocate = gtk_grid_view_size_allocate;

  gobject_class->dispose = gtk_grid_view_dispose;
  gobject_class->get_property = gtk_grid_view_get_property;
  gobject_class->set_property = gtk_grid_view_set_property;

  /* GtkScrollable implementation */
  iface = g_type_default_interface_peek (GTK_TYPE_SCROLLABLE);
  properties[PROP_HADJUSTMENT] =
      g_param_spec_override ("hadjustment",
                             g_object_interface_find_property (iface, "hadjustment"));
  properties[PROP_HSCROLL_POLICY] =
      g_param_spec_override ("hscroll-policy",
                             g_object_interface_find_property (iface, "hscroll-policy"));
  properties[PROP_VADJUSTMENT] =
      g_param_spec_override ("vadjustment",
                             g_object_interface_find_property (iface, "vadjustment"));
  properties[PROP_VSCROLL_POLICY] =
      g_param_spec_override ("vscroll-policy",
                             g_object_interface_find_property (iface, "vscroll-policy"));

  /**
   * GtkGridView:max-columns:
   *
   * Maximum number of columns per row
   *
   * If this number is smaller than GtkGridView:min-columns, that value
   * is used instead.
   */
  properties[PROP_MAX_COLUMNS] =
    g_param_spec_uint ("max-columns",
                       P_("Max columns"),
                       P_("Maximum number of columns per row"),
                       1, G_MAXUINT, DEFAULT_MAX_COLUMNS,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkGridView:min-columns:
   *
   * Minimum number of columns per row
   */
  properties[PROP_MIN_COLUMNS] =
    g_param_spec_uint ("min-columns",
                       P_("Min columns"),
                       P_("Minimum number of columns per row"),
                       1, G_MAXUINT, 1,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkGridView:model:
   *
   * Model for the items displayed
   */
  properties[PROP_MODEL] =
    g_param_spec_object ("model",
                         P_("Model"),
                         P_("Model for the items displayed"),
                         G_TYPE_LIST_MODEL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, I_("grid"));
}

static void
gtk_grid_view_init (GtkGridView *self)
{
  self->min_columns = 1;
  self->max_columns = DEFAULT_MAX_COLUMNS;

  gtk_widget_set_overflow (GTK_WIDGET (self), GTK_OVERFLOW_HIDDEN);
}

/**
 * gtk_grid_view_new:
 *
 * Creates a new empty #GtkGridView.
 *
 * You most likely want to call gtk_grid_view_set_model() to set
 * a model and then set up a way to map its items to widgets next.
 *
 * Returns: a new #GtkGridView
 **/
GtkWidget *
gtk_grid_view_new (void)
{
  return g_object_new (GTK_TYPE_GRID_VIEW, NULL);
}

/**
 * gtk_grid_view_get_model:
 * @self: a #GtkGridView
 *
 * Gets the model that's currently used to read the items displayed.
 *
 * Returns: (nullable) (transfer none): The model in use
 **/
GListModel *
gtk_grid_view_get_model (GtkGridView *self)
{
  g_return_val_if_fail (GTK_IS_GRID_VIEW (self), NULL);

  return self->model;
}

/**
 * gtk_grid_view_set_model:
 * @self: a #GtkGridView
 * @file: (allow-none) (transfer none): the model to use or %NULL for none
 *
 * Sets the #GListModel to use for
 **/
void
gtk_grid_view_set_model (GtkGridView *self,
                         GListModel  *model)
{
  g_return_if_fail (GTK_IS_GRID_VIEW (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));

  if (self->model == model)
    return;

  gtk_grid_view_clear_model (self);

  if (model)
    {
      self->model = g_object_ref (model);

      g_signal_connect (model,
                        "items-changed", 
                        G_CALLBACK (gtk_grid_view_model_items_changed_cb),
                        self);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

/**
 * gtk_grid_view_get_max_columns:
 * @self: a #GtkGridView
 *
 * Gets the maximum number of columns that the grid will use.
 *
 * Returns: The maximum number of columns
 **/
guint
gtk_grid_view_get_max_columns (GtkGridView *self)
{
  g_return_val_if_fail (GTK_IS_GRID_VIEW (self), DEFAULT_MAX_COLUMNS);

  return self->max_columns;
}

/**
 * gtk_grid_view_set_max_columns:
 * @self: a #GtkGridView
 * @max_columns: The maximum number of columns
 *
 * Sets the maximum number of columns to use. This number must be at least 1.
 * 
 * If @max_columns is smaller than the minimum set via
 * gtk_grid_view_set_min_columns(), that value is used instead.
 **/
void
gtk_grid_view_set_max_columns (GtkGridView *self,
                               guint        max_columns)
{
  g_return_if_fail (GTK_IS_GRID_VIEW (self));
  g_return_if_fail (max_columns > 0);

  if (self->max_columns == max_columns)
    return;

  self->max_columns = max_columns;

  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MAX_COLUMNS]);
}

/**
 * gtk_grid_view_get_min_columns:
 * @self: a #GtkGridView
 *
 * Gets the minimum number of columns that the grid will use.
 *
 * Returns: The minimum number of columns
 **/
guint
gtk_grid_view_get_min_columns (GtkGridView *self)
{
  g_return_val_if_fail (GTK_IS_GRID_VIEW (self), 1);

  return self->min_columns;
}

/**
 * gtk_grid_view_set_min_columns:
 * @self: a #GtkGridView
 * @min_columns: The minimum number of columns
 *
 * Sets the minimum number of columns to use. This number must be at least 1.
 * 
 * If @min_columns is smaller than the minimum set via
 * gtk_grid_view_set_max_columns(), that value is ignored.
 **/
void
gtk_grid_view_set_min_columns (GtkGridView *self,
                               guint        min_columns)
{
  g_return_if_fail (GTK_IS_GRID_VIEW (self));
  g_return_if_fail (min_columns > 0);

  if (self->min_columns == min_columns)
    return;

  self->min_columns = min_columns;

  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MIN_COLUMNS]);
}

