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

#include "gtkcoverflow.h"

#include "gtkintl.h"
#include "gtklistbaseprivate.h"
#include "gtkmain.h"
#include "gtkorientable.h"
#include "gtkprivate.h"
#include "gtkrbtreeprivate.h"
#include "gtkwidgetprivate.h"

/* Extra items to display at most above + below the central item */
#define GTK_COVER_FLOW_DISPLAY_ITEMS 5

/* Number of extra space we leave around the covers */
#define GTK_COVER_FLOW_SCALE_ALONG 3
#define GTK_COVER_FLOW_SCALE_ACROSS 2

/**
 * SECTION:gtkcoverflow
 * @title: GtkCoverFlow
 * @short_description: A coverflow list widget
 * @see_also: #GListModel
 *
 * GtkCoverFlow is a widget to present a coverflow list
 */

struct _GtkCoverFlow
{
  GtkListBase parent_instance;

  int size_across;
  int size_along;
};

struct _GtkCoverFlowClass
{
  GtkListBaseClass parent_class;
};

enum
{
  PROP_0,
  PROP_FACTORY,
  PROP_MODEL,

  N_PROPS
};

enum {
  ACTIVATE,
  LAST_SIGNAL
};

G_DEFINE_TYPE (GtkCoverFlow, gtk_cover_flow, GTK_TYPE_LIST_BASE)

static GParamSpec *properties[N_PROPS] = { NULL, };
static guint signals[LAST_SIGNAL] = { 0 };

static gboolean
gtk_cover_flow_get_allocation_along (GtkListBase *base,
                                     guint        pos,
                                     int         *offset,
                                     int         *size)
{
  GtkCoverFlow *self = GTK_COVER_FLOW (base);

  if (offset)
    *offset = pos * self->size_along;
  if (size)
    *size = self->size_along;

  return TRUE;
}

static gboolean
gtk_cover_flow_get_allocation_across (GtkListBase *base,
                                     guint        pos,
                                     int         *offset,
                                     int         *size)
{
  GtkCoverFlow *self = GTK_COVER_FLOW (base);

  if (offset)
    *offset = pos * self->size_across;
  if (size)
    *size = self->size_across;

  return TRUE;
}

static guint
gtk_cover_flow_move_focus_along (GtkListBase *base,
                                 guint        pos,
                                 int          steps)
{
  if (steps < 0)
    return pos - MIN (pos, -steps);
  else
    {
      pos += MIN (gtk_list_base_get_n_items (base) - pos - 1, steps);
    }

  return pos;
}

static guint
gtk_cover_flow_move_focus_across (GtkListBase *base,
                                  guint        pos,
                                  int          steps)
{
  return pos;
}

static gboolean
gtk_cover_flow_get_position_from_allocation (GtkListBase           *base,
                                             int                    across,
                                             int                    along,
                                             guint                 *pos,
                                             cairo_rectangle_int_t *area)
{
  GtkCoverFlow *self = GTK_COVER_FLOW (base);

  if (across >= self->size_across ||
      along >= self->size_along * gtk_list_base_get_n_items (base))
    return FALSE;

  *pos = along / self->size_along;

  if (area)
    {
      area->x = 0;
      area->width = self->size_across;
      area->y = *pos * self->size_along;
      area->height = self->size_along;
    }

  return TRUE;
}

static void
gtk_cover_flow_measure_children (GtkCoverFlow   *self,
                                 GtkOrientation  orientation,
                                 int             for_size,
                                 int            *minimum,
                                 int            *natural)
{
  GtkListItemManagerItem *item;
  int min, nat, child_min, child_nat;

  min = 0;
  nat = 0;

  for (item = gtk_list_item_manager_get_first (gtk_list_base_get_manager (GTK_LIST_BASE (self)));
       item != NULL;
       item = gtk_rb_tree_node_get_next (item))
    {
      /* ignore unavailable items */
      if (item->widget == NULL)
        continue;

      gtk_widget_measure (item->widget,
                          orientation, for_size,
                          &child_min, &child_nat, NULL, NULL);
      min = MAX (min, child_min);
      nat = MAX (nat, child_nat);
    }

  *minimum = min;
  *natural = nat;
}

static void
gtk_cover_flow_measure_across (GtkWidget      *widget,
                               GtkOrientation  orientation,
                               int             for_size,
                               int            *minimum,
                               int            *natural)
{
  GtkCoverFlow *self = GTK_COVER_FLOW (widget);

  if (for_size > 0)
    for_size /= GTK_COVER_FLOW_SCALE_ALONG;

  gtk_cover_flow_measure_children (self, orientation, for_size, minimum, natural);

  *minimum *= GTK_COVER_FLOW_SCALE_ACROSS;
  *natural *= GTK_COVER_FLOW_SCALE_ACROSS;
}

static void
gtk_cover_flow_measure_along (GtkWidget      *widget,
                              GtkOrientation  orientation,
                              int             for_size,
                              int            *minimum,
                              int            *natural)
{
  GtkCoverFlow *self = GTK_COVER_FLOW (widget);

  if (for_size > 0)
    for_size /= GTK_COVER_FLOW_SCALE_ACROSS;

  gtk_cover_flow_measure_children (self, orientation, for_size, minimum, natural);

  *minimum *= GTK_COVER_FLOW_SCALE_ALONG;
  *natural *= GTK_COVER_FLOW_SCALE_ALONG;
}

static void
gtk_cover_flow_measure (GtkWidget      *widget,
                        GtkOrientation  orientation,
                        int             for_size,
                        int            *minimum,
                        int            *natural,
                        int            *minimum_baseline,
                        int            *natural_baseline)
{
  GtkCoverFlow *self = GTK_COVER_FLOW (widget);

  if (orientation == gtk_list_base_get_orientation (GTK_LIST_BASE (self)))
    gtk_cover_flow_measure_along (widget, orientation, for_size, minimum, natural);
  else
    gtk_cover_flow_measure_across (widget, orientation, for_size, minimum, natural);
}

static GskTransform *
transform_translate_oriented (GskTransform     *transform,
                              GtkOrientation    orientation,
                              GtkTextDirection  dir,
                              float             across,
                              float             along)
{
  if (orientation == GTK_ORIENTATION_VERTICAL)
    return gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (across, along));
  else if (dir == GTK_TEXT_DIR_LTR)
    return gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (along, across));
  else
    return gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (-along, across));
}

static void
gtk_cover_flow_size_allocate_child (GtkWidget        *child,
                                    GtkOrientation    orientation,
                                    GtkTextDirection  dir,
                                    GskTransform     *transform,
                                    int               width,
                                    int               height)
{
  if (orientation == GTK_ORIENTATION_VERTICAL)
    {
      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (-width / 2, -height / 2));
      gtk_widget_allocate (child, width, height, -1, transform);
    }
  else
    {
      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (-height / 2, -width / 2));
      gtk_widget_allocate (child, height, width, -1, transform);
    }
}

static void
gtk_cover_flow_size_allocate (GtkWidget *widget,
                              int        width,
                              int        height,
                              int        baseline)
{
  GtkCoverFlow *self = GTK_COVER_FLOW (widget);
  GtkOrientation orientation, opposite_orientation;
  GtkTextDirection dir;
  GtkListItemManagerItem *item;
  GtkListItemManager *manager;
  guint i, pos;
  int x, y, along, across;

  manager = gtk_list_base_get_manager (GTK_LIST_BASE (self));
  orientation = gtk_list_base_get_orientation (GTK_LIST_BASE (self));
  opposite_orientation = OPPOSITE_ORIENTATION (orientation);

  /* step 0: exit early if list is empty */
  if (gtk_list_item_manager_get_root (manager) == NULL)
    return;

  /* step 1: determine size of children */
  along = orientation == GTK_ORIENTATION_HORIZONTAL ? width : height;
  across = opposite_orientation == GTK_ORIENTATION_HORIZONTAL ? width : height;
  self->size_along = along / GTK_COVER_FLOW_SCALE_ALONG;
  self->size_across = across / GTK_COVER_FLOW_SCALE_ACROSS;

  /* step 2: update the adjustments */
  gtk_list_base_update_adjustments (GTK_LIST_BASE (self),
                                    self->size_across,
                                    self->size_along * gtk_list_base_get_n_items (GTK_LIST_BASE (self)),
                                    self->size_across,
                                    self->size_along,
                                    &x, &y);
  pos = gtk_list_base_get_anchor (GTK_LIST_BASE (self));

  /* step 4: actually allocate the widgets */
  dir = _gtk_widget_get_direction (widget);
  i = 0;

  for (item = gtk_list_item_manager_get_first (manager);
       item != NULL;
       item = gtk_rb_tree_node_get_next (item))
    {
      if (item->widget)
        {
          /* start at the center */
          GskTransform *transform = transform_translate_oriented (NULL, orientation, dir, across / 2, along / 2);

          if (i == pos)
            {
              /* nothing to do, we're already centered */
            }
          else if (MAX (pos, i) - MIN (pos, i) < GTK_COVER_FLOW_DISPLAY_ITEMS) /* ABS() doesn't work with guint */
            {
              int diff = i - pos;
              transform = gsk_transform_perspective (transform, MAX (width, height) * 2);
              transform = transform_translate_oriented (transform,
                                                        orientation, dir,
                                                        0,
                                                        diff * self->size_along / 4);
              transform = transform_translate_oriented (transform,
                                                        orientation, dir,
                                                        0,
                                                        (diff < 0 ? -1 : 1) * self->size_along / 2);
              if (orientation == GTK_ORIENTATION_VERTICAL)
                transform = gsk_transform_rotate_3d (transform,
                                                     diff > 0 ? 60 : -60,
                                                     graphene_vec3_x_axis ());
              else
                transform = gsk_transform_rotate_3d (transform,
                                                     diff < 0 ? 60 : -60,
                                                     graphene_vec3_y_axis ());
              transform = transform_translate_oriented (transform,
                                                        orientation, dir,
                                                        0,
                                                        (diff < 0 ? 1 : -1) * self->size_along / 4);
            }
          else
            {
              transform = transform_translate_oriented (transform,
                                                        orientation, dir,
                                                        - 2 * self->size_across,
                                                        - 2 * self->size_along);
            }
          gtk_cover_flow_size_allocate_child (item->widget,
                                              orientation, dir,
                                              transform,
                                              self->size_across,
                                              self->size_along);
        }

      i += item->n_items;
    }
}

static void
gtk_cover_flow_get_property (GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GtkListBase *base = GTK_LIST_BASE (object);

  switch (property_id)
    {
    case PROP_FACTORY:
      g_value_set_object (value, gtk_list_item_manager_get_factory (gtk_list_base_get_manager (base)));
      break;

    case PROP_MODEL:
      g_value_set_object (value, gtk_list_base_get_model (base));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_cover_flow_snapshot (GtkWidget   *widget,
                         GtkSnapshot *snapshot)
{
  GtkWidget *child;
  GtkListItemManagerItem *item;

  item = gtk_list_item_manager_get_nth (gtk_list_base_get_manager (GTK_LIST_BASE (widget)),
                                        gtk_list_base_get_anchor (GTK_LIST_BASE (widget)),
                                        NULL);
  if (item == NULL || item->widget == NULL)
    {
      GTK_WIDGET_CLASS (gtk_cover_flow_parent_class)->snapshot (widget, snapshot);
      return;
    }

  for (child = _gtk_widget_get_first_child (widget);
       child != item->widget;
       child = _gtk_widget_get_next_sibling (child))
    gtk_widget_snapshot_child (widget, child, snapshot);

  for (child = _gtk_widget_get_last_child (widget);
       child != item->widget;
       child = _gtk_widget_get_prev_sibling (child))
    gtk_widget_snapshot_child (widget, child, snapshot);
  
  gtk_widget_snapshot_child (widget, item->widget, snapshot);
}

static void
gtk_cover_flow_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkCoverFlow *self = GTK_COVER_FLOW (object);

  switch (property_id)
    {
    case PROP_FACTORY:
      gtk_cover_flow_set_factory (self, g_value_get_object (value));
      break;

    case PROP_MODEL:
      gtk_cover_flow_set_model (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_cover_flow_activate_item (GtkWidget  *widget,
                             const char *action_name,
                             GVariant   *parameter)
{
  GtkCoverFlow *self = GTK_COVER_FLOW (widget);
  guint pos;

  if (!g_variant_check_format_string (parameter, "u", FALSE))
    return;

  g_variant_get (parameter, "u", &pos);
  if (pos >= gtk_list_base_get_n_items (GTK_LIST_BASE (self)))
    return;

  g_signal_emit (widget, signals[ACTIVATE], 0, pos);
}

static void
gtk_cover_flow_class_init (GtkCoverFlowClass *klass)
{
  GtkListBaseClass *list_base_class = GTK_LIST_BASE_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  list_base_class->list_item_name = "cover";
  list_base_class->list_item_size = sizeof (GtkListItemManagerItem);
  list_base_class->list_item_augment_size = sizeof (GtkListItemManagerItemAugment);
  list_base_class->list_item_augment_func = gtk_list_item_manager_augment_node;
  list_base_class->get_allocation_along = gtk_cover_flow_get_allocation_along;
  list_base_class->get_allocation_across = gtk_cover_flow_get_allocation_across;
  list_base_class->get_position_from_allocation = gtk_cover_flow_get_position_from_allocation;
  list_base_class->move_focus_along = gtk_cover_flow_move_focus_along;
  list_base_class->move_focus_across = gtk_cover_flow_move_focus_across;

  widget_class->measure = gtk_cover_flow_measure;
  widget_class->size_allocate = gtk_cover_flow_size_allocate;
  widget_class->snapshot = gtk_cover_flow_snapshot;

  gobject_class->get_property = gtk_cover_flow_get_property;
  gobject_class->set_property = gtk_cover_flow_set_property;

  /**
   * GtkCoverFlow:factory:
   *
   * Factory for populating list items
   */
  properties[PROP_FACTORY] =
    g_param_spec_object ("factory",
                         P_("Factory"),
                         P_("Factory for populating list items"),
                         GTK_TYPE_LIST_ITEM_FACTORY,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkCoverFlow:model:
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

  /**
   * GtkCoverFlow::activate:
   * @self: The #GtkCoverFlow
   * @position: position of item to activate
   *
   * The ::activate signal is emitted when an item has been activated by the user,
   * usually via activating the GtkCoverFlow|list.activate-item action.
   *
   * This allows for a convenient way to handle activation in a listview.
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

  /**
   * GtkCoverFlow|list.activate-item:
   * @position: position of item to activate
   *
   * Activates the item given in @position by emitting the GtkCoverFlow::activate
   * signal.
   */
  gtk_widget_class_install_action (widget_class,
                                   "list.activate-item",
                                   "u",
                                   gtk_cover_flow_activate_item);

  gtk_widget_class_set_css_name (widget_class, I_("coverflow"));
}

static void
gtk_cover_flow_init (GtkCoverFlow *self)
{
  gtk_list_base_set_anchor_max_widgets (GTK_LIST_BASE (self),
                                        0,
                                        GTK_COVER_FLOW_DISPLAY_ITEMS);

  /* FIXME: this should overwrite the property default, but gobject properties... */
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_HORIZONTAL);
}

/**
 * gtk_cover_flow_new:
 *
 * Creates a new empty #GtkCoverFlow.
 *
 * You most likely want to call gtk_cover_flow_set_factory() to
 * set up a way to map its items to widgets and gtk_cover_flow_set_model()
 * to set a model to provide items next.
 *
 * Returns: a new #GtkCoverFlow
 **/
GtkWidget *
gtk_cover_flow_new (void)
{
  return g_object_new (GTK_TYPE_COVER_FLOW, NULL);
}

/**
 * gtk_cover_flow_new_with_factory:
 * @factory: (transfer full): The factory to populate items with
 *
 * Creates a new #GtkCoverFlow that uses the given @factory for
 * mapping items to widgets.
 *
 * You most likely want to call gtk_cover_flow_set_model() to set
 * a model next.
 *
 * The function takes ownership of the
 * argument, so you can write code like
 * ```
 *   cover_flow = gtk_cover_flow_new_with_factory (
 *     gtk_builder_list_item_factory_newfrom_resource ("/resource.ui"));
 * ```
 *
 * Returns: a new #GtkCoverFlow using the given @factory
 **/
GtkWidget *
gtk_cover_flow_new_with_factory (GtkListItemFactory *factory)
{
  GtkWidget *result;

  g_return_val_if_fail (GTK_IS_LIST_ITEM_FACTORY (factory), NULL);

  result = g_object_new (GTK_TYPE_COVER_FLOW,
                         "factory", factory,
                         NULL);

  g_object_unref (factory);

  return result;
}

/**
 * gtk_cover_flow_get_model:
 * @self: a #GtkCoverFlow
 *
 * Gets the model that's currently used to read the items displayed.
 *
 * Returns: (nullable) (transfer none): The model in use
 **/
GListModel *
gtk_cover_flow_get_model (GtkCoverFlow *self)
{
  g_return_val_if_fail (GTK_IS_COVER_FLOW (self), NULL);

  return gtk_list_base_get_model (GTK_LIST_BASE (self));
}

/**
 * gtk_cover_flow_set_model:
 * @self: a #GtkCoverFlow
 * @model: (allow-none) (transfer none): the model to use or %NULL for none
 *
 * Sets the #GListModel to use.
 *
 * If the @model is a #GtkSelectionModel, it is used for managing the selection.
 * Otherwise, @self creates a #GtkSingleSelection for the selection.
 **/
void
gtk_cover_flow_set_model (GtkCoverFlow *self,
                         GListModel  *model)
{
  g_return_if_fail (GTK_IS_COVER_FLOW (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));

  if (!gtk_list_base_set_model (GTK_LIST_BASE (self), model))
    return;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

/**
 * gtk_cover_flow_get_factory:
 * @self: a #GtkCoverFlow
 *
 * Gets the factory that's currently used to populate list items.
 *
 * Returns: (nullable) (transfer none): The factory in use
 **/
GtkListItemFactory *
gtk_cover_flow_get_factory (GtkCoverFlow *self)
{
  g_return_val_if_fail (GTK_IS_COVER_FLOW (self), NULL);

  return gtk_list_item_manager_get_factory (gtk_list_base_get_manager (GTK_LIST_BASE (self)));
}

/**
 * gtk_cover_flow_set_factory:
 * @self: a #GtkCoverFlow
 * @factory: (allow-none) (transfer none): the factory to use or %NULL for none
 *
 * Sets the #GtkListItemFactory to use for populating list items.
 **/
void
gtk_cover_flow_set_factory (GtkCoverFlow        *self,
                           GtkListItemFactory *factory)
{
  GtkListItemManager *manager;

  g_return_if_fail (GTK_IS_COVER_FLOW (self));
  g_return_if_fail (factory == NULL || GTK_LIST_ITEM_FACTORY (factory));

  manager = gtk_list_base_get_manager (GTK_LIST_BASE (self));

  if (factory == gtk_list_item_manager_get_factory (manager))
    return;

  gtk_list_item_manager_set_factory (manager, factory);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FACTORY]);
}
