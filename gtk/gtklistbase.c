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
#include "gtkbindings.h"
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
 * gtk_list_base_move_focus_along:
 * @self: a #GtkListBase
 * @pos: position from which to move focus
 * @steps: steps to move focus - negative numbers
 *     move focus backwards
 *
 * Moves focus @steps in the direction of the list.
 * If focus cannot be moved, @pos is returned.
 * If focus should be moved out of the widget, %GTK_INVALID_LIST_POSITION
 * is returned.
 *
 * Returns: new focus position
 **/
static guint
gtk_list_base_move_focus_along (GtkListBase *self,
                                guint        pos,
                                int          steps)
{
  return GTK_LIST_BASE_GET_CLASS (self)->move_focus_along (self, pos, steps);
}

/*
 * gtk_list_base_move_focus_across:
 * @self: a #GtkListBase
 * @pos: position from which to move focus
 * @steps: steps to move focus - negative numbers
 *     move focus backwards
 *
 * Moves focus @steps in the direction across the list.
 * If focus cannot be moved, @pos is returned.
 * If focus should be moved out of the widget, %GTK_INVALID_LIST_POSITION
 * is returned.
 *
 * Returns: new focus position
 **/
static guint
gtk_list_base_move_focus_across (GtkListBase *self,
                                 guint        pos,
                                 int          steps)
{
  return GTK_LIST_BASE_GET_CLASS (self)->move_focus_across (self, pos, steps);
}

static guint
gtk_list_base_move_focus (GtkListBase    *self,
                          guint           pos,
                          GtkOrientation  orientation,
                          int             steps)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  if (orientation == GTK_ORIENTATION_HORIZONTAL &&
      gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL)
    steps = -steps;

  if (orientation == priv->orientation)
    return gtk_list_base_move_focus_along (self, pos, steps);
  else
    return gtk_list_base_move_focus_across (self, pos, steps);
}

/*
 * gtk_list_base_get_allocation_along:
 * @self: a #GtkListBase
 * @pos: item to get the size of
 * @offset: (out caller-allocates) (allow-none) set to the offset
 *     of the top/left of the item
 * @size: (out caller-allocates) (allow-none) set to the size of
 *     the item in the direction
 *
 * Computes the allocation of the item in the direction along the sizing
 * axis.
 *
 * Returns: %TRUE if the item exists and has an allocation, %FALSE otherwise
 **/
gboolean
gtk_list_base_get_allocation_along (GtkListBase *self,
                                    guint        pos,
                                    int         *offset,
                                    int         *size)
{
  return GTK_LIST_BASE_GET_CLASS (self)->get_allocation_along (self, pos, offset, size);
}

/*
 * gtk_list_base_get_allocation_across:
 * @self: a #GtkListBase
 * @pos: item to get the size of
 * @offset: (out caller-allocates) (allow-none) set to the offset
 *     of the top/left of the item
 * @size: (out caller-allocates) (allow-none) set to the size of
 *     the item in the direction
 *
 * Computes the allocation of the item in the direction across to the sizing
 * axis.
 *
 * Returns: %TRUE if the item exists and has an allocation, %FALSE otherwise
 **/
static gboolean
gtk_list_base_get_allocation_across (GtkListBase *self,
                                     guint        pos,
                                     int         *offset,
                                     int         *size)
{
  return GTK_LIST_BASE_GET_CLASS (self)->get_allocation_across (self, pos, offset, size);
}

/*
 * gtk_list_base_get_position_from_allocation:
 * @self: a #GtkListBase
 * @across: position in pixels in the direction cross to the list
 * @along:  position in pixels in the direction of the list
 * @pos: (out caller-allocates): set to the looked up position
 * @area: (out caller-allocates) (allow-none): set to the area occupied
 *     by the returned position.
 *
 * Given a coordinate in list coordinates, determine the position of the
 * item that occupies that position.
 *
 * It is possible for @area to not include the point given by (across, along).
 * This will happen for example in the last row of a gridview, where the
 * last item will be returned for the whole width, even if there are empty
 * cells.
 *
 * Returns: %TRUE on success or %FALSE if no position occupies the given offset.
 **/
static guint
gtk_list_base_get_position_from_allocation (GtkListBase           *self,
                                            int                    across,
                                            int                    along,
                                            guint                 *pos,
                                            cairo_rectangle_int_t *area)
{
  return GTK_LIST_BASE_GET_CLASS (self)->get_position_from_allocation (self, across, along, pos, area);
}

/*
 * gtk_list_base_select_item:
 * @self: a #GtkListBase
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
  gboolean success = FALSE;
  guint n_items;

  model = gtk_list_item_manager_get_model (priv->item_manager);
  if (model == NULL)
    return;

  n_items = g_list_model_get_n_items (G_LIST_MODEL (model));
  if (pos >= n_items)
    return;

  if (extend)
    {
      guint extend_pos = gtk_list_item_tracker_get_position (priv->item_manager, priv->selected);

      if (extend_pos < n_items)
        {
          guint max = MAX (extend_pos, pos);
          guint min = MIN (extend_pos, pos);

          if (modify)
            {
              if (gtk_selection_model_is_selected (model, extend_pos))
                {
                  success = gtk_selection_model_select_range (model,
                                                              min,
                                                              max - min + 1,
                                                              FALSE);
                }
              else
                {
                  success = gtk_selection_model_unselect_range (model,
                                                                min,
                                                                max - min + 1);
                }
            }
          else
            {
              success = gtk_selection_model_select_range (model,
                                                          min,
                                                          max - min + 1,
                                                          TRUE);
            }
        }
      /* If there's no range to select or selecting ranges isn't supported
       * by the model, fall through to normal setting.
       */
    }
  if (success)
    return;

  if (modify)
    {
      if (gtk_selection_model_is_selected (model, pos))
        success = gtk_selection_model_unselect_item (model, pos);
      else
        success = gtk_selection_model_select_item (model, pos, FALSE);
    }
  else
    {
      success = gtk_selection_model_select_item (model, pos, TRUE);
    }

  gtk_list_item_tracker_set_position (priv->item_manager,
                                      priv->selected,
                                      pos,
                                      0, 0);
}

static guint
gtk_list_base_get_n_items (GtkListBase *self)
{
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  GtkSelectionModel *model;

  model = gtk_list_item_manager_get_model (priv->item_manager);
  if (model == NULL)
    return 0;

  return g_list_model_get_n_items (G_LIST_MODEL (model));
}

guint
gtk_list_base_get_focus_position (GtkListBase *self)
{
#if 0
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);

  return gtk_list_item_tracker_get_position (priv->item_manager, priv->focus);
#else
  GtkWidget *focus_child = gtk_widget_get_focus_child (GTK_WIDGET (self));
  if (focus_child)
    return gtk_list_item_get_position (GTK_LIST_ITEM (focus_child));
  else
    return GTK_INVALID_LIST_POSITION;
#endif
}

static gboolean
gtk_list_base_focus (GtkWidget        *widget,
                     GtkDirectionType  direction)
{
  GtkListBase *self = GTK_LIST_BASE (widget);
  guint old, pos, n_items;

  pos = gtk_list_base_get_focus_position (self);
  n_items = gtk_list_base_get_n_items (self);
  old = pos;

  if (pos >= n_items)
    {
      if (n_items == 0)
        return FALSE;

      pos = 0;
    }
  else if (gtk_widget_get_focus_child (widget) == NULL)
    {
      /* Focus was outside the list, just grab the old focus item
       * while keeping the selection intact.
       */
      return gtk_list_base_grab_focus_on_item (GTK_LIST_BASE (self), pos, FALSE, FALSE, FALSE);
    }
  else
    {
      switch (direction)
        {
        case GTK_DIR_TAB_FORWARD:
          pos++;
          if (pos >= n_items)
            return FALSE;
          break;

        case GTK_DIR_TAB_BACKWARD:
          if (pos == 0)
            return FALSE;
          pos--;
          break;

        case GTK_DIR_UP:
          pos = gtk_list_base_move_focus (self, pos, GTK_ORIENTATION_VERTICAL, -1);
          break;

        case GTK_DIR_DOWN:
          pos = gtk_list_base_move_focus (self, pos, GTK_ORIENTATION_VERTICAL, 1);
          break;

        case GTK_DIR_LEFT:
          pos = gtk_list_base_move_focus (self, pos, GTK_ORIENTATION_HORIZONTAL, -1);
          break;

        case GTK_DIR_RIGHT:
          pos = gtk_list_base_move_focus (self, pos, GTK_ORIENTATION_HORIZONTAL, 1);
          break;

        default:
          g_assert_not_reached ();
          return TRUE;
        }
    }

  if (old != pos)
    {
      return gtk_list_base_grab_focus_on_item (GTK_LIST_BASE (self), pos, TRUE, FALSE, FALSE);
    }
  else
    {
      return TRUE;
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
gtk_list_base_move_cursor_to_start (GtkWidget *widget,
                                    GVariant  *args,
                                    gpointer   unused)
{
  GtkListBase *self = GTK_LIST_BASE (widget);
  gboolean select, modify, extend;

  if (gtk_list_base_get_n_items (self) == 0)
    return;

  g_variant_get (args, "(bbb)", &select, &modify, &extend);

  gtk_list_base_grab_focus_on_item (GTK_LIST_BASE (self), 0, select, modify, extend);
}

#if 0
static void
gtk_grid_view_compute_scroll_align (GtkGridView   *self,
                                    GtkOrientation orientation,
                                    int            cell_start,
                                    int            cell_end,
                                    double         current_align,
                                    gboolean       current_start,
                                    double        *new_align,
                                    gboolean      *new_start)
{
  int visible_start, visible_size, visible_end;
  int cell_size;

  gtk_list_base_get_adjustment_values (GTK_LIST_BASE (self),
                                       orientation,
                                       &visible_start, NULL, &visible_size);
  visible_end = visible_start + visible_size;
  cell_size = cell_end - cell_start;

  if (cell_size <= visible_size)
    {
      if (cell_start < visible_start)
        {
          *new_align = 0.0;
          *new_start = TRUE;
        }
      else if (cell_end > visible_end)
        {
          *new_align = 1.0;
          *new_start = FALSE;
        }
      else
        {
          /* XXX: start or end here? */
          *new_start = TRUE;
          *new_align = (double) (cell_start - visible_start) / visible_size;
        }
    }
  else
    {
      /* This is the unlikely case of the cell being higher than the visible area */
      if (cell_start > visible_start)
        {
          *new_align = 0.0;
          *new_start = TRUE;
        }
      else if (cell_end < visible_end)
        {
          *new_align = 1.0;
          *new_start = FALSE;
        }
      else
        {
          /* the cell already covers the whole screen */
          *new_align = current_align;
          *new_start = current_start;
        }
    }
}

static void
gtk_grid_view_update_focus_tracker (GtkGridView *self)
{
  GtkWidget *focus_child;
  guint pos;

  focus_child = gtk_widget_get_focus_child (GTK_WIDGET (self));
  if (!GTK_IS_LIST_ITEM (focus_child))
    return;

  pos = gtk_list_item_get_position (GTK_LIST_ITEM (focus_child));
  if (pos != gtk_list_item_tracker_get_position (self->item_manager, self->focus))
    {
      gtk_list_item_tracker_set_position (self->item_manager,
                                          self->focus,
                                          pos,
                                          self->max_columns,
                                          self->max_columns);
    }
}

static void
gtk_grid_view_scroll_to_item (GtkWidget  *widget,
                              const char *action_name,
                              GVariant   *parameter)
{
  GtkGridView *self = GTK_GRID_VIEW (widget);
  int start, end;
  double xalign, yalign;
  gboolean xstart, ystart;
  guint pos;

  if (!g_variant_check_format_string (parameter, "u", FALSE))
    return;

  g_variant_get (parameter, "u", &pos);

  /* figure out primary orientation and if position is valid */
  if (!gtk_grid_view_get_size_at_position (self, pos, &start, &end))
    return;

  end += start;
  gtk_grid_view_compute_scroll_align (self,
                                      gtk_list_base_get_orientation (GTK_LIST_BASE (self)),
                                      start, end,
                                      self->anchor_yalign, self->anchor_ystart,
                                      &yalign, &ystart);

  /* now do the same thing with the other orientation */
  start = floor (self->column_width * (pos % self->n_columns));
  end = floor (self->column_width * ((pos % self->n_columns) + 1));
  gtk_grid_view_compute_scroll_align (self,
                                      gtk_list_base_get_opposite_orientation (GTK_LIST_BASE (self)),
                                      start, end,
                                      self->anchor_xalign, self->anchor_xstart,
                                      &xalign, &xstart);

  gtk_grid_view_set_anchor (self, pos, xalign, xstart, yalign, ystart);

  /* HACK HACK HACK
   *
   * GTK has no way to track the focused child. But we now that when a listitem
   * gets focus, it calls this action. So we update our focus tracker from here
   * because it's the closest we can get to accurate tracking.
   */
  gtk_grid_view_update_focus_tracker (self);
}
#endif

static void
gtk_list_base_move_cursor_page_up (GtkWidget *widget,
                                   GVariant  *args,
                                   gpointer   unused)
{
  GtkListBase *self = GTK_LIST_BASE (widget);
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  gboolean select, modify, extend;
  cairo_rectangle_int_t area, new_area;
  int page_size;
  guint pos, new_pos;

  pos = gtk_list_base_get_focus_position (self);
  page_size = gtk_adjustment_get_page_size (priv->adjustment[priv->orientation]);
  if (!gtk_list_base_get_allocation_along (self, pos, &area.y, &area.height) ||
      !gtk_list_base_get_allocation_across (self, pos, &area.x, &area.width))
    return;
  if (!gtk_list_base_get_position_from_allocation (self,
                                                   area.x + area.width / 2,
                                                   MAX (0, area.y + area.height - page_size),
                                                   &new_pos,
                                                   &new_area))
    return;

  /* We want the whole row to be visible */
  if (new_area.y < MAX (0, area.y + area.height - page_size))
    new_pos = gtk_list_base_move_focus_along (self, new_pos, 1);
  /* But we definitely want to move if we can */
  if (new_pos >= pos)
    {
      new_pos = gtk_list_base_move_focus_along (self, new_pos, -1);
      if (new_pos == pos)
        return;
    }

  g_variant_get (args, "(bbb)", &select, &modify, &extend);

  gtk_list_base_grab_focus_on_item (GTK_LIST_BASE (self), new_pos, select, modify, extend);
}

static void
gtk_list_base_move_cursor_page_down (GtkWidget *widget,
                                     GVariant  *args,
                                     gpointer   unused)
{
  GtkListBase *self = GTK_LIST_BASE (widget);
  GtkListBasePrivate *priv = gtk_list_base_get_instance_private (self);
  gboolean select, modify, extend;
  cairo_rectangle_int_t area, new_area;
  int page_size, end;
  guint pos, new_pos;

  pos = gtk_list_base_get_focus_position (self);
  page_size = gtk_adjustment_get_page_size (priv->adjustment[priv->orientation]);
  end = gtk_adjustment_get_upper (priv->adjustment[priv->orientation]);
  if (end == 0)
    return;

  if (!gtk_list_base_get_allocation_along (self, pos, &area.y, &area.height) ||
      !gtk_list_base_get_allocation_across (self, pos, &area.x, &area.width))
    return;

  if (!gtk_list_base_get_position_from_allocation (self,
                                                   area.x + area.width / 2,
                                                   MIN (end, area.y + page_size) - 1,
                                                   &new_pos,
                                                   &new_area))
    return;

  /* We want the whole row to be visible */
  if (new_area.y + new_area.height > MIN (end, area.y + page_size))
    new_pos = gtk_list_base_move_focus_along (self, new_pos, -1);
  /* But we definitely want to move if we can */
  if (new_pos <= pos)
    {
      new_pos = gtk_list_base_move_focus_along (self, new_pos, 1);
      if (new_pos == pos)
        return;
    }

  g_variant_get (args, "(bbb)", &select, &modify, &extend);

  gtk_list_base_grab_focus_on_item (GTK_LIST_BASE (self), new_pos, select, modify, extend);
}

static void
gtk_list_base_move_cursor_to_end (GtkWidget *widget,
                                  GVariant  *args,
                                  gpointer   unused)
{
  GtkListBase *self = GTK_LIST_BASE (widget);
  gboolean select, modify, extend;
  guint n_items;

  n_items = gtk_list_base_get_n_items (self);
  if (n_items == 0)
    return;

  g_variant_get (args, "(bbb)", &select, &modify, &extend);

  gtk_list_base_grab_focus_on_item (GTK_LIST_BASE (self), n_items - 1, select, modify, extend);
}

static void
gtk_list_base_move_cursor (GtkWidget *widget,
                           GVariant  *args,
                           gpointer   unused)
{
  GtkListBase *self = GTK_LIST_BASE (widget);
  int amount;
  guint orientation;
  guint pos;
  gboolean select, modify, extend;

  g_variant_get (args, "(ubbbi)", &orientation, &select, &modify, &extend, &amount);
 
  pos = gtk_list_base_get_focus_position (self);
  pos = gtk_list_base_move_focus (self, pos, orientation, amount);

  gtk_list_base_grab_focus_on_item (GTK_LIST_BASE (self), pos, select, modify, extend);
}

static void
gtk_list_base_add_move_binding (GtkBindingSet  *binding_set,
                                guint           keyval,
                                GtkOrientation  orientation,
                                int             amount)
{
  gtk_binding_entry_add_callback (binding_set,
                                  keyval,
                                  GDK_CONTROL_MASK,
                                  gtk_list_base_move_cursor,
                                  g_variant_new ("(ubbbi)", orientation, FALSE, FALSE, FALSE, amount),
                                  NULL, NULL);
  gtk_binding_entry_add_callback (binding_set,
                                  keyval,
                                  GDK_SHIFT_MASK,
                                  gtk_list_base_move_cursor,
                                  g_variant_new ("(ubbbi)", orientation, TRUE, FALSE, TRUE, amount),
                                  NULL, NULL);
  gtk_binding_entry_add_callback (binding_set,
                                  keyval,
                                  GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                  gtk_list_base_move_cursor,
                                  g_variant_new ("(ubbbi)", orientation, TRUE, TRUE, TRUE, amount),
                                  NULL, NULL);
}

static void
gtk_list_base_add_custom_move_binding (GtkBindingSet      *binding_set,
                                       guint               keyval,
                                       GtkBindingCallback  callback)
{
  gtk_binding_entry_add_callback (binding_set,
                                  keyval,
                                  0,
                                  callback,
                                  g_variant_new ("(bbb)", TRUE, FALSE, FALSE),
                                  NULL, NULL);
  gtk_binding_entry_add_callback (binding_set,
                                  keyval,
                                  GDK_CONTROL_MASK,
                                  callback,
                                  g_variant_new ("(bbb)", FALSE, FALSE, FALSE),
                                  NULL, NULL);
  gtk_binding_entry_add_callback (binding_set,
                                  keyval,
                                  GDK_SHIFT_MASK,
                                  callback,
                                  g_variant_new ("(bbb)", TRUE, FALSE, TRUE),
                                  NULL, NULL);
  gtk_binding_entry_add_callback (binding_set,
                                  keyval,
                                  GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                  callback,
                                  g_variant_new ("(bbb)", TRUE, TRUE, TRUE),
                                  NULL, NULL);
}

static void
gtk_list_base_class_init (GtkListBaseClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkBindingSet *binding_set;
  gpointer iface;

  widget_class->focus = gtk_list_base_focus;

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

  binding_set = gtk_binding_set_by_class (klass);

  gtk_list_base_add_move_binding (binding_set, GDK_KEY_Up, GTK_ORIENTATION_VERTICAL, -1);
  gtk_list_base_add_move_binding (binding_set, GDK_KEY_KP_Up, GTK_ORIENTATION_VERTICAL, -1);
  gtk_list_base_add_move_binding (binding_set, GDK_KEY_Down, GTK_ORIENTATION_VERTICAL, 1);
  gtk_list_base_add_move_binding (binding_set, GDK_KEY_KP_Down, GTK_ORIENTATION_VERTICAL, 1);
  gtk_list_base_add_move_binding (binding_set, GDK_KEY_Left, GTK_ORIENTATION_HORIZONTAL, -1);
  gtk_list_base_add_move_binding (binding_set, GDK_KEY_KP_Left, GTK_ORIENTATION_HORIZONTAL, -1);
  gtk_list_base_add_move_binding (binding_set, GDK_KEY_Right, GTK_ORIENTATION_HORIZONTAL, 1);
  gtk_list_base_add_move_binding (binding_set, GDK_KEY_KP_Right, GTK_ORIENTATION_HORIZONTAL, 1);

  gtk_list_base_add_custom_move_binding (binding_set, GDK_KEY_Home, gtk_list_base_move_cursor_to_start);
  gtk_list_base_add_custom_move_binding (binding_set, GDK_KEY_KP_Home, gtk_list_base_move_cursor_to_start);
  gtk_list_base_add_custom_move_binding (binding_set, GDK_KEY_End, gtk_list_base_move_cursor_to_end);
  gtk_list_base_add_custom_move_binding (binding_set, GDK_KEY_KP_End, gtk_list_base_move_cursor_to_end);
  gtk_list_base_add_custom_move_binding (binding_set, GDK_KEY_Page_Up, gtk_list_base_move_cursor_page_up);
  gtk_list_base_add_custom_move_binding (binding_set, GDK_KEY_KP_Page_Up, gtk_list_base_move_cursor_page_up);
  gtk_list_base_add_custom_move_binding (binding_set, GDK_KEY_Page_Down, gtk_list_base_move_cursor_page_down);
  gtk_list_base_add_custom_move_binding (binding_set, GDK_KEY_KP_Page_Down, gtk_list_base_move_cursor_page_down);

  gtk_binding_entry_add_action (binding_set, GDK_KEY_a, GDK_CONTROL_MASK, "list.select-all", NULL);
  gtk_binding_entry_add_action (binding_set, GDK_KEY_slash, GDK_CONTROL_MASK, "list.select-all", NULL);
  gtk_binding_entry_add_action (binding_set, GDK_KEY_A, GDK_CONTROL_MASK | GDK_SHIFT_MASK, "list.unselect-all", NULL);
  gtk_binding_entry_add_action (binding_set, GDK_KEY_backslash, GDK_CONTROL_MASK, "list.unselect-all", NULL);
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

