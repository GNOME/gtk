/*
 * Copyright © 2019 Benjamin Otte
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

#include "gtkcolumnviewprivate.h"

#include "gtkboxlayout.h"
#include "gtkbuildable.h"
#include "gtkcolumnlistitemfactoryprivate.h"
#include "gtkcolumnviewcolumnprivate.h"
#include "gtkcolumnviewlayoutprivate.h"
#include "gtkcolumnviewsorterprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkintl.h"
#include "gtklistview.h"
#include "gtkmain.h"
#include "gtkprivate.h"
#include "gtkscrollable.h"
#include "gtkwidgetprivate.h"
#include "gtksizerequest.h"
#include "gtkadjustment.h"
#include "gtkgesturedrag.h"
#include "gtkeventcontrollermotion.h"
#include "gtkdragsource.h"
#include "gtkeventcontrollerkey.h"

/**
 * SECTION:gtkcolumnview
 * @title: GtkColumnView
 * @short_description: A widget for displaying lists in multiple columns
 * @see_also: #GtkColumnViewColumn, #GtkTreeView
 *
 * GtkColumnView is a widget to present a view into a large dynamic list of items
 * using multiple columns with headers.
 *
 * GtkColumnView uses the factories of its columns to generate a cell widget for
 * each column, for each visible item and displays them together as the row for
 * this item. The #GtkColumnView:show-row-separators and
 * #GtkColumnView:show-column-separators properties offer a simple way to display
 * separators between the rows or columns.
 *
 * GtkColumnView allows the user to select items according to the selection
 * characteristics of the model. If the provided model is not a #GtkSelectionModel,
 * GtkColumnView will wrap it in a #GtkSingleSelection. For models that allow
 * multiple selected items, it is possible to turn on _rubberband selection_,
 * using #GtkColumnView:enable-rubberband.
 *
 * The column view supports sorting that can be customized by the user by
 * clicking on column headers. To set this up, the #GtkSorter returned by
 * gtk_column_view_get_sorter() must be attached to a sort model for the data
 * that the view is showing, and the columns must have sorters attached to them
 * by calling gtk_column_view_column_set_sorter(). The initial sort order can be
 * set with gtk_column_view_sort_by_column().
 *
 * The column view also supports interactive resizing and reordering of
 * columns, via Drag-and-Drop of the column headers. This can be enabled or
 * disabled with the #GtkColumnView:reorderable and #GtkColumnViewColumn:resizable
 * properties.
 *
 * To learn more about the list widget framework, see the [overview](#ListWidget).
 *
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * columnview[.column-separators]
 * ├── header
 * │   ├── <column header>
 * ┊   ┊
 * │   ╰── <column header>
 * │
 * ├── listview
 * │
 * ┊
 * ╰── [rubberband]

 * ]|
 *
 * GtkColumnView uses a single CSS node named columnview. It may carry the
 * .column-separators style class, when #GtkColumnView:show-column-separators
 * property is set. Header widets appear below a node with name header.
 * The rows are contained in a GtkListView widget, so there is a listview
 * node with the same structure as for a standalone GtkListView widget. If
 * #GtkColumnView:show-row-separators is set, it will be passed on to the
 * list view, causing its CSS node to carry the .separators style class.
 * For rubberband selection, a node with name rubberband is used.
 */

struct _GtkColumnView
{
  GtkWidget parent_instance;

  GListStore *columns;

  GtkWidget *header;

  GtkListView *listview;
  GtkColumnListItemFactory *factory;

  GtkSorter *sorter;

  GtkAdjustment *hadjustment;

  gboolean reorderable;
  gboolean show_column_separators;

  gboolean in_column_resize;
  gboolean in_column_reorder;
  int drag_pos;
  int drag_x;
  int drag_offset;
  int drag_column_x;

  GtkGesture *drag_gesture;

  guint autoscroll_id;
  double autoscroll_x;
  double autoscroll_delta;
};

struct _GtkColumnViewClass
{
  GtkWidgetClass parent_class;
};

enum
{
  PROP_0,
  PROP_COLUMNS,
  PROP_HADJUSTMENT,
  PROP_HSCROLL_POLICY,
  PROP_MODEL,
  PROP_SHOW_ROW_SEPARATORS,
  PROP_SHOW_COLUMN_SEPARATORS,
  PROP_SORTER,
  PROP_VADJUSTMENT,
  PROP_VSCROLL_POLICY,
  PROP_SINGLE_CLICK_ACTIVATE,
  PROP_REORDERABLE,
  PROP_ENABLE_RUBBERBAND,

  N_PROPS
};

enum {
  ACTIVATE,
  LAST_SIGNAL
};

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_column_view_buildable_add_child (GtkBuildable  *buildable,
                                     GtkBuilder    *builder,
                                     GObject       *child,
                                     const gchar   *type)
{
  if (GTK_IS_COLUMN_VIEW_COLUMN (child))
    {
      if (type != NULL)
        {
          GTK_BUILDER_WARN_INVALID_CHILD_TYPE (buildable, type);
        }
      else
        {
          gtk_column_view_append_column (GTK_COLUMN_VIEW (buildable),
                                         GTK_COLUMN_VIEW_COLUMN (child));
        }
    }
  else
    {
      parent_buildable_iface->add_child (buildable, builder, child, type);
    }
}

static void
gtk_column_view_buildable_interface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = gtk_column_view_buildable_add_child;
}

G_DEFINE_TYPE_WITH_CODE (GtkColumnView, gtk_column_view, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, gtk_column_view_buildable_interface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL))

static GParamSpec *properties[N_PROPS] = { NULL, };
static guint signals[LAST_SIGNAL] = { 0 };

static void
gtk_column_view_measure (GtkWidget      *widget,
                         GtkOrientation  orientation,
                         int             for_size,
                         int            *minimum,
                         int            *natural,
                         int            *minimum_baseline,
                         int            *natural_baseline)
{
  GtkColumnView *self = GTK_COLUMN_VIEW (widget);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      gtk_column_view_measure_across (self, minimum, natural);
    }
  else
    {
      int header_min, header_nat, list_min, list_nat;

      gtk_widget_measure (GTK_WIDGET (self->listview),
                          orientation, for_size,
                          &header_min, &header_nat,
                          NULL, NULL);
      gtk_widget_measure (GTK_WIDGET (self->listview),
                          orientation, for_size,
                          &list_min, &list_nat,
                          NULL, NULL);
      *minimum = header_min + list_min;
      *natural = header_nat + list_nat;
    }
}

static int
gtk_column_view_allocate_columns (GtkColumnView *self,
                                  int            width)
{
  GtkScrollablePolicy scroll_policy;
  int col_min, col_nat, extra, col_size, x;
  int n, n_expand, expand_size, n_extra;
  guint i;
  GtkRequestedSize *sizes;

  n = g_list_model_get_n_items (G_LIST_MODEL (self->columns));
  n_expand = 0;
  sizes = g_newa (GtkRequestedSize, n);
  for (i = 0; i < n; i++)
    {
      GtkColumnViewColumn *column;

      column = g_list_model_get_item (G_LIST_MODEL (self->columns), i);
      if (gtk_column_view_column_get_visible (column))
        {
          gtk_column_view_column_measure (column, &sizes[i].minimum_size, &sizes[i].natural_size);
          if (gtk_column_view_column_get_expand (column))
            n_expand++;
        }
      else
        sizes[i].minimum_size = sizes[i].natural_size = 0;
      g_object_unref (column);
    }

  gtk_column_view_measure_across (self, &col_min, &col_nat);

  scroll_policy = gtk_scrollable_get_hscroll_policy (GTK_SCROLLABLE (self->listview));
  if (scroll_policy == GTK_SCROLL_MINIMUM)
    extra = MAX (width - col_min, 0);
  else
    extra = MAX (width - col_min, col_nat - col_min);

  extra = gtk_distribute_natural_allocation (extra, n, sizes);
  if (n_expand > 0)
    {
      expand_size = extra / n_expand;
      n_extra = extra % n_expand;
    }
  else
    expand_size = n_extra = 0;

  x = 0;
  for (i = 0; i < n; i++)
    {
      GtkColumnViewColumn *column;

      column = g_list_model_get_item (G_LIST_MODEL (self->columns), i);
      if (gtk_column_view_column_get_visible (column))
        {
          col_size = sizes[i].minimum_size;
          if (gtk_column_view_column_get_expand (column))
            {
              col_size += expand_size;
              if (n_extra > 0)
                {
                  col_size++;
                  n_extra--;
                }
            }

          gtk_column_view_column_allocate (column, x, col_size);
          if (self->in_column_reorder && i == self->drag_pos)
            gtk_column_view_column_set_header_position (column, self->drag_x);

          x += col_size;
        }

      g_object_unref (column);
    }

  return x;
}

static void
gtk_column_view_allocate (GtkWidget *widget,
                          int        width,
                          int        height,
                          int        baseline)
{
  GtkColumnView *self = GTK_COLUMN_VIEW (widget);
  int full_width, header_height, min, nat, x;

  x = gtk_adjustment_get_value (self->hadjustment);
  full_width = gtk_column_view_allocate_columns (self, width);

  gtk_widget_measure (self->header, GTK_ORIENTATION_VERTICAL, full_width, &min, &nat, NULL, NULL);
  if (gtk_scrollable_get_vscroll_policy (GTK_SCROLLABLE (self->listview)) == GTK_SCROLL_MINIMUM)
    header_height = min;
  else
    header_height = nat;
  gtk_widget_allocate (self->header, full_width, header_height, -1,
                       gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (-x, 0)));

  gtk_widget_allocate (GTK_WIDGET (self->listview),
                       full_width, height - header_height, -1,
                       gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (-x, header_height)));

  gtk_adjustment_configure (self->hadjustment,  x, 0, full_width, width * 0.1, width * 0.9, width);
}

static void
gtk_column_view_activate_cb (GtkListView   *listview,
                             guint          pos,
                             GtkColumnView *self)
{
  g_signal_emit (self, signals[ACTIVATE], 0, pos);
}

static void
adjustment_value_changed_cb (GtkAdjustment *adjustment,
                             GtkColumnView *self)
{
  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

static void
clear_adjustment (GtkColumnView *self)
{
  if (self->hadjustment == NULL)
    return;

  g_signal_handlers_disconnect_by_func (self->hadjustment, adjustment_value_changed_cb, self);
  g_clear_object (&self->hadjustment);
}

static void
gtk_column_view_dispose (GObject *object)
{
  GtkColumnView *self = GTK_COLUMN_VIEW (object);

  while (g_list_model_get_n_items (G_LIST_MODEL (self->columns)) > 0)
    {
      GtkColumnViewColumn *column = g_list_model_get_item (G_LIST_MODEL (self->columns), 0);
      gtk_column_view_remove_column (self, column);
      g_object_unref (column);
    }

  g_clear_pointer (&self->header, gtk_widget_unparent);

  g_clear_pointer ((GtkWidget **) &self->listview, gtk_widget_unparent);
  g_clear_object (&self->factory);

  g_clear_object (&self->sorter);
  clear_adjustment (self);

  G_OBJECT_CLASS (gtk_column_view_parent_class)->dispose (object);
}

static void
gtk_column_view_finalize (GObject *object)
{
  GtkColumnView *self = GTK_COLUMN_VIEW (object);

  g_object_unref (self->columns);

  G_OBJECT_CLASS (gtk_column_view_parent_class)->finalize (object);
}

static void
gtk_column_view_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkColumnView *self = GTK_COLUMN_VIEW (object);

  switch (property_id)
    {
    case PROP_COLUMNS:
      g_value_set_object (value, self->columns);
      break;

    case PROP_HADJUSTMENT:
      g_value_set_object (value, self->hadjustment);
      break;

    case PROP_HSCROLL_POLICY:
      g_value_set_enum (value, gtk_scrollable_get_hscroll_policy (GTK_SCROLLABLE (self->listview)));
      break;

    case PROP_MODEL:
      g_value_set_object (value, gtk_list_view_get_model (self->listview));
      break;

    case PROP_SHOW_ROW_SEPARATORS:
      g_value_set_boolean (value, gtk_list_view_get_show_separators (self->listview));
      break;

    case PROP_SHOW_COLUMN_SEPARATORS:
      g_value_set_boolean (value, self->show_column_separators);
      break;

    case PROP_VADJUSTMENT:
      g_value_set_object (value, gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (self->listview)));
      break;

    case PROP_VSCROLL_POLICY:
      g_value_set_enum (value, gtk_scrollable_get_vscroll_policy (GTK_SCROLLABLE (self->listview)));
      break;

    case PROP_SORTER:
      g_value_set_object (value, self->sorter);
      break;

    case PROP_SINGLE_CLICK_ACTIVATE:
      g_value_set_boolean (value, gtk_column_view_get_single_click_activate (self));
      break;

    case PROP_REORDERABLE:
      g_value_set_boolean (value, gtk_column_view_get_reorderable (self));
      break;

    case PROP_ENABLE_RUBBERBAND:
      g_value_set_boolean (value, gtk_column_view_get_enable_rubberband (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_column_view_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkColumnView *self = GTK_COLUMN_VIEW (object);
  GtkAdjustment *adjustment;

  switch (property_id)
    {
    case PROP_HADJUSTMENT:
      adjustment = g_value_get_object (value);
      if (adjustment == NULL)
        adjustment = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
      g_object_ref_sink (adjustment);

      if (self->hadjustment != adjustment)
        {
          clear_adjustment (self);

          self->hadjustment = adjustment;

          g_signal_connect (adjustment, "value-changed", G_CALLBACK (adjustment_value_changed_cb), self);

          g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HADJUSTMENT]);
        }
      break;

    case PROP_HSCROLL_POLICY:
      if (gtk_scrollable_get_hscroll_policy (GTK_SCROLLABLE (self->listview)) != g_value_get_enum (value))
        {
          gtk_scrollable_set_hscroll_policy (GTK_SCROLLABLE (self->listview), g_value_get_enum (value));
          g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HSCROLL_POLICY]);
        }
      break;

    case PROP_MODEL:
      gtk_column_view_set_model (self, g_value_get_object (value));
      break;

    case PROP_SHOW_ROW_SEPARATORS:
      gtk_column_view_set_show_row_separators (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_COLUMN_SEPARATORS:
      gtk_column_view_set_show_column_separators (self, g_value_get_boolean (value));
      break;

    case PROP_VADJUSTMENT:
      if (gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (self->listview)) != g_value_get_object (value))
        {
          gtk_scrollable_set_vadjustment (GTK_SCROLLABLE (self->listview), g_value_get_object (value));
          g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VADJUSTMENT]);
        }
      break;

    case PROP_VSCROLL_POLICY:
      if (gtk_scrollable_get_vscroll_policy (GTK_SCROLLABLE (self->listview)) != g_value_get_enum (value))
        {
          gtk_scrollable_set_vscroll_policy (GTK_SCROLLABLE (self->listview), g_value_get_enum (value));
          g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VSCROLL_POLICY]);
        }
      break;

    case PROP_SINGLE_CLICK_ACTIVATE:
      gtk_column_view_set_single_click_activate (self, g_value_get_boolean (value));
      break;

    case PROP_REORDERABLE:
      gtk_column_view_set_reorderable (self, g_value_get_boolean (value));
      break;

    case PROP_ENABLE_RUBBERBAND:
      gtk_column_view_set_enable_rubberband (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_column_view_class_init (GtkColumnViewClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  gpointer iface;

  widget_class->measure = gtk_column_view_measure;
  widget_class->size_allocate = gtk_column_view_allocate;

  gobject_class->dispose = gtk_column_view_dispose;
  gobject_class->finalize = gtk_column_view_finalize;
  gobject_class->get_property = gtk_column_view_get_property;
  gobject_class->set_property = gtk_column_view_set_property;

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
   * GtkColumnView:columns:
   *
   * The list of columns
   */
  properties[PROP_COLUMNS] =
    g_param_spec_object ("columns",
                         P_("Columns"),
                         P_("List of columns"),
                         G_TYPE_LIST_MODEL,
                         G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkColumnView:model:
   *
   * Model for the items displayed
   */
  properties[PROP_MODEL] =
    g_param_spec_object ("model",
                         P_("Model"),
                         P_("Model for the items displayed"),
                         G_TYPE_LIST_MODEL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkColumnView:show-row-separators:
   *
   * Show separators between rows
   */
  properties[PROP_SHOW_ROW_SEPARATORS] =
    g_param_spec_boolean ("show-row-separators",
                          P_("Show row separators"),
                          P_("Show separators between rows"),
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkColumnView:show-column-separators:
   *
   * Show separators between columns
   */
  properties[PROP_SHOW_COLUMN_SEPARATORS] =
    g_param_spec_boolean ("show-column-separators",
                          P_("Show column separators"),
                          P_("Show separators between columns"),
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkColumnView:sorter:
   *
   * Sorter with the sorting choices of the user
   */
  properties[PROP_SORTER] =
    g_param_spec_object ("sorter",
                         P_("Sorter"),
                         P_("Sorter with sorting choices of the user"),
                         GTK_TYPE_SORTER,
                         G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkColumnView:single-click-activate:
   *
   * Activate rows on single click and select them on hover
   */
  properties[PROP_SINGLE_CLICK_ACTIVATE] =
    g_param_spec_boolean ("single-click-activate",
                          P_("Single click activate"),
                          P_("Activate rows on single click"),
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkColumnView:reorderable:
   *
   * Whether columns are reorderable
   */
  properties[PROP_REORDERABLE] =
    g_param_spec_boolean ("reorderable",
                          P_("Reorderable"),
                          P_("Whether columns are reorderable"),
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkColumnView:enable-rubberband:
   *
   * Allow rubberband selection
   */
  properties[PROP_ENABLE_RUBBERBAND] =
    g_param_spec_boolean ("enable-rubberband",
                          P_("Enable rubberband selection"),
                          P_("Allow selecting items by dragging with the mouse"),
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);

  /**
   * GtkColumnView::activate:
   * @self: The #GtkColumnView
   * @position: position of item to activate
   *
   * The ::activate signal is emitted when a row has been activated by the user,
   * usually via activating the GtkListBase|list.activate-item action.
   *
   * This allows for a convenient way to handle activation in a columnview.
   * See gtk_list_item_set_activatable() for details on how to use this signal.
   */
  signals[ACTIVATE] =
    g_signal_new (I_("activate"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__UINT,
                  G_TYPE_NONE, 1,
                  G_TYPE_UINT);
  g_signal_set_va_marshaller (signals[ACTIVATE],
                              G_TYPE_FROM_CLASS (gobject_class),
                              g_cclosure_marshal_VOID__UINTv);

  gtk_widget_class_set_css_name (widget_class, I_("columnview"));
}

static void update_column_resize  (GtkColumnView *self,
                                   double         x);
static void update_column_reorder (GtkColumnView *self,
                                   double         x);

static gboolean
autoscroll_cb (GtkWidget     *widget,
               GdkFrameClock *frame_clock,
               gpointer       data)
{
  GtkColumnView *self = data;

  gtk_adjustment_set_value (self->hadjustment,
                            gtk_adjustment_get_value (self->hadjustment) + self->autoscroll_delta);

  self->autoscroll_x += self->autoscroll_delta;

  if (self->in_column_resize)
    update_column_resize (self, self->autoscroll_x);
  else if (self->in_column_reorder)
    update_column_reorder (self, self->autoscroll_x);

  return G_SOURCE_CONTINUE;
}

static void
add_autoscroll (GtkColumnView *self,
                double         x,
                double         delta)
{
  self->autoscroll_x = x;
  self->autoscroll_delta = delta;

  if (self->autoscroll_id == 0)
    self->autoscroll_id = gtk_widget_add_tick_callback (GTK_WIDGET (self), autoscroll_cb, self, NULL);
}

static void
remove_autoscroll (GtkColumnView *self)
{
  if (self->autoscroll_id != 0)
    {
      gtk_widget_remove_tick_callback (GTK_WIDGET (self), self->autoscroll_id);
      self->autoscroll_id = 0;
    }
}

#define DRAG_WIDTH 6

static gboolean
gtk_column_view_in_resize_rect (GtkColumnView       *self,
                                GtkColumnViewColumn *column,
                                double               x,
                                double               y)
{
  GtkWidget *header;
  graphene_rect_t rect;

  header = gtk_column_view_column_get_header (column);

  if (!gtk_widget_compute_bounds (header, self->header, &rect))
    return FALSE;

  rect.origin.x += rect.size.width - DRAG_WIDTH / 2;
  rect.size.width = DRAG_WIDTH;

  return graphene_rect_contains_point (&rect, &(graphene_point_t) { x, y});
}

static gboolean
gtk_column_view_in_header (GtkColumnView       *self,
                           GtkColumnViewColumn *column,
                           double               x,
                           double               y)
{
  GtkWidget *header;
  graphene_rect_t rect;

header = gtk_column_view_column_get_header (column);

  if (!gtk_widget_compute_bounds (header, self->header, &rect))
    return FALSE;

  return graphene_rect_contains_point (&rect, &(graphene_point_t) { x, y});
}

static void
header_drag_begin (GtkGestureDrag *gesture,
                   double          start_x,
                   double          start_y,
                   GtkColumnView  *self)
{
  int i, n;

  self->drag_pos = -1;

  n = g_list_model_get_n_items (G_LIST_MODEL (self->columns));
  for (i = 0; !self->in_column_resize && i < n; i++)
    {
      GtkColumnViewColumn *column = g_list_model_get_item (G_LIST_MODEL (self->columns), i);

      if (!gtk_column_view_column_get_visible (column))
        {
          g_object_unref (column);
          continue;
        }

      if (i + 1 < n &&
          gtk_column_view_column_get_resizable (column) &&
          gtk_column_view_in_resize_rect (self, column, start_x, start_y))
        {
          int size;

          gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
          if (!gtk_widget_has_focus (GTK_WIDGET (self)))
            gtk_widget_grab_focus (GTK_WIDGET (self));

          gtk_column_view_column_get_allocation (column, NULL, &size);
          gtk_column_view_column_set_fixed_width (column, size);

          self->drag_pos = i;
          self->drag_x = start_x - size;
          self->in_column_resize = TRUE;

          g_object_unref (column);
          break;
        }

      if (gtk_column_view_get_reorderable (self) &&
          gtk_column_view_in_header (self, column, start_x, start_y))
        {
          int pos;

          gtk_column_view_column_get_allocation (column, &pos, NULL);

          self->drag_pos = i;
          self->drag_offset = start_x - pos;

          g_object_unref (column);
          break;
        }

      g_object_unref (column);
    }
}

static void
header_drag_end (GtkGestureDrag *gesture,
                 double          offset_x,
                 double          offset_y,
                 GtkColumnView  *self)
{
  double start_x, x;

  gtk_gesture_drag_get_start_point (gesture, &start_x, NULL);
  x = start_x + offset_x;

  remove_autoscroll (self);

  if (self->in_column_resize)
    {
      self->in_column_resize = FALSE;
    }
  else if (self->in_column_reorder)
    {
      GdkEventSequence *sequence;
      GtkColumnViewColumn *column;
      GtkWidget *header;
      int i;

      self->in_column_reorder = FALSE;

      if (self->drag_pos == -1)
        return;

      column = g_list_model_get_item (G_LIST_MODEL (self->columns), self->drag_pos);
      header = gtk_column_view_column_get_header (column);
      gtk_style_context_remove_class (gtk_widget_get_style_context (header), "dnd");

      sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
      if (!gtk_gesture_handles_sequence (GTK_GESTURE (gesture), sequence))
        return;

      for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->columns)); i++)
        {
          GtkColumnViewColumn *col = g_list_model_get_item (G_LIST_MODEL (self->columns), i);

          if (gtk_column_view_column_get_visible (col))
            {
              int pos, size;

              gtk_column_view_column_get_allocation (col, &pos, &size);
              if (pos <= x && x <= pos + size)
                {
                  gtk_column_view_insert_column (self, i, column);
                  g_object_unref (col);
                  break;
                }
            }

          g_object_unref (col);
        }

      g_object_unref (column);
    }
}

static void
update_column_resize (GtkColumnView *self,
                      double         x)
{
  GtkColumnViewColumn *column;

  column = g_list_model_get_item (G_LIST_MODEL (self->columns), self->drag_pos);
  gtk_column_view_column_set_fixed_width (column, MAX (x - self->drag_x, 0));
  g_object_unref (column);
}

static void
update_column_reorder (GtkColumnView *self,
                       double         x)
{
  GtkColumnViewColumn *column;
  int width;
  int size;

  column = g_list_model_get_item (G_LIST_MODEL (self->columns), self->drag_pos);
  width = gtk_widget_get_allocated_width (GTK_WIDGET (self->header));
  gtk_column_view_column_get_allocation (column, NULL, &size);

  self->drag_x = CLAMP (x - self->drag_offset, 0, width - size);

  gtk_widget_queue_allocate (GTK_WIDGET (self));
  gtk_column_view_column_queue_resize (column);
  g_object_unref (column);
}

#define SCROLL_EDGE_SIZE 15

static void
header_drag_update (GtkGestureDrag *gesture,
                    double          offset_x,
                    double          offset_y,
                    GtkColumnView  *self)
{
  GdkEventSequence *sequence;
  double start_x, x;

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  if (!gtk_gesture_handles_sequence (GTK_GESTURE (gesture), sequence))
    return;

  if (self->drag_pos == -1)
    return;

  if (!self->in_column_resize && !self->in_column_reorder)
    {
      if (gtk_drag_check_threshold (GTK_WIDGET (self), 0, 0, offset_x, 0))
        {
          GtkColumnViewColumn *column;
          GtkWidget *header;

          column = g_list_model_get_item (G_LIST_MODEL (self->columns), self->drag_pos);
          header = gtk_column_view_column_get_header (column);

          gtk_widget_insert_after (header, self->header, gtk_widget_get_last_child (self->header));
          gtk_style_context_add_class (gtk_widget_get_style_context (header), "dnd");

          gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
          if (!gtk_widget_has_focus (GTK_WIDGET (self)))
            gtk_widget_grab_focus (GTK_WIDGET (self));

          self->in_column_reorder = TRUE;

          g_object_unref (column);
        }
    }

  gtk_gesture_drag_get_start_point (gesture, &start_x, NULL);
  x = start_x + offset_x;

  if (self->in_column_resize)
    update_column_resize (self, x);
  else if (self->in_column_reorder)
    update_column_reorder (self, x);

  if (self->in_column_resize || self->in_column_reorder)
    {
      double value, page_size, upper;

      value = gtk_adjustment_get_value (self->hadjustment);
      page_size = gtk_adjustment_get_page_size (self->hadjustment);
      upper = gtk_adjustment_get_upper (self->hadjustment);

      if (x - value < SCROLL_EDGE_SIZE && value > 0)
        add_autoscroll (self, x, - (SCROLL_EDGE_SIZE - (x - value))/3.0);
      else if (value + page_size - x < SCROLL_EDGE_SIZE && value + page_size < upper)
        add_autoscroll (self, x, (SCROLL_EDGE_SIZE - (value + page_size - x))/3.0);
      else
        remove_autoscroll (self);
    }
}

static void
header_motion (GtkEventControllerMotion *controller,
               double                    x,
               double                    y,
               GtkColumnView            *self)
{
  gboolean cursor_set = FALSE;
  int i, n;

  n = g_list_model_get_n_items (G_LIST_MODEL (self->columns));
  for (i = 0; i < n; i++)
    {
      GtkColumnViewColumn *column = g_list_model_get_item (G_LIST_MODEL (self->columns), i);

      if (!gtk_column_view_column_get_visible (column))
        {
          g_object_unref (column);
          continue;
        }

      if (i + 1 < n &&
          gtk_column_view_column_get_resizable (column) &&
          gtk_column_view_in_resize_rect (self, column, x, y))
        {
          gtk_widget_set_cursor_from_name (self->header, "col-resize");
          cursor_set = TRUE;
        }

      g_object_unref (column);
    }

  if (!cursor_set)
    gtk_widget_set_cursor (self->header, NULL);
}

static gboolean
header_key_pressed (GtkEventControllerKey *controller,
                    guint                  keyval,
                    guint                  keycode,
                    GdkModifierType        modifiers,
                    GtkColumnView         *self)
{
  if (self->in_column_reorder)
    {
      if (keyval == GDK_KEY_Escape)
        gtk_gesture_set_state (self->drag_gesture, GTK_EVENT_SEQUENCE_DENIED);
      return TRUE;
    }

  return FALSE;
}

static void
gtk_column_view_init (GtkColumnView *self)
{
  GtkEventController *controller;

  self->columns = g_list_store_new (GTK_TYPE_COLUMN_VIEW_COLUMN);

  self->header = gtk_list_item_widget_new (NULL, "header");
  gtk_widget_set_can_focus (self->header, FALSE);
  gtk_widget_set_layout_manager (self->header, gtk_column_view_layout_new (self));
  gtk_widget_set_parent (self->header, GTK_WIDGET (self));

  controller = GTK_EVENT_CONTROLLER (gtk_gesture_drag_new ());
  g_signal_connect (controller, "drag-begin", G_CALLBACK (header_drag_begin), self);
  g_signal_connect (controller, "drag-update", G_CALLBACK (header_drag_update), self);
  g_signal_connect (controller, "drag-end", G_CALLBACK (header_drag_end), self);
  gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);
  gtk_widget_add_controller (self->header, controller);
  self->drag_gesture = GTK_GESTURE (controller);

  controller = gtk_event_controller_motion_new ();
  g_signal_connect (controller, "motion", G_CALLBACK (header_motion), self);
  gtk_widget_add_controller (self->header, controller);

  controller = gtk_event_controller_key_new ();
  g_signal_connect (controller, "key-pressed", G_CALLBACK (header_key_pressed), self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);

  self->sorter = gtk_column_view_sorter_new ();
  self->factory = gtk_column_list_item_factory_new (self);
  self->listview = GTK_LIST_VIEW (gtk_list_view_new_with_factory (
        GTK_LIST_ITEM_FACTORY (g_object_ref (self->factory))));
  gtk_widget_set_hexpand (GTK_WIDGET (self->listview), TRUE);
  gtk_widget_set_vexpand (GTK_WIDGET (self->listview), TRUE);
  g_signal_connect (self->listview, "activate", G_CALLBACK (gtk_column_view_activate_cb), self);
  gtk_widget_set_parent (GTK_WIDGET (self->listview), GTK_WIDGET (self));

  gtk_css_node_add_class (gtk_widget_get_css_node (GTK_WIDGET (self)),
                          g_quark_from_static_string (I_("view")));

  gtk_widget_set_overflow (GTK_WIDGET (self), GTK_OVERFLOW_HIDDEN);
  gtk_widget_set_focusable (GTK_WIDGET (self), TRUE);

  self->reorderable = TRUE;
}

/**
 * gtk_column_view_new:
 *
 * Creates a new empty #GtkColumnView.
 *
 * You most likely want to call gtk_column_view_set_factory() to
 * set up a way to map its items to widgets and gtk_column_view_set_model()
 * to set a model to provide items next.
 *
 * Returns: a new #GtkColumnView
 **/
GtkWidget *
gtk_column_view_new (void)
{
  return g_object_new (GTK_TYPE_COLUMN_VIEW, NULL);
}

/**
 * gtk_column_view_get_model:
 * @self: a #GtkColumnView
 *
 * Gets the model that's currently used to read the items displayed.
 *
 * Returns: (nullable) (transfer none): The model in use
 **/
GListModel *
gtk_column_view_get_model (GtkColumnView *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW (self), NULL);

  return gtk_list_view_get_model (self->listview);
}

/**
 * gtk_column_view_set_model:
 * @self: a #GtkColumnView
 * @model: (allow-none) (transfer none): the model to use or %NULL for none
 *
 * Sets the #GListModel to use.
 *
 * If the @model is a #GtkSelectionModel, it is used for managing the selection.
 * Otherwise, @self creates a #GtkSingleSelection for the selection.
 **/
void
gtk_column_view_set_model (GtkColumnView *self,
                           GListModel  *model)
{
  g_return_if_fail (GTK_IS_COLUMN_VIEW (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));

  if (gtk_list_view_get_model (self->listview) == model)
    return;

  gtk_list_view_set_model (self->listview, model);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

/**
 * gtk_column_view_get_columns:
 * @self: a #GtkColumnView
 *
 * Gets the list of columns in this column view. This list is constant over
 * the lifetime of @self and can be used to monitor changes to the columns
 * of @self by connecting to the GListModel:items-changed signal.
 *
 * Returns: (transfer none): The list managing the columns
 **/
GListModel *
gtk_column_view_get_columns (GtkColumnView *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW (self), NULL);

  return G_LIST_MODEL (self->columns);
}

/**
 * gtk_column_view_set_show_row_separators:
 * @self: a #GtkColumnView
 * @show_row_separators: %TRUE to show row separators
 *
 * Sets whether the list should show separators
 * between rows.
 */
void
gtk_column_view_set_show_row_separators (GtkColumnView *self,
                                         gboolean     show_row_separators)
{
  g_return_if_fail (GTK_IS_COLUMN_VIEW (self));

  if (gtk_list_view_get_show_separators (self->listview) == show_row_separators)
    return;

  gtk_list_view_set_show_separators (self->listview, show_row_separators);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_ROW_SEPARATORS]);
}

/**
 * gtk_column_view_get_show_row_separators:
 * @self: a #GtkColumnView
 *
 * Returns whether the list should show separators
 * between rows.
 *
 * Returns: %TRUE if the list shows separators
 */
gboolean
gtk_column_view_get_show_row_separators (GtkColumnView *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW (self), FALSE);

  return gtk_list_view_get_show_separators (self->listview);
}

/**
 * gtk_column_view_set_show_column_separators:
 * @self: a #GtkColumnView
 * @show_column_separators: %TRUE to show column separators
 *
 * Sets whether the list should show separators
 * between columns.
 */
void
gtk_column_view_set_show_column_separators (GtkColumnView *self,
                                            gboolean     show_column_separators)
{
  g_return_if_fail (GTK_IS_COLUMN_VIEW (self));

  if (self->show_column_separators == show_column_separators)
    return;

  self->show_column_separators = show_column_separators;

  if (show_column_separators)
    gtk_widget_add_css_class (GTK_WIDGET (self), "column-separators");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self), "column-separators");

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_COLUMN_SEPARATORS]);
}

/**
 * gtk_column_view_get_show_column_separators:
 * @self: a #GtkColumnView
 *
 * Returns whether the list should show separators
 * between columns.
 *
 * Returns: %TRUE if the list shows column separators
 */
gboolean
gtk_column_view_get_show_column_separators (GtkColumnView *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW (self), FALSE);

  return self->show_column_separators;
}

/**
 * gtk_column_view_append_column:
 * @self: a #GtkColumnView
 * @column: a #GtkColumnViewColumn that hasn't been added to a
 *     #GtkColumnView yet
 *
 * Appends the @column to the end of the columns in @self.
 **/
void
gtk_column_view_append_column (GtkColumnView       *self,
                               GtkColumnViewColumn *column)
{
  g_return_if_fail (GTK_IS_COLUMN_VIEW (self));
  g_return_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (column));
  g_return_if_fail (gtk_column_view_column_get_column_view (column) == NULL);

  gtk_column_view_column_set_column_view (column, self);
  g_list_store_append (self->columns, column);
}

/**
 * gtk_column_view_remove_column:
 * @self: a #GtkColumnView
 * @column: a #GtkColumnViewColumn that's part of @self
 *
 * Removes the @column from the list of columns of @self.
 **/
void
gtk_column_view_remove_column (GtkColumnView       *self,
                               GtkColumnViewColumn *column)
{
  guint i;

  g_return_if_fail (GTK_IS_COLUMN_VIEW (self));
  g_return_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (column));
  g_return_if_fail (gtk_column_view_column_get_column_view (column) == self);

  for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->columns)); i++)
    {
      GtkColumnViewColumn *item = g_list_model_get_item (G_LIST_MODEL (self->columns), i);

      g_object_unref (item);
      if (item == column)
        break;

    }

  gtk_column_view_sorter_remove_column (GTK_COLUMN_VIEW_SORTER (self->sorter), column);
  gtk_column_view_column_set_column_view (column, NULL);
  g_list_store_remove (self->columns, i);
}

/**
 * gtk_column_view_insert_column:
 * @self: a #GtkColumnView
 * @position: the position to insert @column at
 * @column: the #GtkColumnViewColumn to insert
 *
 * Inserts a column at the given position in the columns of @self.
 *
 * If @column is already a column of @self, it will be repositioned.
 */
void
gtk_column_view_insert_column (GtkColumnView       *self,
                               guint                position,
                               GtkColumnViewColumn *column)
{
  g_return_if_fail (GTK_IS_COLUMN_VIEW (self));
  g_return_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (column));
  g_return_if_fail (gtk_column_view_column_get_column_view (column) == NULL ||
                    gtk_column_view_column_get_column_view (column) == self);
  g_return_if_fail (position <= g_list_model_get_n_items (G_LIST_MODEL (self->columns)));

  g_object_ref (column);

  if (gtk_column_view_column_get_column_view (column) == self)
    {
      guint i;

      for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->columns)); i++)
        {
          GtkColumnViewColumn *item = g_list_model_get_item (G_LIST_MODEL (self->columns), i);

          g_object_unref (item);
          if (item == column)
            {
              g_list_store_remove (self->columns, i);
              break;
            }
        }
    }
  else
    gtk_column_view_column_set_column_view (column, self);

  g_list_store_insert (self->columns, position, column);

  gtk_column_view_column_queue_resize (column);

  g_object_unref (column);
}

void
gtk_column_view_measure_across (GtkColumnView *self,
                                int           *minimum,
                                int           *natural)
{
  guint i;
  int min, nat;

  min = 0;
  nat = 0;

  for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->columns)); i++)
    {
      GtkColumnViewColumn *column;
      int col_min, col_nat;

      column = g_list_model_get_item (G_LIST_MODEL (self->columns), i);
      gtk_column_view_column_measure (column, &col_min, &col_nat);
      min += col_min;
      nat += col_nat;

      g_object_unref (column);
    }

  *minimum = min;
  *natural = nat;
}

GtkListItemWidget *
gtk_column_view_get_header_widget (GtkColumnView *self)
{
  return GTK_LIST_ITEM_WIDGET (self->header);
}

GtkListView *
gtk_column_view_get_list_view (GtkColumnView *self)
{
  return GTK_LIST_VIEW (self->listview);
}

/**
 * gtk_column_view_get_sorter:
 * @self: a #GtkColumnView
 *
 * Returns the sorter associated with users sorting choices in
 * the column view.
 *
 * To allow users to customizable sorting by clicking on column
 * headers, this sorter needs to be set on the sort
 * model(s) underneath the model that is displayed
 * by the view.
 *
 * See gtk_column_view_column_get_sorter() for setting up
 * per-column sorting.
 *
 * Returns: (transfer none): the #GtkSorter of @self
 */
GtkSorter *
gtk_column_view_get_sorter (GtkColumnView *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW (self), NULL);

  return self->sorter;
}

/**
 * gtk_column_view_sort_by_column:
 * @self: a #GtkColumnView
 * @column: (allow-none): the #GtkColumnViewColumn to sort by, or %NULL
 * @direction: the direction to sort in
 *
 * Sets the sorting of the view.
 *
 * This function should be used to set up the initial sorting. At runtime,
 * users can change the sorting of a column view by clicking on the list headers.
 *
 * This call only has an effect if the sorter returned by gtk_column_view_get_sorter()
 * is set on a sort model, and gtk_column_view_column_set_sorter() has been called
 * on @column to associate a sorter with the column.
 *
 * If @column is %NULL, the view will be unsorted.
 */
void
gtk_column_view_sort_by_column (GtkColumnView       *self,
                                GtkColumnViewColumn *column,
                                GtkSortType          direction)
{
  g_return_if_fail (GTK_IS_COLUMN_VIEW (self));
  g_return_if_fail (column == NULL || GTK_IS_COLUMN_VIEW_COLUMN (column));
  g_return_if_fail (column == NULL || gtk_column_view_column_get_column_view (column) == self);

  if (column == NULL)
    gtk_column_view_sorter_clear (GTK_COLUMN_VIEW_SORTER (self->sorter));
  else
    gtk_column_view_sorter_set_column (GTK_COLUMN_VIEW_SORTER (self->sorter),
                                       column,
                                       direction == GTK_SORT_DESCENDING);
}

/**
 * gtk_column_view_set_single_click_activate:
 * @self: a #GtkColumnView
 * @single_click_activate: %TRUE to activate items on single click
 *
 * Sets whether rows should be activated on single click and
 * selected on hover.
 */
void
gtk_column_view_set_single_click_activate (GtkColumnView *self,
                                           gboolean       single_click_activate)
{
  g_return_if_fail (GTK_IS_COLUMN_VIEW (self));

  if (single_click_activate == gtk_list_view_get_single_click_activate (self->listview))
    return;

  gtk_list_view_set_single_click_activate (self->listview, single_click_activate);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SINGLE_CLICK_ACTIVATE]);
}

/**
 * gtk_column_view_get_single_click_activate:
 * @self: a #GtkColumnView
 *
 * Returns whether rows will be activated on single click and
 * selected on hover.
 *
 * Returns: %TRUE if rows are activated on single click
 */
gboolean
gtk_column_view_get_single_click_activate (GtkColumnView *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW (self), FALSE);

  return gtk_list_view_get_single_click_activate (self->listview);
}

/**
 * gtk_column_view_set_reorderable:
 * @self: a #GtkColumnView
 * @reorderable: whether columns should be reorderable
 *
 * Sets whether columns should be reorderable by dragging.
 */
void
gtk_column_view_set_reorderable (GtkColumnView *self,
                                 gboolean       reorderable)
{
  g_return_if_fail (GTK_IS_COLUMN_VIEW (self));

  if (self->reorderable == reorderable)
    return;

  self->reorderable = reorderable;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_REORDERABLE]);
}

/**
 * gtk_column_view_get_reorderable:
 * @self: a #GtkColumnView
 *
 * Returns whether columns are reorderable.
 *
 * Returns: %TRUE if columns are reorderable
 */
gboolean
gtk_column_view_get_reorderable (GtkColumnView *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW (self), TRUE);

  return self->reorderable;
}

/**
 * gtk_column_view_set_enable_rubberband:
 * @self: a #GtkColumnView
 * @enable_rubberband: %TRUE to enable rubberband selection
 *
 * Sets whether selections can be changed by dragging with the mouse.
 */
void
gtk_column_view_set_enable_rubberband (GtkColumnView *self,
                                       gboolean       enable_rubberband)
{
  g_return_if_fail (GTK_IS_COLUMN_VIEW (self));

  if (enable_rubberband == gtk_list_view_get_enable_rubberband (self->listview))
    return;

  gtk_list_view_set_enable_rubberband (self->listview, enable_rubberband);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ENABLE_RUBBERBAND]);
}

/**
 * gtk_column_view_get_enable_rubberband:
 * @self: a #GtkColumnView
 *
 * Returns whether rows can be selected by dragging with the mouse.
 *
 * Returns: %TRUE if rubberband selection is enabled
 */
gboolean
gtk_column_view_get_enable_rubberband (GtkColumnView *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW (self), FALSE);

  return gtk_list_view_get_enable_rubberband (self->listview);
}
