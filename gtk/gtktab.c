/* gtktab.c
 *
 * Copyright (C) 2016 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtktab.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkenums.h"
#include "gtktypebuiltins.h"
#include "gtkboxgadgetprivate.h"
#include "gtkwidgetprivate.h"

typedef struct _GtkTabPrivate GtkTabPrivate;

struct _GtkTabPrivate
{
  gchar           *title;
  GtkWidget       *widget;

  GtkWidget       *child;
  GtkCssGadget    *gadget;
  GdkWindow       *event_window;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkTab, gtk_tab, GTK_TYPE_CONTAINER)

enum {
  PROP_0,
  PROP_TITLE,
  PROP_WIDGET,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

enum {
  ACTIVATE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
gtk_tab_get_property (GObject    *object,
                      guint       prop_id,
                      GValue     *value,
                      GParamSpec *pspec)
{
  GtkTab *self = GTK_TAB (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      g_value_set_string (value, gtk_tab_get_title (self));
      break;

    case PROP_WIDGET:
      g_value_set_object (value, gtk_tab_get_widget (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_tab_set_property (GObject      *object,
                      guint         prop_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
  GtkTab *self = GTK_TAB (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      gtk_tab_set_title (self, g_value_get_string (value));
      break;

    case PROP_WIDGET:
      gtk_tab_set_widget (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_tab_finalize (GObject *object)
{
  GtkTab *self = GTK_TAB (object);
  GtkTabPrivate *priv = gtk_tab_get_instance_private (self);

  g_free (priv->title);

  g_clear_object (&priv->gadget);

  G_OBJECT_CLASS (gtk_tab_parent_class)->finalize (object);
}

static void
gtk_tab_destroy (GtkWidget *widget)
{
  GtkTab *self = GTK_TAB (widget);
  GtkTabPrivate *priv = gtk_tab_get_instance_private (self);

  if (priv->widget)
    {
      g_object_remove_weak_pointer (G_OBJECT (priv->widget), (gpointer *)&priv->widget);
      priv->widget = NULL;
    }

  GTK_WIDGET_CLASS (gtk_tab_parent_class)->destroy (widget);
}

static void
gtk_tab_realize (GtkWidget *widget)
{
  GtkTab *self = GTK_TAB (widget);
  GtkTabPrivate *priv = gtk_tab_get_instance_private (self);
  GdkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;
  GtkAllocation allocation;

  gtk_widget_set_realized (widget, TRUE);

  gtk_widget_get_allocation (widget, &allocation);

  window = gtk_widget_get_parent_window (widget);
  gtk_widget_set_window (widget, window);
  g_object_ref (window);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_BUTTON_PRESS_MASK |
                            GDK_BUTTON_RELEASE_MASK | GDK_KEY_PRESS_MASK |
                            GDK_POINTER_MOTION_MASK | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y;

  priv->event_window = gdk_window_new (gtk_widget_get_parent_window (widget),
                                       &attributes, attributes_mask);
  gtk_widget_register_window (widget, priv->event_window);
}

static void
gtk_tab_unrealize (GtkWidget *widget)
{
  GtkTab *self = GTK_TAB (widget);
  GtkTabPrivate *priv = gtk_tab_get_instance_private (self);

  gtk_widget_unregister_window (widget, priv->event_window);
  gdk_window_destroy (priv->event_window);
  priv->event_window = NULL;

  GTK_WIDGET_CLASS (gtk_tab_parent_class)->unrealize (widget);
}

static void
gtk_tab_map (GtkWidget *widget)
{
  GtkTab *self = GTK_TAB (widget);
  GtkTabPrivate *priv = gtk_tab_get_instance_private (self);

  GTK_WIDGET_CLASS (gtk_tab_parent_class)->map (widget);

  gdk_window_show_unraised (priv->event_window);
}

static void
gtk_tab_unmap (GtkWidget *widget)
{
  GtkTab *self = GTK_TAB (widget);
  GtkTabPrivate *priv = gtk_tab_get_instance_private (self);

  gdk_window_hide (priv->event_window);

  GTK_WIDGET_CLASS (gtk_tab_parent_class)->unmap (widget);
}

static gboolean
gtk_tab_enter (GtkWidget        *widget,
               GdkEventCrossing *event)
{
  gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_PRELIGHT, FALSE);

  return TRUE;
}

static gboolean
gtk_tab_leave (GtkWidget        *widget,
               GdkEventCrossing *event)
{
  gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_PRELIGHT);

  return TRUE;
}

static gboolean
gtk_tab_button_press (GtkWidget      *widget,
                      GdkEventButton *event)
{
  if (event->button != GDK_BUTTON_PRIMARY)
    return FALSE;

  g_signal_emit (widget, signals[ACTIVATE], 0);

  return TRUE;
}

static void
gtk_tab_get_preferred_width (GtkWidget *widget,
                             gint      *minimum,
                             gint      *natural)
{
  GtkTab *self = GTK_TAB (widget);
  GtkTabPrivate *priv = gtk_tab_get_instance_private (self);

  gtk_css_gadget_get_preferred_size (priv->gadget,
                                     GTK_ORIENTATION_HORIZONTAL,
                                     -1,
                                     minimum, natural,
                                     NULL, NULL);
}

static void
gtk_tab_get_preferred_height (GtkWidget *widget,
                              gint      *minimum,
                              gint      *natural)
{
  GtkTab *self = GTK_TAB (widget);
  GtkTabPrivate *priv = gtk_tab_get_instance_private (self);

  gtk_css_gadget_get_preferred_size (priv->gadget,
                                     GTK_ORIENTATION_VERTICAL,
                                     -1,
                                     minimum, natural,
                                     NULL, NULL);
}

static void
gtk_tab_get_preferred_width_for_height (GtkWidget *widget,
                                        gint       height,
                                        gint      *minimum,
                                        gint      *natural)
{
  GtkTab *self = GTK_TAB (widget);
  GtkTabPrivate *priv = gtk_tab_get_instance_private (self);

  gtk_css_gadget_get_preferred_size (priv->gadget,
                                     GTK_ORIENTATION_HORIZONTAL,
                                     height,
                                     minimum, natural,
                                     NULL, NULL);
}

static void
gtk_tab_get_preferred_height_for_width (GtkWidget *widget,
                                        gint       width,
                                        gint      *minimum,
                                        gint      *natural)
{
  GtkTab *self = GTK_TAB (widget);
  GtkTabPrivate *priv = gtk_tab_get_instance_private (self);

  gtk_css_gadget_get_preferred_size (priv->gadget,
                                     GTK_ORIENTATION_VERTICAL,
                                     width,
                                     minimum, natural,
                                     NULL, NULL);
}

static void
gtk_tab_size_allocate (GtkWidget     *widget,
                       GtkAllocation *allocation)
{
  GtkTab *self = GTK_TAB (widget);
  GtkTabPrivate *priv = gtk_tab_get_instance_private (self);
  GtkAllocation clip;

  gtk_widget_set_allocation (widget, allocation);

  gtk_css_gadget_allocate (priv->gadget,
                           allocation,
                           gtk_widget_get_allocated_baseline (widget),
                           &clip);

  gtk_widget_set_clip (widget, &clip);

  if (gtk_widget_get_realized (widget))
    {
      GtkAllocation border_allocation;

      gtk_css_gadget_get_border_allocation (priv->gadget, &border_allocation, NULL);
      gdk_window_move_resize (priv->event_window,
                              border_allocation.x, border_allocation.y,
                              border_allocation.width, border_allocation.height);
      if (gtk_widget_get_mapped (widget))
        gdk_window_show_unraised (priv->event_window);
    }
}

static gboolean
gtk_tab_draw (GtkWidget *widget,
              cairo_t   *cr)
{
  GtkTab *self = GTK_TAB (widget);
  GtkTabPrivate *priv = gtk_tab_get_instance_private (self);

  gtk_css_gadget_draw (priv->gadget, cr);

  return FALSE;
}

static void
gtk_tab_add (GtkContainer *container,
             GtkWidget    *child)
{
  GtkTab *self = GTK_TAB (container);
  GtkTabPrivate *priv = gtk_tab_get_instance_private (self);

  if (priv->child)
    {
      g_warning ("GtkTab cannot have more than one child");
      return;
    }

  priv->child = child;
  gtk_widget_set_parent (child, GTK_WIDGET (container));
  gtk_box_gadget_insert_widget (GTK_BOX_GADGET (priv->gadget), 0, child);
  gtk_box_gadget_set_gadget_expand (GTK_BOX_GADGET (priv->gadget), G_OBJECT (child), TRUE);
}

static void
gtk_tab_remove (GtkContainer *container,
                GtkWidget    *child)
{
  GtkTab *self = GTK_TAB (container);
  GtkTabPrivate *priv = gtk_tab_get_instance_private (self);

  if (priv->child == child)
    {
      gtk_box_gadget_remove_widget (GTK_BOX_GADGET (priv->gadget), child);
      gtk_widget_unparent (child);
      priv->child = NULL;
    }
}

static void
gtk_tab_forall (GtkContainer *container,
                gboolean      include_internals,
                GtkCallback   callback,
                gpointer      data)
{
  GtkTab *self = GTK_TAB (container);
  GtkTabPrivate *priv = gtk_tab_get_instance_private (self);

  if (priv->child)
    (*callback) (priv->child, data);
}

static void
gtk_tab_class_init (GtkTabClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->get_property = gtk_tab_get_property;
  object_class->set_property = gtk_tab_set_property;
  object_class->finalize = gtk_tab_finalize;

  widget_class->destroy = gtk_tab_destroy;
  widget_class->realize = gtk_tab_realize;
  widget_class->unrealize = gtk_tab_unrealize;
  widget_class->map = gtk_tab_map;
  widget_class->unmap = gtk_tab_unmap;
  widget_class->enter_notify_event = gtk_tab_enter;
  widget_class->leave_notify_event = gtk_tab_leave;
  widget_class->button_press_event = gtk_tab_button_press;
  widget_class->get_preferred_width = gtk_tab_get_preferred_width;
  widget_class->get_preferred_height = gtk_tab_get_preferred_height;
  widget_class->get_preferred_width_for_height = gtk_tab_get_preferred_width_for_height;
  widget_class->get_preferred_height_for_width = gtk_tab_get_preferred_height_for_width;
  widget_class->size_allocate = gtk_tab_size_allocate;
  widget_class->draw = gtk_tab_draw;

  container_class->add = gtk_tab_add;
  container_class->remove = gtk_tab_remove;
  container_class->forall = gtk_tab_forall;

  gtk_widget_class_set_css_name (widget_class, "tab");

  properties[PROP_TITLE] =
    g_param_spec_string ("title", P_("Title"), P_("Title"),
                         NULL,
                         GTK_PARAM_READWRITE);

  properties[PROP_WIDGET] =
    g_param_spec_object ("widget", P_("Widget"), P_("The widget the tab represents"),
                         GTK_TYPE_WIDGET,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals[ACTIVATE] =
    g_signal_new (I_("activate"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkTabClass, activate),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  widget_class->activate_signal = signals[ACTIVATE];

}

static void
gtk_tab_init (GtkTab *self)
{
  GtkTabPrivate *priv = gtk_tab_get_instance_private (self);
  GtkCssNode *widget_node;

  gtk_widget_set_can_focus (GTK_WIDGET (self), TRUE);
  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

  widget_node = gtk_widget_get_css_node (GTK_WIDGET (self));
  priv->gadget = gtk_box_gadget_new_for_node (widget_node, GTK_WIDGET (self));
  gtk_box_gadget_set_draw_focus (GTK_BOX_GADGET (priv->gadget), TRUE);
}

const gchar *
gtk_tab_get_title (GtkTab *self)
{
  GtkTabPrivate *priv = gtk_tab_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_TAB (self), NULL);

  return priv->title;
}

void
gtk_tab_set_title (GtkTab      *self,
                   const gchar *title)
{
  GtkTabPrivate *priv = gtk_tab_get_instance_private (self);

  g_return_if_fail (GTK_IS_TAB (self));

  g_free (priv->title);
  priv->title = g_strdup (title);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]);
}

GtkWidget *
gtk_tab_get_widget (GtkTab *self)
{
  GtkTabPrivate *priv = gtk_tab_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_TAB (self), NULL);

  return priv->widget;
}

void
gtk_tab_set_widget (GtkTab    *self,
                    GtkWidget *widget)
{
  GtkTabPrivate *priv = gtk_tab_get_instance_private (self);

  g_return_if_fail (GTK_IS_TAB (self));

  if (priv->widget == widget)
    return;

  if (priv->widget)
    g_object_remove_weak_pointer (G_OBJECT (priv->widget), (gpointer *)&priv->widget);

  priv->widget = widget;

  if (widget)
    g_object_add_weak_pointer (G_OBJECT (priv->widget), (gpointer *)&priv->widget);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_WIDGET]);
}

void
gtk_tab_set_child (GtkTab    *self,
                   GtkWidget *child)
{
  g_return_if_fail (GTK_IS_TAB (self));
  g_return_if_fail (GTK_IS_WIDGET (child));

  gtk_tab_add (GTK_CONTAINER (self), child);
}
