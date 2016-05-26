/* gtktabstrip.c
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

#include "gtktabstrip.h"
#include "gtktab.h"
#include "gtksimpletab.h"
#include "gtkclosabletab.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkorientable.h"
#include "gtkscrolledwindow.h"
#include "gtkbutton.h"
#include "gtkbox.h"
#include "gtkadjustmentprivate.h"
#include "gtkboxgadgetprivate.h"
#include "gtkwidgetprivate.h"

/*
 * TODO:
 * - reordering
 * - dnd
 */

typedef struct
{
  GtkCssGadget    *gadget;
  GtkStack        *stack;
  GtkPositionType  edge;
  gboolean         closable;
  gboolean         scrollable;
  gboolean         reversed;
  GtkWidget       *scrolledwindow;
  GtkWidget       *tabs;
  GtkWidget       *start_scroll;
  GtkWidget       *end_scroll;
  GtkScrollType    autoscroll_mode;
  guint            autoscroll_id;
  gboolean         autoscroll_goal_set;
  gdouble          autoscroll_goal;
} GtkTabStripPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GtkTabStrip, gtk_tab_strip, GTK_TYPE_CONTAINER)

enum {
  PROP_0,
  PROP_STACK,
  PROP_EDGE,
  PROP_CLOSABLE,
  PROP_SCROLLABLE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

enum {
  CREATE_TAB,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
gtk_tab_strip_add (GtkContainer *container,
                   GtkWidget    *widget)
{
  g_warning ("Can't add children to %s", G_OBJECT_TYPE_NAME (container));
}

static void
gtk_tab_strip_remove (GtkContainer *container,
                      GtkWidget    *widget)
{
  g_warning ("Can't remove children from %s", G_OBJECT_TYPE_NAME (container));
}

static void
gtk_tab_strip_forall (GtkContainer *container,
                      gboolean      include_internals,
                      GtkCallback   callback,
                      gpointer      callback_data)
{
  GtkTabStrip *self = GTK_TAB_STRIP (container);
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  if (include_internals)
    {
      (*callback) (priv->start_scroll, callback_data);
      (*callback) (priv->scrolledwindow, callback_data);
      (*callback) (priv->end_scroll, callback_data);
    }
}

static GType
gtk_tab_strip_child_type (GtkContainer *container)
{
  return G_TYPE_NONE;
}

static void
gtk_tab_strip_destroy (GtkWidget *widget)
{
  GtkTabStrip *self = GTK_TAB_STRIP (widget);
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  gtk_tab_strip_set_stack (self, NULL);

  g_clear_object (&priv->stack);

  GTK_WIDGET_CLASS (gtk_tab_strip_parent_class)->destroy (widget);
}

static void
gtk_tab_strip_get_preferred_width (GtkWidget *widget,
                                   gint      *minimum_size,
                                   gint      *natural_size)
{
  GtkTabStrip *self = GTK_TAB_STRIP (widget);
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  gtk_css_gadget_get_preferred_size (priv->gadget,
                                     GTK_ORIENTATION_HORIZONTAL,
                                     -1,
                                     minimum_size, natural_size,
                                     NULL, NULL);
}

static void
gtk_tab_strip_get_preferred_height (GtkWidget *widget,
                                    gint      *minimum_size,
                                    gint      *natural_size)
{
  GtkTabStrip *self = GTK_TAB_STRIP (widget);
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  gtk_css_gadget_get_preferred_size (priv->gadget,
                                     GTK_ORIENTATION_VERTICAL,
                                     -1,
                                     minimum_size, natural_size,
                                     NULL, NULL);
}

static void
gtk_tab_strip_get_preferred_width_for_height (GtkWidget *widget,
                                              gint       height,
                                              gint      *minimum_size,
                                              gint      *natural_size)
{
  GtkTabStrip *self = GTK_TAB_STRIP (widget);
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  gtk_css_gadget_get_preferred_size (priv->gadget,
                                     GTK_ORIENTATION_HORIZONTAL,
                                     height,
                                     minimum_size, natural_size,
                                     NULL, NULL);
}

static void
gtk_tab_strip_get_preferred_height_for_width (GtkWidget *widget,
                                              gint       width,
                                              gint      *minimum_size,
                                              gint      *natural_size)
{
  GtkTabStrip *self = GTK_TAB_STRIP (widget);
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  gtk_css_gadget_get_preferred_size (priv->gadget,
                                     GTK_ORIENTATION_VERTICAL,
                                     width,
                                     minimum_size, natural_size,
                                     NULL, NULL);
}

static void
gtk_tab_strip_size_allocate (GtkWidget     *widget,
                             GtkAllocation *allocation)
{
  GtkTabStrip *self = GTK_TAB_STRIP (widget);
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);
  GtkAllocation clip;

  gtk_widget_set_allocation (widget, allocation);

  gtk_css_gadget_allocate (priv->gadget,
                           allocation,
                           gtk_widget_get_allocated_baseline (widget),
                           &clip);

  gtk_widget_set_clip (widget, &clip);
}

static gboolean
gtk_tab_strip_draw (GtkWidget *widget,
                    cairo_t   *cr)
{
  GtkTabStrip *self = GTK_TAB_STRIP (widget);
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  gtk_css_gadget_draw (priv->gadget, cr);

  return FALSE;
}

static gboolean
gtk_tab_strip_get_orientation (GtkTabStrip *self)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  if (priv->edge == GTK_POS_TOP || priv->edge == GTK_POS_BOTTOM)
    return GTK_ORIENTATION_HORIZONTAL;
  else
    return GTK_ORIENTATION_VERTICAL;
}

static void
update_node_ordering (GtkTabStrip *self)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);
  gboolean reverse;

  reverse = gtk_tab_strip_get_orientation (self) == GTK_ORIENTATION_HORIZONTAL &&
            gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL;

  if ((reverse && !priv->reversed) ||
      (!reverse && priv->reversed))
    {
      gtk_box_gadget_reverse_children (GTK_BOX_GADGET (priv->gadget));
      priv->reversed = reverse;
    }
}

static void
gtk_tab_strip_direction_changed (GtkWidget        *widget,
                                 GtkTextDirection  previous_direction)
{
  update_node_ordering (GTK_TAB_STRIP (widget));

  GTK_WIDGET_CLASS (gtk_tab_strip_parent_class)->direction_changed (widget, previous_direction);
}

static void
gtk_tab_strip_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkTabStrip *self = GTK_TAB_STRIP (object);

  switch (prop_id)
    {
    case PROP_STACK:
      g_value_set_object (value, gtk_tab_strip_get_stack (self));
      break;

    case PROP_EDGE:
      g_value_set_enum (value, gtk_tab_strip_get_edge (self));
      break;

    case PROP_CLOSABLE:
      g_value_set_boolean (value, gtk_tab_strip_get_closable (self));
      break;

    case PROP_SCROLLABLE:
      g_value_set_boolean (value, gtk_tab_strip_get_scrollable (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_tab_strip_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkTabStrip *self = GTK_TAB_STRIP (object);

  switch (prop_id)
    {
    case PROP_STACK:
      gtk_tab_strip_set_stack (self, g_value_get_object (value));
      break;

    case PROP_EDGE:
      gtk_tab_strip_set_edge (self, g_value_get_enum (value));
      break;

    case PROP_CLOSABLE:
      gtk_tab_strip_set_closable (self, g_value_get_boolean (value));
      break;

    case PROP_SCROLLABLE:
      gtk_tab_strip_set_scrollable (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_tab_strip_finalize (GObject *object)
{
  GtkTabStrip *self = GTK_TAB_STRIP (object);
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  g_clear_object (&priv->gadget);

  G_OBJECT_CLASS (gtk_tab_strip_parent_class)->finalize (object);
}

static GtkTab *gtk_tab_strip_real_create_tab (GtkTabStrip *self,
                                              GtkWidget   *widget);

static void
gtk_tab_strip_class_init (GtkTabStripClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->get_property = gtk_tab_strip_get_property;
  object_class->set_property = gtk_tab_strip_set_property;
  object_class->finalize = gtk_tab_strip_finalize;

  widget_class->destroy = gtk_tab_strip_destroy;
  widget_class->get_preferred_width = gtk_tab_strip_get_preferred_width;
  widget_class->get_preferred_height = gtk_tab_strip_get_preferred_height;
  widget_class->get_preferred_width_for_height = gtk_tab_strip_get_preferred_width_for_height;
  widget_class->get_preferred_height_for_width = gtk_tab_strip_get_preferred_height_for_width;
  widget_class->size_allocate = gtk_tab_strip_size_allocate;
  widget_class->draw = gtk_tab_strip_draw;
  widget_class->direction_changed = gtk_tab_strip_direction_changed;

  container_class->add = gtk_tab_strip_add;
  container_class->remove = gtk_tab_strip_remove;
  container_class->forall = gtk_tab_strip_forall;
  container_class->child_type = gtk_tab_strip_child_type;

  klass->create_tab = gtk_tab_strip_real_create_tab;

  signals[CREATE_TAB] =
    g_signal_new (I_("create-tab"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkTabStripClass, create_tab),
                  gtk_object_handled_accumulator, NULL,
                  NULL,
                  GTK_TYPE_TAB, 1,
                  GTK_TYPE_WIDGET);

  properties[PROP_STACK] =
    g_param_spec_object ("stack", P_("Stack"), P_("The stack of items to manage"),
                         GTK_TYPE_STACK,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_EDGE] =
    g_param_spec_enum ("edge", P_("Edge"), P_("The edge for the tab-strip"),
                       GTK_TYPE_POSITION_TYPE,
                       GTK_POS_TOP,
                       GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_CLOSABLE] =
    g_param_spec_boolean ("closable", P_("Closable"), P_("Whether tabs can be closed"),
                          FALSE,
                          GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_SCROLLABLE] =
    g_param_spec_boolean ("scrollable", P_("Scrollable"), P_("Whether tabs can be scrolled"),
                          FALSE,
                          GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "tabs");
}

static void
update_scrolling (GtkTabStrip *self)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);
  GtkPolicyType hscroll = GTK_POLICY_NEVER;
  GtkPolicyType vscroll = GTK_POLICY_NEVER;

  if (priv->scrollable)
    {
      if (gtk_tab_strip_get_orientation (self) == GTK_ORIENTATION_HORIZONTAL)
        hscroll = GTK_POLICY_EXTERNAL;
      else
        vscroll = GTK_POLICY_EXTERNAL;
    }

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->scrolledwindow),
                                  hscroll, vscroll);
}

static GtkAdjustment *
gtk_tab_strip_get_scroll_adjustment (GtkTabStrip *self)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  if (gtk_tab_strip_get_orientation (self) == GTK_ORIENTATION_HORIZONTAL)
    return gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (priv->scrolledwindow));
  else
    return gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (priv->scrolledwindow));
}

static gboolean
autoscroll_cb (GtkWidget     *widget,
               GdkFrameClock *frame_clock,
               gpointer       data)
{
  GtkTabStrip *self = data;
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);
  GtkAdjustment *adj;
  gdouble value;

  adj = gtk_tab_strip_get_scroll_adjustment (self);

  value = gtk_adjustment_get_value (adj);

  if (priv->autoscroll_goal_set && ABS (value - priv->autoscroll_goal) < 5)
    value = priv->autoscroll_goal;
  else if (priv->autoscroll_mode == GTK_SCROLL_STEP_FORWARD)
    value += 5;
  else
    value -= 5;

  gtk_adjustment_set_value (adj, value);

  if (priv->autoscroll_goal_set && value == priv->autoscroll_goal)
    {
      priv->autoscroll_id = 0;
      return G_SOURCE_REMOVE;
    }
  else
    {
      return G_SOURCE_CONTINUE;
    }
}

static GtkScrollType
autoscroll_mode_for_button (GtkTabStrip *self,
                            GtkWidget   *button)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  if ((button == priv->start_scroll && !priv->reversed) ||
      (button == priv->end_scroll && priv->reversed))
    return GTK_SCROLL_STEP_BACKWARD;
  else
    return GTK_SCROLL_STEP_FORWARD;
}

static void
add_autoscroll (GtkTabStrip *self,
                GtkWidget   *button)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  if (priv->autoscroll_id != 0)
    return;

  priv->autoscroll_mode = autoscroll_mode_for_button (self, button);
  priv->autoscroll_goal_set = FALSE;
  priv->autoscroll_id = gtk_widget_add_tick_callback (GTK_WIDGET (self),
                                                      autoscroll_cb, self, NULL);
}

static void
remove_autoscroll (GtkTabStrip *self)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  if (priv->autoscroll_id == 0)
    return;

  gtk_widget_remove_tick_callback (GTK_WIDGET (self), priv->autoscroll_id);
  priv->autoscroll_id = 0;
}

static gdouble
find_autoscroll_goal (GtkTabStrip   *self,
                      gboolean       force_step,
                      GtkScrollType  mode)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);
  GtkAdjustment *adj;
  gdouble value;
  gdouble goal;
  GList *children, *l;

  adj = gtk_tab_strip_get_scroll_adjustment (self);

  if (mode == GTK_SCROLL_STEP_FORWARD)
    value = gtk_adjustment_get_value (adj) + gtk_adjustment_get_page_size (adj);
  else
    value = gtk_adjustment_get_value (adj);

  if (force_step)
    {
      if (mode == GTK_SCROLL_STEP_FORWARD)
        value += 5;
      else
        value -= 5;
      value = CLAMP (value, gtk_adjustment_get_lower (adj), gtk_adjustment_get_upper (adj));
    }

  goal = 0;
  children = gtk_container_get_children (GTK_CONTAINER (priv->tabs));
  for (l = children; l; l = l->next)
    {
      GtkWidget *child = l->data;
      GtkAllocation alloc;
      gint start, end;

      gtk_widget_get_allocation (child, &alloc);
      if (gtk_tab_strip_get_orientation (self) == GTK_ORIENTATION_HORIZONTAL)
        {
          start = alloc.x;
          end = alloc.x + alloc.width;
        }
      else
        {
          start = alloc.y;
          end = alloc.y + alloc.height;
        }

      if (start <= value && value <= end)
        {
          if (mode == GTK_SCROLL_STEP_FORWARD)
            goal = end - gtk_adjustment_get_page_size (adj);
          else
            goal = start;
          break;
        }
    }
  g_list_free (children);

  return goal;
}

static void
finish_autoscroll (GtkTabStrip *self)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);
  gdouble goal;

  if (priv->autoscroll_id == 0 || priv->autoscroll_goal_set)
    return;

  goal = find_autoscroll_goal (self, FALSE, priv->autoscroll_mode);

  priv->autoscroll_goal = goal;
  priv->autoscroll_goal_set = TRUE;
}

static gboolean
scroll_button_event (GtkWidget      *button,
                     GdkEventButton *event,
                     GtkTabStrip    *self)
{
  if (event->type == GDK_BUTTON_PRESS)
    {
      remove_autoscroll (self);
      add_autoscroll (self, button);
    }
  else if (event->type == GDK_BUTTON_RELEASE)
    finish_autoscroll (self);

  return FALSE;
}

static void
scroll_button_activate (GtkWidget   *button,
                        GtkTabStrip *self)
{
  GtkAdjustment *adj;
  gdouble goal;
  GtkScrollType mode;

  mode = autoscroll_mode_for_button (self, button);
  goal = find_autoscroll_goal (self, TRUE, mode);

  adj = gtk_tab_strip_get_scroll_adjustment (self);
  gtk_adjustment_animate_to_value (adj, goal);
}

static void
adjustment_changed (GtkTabStrip *self)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);
  GtkAdjustment *adj;
  gdouble value;
  gboolean at_lower, at_upper;

  adj = gtk_tab_strip_get_scroll_adjustment (self);
  value = gtk_adjustment_get_value (adj);

  at_lower = value <= gtk_adjustment_get_lower (adj);
  at_upper = value >= gtk_adjustment_get_upper (adj) - gtk_adjustment_get_page_size (adj);
  gtk_widget_set_visible (priv->start_scroll, !(at_lower && at_upper));
  gtk_widget_set_visible (priv->end_scroll, !(at_lower && at_upper));
  if (priv->reversed)
    {
      gtk_widget_set_sensitive (priv->start_scroll, !at_upper);
      gtk_widget_set_sensitive (priv->end_scroll, !at_lower);
    }
  else
    {
      gtk_widget_set_sensitive (priv->start_scroll, !at_lower);
      gtk_widget_set_sensitive (priv->end_scroll, !at_upper);
    }
}

static void
gtk_tab_strip_init (GtkTabStrip *self)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);
  GtkAdjustment *adj;
  GtkCssNode *widget_node;

  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

  priv->edge = GTK_POS_TOP;
  priv->scrollable = FALSE;
  priv->closable = FALSE;

  widget_node = gtk_widget_get_css_node (GTK_WIDGET (self));
  priv->gadget = gtk_box_gadget_new_for_node (widget_node, GTK_WIDGET (self));
  gtk_box_gadget_set_orientation (GTK_BOX_GADGET (priv->gadget), GTK_ORIENTATION_HORIZONTAL);

  priv->start_scroll = gtk_button_new_from_icon_name ("pan-start-symbolic", GTK_ICON_SIZE_MENU);
  gtk_button_set_relief (GTK_BUTTON (priv->start_scroll), GTK_RELIEF_NONE);
  gtk_widget_show (priv->start_scroll);
  gtk_widget_set_no_show_all (priv->start_scroll, TRUE);
  gtk_widget_set_focus_on_click (priv->start_scroll, FALSE);
  gtk_widget_set_parent (priv->start_scroll, GTK_WIDGET (self));
  gtk_box_gadget_insert_widget (GTK_BOX_GADGET (priv->gadget), 0, priv->start_scroll);
  g_signal_connect (priv->start_scroll, "button-press-event",
                    G_CALLBACK (scroll_button_event), self);
  g_signal_connect (priv->start_scroll, "button-release-event",
                    G_CALLBACK (scroll_button_event), self);
  g_signal_connect (priv->start_scroll, "activate",
                    G_CALLBACK (scroll_button_activate), self);

  priv->scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (priv->scrolledwindow);
  gtk_widget_set_parent (priv->scrolledwindow, GTK_WIDGET (self));
  gtk_box_gadget_insert_widget (GTK_BOX_GADGET (priv->gadget), 1, priv->scrolledwindow);
  gtk_box_gadget_set_gadget_expand (GTK_BOX_GADGET (priv->gadget), G_OBJECT (priv->scrolledwindow), TRUE);
  update_scrolling (self);

  priv->end_scroll = gtk_button_new_from_icon_name ("pan-end-symbolic", GTK_ICON_SIZE_MENU);
  gtk_button_set_relief (GTK_BUTTON (priv->end_scroll), GTK_RELIEF_NONE);
  gtk_widget_show (priv->end_scroll);
  gtk_widget_set_no_show_all (priv->end_scroll, TRUE);
  gtk_widget_set_focus_on_click (priv->end_scroll, FALSE);
  gtk_widget_set_parent (priv->end_scroll, GTK_WIDGET (self));
  gtk_box_gadget_insert_widget (GTK_BOX_GADGET (priv->gadget), 2, priv->end_scroll);
  g_signal_connect (priv->end_scroll, "button-press-event",
                    G_CALLBACK (scroll_button_event), self);
  g_signal_connect (priv->end_scroll, "button-release-event",
                    G_CALLBACK (scroll_button_event), self);
  g_signal_connect (priv->end_scroll, "activate",
                    G_CALLBACK (scroll_button_activate), self);

  adj = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (priv->scrolledwindow));
  g_signal_connect_swapped (adj, "changed", G_CALLBACK (adjustment_changed), self);
  g_signal_connect_swapped (adj, "value-changed", G_CALLBACK (adjustment_changed), self);

  adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (priv->scrolledwindow));
  g_signal_connect_swapped (adj, "changed", G_CALLBACK (adjustment_changed), self);
  g_signal_connect_swapped (adj, "value-changed", G_CALLBACK (adjustment_changed), self);

  priv->tabs = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_show (priv->tabs);
  gtk_container_add (GTK_CONTAINER (priv->scrolledwindow), priv->tabs);
}

static void
gtk_tab_strip_child_position_changed (GtkTabStrip *self,
                                      GParamSpec  *pspec,
                                      GtkWidget   *child)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);
  GtkWidget *parent;
  GtkTab *tab;
  guint position;

  tab = g_object_get_data (G_OBJECT (child), "GTK_TAB");

  if (!tab || !GTK_IS_TAB (tab))
    return;

  parent = gtk_widget_get_parent (child);

  gtk_container_child_get (GTK_CONTAINER (parent), child,
                           "position", &position,
                           NULL);

  gtk_container_child_set (GTK_CONTAINER (priv->tabs), GTK_WIDGET (tab),
                           "position", position,
                           NULL);
}

static void
gtk_tab_strip_child_title_changed (GtkTabStrip *self,
                                   GParamSpec  *pspec,
                                   GtkWidget   *child)
{
  g_autofree gchar *title = NULL;
  GtkWidget *parent;
  GtkTab *tab;

  tab = g_object_get_data (G_OBJECT (child), "GTK_TAB");

  if (!GTK_IS_TAB (tab))
    return;

  parent = gtk_widget_get_parent (child);

  gtk_container_child_get (GTK_CONTAINER (parent), child,
                           "title", &title,
                           NULL);

  gtk_tab_set_title (tab, title);
}

static void
update_visible_child (GtkWidget *tab,
                      gpointer   user_data)
{
  GtkWidget *visible_child = user_data;

  if (GTK_IS_TAB (tab))
    {
      if (gtk_tab_get_widget (GTK_TAB (tab)) == visible_child)
        gtk_widget_set_state_flags (tab, GTK_STATE_FLAG_CHECKED, FALSE);
      else
        gtk_widget_unset_state_flags (tab, GTK_STATE_FLAG_CHECKED);
    }
}

static void
gtk_tab_strip_stack_notify_visible_child (GtkTabStrip *self,
                                          GParamSpec  *pspec,
                                          GtkStack    *stack)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);
  GtkWidget *visible_child;

  visible_child = gtk_stack_get_visible_child (stack);

  gtk_container_foreach (GTK_CONTAINER (priv->tabs), update_visible_child, visible_child);
}

static void
tab_activated (GtkTab      *tab,
               GtkTabStrip *self)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);
  GtkWidget *widget;

  widget = gtk_tab_get_widget (tab);
  if (widget)
    gtk_stack_set_visible_child (priv->stack, widget);
}

static GtkTab *
gtk_tab_strip_real_create_tab (GtkTabStrip *self,
                               GtkWidget   *widget)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  return g_object_new (priv->closable ? GTK_TYPE_CLOSABLE_TAB : GTK_TYPE_SIMPLE_TAB,
                       "widget", widget,
                       NULL);
}

static void
gtk_tab_strip_stack_add (GtkTabStrip *self,
                         GtkWidget   *widget,
                         GtkStack    *stack)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);
  GtkTab *tab;
  gint position = 0;

  gtk_container_child_get (GTK_CONTAINER (stack), widget,
                           "position", &position,
                           NULL);

  g_signal_emit (self, signals[CREATE_TAB], 0, widget, &tab);

  gtk_tab_set_edge (tab, priv->edge);

  g_object_set_data (G_OBJECT (widget), "GTK_TAB", tab);

  g_signal_connect (tab, "activate",
                    G_CALLBACK (tab_activated), self);

  g_signal_connect_object (widget, "child-notify::position",
                           G_CALLBACK (gtk_tab_strip_child_position_changed), self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (widget, "child-notify::title",
                           G_CALLBACK (gtk_tab_strip_child_title_changed), self,
                           G_CONNECT_SWAPPED);

  gtk_box_pack_start (GTK_BOX (priv->tabs), GTK_WIDGET (tab), TRUE, TRUE, 0);

  g_object_bind_property (widget, "visible", tab, "visible", G_BINDING_SYNC_CREATE);

  gtk_tab_strip_child_title_changed (self, NULL, widget);
  gtk_tab_strip_stack_notify_visible_child (self, NULL, stack);
}

static void
gtk_tab_strip_stack_remove (GtkTabStrip *self,
                            GtkWidget   *widget,
                            GtkStack    *stack)
{
  GtkTab *tab;

  tab = g_object_get_data (G_OBJECT (widget), "GTK_TAB");

  if (GTK_IS_TAB (tab))
    gtk_container_remove (GTK_CONTAINER (self), GTK_WIDGET (tab));
}

GtkWidget *
gtk_tab_strip_new (void)
{
  return g_object_new (GTK_TYPE_TAB_STRIP, NULL);
}

GtkStack *
gtk_tab_strip_get_stack (GtkTabStrip *self)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_TAB_STRIP (self), NULL);

  return priv->stack;
}

static void
gtk_tab_strip_cold_plug (GtkWidget *widget,
                         gpointer   user_data)
{
  GtkTabStrip *self = user_data;
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  gtk_tab_strip_stack_add (self, widget, priv->stack);
}

void
gtk_tab_strip_set_stack (GtkTabStrip *self,
                         GtkStack    *stack)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  g_return_if_fail (GTK_IS_TAB_STRIP (self));
  g_return_if_fail (!stack || GTK_IS_STACK (stack));

  if (priv->stack == stack)
    return;

  if (priv->stack != NULL)
    {
      g_signal_handlers_disconnect_by_func (priv->stack,
                                            G_CALLBACK (gtk_tab_strip_stack_notify_visible_child),
                                            self);

      g_signal_handlers_disconnect_by_func (priv->stack,
                                            G_CALLBACK (gtk_tab_strip_stack_add),
                                            self);

      g_signal_handlers_disconnect_by_func (priv->stack,
                                            G_CALLBACK (gtk_tab_strip_stack_remove),
                                            self);

      gtk_container_foreach (GTK_CONTAINER (self), (GtkCallback)gtk_widget_destroy, NULL);

      g_clear_object (&priv->stack);
    }

  if (stack != NULL)
    {
      priv->stack = g_object_ref (stack);

      g_signal_connect_object (priv->stack,
                               "notify::visible-child",
                               G_CALLBACK (gtk_tab_strip_stack_notify_visible_child),
                               self,
                               G_CONNECT_SWAPPED);

      g_signal_connect_object (priv->stack,
                               "add",
                               G_CALLBACK (gtk_tab_strip_stack_add),
                               self,
                               G_CONNECT_SWAPPED);

      g_signal_connect_object (priv->stack,
                               "remove",
                               G_CALLBACK (gtk_tab_strip_stack_remove),
                               self,
                               G_CONNECT_SWAPPED);

      gtk_container_foreach (GTK_CONTAINER (priv->stack),
                             gtk_tab_strip_cold_plug,
                             self);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STACK]);
}

GtkPositionType
gtk_tab_strip_get_edge (GtkTabStrip *self)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_TAB_STRIP (self), GTK_POS_TOP);

  return priv->edge;
}

static void
update_edge (GtkWidget *widget,
             gpointer   data)
{
  GtkTabStrip *self = data;
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  if (GTK_IS_TAB (widget))
    gtk_tab_set_edge (GTK_TAB (widget), priv->edge);
}

void
gtk_tab_strip_set_edge (GtkTabStrip     *self,
                        GtkPositionType  edge)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);
  GtkStyleContext *context;
  GtkOrientation orientation;
  GtkWidget *image;
  const char *classes[] = {
    "left",
    "right",
    "top",
    "bottom"
  };
  GtkOrientation orientations[] = {
    GTK_ORIENTATION_VERTICAL,
    GTK_ORIENTATION_VERTICAL,
    GTK_ORIENTATION_HORIZONTAL,
    GTK_ORIENTATION_HORIZONTAL
  };
  const char *start_icon[] = {
    "pan-start-symbolic",
    "pan-up-symbolic"
  };
  const char *end_icon[] = {
    "pan-end-symbolic",
    "pan-down-symbolic"
  };

  g_return_if_fail (GTK_IS_TAB_STRIP (self));

  if (priv->edge == edge)
    return;

  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_remove_class (context, classes[priv->edge]);

  priv->edge = edge;

  gtk_style_context_add_class (context, classes[priv->edge]);

  orientation = orientations[priv->edge];
  gtk_box_gadget_set_orientation (GTK_BOX_GADGET (priv->gadget), orientation);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (priv->tabs), orientation);

  image = gtk_button_get_image (GTK_BUTTON (priv->start_scroll));
  gtk_image_set_from_icon_name (GTK_IMAGE (image), start_icon[orientation], GTK_ICON_SIZE_MENU);

  image = gtk_button_get_image (GTK_BUTTON (priv->end_scroll));
  gtk_image_set_from_icon_name (GTK_IMAGE (image), end_icon[orientation], GTK_ICON_SIZE_MENU);

  update_scrolling (self);
  adjustment_changed (self);

  gtk_container_foreach (GTK_CONTAINER (priv->tabs), update_edge, self);

  update_node_ordering (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EDGE]);
}

void
gtk_tab_strip_set_closable (GtkTabStrip *self,
                            gboolean     closable)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  g_return_if_fail (GTK_IS_TAB_STRIP (self));

  if (priv->closable == closable)
    return;

  priv->closable = closable;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CLOSABLE]);
}

gboolean
gtk_tab_strip_get_closable (GtkTabStrip *self)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_TAB_STRIP (self), FALSE);

  return priv->closable;
}

void
gtk_tab_strip_set_scrollable (GtkTabStrip *self,
                              gboolean     scrollable)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  g_return_if_fail (GTK_IS_TAB_STRIP (self));

  if (priv->scrollable == scrollable)
    return;

  priv->scrollable = scrollable;

  update_scrolling (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SCROLLABLE]);
}

gboolean
gtk_tab_strip_get_scrollable (GtkTabStrip *self)
{
  GtkTabStripPrivate *priv = gtk_tab_strip_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_TAB_STRIP (self), FALSE);

  return priv->scrollable;
}
