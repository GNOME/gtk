/*
 * Copyright (C) 2015 Rafał Lużyński <digitalfreak@lingonborough.com>
 * Copyright (C) 2015 Red Hat
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
 *
 * Authors: Rafał Lużyński <digitalfreak@lingonborough.com>
 *          Carlos Soriano <csoriano@gnome.org>
 */

#include "config.h"

#include "gtkhidingbox.h"
#include "gtkwidgetprivate.h"
#include "gtkintl.h"
#include "gtksizerequest.h"
#include "gtkbuildable.h"
#include "gtkrevealer.h"
#include "gtkadjustment.h"
#include "gtkscrolledwindow.h"
#include "gtkbox.h"

//TODO remove
#include "gtkbutton.h"

#include "glib.h"

#define INVERT_ANIMATION_TIME 500 //ms

typedef enum {
  ANIMATION_PHASE_NONE,
  ANIMATION_PHASE_OUT,
  ANIMATION_PHASE_MOVE,
  ANIMATION_PHASE_IN
} HidingBoxAnimationPhase;

struct _GtkHidingBoxPrivate
{
  GList *children;
  gint16 spacing;
  gint inverted :1;
  GList *widgets_to_hide;
  GList *widgets_to_show;
  GList *widgets_to_remove;
  GList *widgets_shown;
  HidingBoxAnimationPhase animation_phase;
  gint current_width;
  gint current_height;
  guint needs_update :1;

  gboolean invert_animation;

  GtkWidget *scrolled_window;
  GtkWidget *box;
  GtkAdjustment *hadjustment;

  guint tick_id;
  guint64 initial_time;
};

static void
gtk_hiding_box_buildable_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkHidingBox, gtk_hiding_box, GTK_TYPE_CONTAINER,
                         G_ADD_PRIVATE (GtkHidingBox)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, gtk_hiding_box_buildable_init))


static void
gtk_hiding_box_buildable_add_child (GtkBuildable *buildable,
                                    GtkBuilder   *builder,
                                    GObject      *child,
                                    const gchar  *type)
{
  GtkHidingBoxPrivate *priv = gtk_hiding_box_get_instance_private (buildable);

  if (!type)
    {
      gtk_container_add (GTK_CONTAINER (buildable), GTK_WIDGET (child));
      priv->needs_update = TRUE;
    }
  else
    {
      GTK_BUILDER_WARN_INVALID_CHILD_TYPE (GTK_HIDING_BOX (buildable), type);
    }
}

static void
gtk_hiding_box_buildable_init (GtkBuildableIface *iface)
{
  iface->add_child = gtk_hiding_box_buildable_add_child;
}

enum {
  PROP_0,
  PROP_SPACING,
  PROP_INVERTED,
  LAST_PROP
};

static GParamSpec *hiding_box_properties[LAST_PROP] = { NULL, };

static void
gtk_hiding_box_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkHidingBox *box = GTK_HIDING_BOX (object);

  switch (prop_id)
    {
    case PROP_SPACING:
      gtk_hiding_box_set_spacing (box, g_value_get_int (value));
      break;
    case PROP_INVERTED:
      gtk_hiding_box_set_inverted (box, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_hiding_box_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GtkHidingBox *box = GTK_HIDING_BOX (object);
  GtkHidingBoxPrivate *priv = gtk_hiding_box_get_instance_private (box);

  switch (prop_id)
    {
    case PROP_SPACING:
      g_value_set_int (value, priv->spacing);
      break;
    case PROP_INVERTED:
      g_value_set_boolean (value, priv->inverted);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_hiding_box_add (GtkContainer *container,
                    GtkWidget    *widget)
{
  GtkHidingBox *box = GTK_HIDING_BOX (container);
  GtkWidget *revealer;
  GtkHidingBoxPrivate *priv = gtk_hiding_box_get_instance_private (box);
  GtkStyleContext *style_context;

  revealer = gtk_revealer_new ();
  style_context = gtk_widget_get_style_context (revealer);
  gtk_style_context_add_class (style_context, "pathbar-initial-opacity");
  gtk_revealer_set_transition_type (GTK_REVEALER (revealer),
                                    GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT);
  gtk_container_add (GTK_CONTAINER (revealer), widget);
  g_print ("box fine? %s \n", G_OBJECT_TYPE_NAME (priv->box));
  gtk_container_add (GTK_CONTAINER (priv->box), revealer);
  priv->children = g_list_append (priv->children, widget);
  gtk_widget_show (revealer);

  g_print ("add\n");
}

static void
really_remove_child (GtkHidingBox *self,
                     GtkWidget    *widget)
{
  GList *child;
  GtkHidingBoxPrivate *priv = gtk_hiding_box_get_instance_private (self);

  g_print ("really remove child %p %s\n", widget, gtk_button_get_label (GTK_BUTTON (widget)));
  for (child = priv->widgets_to_remove; child != NULL; child = child->next)
    {
      GtkWidget *revealer;

      revealer = gtk_widget_get_parent (child->data);
      g_print ("aver %p\n", child->data);
      if (child->data == widget && !gtk_revealer_get_child_revealed (GTK_REVEALER (revealer)))
        {
          g_print ("############################## INSIDE\n");
          gboolean was_visible = gtk_widget_get_visible (widget);

          priv->widgets_to_remove = g_list_remove (priv->widgets_to_remove,
                                                   child->data);
          gtk_container_remove (GTK_CONTAINER (priv->box), revealer);

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
  GtkHidingBox *self = GTK_HIDING_BOX (user_data);

  g_print ("unrevelaed really remove child %p %s\n", widget, G_OBJECT_TYPE_NAME (widget));
  really_remove_child (self, gtk_bin_get_child (GTK_BIN (widget)));
}

static void
gtk_hiding_box_remove (GtkContainer *container,
                       GtkWidget    *widget)
{
  GList *child;
  GtkHidingBox *box = GTK_HIDING_BOX (container);
  GtkHidingBoxPrivate *priv = gtk_hiding_box_get_instance_private (box);
  GtkWidget *to_remove;

  g_print ("remove %p %s\n", widget, G_OBJECT_TYPE_NAME (widget));
  if (GTK_IS_REVEALER (widget) && gtk_widget_get_parent (widget) == priv->box)
    to_remove = gtk_bin_get_child (widget);
  else
    to_remove = widget;

  priv->widgets_to_remove = g_list_append (priv->widgets_to_remove, to_remove);
  priv->children = g_list_remove (priv->children, to_remove);
  priv->needs_update = TRUE;
  gtk_widget_queue_resize (GTK_WIDGET (container));
}

static void
gtk_hiding_box_forall (GtkContainer *container,
                       gboolean      include_internals,
                       GtkCallback   callback,
                       gpointer      callback_data)
{
  GtkHidingBox *box = GTK_HIDING_BOX (container);
  GtkHidingBoxPrivate *priv = gtk_hiding_box_get_instance_private (box);
  GList *child;

  for (child = priv->children; child != NULL; child = child->next)
    (* callback) (child->data, callback_data);

  if (include_internals)
  {
    (* callback) (priv->scrolled_window, callback_data);

    for (child = priv->widgets_to_remove; child != NULL; child = child->next)
      (* callback) (child->data, callback_data);
  }
}

static void
clear_animation_state (GtkHidingBox *self)
{
  GtkHidingBoxPrivate *priv = gtk_hiding_box_get_instance_private (self);

  priv->animation_phase = ANIMATION_PHASE_NONE;
  g_list_free (priv->widgets_to_show);
  priv->widgets_to_show = NULL;
  g_list_free (priv->widgets_to_hide);
  priv->widgets_to_hide = NULL;
}

static gboolean
update_children_visibility (GtkHidingBox     *box,
                            GtkAllocation    *allocation,
                            GtkRequestedSize *sizes,
                            gboolean          update,
                            gint             *children_size,
                            gint             *n_visible_children_expanding)
{
  GtkHidingBoxPrivate *priv = gtk_hiding_box_get_instance_private (box);
  GtkWidget *child_widget;
  GList *child;
  GtkRequestedSize *sizes_temp;
  gint i;
  GList *children;
  GList *temp_widgets_to_show = NULL;
  gboolean allocate_more_children = TRUE;
  gint n_visible_children = 0;

  g_list_free (priv->widgets_to_show);
  priv->widgets_to_show = NULL;
  g_list_free (priv->widgets_to_hide);
  priv->widgets_to_hide = NULL;

  *n_visible_children_expanding = 0;
  *children_size = -priv->spacing;
  children = g_list_copy (priv->children);
  sizes_temp = g_newa (GtkRequestedSize, g_list_length (priv->children));
  if (priv->inverted)
    children = g_list_reverse (children);

  /* Retrieve desired size for visible children. */
  for (i = 0, child = children; child != NULL; i++, child = child->next)
    {
      child_widget = GTK_WIDGET (child->data);

      gtk_widget_get_preferred_width_for_height (child_widget,
                                                 allocation->height,
                                                 &sizes_temp[i].minimum_size,
                                                 &sizes_temp[i].natural_size);
      /* Assert the api is working properly */
      if (sizes_temp[i].minimum_size < 0)
        g_error ("GtkHidingBox child %s minimum width: %d < 0 for height %d",
                 gtk_widget_get_name (child_widget),
                 sizes_temp[i].minimum_size, allocation->height);

      if (sizes_temp[i].natural_size < sizes_temp[i].minimum_size)
        g_error ("GtkHidingBox child %s natural width: %d < minimum %d for height %d",
                 gtk_widget_get_name (child_widget),
                 sizes_temp[i].natural_size, sizes_temp[i].minimum_size,
                 allocation->height);

      *children_size += sizes_temp[i].minimum_size + priv->spacing;
      sizes_temp[i].data = child_widget;

      if (!allocate_more_children || *children_size > allocation->width)
        {
          allocate_more_children = FALSE;
          if (gtk_revealer_get_child_revealed (GTK_REVEALER (gtk_widget_get_parent (child_widget))))
            priv->widgets_to_hide = g_list_append (priv->widgets_to_hide, child_widget);

          continue;
        }

      if (gtk_widget_get_hexpand (child_widget))
        (n_visible_children_expanding)++;
      (n_visible_children)++;

      if (!g_list_find (priv->widgets_to_remove, child_widget))
        priv->widgets_to_show = g_list_append (priv->widgets_to_show, child_widget);
    }

  for (i = 0; i < n_visible_children; i++)
    {
      if (priv->inverted)
        {
          sizes[n_visible_children - i - 1].minimum_size = sizes_temp[i].minimum_size;
          sizes[n_visible_children - i - 1].natural_size = sizes_temp[i].natural_size;
        }
      else
        {
          sizes[i].minimum_size = sizes_temp[i].minimum_size;
          sizes[i].natural_size = sizes_temp[i].natural_size;
        }
    }

  g_list_free (children);
}

static gboolean
needs_update (GtkHidingBox  *box,
              GtkAllocation *allocation)
{
  GtkHidingBoxPrivate *priv = gtk_hiding_box_get_instance_private (box);
  GtkWidget *child_widget;
  GList *child;
  GtkRequestedSize *sizes_temp;
  gint i;
  GList *children;
  GList *widget_shown;
  GList *widgets_to_show = NULL;
  gboolean needs_update = FALSE;
  gboolean allocate_more_children = TRUE;
  gint children_size = -priv->spacing;

  if (priv->needs_update)
    return TRUE;

  if (allocation->width != priv->current_width ||
      allocation->height != priv->current_height)
    return TRUE;

  children = g_list_copy (priv->children);
  sizes_temp = g_newa (GtkRequestedSize, g_list_length (priv->children));
  if (priv->inverted)
    children = g_list_reverse (children);

  widget_shown = priv->widgets_shown;
  /* Retrieve desired size for visible children. */
  for (i = 0, child = children; child != NULL; i++, child = child->next)
    {
      child_widget = GTK_WIDGET (child->data);
      if (!gtk_widget_get_visible (child_widget) || !allocate_more_children)
        {
          needs_update = child->data != widget_shown->data;
          widgets_to_show = g_list_prepend (widgets_to_show, child->data);
          widget_shown = widget_shown->next;
          if (needs_update)
            {
              break;
            }
          continue;
        }
      else
        {
          needs_update = child->data == widget_shown->data;
          if (needs_update)
            {
              break;
            }
        }

      gtk_widget_get_preferred_width_for_height (child_widget,
                                                 allocation->height,
                                                 &sizes_temp[i].minimum_size,
                                                 &sizes_temp[i].natural_size);
      /* Assert the api is working properly */
      if (sizes_temp[i].minimum_size < 0)
        g_error ("GtkHidingBox child %s minimum width: %d < 0 for height %d",
                 gtk_widget_get_name (child_widget),
                 sizes_temp[i].minimum_size, allocation->height);

      if (sizes_temp[i].natural_size < sizes_temp[i].minimum_size)
        g_error ("GtkHidingBox child %s natural width: %d < minimum %d for height %d",
                 gtk_widget_get_name (child_widget),
                 sizes_temp[i].natural_size, sizes_temp[i].minimum_size,
                 allocation->height);

      children_size += sizes_temp[i].minimum_size + priv->spacing;
      sizes_temp[i].data = child_widget;

      if (children_size > allocation->width)
        {
          allocate_more_children = FALSE;
        }
    }

  needs_update = needs_update || widget_shown != NULL;

  g_list_free (children);

  return needs_update;
}

static void
opacity_on (GObject    *widget,
            GParamSpec *pspec,
            gpointer    user_data)
{
  GtkHidingBoxPrivate *priv = gtk_hiding_box_get_instance_private (GTK_HIDING_BOX (user_data));

  g_print ("############opacity on!!!!!\n");
  g_signal_handlers_disconnect_by_func (widget, opacity_on, user_data);
  priv->widgets_to_show = g_list_remove (priv->widgets_to_show,
                                         gtk_bin_get_child (GTK_BIN (widget)));
}

static void
opacity_off (GObject    *widget,
             GParamSpec *pspec,
             gpointer    user_data)
{
  GtkHidingBoxPrivate *priv = gtk_hiding_box_get_instance_private (GTK_HIDING_BOX (user_data));

  g_print ("############opacity off!!!!!\n");
  g_signal_handlers_disconnect_by_func (widget, opacity_off, user_data);
  priv->widgets_to_hide = g_list_remove (priv->widgets_to_hide,
                                         gtk_bin_get_child (GTK_BIN (widget)));
}

static void
idle_update_revealers (GtkHidingBox *box)
{
  GtkHidingBoxPrivate *priv = gtk_hiding_box_get_instance_private (box);
  GList *l;

  for (l = priv->widgets_to_hide; l != NULL; l = l->next)
    {
      GtkRevealer *revealer;

      g_print ("update revealer hide %s\n", gtk_button_get_label (GTK_BUTTON (l->data)));
      revealer = GTK_REVEALER (gtk_widget_get_parent (l->data));
      if (gtk_revealer_get_reveal_child (revealer))
        {
          GtkStyleContext *style_context;

          style_context = gtk_widget_get_style_context (GTK_WIDGET (revealer));
          gtk_style_context_remove_class (style_context, "pathbar-initial-opacity");
          gtk_style_context_remove_class (style_context, "pathbar-opacity-on");
          gtk_style_context_add_class (style_context, "pathbar-opacity-off");
          g_signal_connect (revealer, "notify::child-revealed", (GCallback) opacity_off, box);
          gtk_revealer_set_reveal_child (revealer, FALSE);
        }
    }

  for (l = priv->widgets_to_remove; l != NULL; l = l->next)
    {
      GtkRevealer *revealer;

      g_print ("update revealer remove %s\n", gtk_button_get_label (GTK_BUTTON (l->data)));
      revealer = GTK_REVEALER (gtk_widget_get_parent (l->data));
      if (gtk_revealer_get_child_revealed (revealer))
        {
          GtkStyleContext *style_context;

          style_context = gtk_widget_get_style_context (GTK_WIDGET (revealer));
          gtk_style_context_remove_class (style_context, "pathbar-initial-opacity");
          gtk_style_context_remove_class (style_context, "pathbar-opacity-on");
          gtk_style_context_add_class (style_context, "pathbar-opacity-off");
          g_signal_connect (revealer, "notify::child-revealed",
                            (GCallback) unrevealed_really_remove_child, box);
          gtk_revealer_set_reveal_child (revealer, FALSE);
        }
      else
        {
          g_print ("widget to remove NOT revealed %p\n", l->data);
          really_remove_child (box, l->data);
        }
    }

  if (priv->widgets_to_remove || priv->widgets_to_hide)
    return;

  for (l = priv->widgets_to_show; l != NULL; l = l->next)
    {
      GtkRevealer *revealer;

      revealer = GTK_REVEALER (gtk_widget_get_parent (l->data));
      if (!gtk_revealer_get_reveal_child (revealer))
        {
          GtkStyleContext *style_context;

          style_context = gtk_widget_get_style_context (GTK_WIDGET (revealer));
          gtk_style_context_remove_class (style_context, "pathbar-opacity-off");
          gtk_style_context_remove_class (style_context, "pathbar-initial-opacity");
          gtk_style_context_add_class (style_context, "pathbar-opacity-on");
          gtk_revealer_set_reveal_child (revealer, TRUE);
          g_signal_connect (revealer, "notify::child-revealed", (GCallback) opacity_on, box);
        }
    }

}

static void
gtk_hiding_box_size_allocate (GtkWidget     *widget,
                              GtkAllocation *allocation)
{
  GtkHidingBox *box = GTK_HIDING_BOX (widget);
  GtkHidingBoxPrivate *priv = gtk_hiding_box_get_instance_private (box);
  GtkAllocation child_allocation;
  GtkRequestedSize *sizes;
  gint n_visible_children_expanding = 0;
  gint children_size = 0;

  gtk_widget_set_allocation (widget, allocation);

  sizes = g_newa (GtkRequestedSize, g_list_length (priv->children));
  update_children_visibility (box, allocation, sizes, FALSE, &children_size,
                              &n_visible_children_expanding);

  idle_update_revealers (box);

  child_allocation.x = allocation->x;
  child_allocation.y = allocation->y;
  child_allocation.width = allocation->width;
  child_allocation.height = allocation->height;
  gtk_widget_size_allocate (priv->scrolled_window, &child_allocation);

  _gtk_widget_set_simple_clip (widget, NULL);
}

static void
finish_invert_animation (GtkHidingBox *self)
{
  GtkHidingBoxPrivate *priv = gtk_hiding_box_get_instance_private (self);

  priv->invert_animation = FALSE;
}


static void
invert_animation_on_tick (GtkWidget     *widget,
                          GdkFrameClock *frame_clock,
                          gpointer       user_data)
{
  GtkHidingBox *self = GTK_HIDING_BOX (user_data);
  GtkHidingBoxPrivate *priv = gtk_hiding_box_get_instance_private (self);
  guint64 elapsed;
  gfloat progress;
  gdouble adjustment_value;

  if (!priv->initial_time)
    priv->initial_time = gdk_frame_clock_get_frame_time (frame_clock);

  elapsed = gdk_frame_clock_get_frame_time (frame_clock) - priv->initial_time;
  progress = elapsed / INVERT_ANIMATION_TIME;

  if (progress >= 1)
    {
      finish_invert_animation (self);

      return;
    }

  if (priv->inverted)
    adjustment_value = 1 / (progress * (gtk_adjustment_get_lower (priv->hadjustment) - gtk_adjustment_get_upper (priv->hadjustment)));
  else
    adjustment_value = progress * (gtk_adjustment_get_lower (priv->hadjustment) - gtk_adjustment_get_upper (priv->hadjustment));

  gtk_adjustment_set_value (priv->hadjustment, adjustment_value);
}

static void
start_invert_animation (GtkHidingBox *self)
{
  GtkHidingBoxPrivate *priv = gtk_hiding_box_get_instance_private (self);
  GList *child;

  priv->invert_animation = TRUE;

  for (child = priv->children; child != NULL; child = child->next)
    {
      GtkWidget *revealer;

      revealer = gtk_widget_get_parent (GTK_WIDGET (child->data));
      gtk_revealer_set_transition_duration (GTK_REVEALER (revealer), 0);
      gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), TRUE);
    }

  priv->tick_id = gtk_widget_add_tick_callback (priv->scrolled_window,
                                                (GtkTickCallback) invert_animation_on_tick,
                                                self, NULL);
}

static void
hadjustment_on_changed (GtkAdjustment *hadjustment,
                        gpointer       user_data)
{
  GtkHidingBox *box = GTK_HIDING_BOX (user_data);
  GtkHidingBoxPrivate *priv = gtk_hiding_box_get_instance_private (box);

  if (priv->invert_animation)
    return;

  if (priv->inverted)
    gtk_adjustment_set_value (hadjustment, gtk_adjustment_get_upper (hadjustment));
  else
    gtk_adjustment_set_value (hadjustment, gtk_adjustment_get_lower (hadjustment));
}

static void
gtk_hiding_box_get_preferred_width (GtkWidget *widget,
                                    gint      *minimum_width,
                                    gint      *natural_width)
{
  GtkHidingBox *box = GTK_HIDING_BOX (widget);
  GtkHidingBoxPrivate *priv = gtk_hiding_box_get_instance_private (box);
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

  /* Natural must also include the spacing */
  if (priv->spacing && n_visible_children > 1)
    *natural_width += priv->spacing * (n_visible_children - 1);

  g_list_free (children);
}

static void
gtk_hiding_box_get_preferred_height (GtkWidget *widget,
                                     gint      *minimum_height,
                                     gint      *natural_height)
{
  GtkHidingBox *box = GTK_HIDING_BOX (widget);
  GtkHidingBoxPrivate *priv = gtk_hiding_box_get_instance_private (box);
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
gtk_hiding_box_get_request_mode (GtkWidget *self)
{
  return GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT;
}

static void
on_what (gpointer  data,
         GObject  *where_the_object_was)
{
  G_BREAKPOINT ();
}

static void
gtk_hiding_box_init (GtkHidingBox *box)
{
  GtkHidingBoxPrivate *priv = gtk_hiding_box_get_instance_private (box);
  GtkAdjustment *hadjustment;
  GtkWidget *hscrollbar;

  gtk_widget_set_has_window (GTK_WIDGET (box), FALSE);
  priv->scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  priv->hadjustment = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (priv->scrolled_window));
  g_signal_connect (priv->hadjustment, "changed", (GCallback) hadjustment_on_changed, box);
  hscrollbar = gtk_scrolled_window_get_hscrollbar (GTK_SCROLLED_WINDOW (priv->scrolled_window));
  gtk_widget_hide (hscrollbar);
  priv->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  g_object_weak_ref (G_OBJECT (priv->box), on_what, NULL);
  gtk_container_add (GTK_CONTAINER (priv->scrolled_window), priv->box);
  gtk_widget_set_parent (priv->scrolled_window, GTK_WIDGET (box));

  priv->invert_animation = FALSE;
  priv->spacing = 0;
  priv->inverted = FALSE;
  priv->widgets_to_hide = NULL;
  priv->widgets_to_show = NULL;
  priv->widgets_to_remove = NULL;
  priv->widgets_shown = NULL;
  priv->animation_phase = ANIMATION_PHASE_NONE;

  gtk_widget_show_all (priv->scrolled_window);
}

static void
gtk_hiding_box_class_init (GtkHidingBoxClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);

  object_class->set_property = gtk_hiding_box_set_property;
  object_class->get_property = gtk_hiding_box_get_property;

  widget_class->size_allocate = gtk_hiding_box_size_allocate;
  widget_class->get_preferred_width = gtk_hiding_box_get_preferred_width;
  widget_class->get_preferred_height = gtk_hiding_box_get_preferred_height;
  widget_class->get_request_mode = gtk_hiding_box_get_request_mode;

  container_class->add = gtk_hiding_box_add;
  container_class->remove = gtk_hiding_box_remove;
  container_class->forall = gtk_hiding_box_forall;

  hiding_box_properties[PROP_SPACING] =
           g_param_spec_int ("spacing",
                             _("Spacing"),
                             _("The amount of space between children"),
                             0, G_MAXINT, 0,
                             G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  hiding_box_properties[PROP_INVERTED] =
           g_param_spec_int ("inverted",
                             _("Direction of hiding children inverted"),
                             P_("If false the container will start hiding widgets from the end when there is not enough space, and the oposite in case inverted is true."),
                             0, G_MAXINT, 0,
                             G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, hiding_box_properties);
}

/**
 * gtk_hiding_box_new:
 *
 * Creates a new #GtkHidingBox.
 *
 * Returns: a new #GtkHidingBox.
 **/
GtkWidget *
gtk_hiding_box_new (void)
{
  return g_object_new (GTK_TYPE_HIDING_BOX, NULL);
}

/**
 * gtk_hiding_box_set_spacing:
 * @box: a #GtkHidingBox
 * @spacing: the number of pixels to put between children
 *
 * Sets the #GtkHidingBox:spacing property of @box, which is the
 * number of pixels to place between children of @box.
 */
void
gtk_hiding_box_set_spacing (GtkHidingBox *box,
                            gint          spacing)
{
  GtkHidingBoxPrivate *priv ;

  g_return_if_fail (GTK_IS_HIDING_BOX (box));

  priv = gtk_hiding_box_get_instance_private (box);

  if (priv->spacing != spacing)
    {
      priv->spacing = spacing;

      g_object_notify (G_OBJECT (box), "spacing");

      gtk_widget_queue_resize (GTK_WIDGET (box));
    }
}

/**
 * gtk_hiding_box_get_spacing:
 * @box: a #GtkHidingBox
 *
 * Gets the value set by gtk_hiding_box_set_spacing().
 *
 * Returns: spacing between children
 **/
gint
gtk_hiding_box_get_spacing (GtkHidingBox *box)
{
  GtkHidingBoxPrivate *priv ;

  g_return_val_if_fail (GTK_IS_HIDING_BOX (box), 0);

  priv = gtk_hiding_box_get_instance_private (box);

  return priv->spacing;
}


void
gtk_hiding_box_set_inverted (GtkHidingBox *box,
                             gboolean      inverted)
{
  GtkHidingBoxPrivate *priv ;

  g_return_if_fail (GTK_IS_HIDING_BOX (box));

  priv = gtk_hiding_box_get_instance_private (box);

  if (priv->inverted != inverted)
    {
      priv->inverted = inverted != FALSE;

      g_object_notify (G_OBJECT (box), "inverted");

      gtk_widget_queue_resize (GTK_WIDGET (box));
    }
}

gboolean
gtk_hiding_box_get_inverted (GtkHidingBox *box)
{
  GtkHidingBoxPrivate *priv ;

  g_return_val_if_fail (GTK_IS_HIDING_BOX (box), 0);

  priv = gtk_hiding_box_get_instance_private (box);

  return priv->inverted;
}

GList *
gtk_hiding_box_get_overflow_children (GtkHidingBox *box)
{
  GtkHidingBoxPrivate *priv ;
  GList *result = NULL;
  GList *l;

  g_return_val_if_fail (GTK_IS_HIDING_BOX (box), 0);

  priv = gtk_hiding_box_get_instance_private (box);

  for (l = priv->children; l != NULL; l = l->next)
    if (gtk_widget_is_visible (l->data) && !gtk_widget_get_child_visible (l->data))
      result = g_list_append (result, l->data);

  return result;
}
