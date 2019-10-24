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

#include "gtklistbaseprivate.h"

#include "gtkadjustment.h"
#include "gtkintl.h"
#include "gtkorientableprivate.h"
#include "gtkscrollable.h"
#include "gtktypebuiltins.h"

typedef struct _GtkListBasePrivate GtkListBasePrivate;

struct _GtkListBasePrivate
{
  GtkListItemManager *item_manager;
  GtkOrientation orientation;
  GtkAdjustment *adjustment[2];
  GtkScrollablePolicy scroll_policy[2];

  /* the last item that was selected - basically the location to extend selections from */
  GtkListItemTracker *selected;
};

enum
{
  PROP_0,
  PROP_HADJUSTMENT,
  PROP_HSCROLL_POLICY,
  PROP_ORIENTATION,
  PROP_VADJUSTMENT,
  PROP_VSCROLL_POLICY,

  N_PROPS
};

/* HACK: We want the g_class argument in our instance init func and G_DEFINE_TYPE() won't let us */
static void gtk_list_base_init_real (GtkListBase *self, GtkListBaseClass *g_class);
#define g_type_register_static_simple(a,b,c,d,e,evil,f) g_type_register_static_simple(a,b,c,d,e, (GInstanceInitFunc) gtk_list_base_init_real, f);
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GtkListBase, gtk_list_base, GTK_TYPE_WIDGET,
                                                              G_ADD_PRIVATE (GtkListBase)
                                                              G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL)
                                                              G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL))
#undef g_type_register_static_simple
G_GNUC_UNUSED static void gtk_list_base_init (GtkListBase *self) { }

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
gtk_list_base_adjustment_value_changed_cb (GtkAdjustment *adjustment,
                                           GtkListBase   *self)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  GtkOrientation orientation;

  orientation = adjustment == priv->adjustment[GTK_ORIENTATION_HORIZONTAL] 
                ? GTK_ORIENTATION_HORIZONTAL
                : GTK_ORIENTATION_VERTICAL;

  GTK_LIST_BASE_GET_CLASS (self)->adjustment_value_changed (self, orientation);
}

static void
gtk_list_base_clear_adjustment (GtkListBase    *self,
                                GtkOrientation  orientation)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  if (priv->adjustment[orientation] == NULL)
    return;

  g_signal_handlers_disconnect_by_func (priv->adjustment[orientation],
                                        gtk_list_base_adjustment_value_changed_cb,
                                        self);
  g_clear_object (&priv->adjustment[orientation]);
}

/*
 * gtk_list_base_select_item:
 * @self: a %GtkListBase
 * @pos: item to select
 * @modify: %TRUE if the selection should be modified, %FALSE
 *     if a new selection should be done. This is usually set
 *     to %TRUE if the user keeps the <Shift> key pressed.
 * @extend_pos: %TRUE if the selection should be extended.
 *     Selections are usually extended from the last selected
 *     position if the user presses the <Ctrl> key.
 *
 * Selects the item at @pos according to how GTK list widgets modify
 * selections, both when clicking rows with the mouse or when using
 * the keyboard.
 **/
void
gtk_list_base_select_item (GtkListBase *self,
                           guint        pos,
                           gboolean     modify,
                           gboolean     extend)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  GtkSelectionModel *model;

  model = gtk_list_item_manager_get_model (priv->item_manager);
  if (model == NULL)
    return;

  if (gtk_selection_model_user_select_item (model,
                                            pos,
                                            modify,
                                            extend ? gtk_list_item_tracker_get_position (priv->item_manager, priv->selected)
                                                   : GTK_INVALID_LIST_POSITION))
    {
      gtk_list_item_tracker_set_position (priv->item_manager,
                                          priv->selected,
                                          pos,
                                          0, 0);
    }
}

static void
gtk_list_base_dispose (GObject *object)
{
  GtkListBase *self = GTK_LIST_BASE (object);
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  gtk_list_base_clear_adjustment (self, GTK_ORIENTATION_HORIZONTAL);
  gtk_list_base_clear_adjustment (self, GTK_ORIENTATION_VERTICAL);

  if (priv->selected)
    {
      gtk_list_item_tracker_free (priv->item_manager, priv->selected);
      priv->selected = NULL;
    }
  g_clear_object (&priv->item_manager);

  G_OBJECT_CLASS (gtk_list_base_parent_class)->dispose (object);
}

static void
gtk_list_base_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkListBase *self = GTK_LIST_BASE (object);
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  switch (property_id)
    {
    case PROP_HADJUSTMENT:
      g_value_set_object (value, priv->adjustment[GTK_ORIENTATION_HORIZONTAL]);
      break;

    case PROP_HSCROLL_POLICY:
      g_value_set_enum (value, priv->scroll_policy[GTK_ORIENTATION_HORIZONTAL]);
      break;

    case PROP_ORIENTATION:
      g_value_set_enum (value, priv->orientation);
      break;

    case PROP_VADJUSTMENT:
      g_value_set_object (value, priv->adjustment[GTK_ORIENTATION_VERTICAL]);
      break;

    case PROP_VSCROLL_POLICY:
      g_value_set_enum (value, priv->scroll_policy[GTK_ORIENTATION_VERTICAL]);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_list_base_set_adjustment (GtkListBase    *self,
                              GtkOrientation  orientation,
                              GtkAdjustment  *adjustment)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  if (priv->adjustment[orientation] == adjustment)
    return;

  if (adjustment == NULL)
    adjustment = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
  g_object_ref_sink (adjustment);

  gtk_list_base_clear_adjustment (self, orientation);

  priv->adjustment[orientation] = adjustment;

  g_signal_connect (adjustment, "value-changed",
		    G_CALLBACK (gtk_list_base_adjustment_value_changed_cb),
		    self);

  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

static void
gtk_list_base_set_scroll_policy (GtkListBase         *self,
                                 GtkOrientation       orientation,
                                 GtkScrollablePolicy  scroll_policy)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  if (priv->scroll_policy[orientation] == scroll_policy)
    return;

  priv->scroll_policy[orientation] = scroll_policy;

  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self),
                            orientation == GTK_ORIENTATION_HORIZONTAL
                            ? properties[PROP_HSCROLL_POLICY]
                            : properties[PROP_VSCROLL_POLICY]);
}

static void
gtk_list_base_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkListBase *self = GTK_LIST_BASE (object);
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  switch (property_id)
    {
    case PROP_HADJUSTMENT:
      gtk_list_base_set_adjustment (self, GTK_ORIENTATION_HORIZONTAL, g_value_get_object (value));
      break;

    case PROP_HSCROLL_POLICY:
      gtk_list_base_set_scroll_policy (self, GTK_ORIENTATION_HORIZONTAL, g_value_get_enum (value));
      break;

    case PROP_ORIENTATION:
      {
        GtkOrientation orientation = g_value_get_enum (value);
        if (priv->orientation != orientation)
          {
            priv->orientation = orientation;
            _gtk_orientable_set_style_classes (GTK_ORIENTABLE (self));
            gtk_widget_queue_resize (GTK_WIDGET (self));
            g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ORIENTATION]);
          }
      }
      break;

    case PROP_VADJUSTMENT:
      gtk_list_base_set_adjustment (self, GTK_ORIENTATION_VERTICAL, g_value_get_object (value));
      break;

    case PROP_VSCROLL_POLICY:
      gtk_list_base_set_scroll_policy (self, GTK_ORIENTATION_VERTICAL, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_list_base_select_item_action (GtkWidget  *widget,
                                  const char *action_name,
                                  GVariant   *parameter)
{
  GtkListBase *self = GTK_LIST_BASE (widget);
  guint pos;
  gboolean modify, extend;

  g_variant_get (parameter, "(ubb)", &pos, &modify, &extend);

  gtk_list_base_select_item (self, pos, modify, extend);
}

static void
gtk_list_base_select_all (GtkWidget  *widget,
                          const char *action_name,
                          GVariant   *parameter)
{
  GtkListBase *self = GTK_LIST_BASE (widget);
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  GtkSelectionModel *selection_model;

  selection_model = gtk_list_item_manager_get_model (priv->item_manager);
  if (selection_model == NULL)
    return;

  gtk_selection_model_select_all (selection_model);
}

static void
gtk_list_base_unselect_all (GtkWidget  *widget,
                            const char *action_name,
                            GVariant   *parameter)
{
  GtkListBase *self = GTK_LIST_BASE (widget);
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  GtkSelectionModel *selection_model;

  selection_model = gtk_list_item_manager_get_model (priv->item_manager);
  if (selection_model == NULL)
    return;

  gtk_selection_model_unselect_all (selection_model);
}

static void
gtk_list_base_class_init (GtkListBaseClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  gpointer iface;

  gobject_class->dispose = gtk_list_base_dispose;
  gobject_class->get_property = gtk_list_base_get_property;
  gobject_class->set_property = gtk_list_base_set_property;

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
   * GtkListBase:orientation:
   *
   * The orientation of the list. See GtkOrientable:orientation
   * for details.
   */
  properties[PROP_ORIENTATION] =
    g_param_spec_enum ("orientation",
                       P_("Orientation"),
                       P_("The orientation of the orientable"),
                       GTK_TYPE_ORIENTATION,
                       GTK_ORIENTATION_VERTICAL,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);

  /**
   * GtkListBase|list.select-item:
   * @position: position of item to select
   * @modify: %TRUE to toggle the existing selection, %FALSE to select
   * @extend: %TRUE to extend the selection
   *
   * Changes selection.
   *
   * If @extend is %TRUE and the model supports selecting ranges, the
   * affected items are all items from the last selected item to the item
   * in @position.
   * If @extend is %FALSE or selecting ranges is not supported, only the
   * item in @position is affected.
   *
   * If @modify is %TRUE, the affected items will be set to the same state.
   * If @modify is %FALSE, the affected items will be selected and
   * all other items will be deselected.
   */
  gtk_widget_class_install_action (widget_class,
                                   "list.select-item",
                                   "(ubb)",
                                   gtk_list_base_select_item_action);

  /**
   * GtkListBase|list.select-all:
   *
   * If the selection model supports it, select all items in the model.
   * If not, do nothing.
   */
  gtk_widget_class_install_action (widget_class,
                                   "list.select-all",
                                   NULL,
                                   gtk_list_base_select_all);

  /**
   * GtkListBase|list.unselect-all:
   *
   * If the selection model supports it, unselect all items in the model.
   * If not, do nothing.
   */
  gtk_widget_class_install_action (widget_class,
                                   "list.unselect-all",
                                   NULL,
                                   gtk_list_base_unselect_all);

}

static void
gtk_list_base_init_real (GtkListBase      *self,
                         GtkListBaseClass *g_class)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  priv->item_manager = gtk_list_item_manager_new_for_size (GTK_WIDGET (self),
                                                           g_class->list_item_name,
                                                           g_class->list_item_size,
                                                           g_class->list_item_augment_size,
                                                           g_class->list_item_augment_func);
  priv->selected = gtk_list_item_tracker_new (priv->item_manager);

  priv->adjustment[GTK_ORIENTATION_HORIZONTAL] = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
  priv->adjustment[GTK_ORIENTATION_VERTICAL] = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

  priv->orientation = GTK_ORIENTATION_VERTICAL;

  gtk_widget_set_overflow (GTK_WIDGET (self), GTK_OVERFLOW_HIDDEN);
}

static gboolean
gtk_list_base_adjustment_is_flipped (GtkListBase    *self,
                                     GtkOrientation  orientation)
{
  if (orientation == GTK_ORIENTATION_VERTICAL)
    return FALSE;

  return gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL;
}

void
gtk_list_base_get_adjustment_values (GtkListBase    *self,
                                     GtkOrientation  orientation,
                                     int            *value,
                                     int            *size,
                                     int            *page_size)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  int val, upper, ps;

  val = gtk_adjustment_get_value (priv->adjustment[orientation]);
  upper = gtk_adjustment_get_upper (priv->adjustment[orientation]);
  ps = gtk_adjustment_get_page_size (priv->adjustment[orientation]);
  if (gtk_list_base_adjustment_is_flipped (self, orientation))
    val = upper - ps - val;

  if (value)
    *value = val;
  if (size)
    *size = upper;
  if (page_size)
    *page_size = ps;
}

int
gtk_list_base_set_adjustment_values (GtkListBase    *self,
                                     GtkOrientation  orientation,
                                     int             value,
                                     int             size,
                                     int             page_size)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  size = MAX (size, page_size);
  value = MAX (value, 0);
  value = MIN (value, size - page_size);

  g_signal_handlers_block_by_func (priv->adjustment[orientation],
                                   gtk_list_base_adjustment_value_changed_cb,
                                   self);
  gtk_adjustment_configure (priv->adjustment[orientation],
                            gtk_list_base_adjustment_is_flipped (self, orientation)
                              ? size - page_size - value
                              : value,
                            0,
                            size,
                            page_size * 0.1,
                            page_size * 0.9,
                            page_size);
  g_signal_handlers_unblock_by_func (priv->adjustment[orientation],
                                     gtk_list_base_adjustment_value_changed_cb,
                                     self);

  return value;
}

GtkScrollablePolicy
gtk_list_base_get_scroll_policy (GtkListBase    *self,
                                 GtkOrientation  orientation)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  return priv->scroll_policy[orientation];
}

GtkOrientation
gtk_list_base_get_orientation (GtkListBase *self)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  return priv->orientation;
}

GtkListItemManager *
gtk_list_base_get_manager (GtkListBase *self)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  return priv->item_manager;
}

/*
 * gtk_list_base_grab_focus_on_item:
 * @self: a #GtkListBase
 * @pos: position of the item to focus
 * @select: %TRUE to select the item
 * @modify: if selecting, %TRUE to modify the selected
 *     state, %FALSE to always select
 * @extend: if selecting, %TRUE to extend the selection,
 *     %FALSE to only operate on this item
 *
 * Tries to grab focus on the given item. If there is no item
 * at this position or grabbing focus failed, %FALSE will be
 * returned.
 *
 * Returns: %TRUE if focusing the item succeeded
 **/
gboolean
gtk_list_base_grab_focus_on_item (GtkListBase *self,
                                  guint        pos,
                                  gboolean     select,
                                  gboolean     modify,
                                  gboolean     extend)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  GtkListItemManagerItem *item;

  item = gtk_list_item_manager_get_nth (priv->item_manager, pos, NULL);
  if (item == NULL)
    return FALSE;

  if (!item->widget)
    {
      GtkListItemTracker *tracker = gtk_list_item_tracker_new (priv->item_manager);

      /* We need a tracker here to create the widget.
       * That needs to have happened or we can't grab it.
       * And we can't use a different tracker, because they manage important rows,
       * so we create a temporary one. */
      gtk_list_item_tracker_set_position (priv->item_manager, tracker, pos, 0, 0);

      item = gtk_list_item_manager_get_nth (priv->item_manager, pos, NULL);
      g_assert (item->widget);

      if (!gtk_widget_grab_focus (item->widget))
          return FALSE;

      gtk_list_item_tracker_free (priv->item_manager, tracker);
    }
  else
    {
      if (!gtk_widget_grab_focus (item->widget))
          return FALSE;
    }

  if (select)
    gtk_list_base_select_item (self, pos, modify, extend);

  return TRUE;
}

