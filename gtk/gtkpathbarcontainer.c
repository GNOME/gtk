/*
 * Copyright (C) 2016 Carlos Soriano <csoriano@gnome.org>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include "glib.h"

#include "gtkpathbarcontainer.h"
#include "gtkwidgetprivate.h"
#include "gtkintl.h"
#include "gtksizerequest.h"
#include "gtkbuildable.h"
#include "gtkrevealer.h"
#include "gtkbox.h"

//TODO remove
#include "gtkbutton.h"

#define REVEALER_ANIMATION_TIME 250 //ms
#define INVERT_ANIMATION_SPEED 1.2 //px/ms
#define INVERT_ANIMATION_MAX_TIME 10000 //ms

struct _GtkPathBarContainerPrivate
{
  GList *children;
  gint inverted :1;
  GList *children_to_hide;
  GList *children_to_show;
  GList *children_to_remove;

  gboolean invert_animation;

  GdkWindow *bin_window;
  GdkWindow *view_window;

  GtkWidget *children_box;

  guint invert_animation_tick_id;
  float invert_animation_progress;
  guint64 invert_animation_initial_time;
  gint invert_animation_initial_width;
  gboolean allocated;

  guint parent_available_width;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkPathBarContainer, gtk_path_bar_container, GTK_TYPE_BIN)

enum {
  PROP_0,
  PROP_INVERTED,
  PROP_CHILDREN_SHOWN,
  LAST_PROP
};

enum {
  INVERT_ANIMATION_DONE,
  LAST_SIGNAL
};

static guint path_bar_container_signals[LAST_SIGNAL] = { 0 };
static GParamSpec *path_bar_container_properties[LAST_PROP] = { NULL, };

static void
gtk_path_bar_container_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkPathBarContainer *children_box = GTK_PATH_BAR_CONTAINER (object);

  switch (prop_id)
    {
    case PROP_INVERTED:
      gtk_path_bar_container_set_inverted (children_box, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_path_bar_container_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GtkPathBarContainer *children_box = GTK_PATH_BAR_CONTAINER (object);
  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (children_box);

  switch (prop_id)
    {
    case PROP_INVERTED:
      g_value_set_boolean (value, priv->inverted);
      break;
    case PROP_CHILDREN_SHOWN:
      g_value_set_pointer (value, priv->children_to_show);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

void
gtk_path_bar_container_add (GtkPathBarContainer *self,
                           GtkWidget           *widget)
{
  GtkPathBarContainer *children_box = GTK_PATH_BAR_CONTAINER (self);
  GtkWidget *revealer;
  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (children_box);
  GtkStyleContext *style_context;

  revealer = gtk_revealer_new ();
  style_context = gtk_widget_get_style_context (revealer);
  gtk_revealer_set_transition_type (GTK_REVEALER (revealer),
                                    GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT);
  gtk_container_add (GTK_CONTAINER (revealer), widget);
  gtk_container_add (GTK_CONTAINER (priv->children_box), revealer);
  gtk_revealer_set_transition_duration (GTK_REVEALER (revealer),
                                        REVEALER_ANIMATION_TIME);
  priv->children = g_list_append (priv->children, widget);
  gtk_widget_show_all (revealer);

  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

static void
really_remove_child (GtkPathBarContainer *self,
                     GtkWidget           *widget)
{
  GList *child;
  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (self);

  for (child = priv->children_to_remove; child != NULL; child = child->next)
    {
      GtkWidget *revealer;

      revealer = gtk_widget_get_parent (child->data);
      if (child->data == widget && !gtk_revealer_get_child_revealed (GTK_REVEALER (revealer)))
        {
          gboolean was_visible = gtk_widget_get_visible (widget);

          priv->children_to_remove = g_list_remove (priv->children_to_remove,
                                                   child->data);
          gtk_container_remove (GTK_CONTAINER (priv->children_box), revealer);

          if (was_visible)
            gtk_widget_queue_resize (GTK_WIDGET (self));

          break;
        }
    }
}

static void
unrevealed_really_remove_child (GObject    *widget,
                                GParamSpec *pspec,
                                gpointer    user_data)
{
  GtkPathBarContainer *self = GTK_PATH_BAR_CONTAINER (user_data);

  g_signal_handlers_disconnect_by_func (widget, unrevealed_really_remove_child, self);
  really_remove_child (self, gtk_bin_get_child (GTK_BIN (widget)));
}

void
gtk_path_bar_container_remove (GtkPathBarContainer *self,
                               GtkWidget    *widget)
{
  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (self);
  GtkWidget *to_remove;

  if (GTK_IS_REVEALER (widget) && gtk_widget_get_parent (widget) == priv->children_box)
    to_remove = gtk_bin_get_child (GTK_BIN (widget));
  else
    to_remove = widget;

  priv->children_to_remove = g_list_append (priv->children_to_remove, to_remove);
  priv->children = g_list_remove (priv->children, to_remove);

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
get_children_preferred_size_for_requisition (GtkPathBarContainer *self,
                                             GtkRequisition      *available_size,
                                             gboolean             inverted,
                                             GtkRequisition      *minimum_size,
                                             GtkRequisition      *natural_size,
                                             GtkRequisition      *distributed_size)
{
  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (self);
  GtkWidget *child_widget;
  GList *child;
  GtkRequestedSize child_width;
  GtkRequestedSize child_height;
  GtkRequestedSize revealer_width;
  GtkRequestedSize *sizes;
  GtkWidget *revealer;
  gint i;
  gint n_children = 0;
  GList *children;
  gint current_children_min_width = 0;
  gint current_child_min_width = 0;
  gint current_children_nat_width = 0;
  gint current_child_nat_width = 0;
  gint current_children_min_height = 0;
  gint current_children_nat_height = 0;
  gint full_children_current_width = 0;

  children = g_list_copy (priv->children);

  sizes = g_new(GtkRequestedSize, g_list_length (children));

  if (inverted)
    children = g_list_reverse (children);

  /* Retrieve desired size for visible children. */
  for (i = 0, child = children; child != NULL; i++, child = child->next)
    {
      child_widget = GTK_WIDGET (child->data);
      revealer = gtk_widget_get_parent (child_widget);

      gtk_widget_get_preferred_width_for_height (child_widget,
                                                 available_size->height,
                                                 &child_width.minimum_size,
                                                 &child_width.natural_size);

      gtk_widget_get_preferred_height_for_width (child_widget,
                                                 current_children_nat_width,
                                                 &child_height.minimum_size,
                                                 &child_height.natural_size);

      gtk_widget_get_preferred_width_for_height (revealer,
                                                 available_size->height,
                                                 &revealer_width.minimum_size,
                                                 &revealer_width.natural_size);

      /* Minimum size is always the first child whole size */
      if (i > 0)
        {
          current_child_min_width = revealer_width.minimum_size;
          current_child_nat_width = revealer_width.natural_size;
        }
      else
        {
          current_child_min_width = child_width.minimum_size;
          current_child_nat_width = child_width.natural_size;
        }

      full_children_current_width += current_child_min_width;
      if (full_children_current_width > available_size->width && priv->invert_animation)
        break;

      current_children_min_height = MAX (current_children_min_height, child_height.minimum_size);
      current_children_nat_height = MAX (current_children_nat_height, child_height.natural_size);
      current_children_min_width += current_child_min_width;
      current_children_nat_width += current_child_nat_width;

      sizes[i].minimum_size = current_child_min_width;
      sizes[i].natural_size = current_child_nat_width;

      n_children++;
    }

  g_print ("available size %d %d\n", available_size->width, n_children);
  gtk_distribute_natural_allocation (MAX (0, available_size->width - current_children_min_width),
                                     n_children, sizes);

  minimum_size->width = current_children_min_width;
  minimum_size->height = current_children_min_height;

  natural_size->width = current_children_nat_width;
  natural_size->height = current_children_nat_height;

  current_children_min_width = 0;
  for (i = 0; i < n_children; i++)
    current_children_min_width += sizes[i].minimum_size;

  distributed_size->width = current_children_min_width;
  distributed_size->height = MIN (available_size->height, natural_size->height);


  g_list_free (children);
}

static void
update_children_visibility (GtkPathBarContainer *self)
{
  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (self);
  GtkWidget *child_widget;
  GList *child;
  GList *child_to_compare;
  GList *children_to_show = NULL;
  GList *children_to_hide = NULL;
  gboolean children_changed = FALSE;
  GtkRequestedSize *sizes_temp;
  GtkRequisition available_size;
  GtkAllocation allocation;
  gint i;
  GList *children;
  gboolean allocate_more_children = TRUE;
  gint current_children_width = 0;

  g_list_free (priv->children_to_show);
  priv->children_to_show = NULL;
  g_list_free (priv->children_to_hide);
  priv->children_to_hide = NULL;
  children = g_list_copy (priv->children);
  sizes_temp = g_newa (GtkRequestedSize, g_list_length (priv->children));

  gtk_widget_get_allocation (GTK_WIDGET (self), &allocation);
  available_size.width = priv->parent_available_width;
  available_size.height = allocation.height;

  if (priv->inverted)
    children = g_list_reverse (children);

  /* Retrieve desired size for visible children. */
  for (i = 0, child = children; child != NULL; i++, child = child->next)
    {
      child_widget = GTK_WIDGET (child->data);

      gtk_widget_get_preferred_width_for_height (child_widget,
                                                 available_size.height,
                                                 &sizes_temp[i].minimum_size,
                                                 &sizes_temp[i].natural_size);

      current_children_width += sizes_temp[i].minimum_size;

      if (!allocate_more_children || current_children_width > available_size.width)
        {
          allocate_more_children = FALSE;
          if (gtk_revealer_get_child_revealed (GTK_REVEALER (gtk_widget_get_parent (child_widget))))
            children_to_hide = g_list_prepend (children_to_hide, child_widget);

          continue;
        }

      if (!g_list_find (priv->children_to_remove, child_widget))
        children_to_show = g_list_prepend (children_to_show, child_widget);
    }

  for (child = children_to_show, child_to_compare = priv->children_to_show;
       child != NULL && child_to_compare != NULL;
       child = child->next, child_to_compare = child_to_compare->next)
    {
      if (child->data != child_to_compare)
        {
          children_changed = TRUE;
          break;
        }
    }

  children_changed = children_changed || child != child_to_compare;

  for (child = children_to_hide, child_to_compare = priv->children_to_hide;
       child != NULL && child_to_compare != NULL;
       child = child->next, child_to_compare = child_to_compare->next)
    {
      if (child->data != child_to_compare)
        {
          children_changed = TRUE;
          break;
        }
    }

  children_changed = children_changed || child != child_to_compare;
  children_changed = TRUE;

  if (children_changed)
    {
      g_list_free (priv->children_to_show);
      priv->children_to_show = g_list_copy (children_to_show);
      g_list_free (priv->children_to_hide);
      priv->children_to_hide = g_list_copy (children_to_hide);
      if (!priv->inverted)
        {
          priv->children_to_show = g_list_reverse (priv->children_to_show);
          priv->children_to_hide = g_list_reverse (priv->children_to_hide);
        }
      g_object_notify (G_OBJECT (self), "children-shown");
    }

  g_list_free (children);
  g_list_free (children_to_hide);
  g_list_free (children_to_show);
}

static void
add_opacity_class (GtkWidget   *widget,
                   const gchar *class_name)
{
  GtkStyleContext *style_context;

  style_context = gtk_widget_get_style_context (widget);

  gtk_style_context_add_class (style_context, class_name);
}

static void
remove_opacity_classes (GtkWidget *widget)
{
  GtkStyleContext *style_context;

  style_context = gtk_widget_get_style_context (widget);

  gtk_style_context_remove_class (style_context, "pathbar-initial-opacity");
  gtk_style_context_remove_class (style_context, "pathbar-opacity-on");
  gtk_style_context_remove_class (style_context, "pathbar-opacity-off");
}

static void
revealer_on_show_completed (GObject    *widget,
                            GParamSpec *pspec,
                            gpointer    user_data)
{
  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (GTK_PATH_BAR_CONTAINER (user_data));

  remove_opacity_classes (GTK_WIDGET (widget));
  g_signal_handlers_disconnect_by_func (widget, revealer_on_show_completed, user_data);
  priv->children_to_show = g_list_remove (priv->children_to_show,
                                         gtk_bin_get_child (GTK_BIN (widget)));
}

static void
revealer_on_hide_completed (GObject    *widget,
                            GParamSpec *pspec,
                            gpointer    user_data)
{
  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (GTK_PATH_BAR_CONTAINER (user_data));

  remove_opacity_classes (GTK_WIDGET (widget));
  g_signal_handlers_disconnect_by_func (widget, revealer_on_hide_completed,
                                        user_data);
  priv->children_to_hide = g_list_remove (priv->children_to_hide,
                                         gtk_bin_get_child (GTK_BIN (widget)));
}

static void
idle_update_revealers (GtkPathBarContainer *self)
{
  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (self);
  GList *l;

  /* The invert animation is handled in a tick callback, do nothing here */
  if (priv->invert_animation)
    return;

  for (l = priv->children_to_hide; l != NULL; l = l->next)
    {
      GtkWidget *revealer;

      revealer = gtk_widget_get_parent (l->data);
      if (gtk_revealer_get_child_revealed (GTK_REVEALER (revealer)) &&
          gtk_revealer_get_reveal_child (GTK_REVEALER (revealer)))
        {
          g_signal_handlers_disconnect_by_func (revealer, revealer_on_hide_completed, self);
          g_signal_connect (revealer, "notify::child-revealed", (GCallback) revealer_on_hide_completed, self);

          remove_opacity_classes (revealer);
          //add_opacity_class (revealer, "pathbar-opacity-off");

          gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), FALSE);
        }
    }

  for (l = priv->children_to_remove; l != NULL; l = l->next)
    {
      GtkWidget *revealer;

      revealer = gtk_widget_get_parent (l->data);
      if (gtk_revealer_get_child_revealed (GTK_REVEALER (revealer)))
        {
          g_signal_handlers_disconnect_by_func (revealer, revealer_on_hide_completed, self);
          g_signal_connect (revealer, "notify::child-revealed",
                            (GCallback) unrevealed_really_remove_child, self);

          remove_opacity_classes (revealer);
          //add_opacity_class (revealer, "pathbar-opacity-off");

          gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), FALSE);
        }
      else
        {
          really_remove_child (self, l->data);
        }
    }

  /* We want to defer to show revealers until the animation of those that needs
   * to be hidden or removed are done
   */
  if (priv->children_to_remove || priv->children_to_hide)
    return;

  for (l = priv->children_to_show; l != NULL; l = l->next)
    {
      GtkWidget *revealer;

      revealer = gtk_widget_get_parent (l->data);
      if (!gtk_revealer_get_reveal_child (GTK_REVEALER (revealer)))
        {
          g_signal_handlers_disconnect_by_func (revealer, revealer_on_show_completed, self);
          g_signal_connect (revealer, "notify::child-revealed", (GCallback) revealer_on_show_completed, self);

          remove_opacity_classes (revealer);
          add_opacity_class (revealer, "pathbar-opacity-on");

          gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), TRUE);
        }
    }

}

static gint
get_max_scroll (GtkPathBarContainer *self)
{
  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (self);
  GtkRequisition children_used_min_size;
  GtkRequisition children_used_nat_size;
  GtkRequisition children_distributed_size;
  GtkAllocation allocation;
  GtkRequisition available_size;
  gint children_width;
  gdouble max_scroll;


  gtk_widget_get_allocation (GTK_WIDGET (self), &allocation);
  available_size.width = priv->parent_available_width;
  available_size.height = allocation.height;
  get_children_preferred_size_for_requisition (self, &available_size,
                                               priv->inverted,
                                               &children_used_min_size,
                                               &children_used_nat_size,
                                               &children_distributed_size);

  children_width = gtk_widget_get_allocated_width (priv->children_box);

  if (priv->invert_animation)
    {
      if (priv->inverted)
        {
          g_print ("max scroll %d %d %d\n", available_size.width, children_width, children_distributed_size.width);
          max_scroll = MAX (0, children_width - children_distributed_size.width);
        }
      else
        {
          g_print ("max scroll non inverted %d %d %d\n", available_size.width, children_width, children_distributed_size.width);
          max_scroll = MAX (0, children_width - children_distributed_size.width);
        }
    }
  else
    {
      max_scroll = 0;
    }

  return max_scroll;
}

static void
update_scrolling (GtkPathBarContainer *self)
{
  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (self);
  GtkAllocation allocation;
  GtkAllocation child_allocation;
  gint scroll_value;
  gint max_scroll;

  gtk_widget_get_allocation (GTK_WIDGET (self), &allocation);
  gtk_widget_get_allocation (priv->children_box, &child_allocation);
  max_scroll = get_max_scroll (self);

  if (gtk_widget_get_realized (GTK_WIDGET (self)))
    {
      if (priv->invert_animation)
        {
          /* We only move the window to the left of the allocation, so negative values */
          if (priv->inverted)
            scroll_value = - priv->invert_animation_progress * max_scroll;
          else
            scroll_value = - (1 - priv->invert_animation_progress) * max_scroll;
        }
      else
        {
          scroll_value = 0;
        }

      gdk_window_move_resize (priv->bin_window,
                              scroll_value, 0,
                              child_allocation.width, child_allocation.height);
    }
}

static void
gtk_path_bar_container_size_allocate (GtkWidget     *widget,
                                      GtkAllocation *allocation)
{
  GtkPathBarContainer *self = GTK_PATH_BAR_CONTAINER (widget);
  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (self);
  GtkAllocation child_allocation;
  GtkRequisition minimum_size;
  GtkRequisition natural_size;

  gtk_widget_set_allocation (widget, allocation);

  idle_update_revealers (self);

  gtk_widget_get_preferred_size (priv->children_box, &minimum_size, &natural_size);
  child_allocation.x = 0;
  child_allocation.y = 0;
  child_allocation.width = MAX (minimum_size.width,
                                MIN (allocation->width, natural_size.width));
  child_allocation.height = MAX (minimum_size.height,
                                 MIN (allocation->height, natural_size.height));
  gtk_widget_size_allocate (priv->children_box, &child_allocation);

  update_scrolling (self);

  if (gtk_widget_get_realized (widget))
    {
      gdk_window_move_resize (priv->view_window,
                              allocation->x, allocation->y,
                              allocation->width, allocation->height);
      gdk_window_show (priv->view_window);
    }

  priv->allocated = TRUE;
}

gint
gtk_path_bar_container_get_unused_width (GtkPathBarContainer *self)
{
  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (self);
  GtkAllocation allocation;
  GtkAllocation child_allocation;

  gtk_widget_get_allocation (priv->children_box, &child_allocation);
  gtk_widget_get_allocation (GTK_WIDGET (self), &allocation);

  return allocation.width - child_allocation.width;
}


static void
finish_invert_animation (GtkPathBarContainer *self)
{
  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (self);
  GtkAllocation allocation;
  GtkRequisition available_size;
  GList *children_to_hide_copy;
  GList *l;

  /* Hide the revealers that need to be hidden now. */
  update_children_visibility (self);

  children_to_hide_copy = g_list_copy (priv->children_to_hide);
  for (l = children_to_hide_copy; l != NULL; l = l->next)
    {
      GtkWidget *revealer;

      revealer = gtk_widget_get_parent (l->data);
      remove_opacity_classes (revealer);

      add_opacity_class (revealer, "pathbar-opacity-off");
      g_signal_handlers_disconnect_by_func (revealer, revealer_on_hide_completed, self);
      g_signal_connect (revealer, "notify::child-revealed", (GCallback) revealer_on_hide_completed, self);

      /* If the animation we just did was to the inverted state, we
       * have the revealers that need to be hidden out of the view, so
       * there's no point on animating them.
       * Not only that, we want to update the scroll in a way that takes
       * into account the state when the animation is finished, if not
       * we are going to show the animation of the revealers next time
       * the scroll is updated
       */
      gtk_revealer_set_transition_duration (GTK_REVEALER (revealer),
                                            0);
      gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), FALSE);
      gtk_revealer_set_transition_duration (GTK_REVEALER (revealer),
                                            REVEALER_ANIMATION_TIME);
    }

  priv->invert_animation = FALSE;
  priv->invert_animation_progress = 0;
  priv->invert_animation_initial_time = 0;
  priv->invert_animation_initial_width = 0;
  gtk_widget_remove_tick_callback (priv->children_box,
                                   priv->invert_animation_tick_id);
  priv->invert_animation_tick_id = 0;

  g_signal_emit_by_name (self, "invert-animation-done");
}


static gboolean
invert_animation_on_tick (GtkWidget     *widget,
                          GdkFrameClock *frame_clock,
                          gpointer       user_data)
{
  GtkPathBarContainer *self = GTK_PATH_BAR_CONTAINER (user_data);
  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (self);
  guint64 elapsed;
  gint max_scroll;
  double animation_speed;
  GtkAllocation child_allocation;
  GtkAllocation allocation;

  /* Initialize the frame clock the first time this is called */
  if (priv->invert_animation_initial_time == 0)
    priv->invert_animation_initial_time = gdk_frame_clock_get_frame_time (frame_clock);

  gtk_widget_get_allocation (GTK_WIDGET (self), &allocation);
  gtk_widget_get_allocation (priv->children_box, &child_allocation);

  max_scroll = get_max_scroll (self);

  if (!priv->allocated)
    return TRUE;

  if (!max_scroll)
    {
      g_print ("no max scroll\n");
      finish_invert_animation (self);
      return FALSE;
    }

  /* If there are several items the animation can take some time, so let's limit
   * it to some extend
   */
  if (max_scroll / INVERT_ANIMATION_SPEED > INVERT_ANIMATION_MAX_TIME)
    animation_speed = (float) max_scroll / INVERT_ANIMATION_MAX_TIME;
  else
    animation_speed = INVERT_ANIMATION_SPEED;

  elapsed = gdk_frame_clock_get_frame_time (frame_clock) - priv->invert_animation_initial_time;
  priv->invert_animation_progress = MIN (1, elapsed * animation_speed / (1000. * max_scroll));
  g_print ("################animation progres %d %d %f %f\n", gtk_widget_get_allocated_width (GTK_WIDGET (self)), max_scroll, elapsed / 1000., priv->invert_animation_progress);
  update_scrolling (self);

  if (priv->invert_animation_progress >= 1)
    {
      finish_invert_animation (self);

      return FALSE;
    }

  gtk_widget_queue_allocate (gtk_widget_get_parent (GTK_WIDGET (self)));

  return TRUE;
}

static void
start_invert_animation (GtkPathBarContainer *self)
{
  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (self);
  GList *child;

  if (priv->invert_animation)
    finish_invert_animation (self);

  priv->invert_animation_initial_width = gtk_widget_get_allocated_width (GTK_WIDGET (self));
  priv->invert_animation = TRUE;
  priv->invert_animation_progress = 0;
  priv->allocated = FALSE;

  for (child = priv->children; child != NULL; child = child->next)
    {
      GtkWidget *revealer;

      revealer = gtk_widget_get_parent (GTK_WIDGET (child->data));

      remove_opacity_classes (revealer);
      if (!gtk_revealer_get_child_revealed (GTK_REVEALER (revealer)))
        add_opacity_class (revealer, "pathbar-opacity-on");

      gtk_revealer_set_transition_duration (GTK_REVEALER (revealer), 0);
      gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), TRUE);
      gtk_revealer_set_transition_duration (GTK_REVEALER (revealer),
                                            REVEALER_ANIMATION_TIME);
    }

  priv->invert_animation_tick_id = gtk_widget_add_tick_callback (priv->children_box,
                                                                 (GtkTickCallback) invert_animation_on_tick,
                                                                 self, NULL);
}

static void
gtk_path_bar_container_get_preferred_width (GtkWidget *widget,
                                            gint      *minimum_width,
                                            gint      *natural_width)
{
  GtkPathBarContainer *self = GTK_PATH_BAR_CONTAINER (widget);
  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (self);
  gint child_minimum_width;
  gint child_natural_width;
  GList *child;
  gint n_visible_children;
  gboolean have_min = FALSE;
  GList *children;

  *minimum_width = 0;
  *natural_width = 0;

  children = g_list_copy (priv->children);
  if (priv->inverted)
    children = g_list_reverse (children);

  n_visible_children = 0;
  for (child = children; child != NULL; child = child->next)
    {
      if (!gtk_widget_is_visible (child->data))
        continue;

      ++n_visible_children;
      gtk_widget_get_preferred_width (child->data, &child_minimum_width, &child_natural_width);
      /* Minimum is a minimum of the first visible child */
      if (!have_min)
        {
          *minimum_width = child_minimum_width;
          have_min = TRUE;
        }
      /* Natural is a sum of all visible children */
      *natural_width += child_natural_width;
    }

  g_list_free (children);
}

static void
gtk_path_bar_container_get_preferred_width_for_height (GtkWidget *widget,
                                                      gint       height,
                                                      gint      *minimum_width_out,
                                                      gint      *natural_width_out)
{
  gtk_path_bar_container_get_preferred_width (widget, minimum_width_out, natural_width_out);
}

static void
gtk_path_bar_container_get_preferred_height (GtkWidget *widget,
                                            gint      *minimum_height,
                                            gint      *natural_height)
{
  GtkPathBarContainer *children_box = GTK_PATH_BAR_CONTAINER (widget);
  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (children_box);
  gint child_minimum_height;
  gint child_natural_height;
  GList *child;

  *minimum_height = 0;
  *natural_height = 0;
  for (child = priv->children; child != NULL; child = child->next)
    {
      if (!gtk_widget_is_visible (child->data))
        continue;

      gtk_widget_get_preferred_height (child->data, &child_minimum_height, &child_natural_height);
      *minimum_height = MAX (*minimum_height, child_minimum_height);
      *natural_height = MAX (*natural_height, child_natural_height);
    }
}

static GtkSizeRequestMode
gtk_path_bar_container_get_request_mode (GtkWidget *self)
{
  return GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT;
}

static void
gtk_path_bar_container_container_add (GtkContainer *container,
                              GtkWidget    *child)
{
  g_return_if_fail (child != NULL);

  g_error ("Path bar cannot add children. Use the path bar API instead");

  return;
}

static void
gtk_path_bar_container_container_remove (GtkContainer *container,
                                 GtkWidget    *child)
{
  g_return_if_fail (child != NULL);

  //g_error ("Path bar cannot remove children. Use the path bar API instead");

  return;
}

static void
gtk_path_bar_container_unrealize (GtkWidget *widget)
{
  GtkPathBarContainer *self = GTK_PATH_BAR_CONTAINER (widget);
  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (self);

  gtk_widget_unregister_window (widget, priv->bin_window);
  gdk_window_destroy (priv->bin_window);
  priv->view_window = NULL;

  GTK_WIDGET_CLASS (gtk_path_bar_container_parent_class)->unrealize (widget);
}

static void
gtk_path_bar_container_realize (GtkWidget *widget)
{
  GtkPathBarContainer *self = GTK_PATH_BAR_CONTAINER (widget);
  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (self);
  GtkAllocation allocation;
  GdkWindowAttr attributes = { 0 };
  GdkWindowAttributesType attributes_mask;
  GtkRequisition children_used_min_size;
  GtkRequisition children_used_nat_size;
  GtkRequisition available_size;
  GtkRequisition distributed_size;

  gtk_widget_set_realized (widget, TRUE);

  gtk_widget_get_allocation (widget, &allocation);

  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes_mask = (GDK_WA_X | GDK_WA_Y) | GDK_WA_VISUAL;

  priv->view_window = gdk_window_new (gtk_widget_get_parent_window (GTK_WIDGET (self)),
                                      &attributes, attributes_mask);
  gtk_widget_set_window (widget, priv->view_window);
  gtk_widget_register_window (widget, priv->view_window);

  available_size.width = priv->parent_available_width;
  available_size.height = allocation.height;
  get_children_preferred_size_for_requisition (self, &available_size,
                                               priv->inverted,
                                               &children_used_min_size,
                                               &children_used_nat_size,
                                               &distributed_size);

  attributes.x = 0;
  attributes.y = 0;
  attributes.width = distributed_size.width;
  attributes.height = distributed_size.height;

  priv->bin_window = gdk_window_new (priv->view_window, &attributes,
                                     attributes_mask);
  gtk_widget_register_window (widget, priv->bin_window);

  gtk_widget_set_parent_window (priv->children_box, priv->bin_window);

  gdk_window_show (priv->bin_window);
  gdk_window_show (priv->view_window);
  gtk_widget_show_all (priv->children_box);
}

static gboolean
gtk_path_bar_container_draw (GtkWidget *widget,
                     cairo_t   *cr)
{
  GtkPathBarContainer *self = GTK_PATH_BAR_CONTAINER (widget);
  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (self);

  if (gtk_cairo_should_draw_window (cr, priv->bin_window))
    {
      GTK_WIDGET_CLASS (gtk_path_bar_container_parent_class)->draw (widget, cr);
    }

  return GDK_EVENT_PROPAGATE;
}

static void
real_get_preferred_size_for_requisition (GtkWidget      *widget,
                                         GtkRequisition *available_size,
                                         GtkRequisition *minimum_size,
                                         GtkRequisition *natural_size,
                                         GtkRequisition *distributed_size)
{
  GtkPathBarContainer *self = GTK_PATH_BAR_CONTAINER (widget);
  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (self);

   get_children_preferred_size_for_requisition (self, available_size,
                                                priv->inverted,
                                                minimum_size,
                                                natural_size,
                                                distributed_size);
  if (priv->invert_animation)
    {
      minimum_size->width += (1 - priv->invert_animation_progress) *
                              (priv->invert_animation_initial_width - minimum_size->width);
      natural_size->width += (1 - priv->invert_animation_progress) *
                              (priv->invert_animation_initial_width - natural_size->width);
      distributed_size->width += (1 - priv->invert_animation_progress) *
                                  (priv->invert_animation_initial_width - distributed_size->width);
    }
}

void
gtk_path_bar_container_get_preferred_size_for_requisition (GtkWidget      *widget,
                                                           GtkRequisition *available_size,
                                                           GtkRequisition *minimum_size,
                                                           GtkRequisition *natural_size,
                                                           GtkRequisition *distributed_size)
{

  real_get_preferred_size_for_requisition (widget,
                                           available_size,
                                           minimum_size,
                                           natural_size,
                                           distributed_size);
}

static void
gtk_path_bar_container_init (GtkPathBarContainer *self)
{
  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (self);

  gtk_widget_set_has_window (GTK_WIDGET (self), TRUE);
  gtk_widget_set_redraw_on_allocate (GTK_WIDGET (self), TRUE);

  priv->children_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_parent_window (priv->children_box, priv->bin_window);
  GTK_CONTAINER_CLASS (gtk_path_bar_container_parent_class)->add (GTK_CONTAINER (self), priv->children_box);

  gtk_widget_show (priv->children_box);

  priv->invert_animation = FALSE;
  priv->inverted = FALSE;
  priv->invert_animation_tick_id = 0;
  priv->children_to_hide = NULL;
  priv->children_to_show = NULL;
  priv->children_to_remove = NULL;
}

static void
gtk_path_bar_container_class_init (GtkPathBarContainerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);

  object_class->set_property = gtk_path_bar_container_set_property;
  object_class->get_property = gtk_path_bar_container_get_property;

  widget_class->size_allocate = gtk_path_bar_container_size_allocate;
  widget_class->get_preferred_width = gtk_path_bar_container_get_preferred_width;
  widget_class->get_preferred_height = gtk_path_bar_container_get_preferred_height;
  widget_class->get_preferred_width_for_height = gtk_path_bar_container_get_preferred_width_for_height;
  widget_class->get_request_mode = gtk_path_bar_container_get_request_mode;
  widget_class->realize = gtk_path_bar_container_realize;
  widget_class->unrealize = gtk_path_bar_container_unrealize;
  widget_class->draw = gtk_path_bar_container_draw;

  container_class->add = gtk_path_bar_container_container_add;
  container_class->remove = gtk_path_bar_container_container_remove;

  class->get_preferred_size_for_requisition = real_get_preferred_size_for_requisition;

  path_bar_container_properties[PROP_INVERTED] =
           g_param_spec_int ("inverted",
                             _("Direction of hiding children inverted"),
                             P_("If false the container will start hiding widgets from the end when there is not enough space, and the oposite in case inverted is true."),
                             0, G_MAXINT, 0,
                             G_PARAM_READWRITE);

  path_bar_container_properties[PROP_CHILDREN_SHOWN] =
           g_param_spec_pointer ("children-shown",
                             _("Widgets that are shown"),
                             P_("The widgets that due to overflow are going to be shown."),
                             G_PARAM_READABLE);


   path_bar_container_signals[INVERT_ANIMATION_DONE] =
                g_signal_new ("invert-animation-done",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (GtkPathBarContainerClass, invert_animation_done),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);

  g_object_class_install_properties (object_class, LAST_PROP, path_bar_container_properties);
}

/**
 * gtk_path_bar_container_new:
 *
 * Creates a new #GtkPathBarContainer.
 *
 * Returns: a new #GtkPathBarContainer.
 **/
GtkWidget *
gtk_path_bar_container_new (void)
{
  return g_object_new (GTK_TYPE_PATH_BAR_CONTAINER, NULL);
}

void
gtk_path_bar_container_adapt_to_size (GtkPathBarContainer *self,
                                      GtkRequisition      *available_size)
{
  g_return_if_fail (GTK_IS_PATH_BAR_CONTAINER (self));

  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (self);

  priv->parent_available_width = available_size->width;

  update_children_visibility (self);
  idle_update_revealers (self);
}

void
gtk_path_bar_container_set_inverted (GtkPathBarContainer *self,
                                     gboolean             inverted)
{
  g_return_if_fail (GTK_IS_PATH_BAR_CONTAINER (self));

  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (self);

  if (priv->inverted != inverted)
    {
      priv->inverted = inverted != FALSE;

      g_object_notify (G_OBJECT (self), "inverted");

      if (_gtk_widget_get_mapped (GTK_WIDGET (self)))
        start_invert_animation (self);
      gtk_widget_queue_resize (GTK_WIDGET (self));
    }
}

GList *
gtk_path_bar_container_get_children (GtkPathBarContainer *self)
{
  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (self);
  GList *children = NULL;
  GList *l;

  g_return_val_if_fail (GTK_IS_PATH_BAR_CONTAINER (self), NULL);

  for (l = priv->children; l != NULL; l = l->next)
    {
      if (!g_list_find (priv->children_to_remove, l->data))
        children = g_list_prepend (children, l->data);
    }

  return g_list_reverse (children);
}

GList *
gtk_path_bar_container_get_shown_children (GtkPathBarContainer *self)
{
  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_PATH_BAR_CONTAINER (self), NULL);

  return priv->children_to_show;
}

void
gtk_path_bar_container_remove_all_children (GtkPathBarContainer *self)
{
  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (self);

  g_return_if_fail (GTK_IS_PATH_BAR_CONTAINER (self));

  gtk_container_foreach (GTK_CONTAINER (priv->children_box),
                         (GtkCallback) gtk_widget_destroy, NULL);

  g_list_free (priv->children_to_remove);
  priv->children_to_remove = NULL;

  g_list_free (priv->children);
  priv->children = NULL;
}

gboolean
gtk_path_bar_container_get_inverted (GtkPathBarContainer *self)
{
  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (self);
  g_return_val_if_fail (GTK_IS_PATH_BAR_CONTAINER (self), 0);

  priv = gtk_path_bar_container_get_instance_private (self);

  return priv->inverted;
}

GList *
gtk_path_bar_container_get_overflow_children (GtkPathBarContainer *self)
{
  GtkPathBarContainerPrivate *priv = gtk_path_bar_container_get_instance_private (self);
  GList *result = NULL;
  GList *l;

  g_return_val_if_fail (GTK_IS_PATH_BAR_CONTAINER (self), 0);

  priv = gtk_path_bar_container_get_instance_private (self);

  for (l = priv->children; l != NULL; l = l->next)
    if (gtk_widget_is_visible (l->data) && !gtk_widget_get_child_visible (l->data))
      result = g_list_append (result, l->data);

  return result;
}
