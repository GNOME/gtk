/*
 * Copyright (c) 2013 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 *
 */

#include "config.h"

#include <gtk/gtk.h>
#include "gtkstack.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include <math.h>
#include <string.h>

/**
 * SECTION:gtkstack
 * @Short_description: A stacking container
 * @Title: GtkStack
 * @See_also: #GtkNotebook, #GtkStackSwitcher
 *
 * The GtkStack widget is a container which only shows
 * one of its children at a time. In contrast to GtkNotebook,
 * GtkStack does not provide a means for users to change the
 * visible child. Instead, the #GtkStackSwitcher widget can be
 * used with GtkStack to provide this functionality.
 *
 * Transitions between pages can be animated as slides or
 * fades. This can be controlled with gtk_stack_set_transition_type().
 * These animations respect the #GtkSettings:gtk-enable-animations
 * setting.
 *
 * The GtkStack widget was added in GTK+ 3.10.
 */

/**
 * GtkStackTransitionType:
 * @GTK_STACK_TRANSITION_TYPE_NONE: No transition
 * @GTK_STACK_TRANSITION_TYPE_CROSSFADE: A cross-fade
 * @GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT: Slide from left to right
 * @GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT: Slide from right to left
 * @GTK_STACK_TRANSITION_TYPE_SLIDE_UP: Slide from bottom up
 * @GTK_STACK_TRANSITION_TYPE_SLIDE_DOWN: Slide from top down
 * @GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT: Slide from left or right according to the children order
 * @GTK_STACK_TRANSITION_TYPE_SLIDE_UP_DOWN: Slide from top down or bottom up according to the order
 * @GTK_STACK_TRANSITION_TYPE_OVER_UP: Cover the old page by sliding up. Since 3.12
 * @GTK_STACK_TRANSITION_TYPE_OVER_DOWN: Cover the old page by sliding down. Since: 3.12
 * @GTK_STACK_TRANSITION_TYPE_OVER_LEFT: Cover the old page by sliding to the left. Since: 3.12
 * @GTK_STACK_TRANSITION_TYPE_OVER_RIGHT: Cover the old page by sliding to the right. Since: 3.12
 * @GTK_STACK_TRANSITION_TYPE_UNDER_UP: Uncover the new page by sliding up. Since 3.12
 * @GTK_STACK_TRANSITION_TYPE_UNDER_DOWN: Uncover the new page by sliding down. Since: 3.12
 * @GTK_STACK_TRANSITION_TYPE_UNDER_LEFT: Uncover the new page by sliding to the left. Since: 3.12
 * @GTK_STACK_TRANSITION_TYPE_UNDER_RIGHT: Uncover the new page by sliding to the right. Since: 3.12
 * @GTK_STACK_TRANSITION_TYPE_OVER_UP_DOWN: Cover the old page sliding up or uncover the new page sliding down, according to order. Since: 3.12
 * @GTK_STACK_TRANSITION_TYPE_OVER_DOWN_UP: Cover the old page sliding down or uncover the new page sliding up, according to order. Since: 3.14
 * @GTK_STACK_TRANSITION_TYPE_OVER_LEFT_RIGHT: Cover the old page sliding left or uncover the new page sliding right, according to order. Since: 3.14
 * @GTK_STACK_TRANSITION_TYPE_OVER_RIGHT_LEFT: Cover the old page sliding right or uncover the new page sliding left, according to order. Since: 3.14
 *
 * These enumeration values describe the possible transitions
 * between pages in a #GtkStack widget.
 *
 * New values may be added to this enumeration over time.
 */

/* TODO:
 *  filter events out events to the last_child widget during transitions
 */

enum  {
  PROP_0,
  PROP_HOMOGENEOUS,
  PROP_HHOMOGENEOUS,
  PROP_VHOMOGENEOUS,
  PROP_VISIBLE_CHILD,
  PROP_VISIBLE_CHILD_NAME,
  PROP_TRANSITION_DURATION,
  PROP_TRANSITION_TYPE,
  PROP_TRANSITION_RUNNING,
  LAST_PROP
};

enum
{
  CHILD_PROP_0,
  CHILD_PROP_NAME,
  CHILD_PROP_TITLE,
  CHILD_PROP_ICON_NAME,
  CHILD_PROP_POSITION,
  CHILD_PROP_NEEDS_ATTENTION
};

typedef struct _GtkStackChildInfo GtkStackChildInfo;

struct _GtkStackChildInfo {
  GtkWidget *widget;
  gchar *name;
  gchar *title;
  gchar *icon_name;
  gboolean needs_attention;
  GtkWidget *last_focus;
};

typedef struct {
  GList *children;

  GdkWindow* bin_window;
  GdkWindow* view_window;

  GtkStackChildInfo *visible_child;

  gboolean hhomogeneous;
  gboolean vhomogeneous;

  GtkStackTransitionType transition_type;
  guint transition_duration;

  GtkStackChildInfo *last_visible_child;
  cairo_surface_t *last_visible_surface;
  GtkAllocation last_visible_surface_allocation;
  gdouble transition_pos;
  guint tick_id;
  gint64 start_time;
  gint64 end_time;

  GtkStackTransitionType active_transition_type;
} GtkStackPrivate;

static GParamSpec *stack_props[LAST_PROP] = { NULL, };

static void     gtk_stack_add                            (GtkContainer  *widget,
                                                          GtkWidget     *child);
static void     gtk_stack_remove                         (GtkContainer  *widget,
                                                          GtkWidget     *child);
static void     gtk_stack_forall                         (GtkContainer  *container,
                                                          gboolean       include_internals,
                                                          GtkCallback    callback,
                                                          gpointer       callback_data);
static void     gtk_stack_compute_expand                 (GtkWidget     *widget,
                                                          gboolean      *hexpand,
                                                          gboolean      *vexpand);
static void     gtk_stack_size_allocate                  (GtkWidget     *widget,
                                                          GtkAllocation *allocation);
static gboolean gtk_stack_draw                           (GtkWidget     *widget,
                                                          cairo_t       *cr);
static void     gtk_stack_get_preferred_height           (GtkWidget     *widget,
                                                          gint          *minimum_height,
                                                          gint          *natural_height);
static void     gtk_stack_get_preferred_height_for_width (GtkWidget     *widget,
                                                          gint           width,
                                                          gint          *minimum_height,
                                                          gint          *natural_height);
static void     gtk_stack_get_preferred_width            (GtkWidget     *widget,
                                                          gint          *minimum_width,
                                                          gint          *natural_width);
static void     gtk_stack_get_preferred_width_for_height (GtkWidget     *widget,
                                                          gint           height,
                                                          gint          *minimum_width,
                                                          gint          *natural_width);
static void     gtk_stack_finalize                       (GObject       *obj);
static void     gtk_stack_get_property                   (GObject       *object,
                                                          guint          property_id,
                                                          GValue        *value,
                                                          GParamSpec    *pspec);
static void     gtk_stack_set_property                   (GObject       *object,
                                                          guint          property_id,
                                                          const GValue  *value,
                                                          GParamSpec    *pspec);
static void     gtk_stack_get_child_property             (GtkContainer  *container,
                                                          GtkWidget     *child,
                                                          guint          property_id,
                                                          GValue        *value,
                                                          GParamSpec    *pspec);
static void     gtk_stack_set_child_property             (GtkContainer  *container,
                                                          GtkWidget     *child,
                                                          guint          property_id,
                                                          const GValue  *value,
                                                          GParamSpec    *pspec);
static void     gtk_stack_unschedule_ticks               (GtkStack      *stack);
static gint     get_bin_window_x                         (GtkStack      *stack,
                                                          GtkAllocation *allocation);
static gint     get_bin_window_y                         (GtkStack      *stack,
                                                          GtkAllocation *allocation);

G_DEFINE_TYPE_WITH_PRIVATE (GtkStack, gtk_stack, GTK_TYPE_CONTAINER)

static void
gtk_stack_init (GtkStack *stack)
{
  gtk_widget_set_has_window ((GtkWidget*) stack, TRUE);
  gtk_widget_set_redraw_on_allocate ((GtkWidget*) stack, TRUE);
}

static void
gtk_stack_dispose (GObject *obj)
{
  GtkStack *stack = GTK_STACK (obj);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  priv->visible_child = NULL;

  G_OBJECT_CLASS (gtk_stack_parent_class)->dispose (obj);
}

static void
gtk_stack_finalize (GObject *obj)
{
  GtkStack *stack = GTK_STACK (obj);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  gtk_stack_unschedule_ticks (stack);

  if (priv->last_visible_surface != NULL)
    cairo_surface_destroy (priv->last_visible_surface);

  G_OBJECT_CLASS (gtk_stack_parent_class)->finalize (obj);
}

static void
gtk_stack_get_property (GObject   *object,
                        guint       property_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  GtkStack *stack = GTK_STACK (object);

  switch (property_id)
    {
    case PROP_HOMOGENEOUS:
      g_value_set_boolean (value, gtk_stack_get_homogeneous (stack));
      break;
    case PROP_HHOMOGENEOUS:
      g_value_set_boolean (value, gtk_stack_get_hhomogeneous (stack));
      break;
    case PROP_VHOMOGENEOUS:
      g_value_set_boolean (value, gtk_stack_get_vhomogeneous (stack));
      break;
    case PROP_VISIBLE_CHILD:
      g_value_set_object (value, gtk_stack_get_visible_child (stack));
      break;
    case PROP_VISIBLE_CHILD_NAME:
      g_value_set_string (value, gtk_stack_get_visible_child_name (stack));
      break;
    case PROP_TRANSITION_DURATION:
      g_value_set_uint (value, gtk_stack_get_transition_duration (stack));
      break;
    case PROP_TRANSITION_TYPE:
      g_value_set_enum (value, gtk_stack_get_transition_type (stack));
      break;
    case PROP_TRANSITION_RUNNING:
      g_value_set_boolean (value, gtk_stack_get_transition_running (stack));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_stack_set_property (GObject     *object,
                        guint         property_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  GtkStack *stack = GTK_STACK (object);

  switch (property_id)
    {
    case PROP_HOMOGENEOUS:
      gtk_stack_set_homogeneous (stack, g_value_get_boolean (value));
      break;
    case PROP_HHOMOGENEOUS:
      gtk_stack_set_hhomogeneous (stack, g_value_get_boolean (value));
      break;
    case PROP_VHOMOGENEOUS:
      gtk_stack_set_vhomogeneous (stack, g_value_get_boolean (value));
      break;
    case PROP_VISIBLE_CHILD:
      gtk_stack_set_visible_child (stack, g_value_get_object (value));
      break;
    case PROP_VISIBLE_CHILD_NAME:
      gtk_stack_set_visible_child_name (stack, g_value_get_string (value));
      break;
    case PROP_TRANSITION_DURATION:
      gtk_stack_set_transition_duration (stack, g_value_get_uint (value));
      break;
    case PROP_TRANSITION_TYPE:
      gtk_stack_set_transition_type (stack, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_stack_realize (GtkWidget *widget)
{
  GtkStack *stack = GTK_STACK (widget);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkAllocation allocation;
  GdkWindowAttr attributes = { 0 };
  GdkWindowAttributesType attributes_mask;
  GtkStackChildInfo *info;
  GList *l;

  gtk_widget_set_realized (widget, TRUE);

  gtk_widget_get_allocation (widget, &allocation);

  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.event_mask =
    gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;
  attributes_mask = (GDK_WA_X | GDK_WA_Y) | GDK_WA_VISUAL;

  priv->view_window =
    gdk_window_new (gtk_widget_get_parent_window ((GtkWidget*) stack),
                    &attributes, attributes_mask);
  gtk_widget_set_window (widget, priv->view_window);
  gtk_widget_register_window (widget, priv->view_window);

  attributes.x = get_bin_window_x (stack, &allocation);
  attributes.y = 0;
  attributes.width = allocation.width;
  attributes.height = allocation.height;

  for (l = priv->children; l != NULL; l = l->next)
    {
      info = l->data;
      attributes.event_mask |= gtk_widget_get_events (info->widget);
    }

  priv->bin_window =
    gdk_window_new (priv->view_window, &attributes, attributes_mask);
  gtk_widget_register_window (widget, priv->bin_window);

  for (l = priv->children; l != NULL; l = l->next)
    {
      info = l->data;

      gtk_widget_set_parent_window (info->widget, priv->bin_window);
    }

  gdk_window_show (priv->bin_window);
}

static void
gtk_stack_unrealize (GtkWidget *widget)
{
  GtkStack *stack = GTK_STACK (widget);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  gtk_widget_unregister_window (widget, priv->bin_window);
  gdk_window_destroy (priv->bin_window);
  priv->view_window = NULL;

  GTK_WIDGET_CLASS (gtk_stack_parent_class)->unrealize (widget);
}

static void
gtk_stack_class_init (GtkStackClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->get_property = gtk_stack_get_property;
  object_class->set_property = gtk_stack_set_property;
  object_class->dispose = gtk_stack_dispose;
  object_class->finalize = gtk_stack_finalize;

  widget_class->size_allocate = gtk_stack_size_allocate;
  widget_class->draw = gtk_stack_draw;
  widget_class->realize = gtk_stack_realize;
  widget_class->unrealize = gtk_stack_unrealize;
  widget_class->get_preferred_height = gtk_stack_get_preferred_height;
  widget_class->get_preferred_height_for_width = gtk_stack_get_preferred_height_for_width;
  widget_class->get_preferred_width = gtk_stack_get_preferred_width;
  widget_class->get_preferred_width_for_height = gtk_stack_get_preferred_width_for_height;
  widget_class->compute_expand = gtk_stack_compute_expand;

  container_class->add = gtk_stack_add;
  container_class->remove = gtk_stack_remove;
  container_class->forall = gtk_stack_forall;
  container_class->set_child_property = gtk_stack_set_child_property;
  container_class->get_child_property = gtk_stack_get_child_property;
  gtk_container_class_handle_border_width (container_class);

  stack_props[PROP_HOMOGENEOUS] =
      g_param_spec_boolean ("homogeneous", P_("Homogeneous"), P_("Homogeneous sizing"),
                            TRUE,
                            GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkStack:hhomogeneous:
   *
   * %TRUE if the stack allocates the same width for all children.
   *
   * Since: 3.16
   */
  stack_props[PROP_HHOMOGENEOUS] =
      g_param_spec_boolean ("hhomogeneous", P_("Horizontally homogeneous"), P_("Horizontally homogeneous sizing"),
                            TRUE,
                            GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkStack:vhomogeneous:
   *
   * %TRUE if the stack allocates the same height for all children.
   *
   * Since: 3.16
   */
  stack_props[PROP_VHOMOGENEOUS] =
      g_param_spec_boolean ("vhomogeneous", P_("Vertically homogeneous"), P_("Vertically homogeneous sizing"),
                            TRUE,
                            GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_EXPLICIT_NOTIFY);
  stack_props[PROP_VISIBLE_CHILD] =
      g_param_spec_object ("visible-child", P_("Visible child"), P_("The widget currently visible in the stack"),
                           GTK_TYPE_WIDGET,
                           GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
  stack_props[PROP_VISIBLE_CHILD_NAME] =
      g_param_spec_string ("visible-child-name", P_("Name of visible child"), P_("The name of the widget currently visible in the stack"),
                           NULL,
                           GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
  stack_props[PROP_TRANSITION_DURATION] =
      g_param_spec_uint ("transition-duration", P_("Transition duration"), P_("The animation duration, in milliseconds"),
                         0, G_MAXUINT, 200,
                         GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_EXPLICIT_NOTIFY);
  stack_props[PROP_TRANSITION_TYPE] =
      g_param_spec_enum ("transition-type", P_("Transition type"), P_("The type of animation used to transition"),
                         GTK_TYPE_STACK_TRANSITION_TYPE, GTK_STACK_TRANSITION_TYPE_NONE,
                         GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_EXPLICIT_NOTIFY);
  stack_props[PROP_TRANSITION_RUNNING] =
      g_param_spec_boolean ("transition-running", P_("Transition running"), P_("Whether or not the transition is currently running"),
                            FALSE,
                            GTK_PARAM_READABLE);

  g_object_class_install_properties (object_class, LAST_PROP, stack_props);

  gtk_container_class_install_child_property (container_class, CHILD_PROP_NAME,
    g_param_spec_string ("name",
                         P_("Name"),
                         P_("The name of the child page"),
                         NULL,
                         GTK_PARAM_READWRITE));

  gtk_container_class_install_child_property (container_class, CHILD_PROP_TITLE,
    g_param_spec_string ("title",
                         P_("Title"),
                         P_("The title of the child page"),
                         NULL,
                         GTK_PARAM_READWRITE));

  gtk_container_class_install_child_property (container_class, CHILD_PROP_ICON_NAME,
    g_param_spec_string ("icon-name",
                         P_("Icon name"),
                         P_("The icon name of the child page"),
                         NULL,
                         GTK_PARAM_READWRITE));

  gtk_container_class_install_child_property (container_class, CHILD_PROP_POSITION,
    g_param_spec_int ("position",
                      P_("Position"),
                      P_("The index of the child in the parent"),
                      -1, G_MAXINT, 0,
                      GTK_PARAM_READWRITE));

  /**
   * GtkStack:needs-attention:
   *
   * Sets a flag specifying whether the child requires the user attention.
   * This is used by the #GtkStackSwitcher to change the appearance of the
   * corresponding button when a page needs attention and it is not the
   * current one.
   *
   * Since: 3.12
   */
  gtk_container_class_install_child_property (container_class, CHILD_PROP_NEEDS_ATTENTION,
    g_param_spec_boolean ("needs-attention",
                         P_("Needs Attention"),
                         P_("Whether this page needs attention"),
                         FALSE,
                         GTK_PARAM_READWRITE));
}

/**
 * gtk_stack_new:
 *
 * Creates a new #GtkStack container.
 *
 * Returns: a new #GtkStack
 *
 * Since: 3.10
 */
GtkWidget *
gtk_stack_new (void)
{
  return g_object_new (GTK_TYPE_STACK, NULL);
}

static GtkStackChildInfo *
find_child_info_for_widget (GtkStack  *stack,
                            GtkWidget *child)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkStackChildInfo *info;
  GList *l;

  for (l = priv->children; l != NULL; l = l->next)
    {
      info = l->data;
      if (info->widget == child)
        return info;
    }

  return NULL;
}

static void
reorder_child (GtkStack  *stack,
               GtkWidget *child,
               gint       position)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GList *l;
  GList *old_link = NULL;
  GList *new_link = NULL;
  GtkStackChildInfo *child_info = NULL;
  gint num = 0;

  l = priv->children;

  /* Loop to find the old position and link of child, new link of child and
   * total number of children. new_link will be NULL if the child should be
   * moved to the end (in case of position being < 0 || >= num)
   */
  while (l && (new_link == NULL || old_link == NULL))
    {
      /* Record the new position if found */
      if (position == num)
        new_link = l;

      if (old_link == NULL)
        {
          GtkStackChildInfo *info;
          info = l->data;

          /* Keep trying to find the current position and link location of the child */
          if (info->widget == child)
            {
              old_link = l;
              child_info = info;
            }
        }

      l = l->next;
      num++;
    }

  g_return_if_fail (old_link != NULL);

  if (old_link == new_link || (old_link->next == NULL && new_link == NULL))
    return;

  priv->children = g_list_delete_link (priv->children, old_link);
  priv->children = g_list_insert_before (priv->children, new_link, child_info);

  gtk_widget_child_notify (child, "position");
}

static void
gtk_stack_get_child_property (GtkContainer *container,
                              GtkWidget    *child,
                              guint         property_id,
                              GValue       *value,
                              GParamSpec   *pspec)
{
  GtkStack *stack = GTK_STACK (container);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkStackChildInfo *info;

  info = find_child_info_for_widget (stack, child);
  if (info == NULL)
    {
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      return;
    }

  switch (property_id)
    {
    case CHILD_PROP_NAME:
      g_value_set_string (value, info->name);
      break;

    case CHILD_PROP_TITLE:
      g_value_set_string (value, info->title);
      break;

    case CHILD_PROP_ICON_NAME:
      g_value_set_string (value, info->icon_name);
      break;

    case CHILD_PROP_POSITION:
      g_value_set_int (value, g_list_index (priv->children, info));
      break;

    case CHILD_PROP_NEEDS_ATTENTION:
      g_value_set_boolean (value, info->needs_attention);
      break;

    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
gtk_stack_set_child_property (GtkContainer *container,
                              GtkWidget    *child,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkStack *stack = GTK_STACK (container);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkStackChildInfo *info;
  GtkStackChildInfo *info2;
  gchar *name;
  GList *l;

  info = find_child_info_for_widget (stack, child);
  if (info == NULL)
    {
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      return;
    }

  switch (property_id)
    {
    case CHILD_PROP_NAME:
      name = g_value_dup_string (value);
      for (l = priv->children; l != NULL; l = l->next)
        {
          info2 = l->data;
          if (info == info2)
            continue;
          if (g_strcmp0 (info2->name, name) == 0)
            {
              g_warning ("Duplicate child name in GtkStack: %s\n", name);
              break;
            }
        }

      g_free (info->name);
      info->name = name;

      gtk_container_child_notify (container, child, "name");

      if (priv->visible_child == info)
        g_object_notify_by_pspec (G_OBJECT (stack),
                                  stack_props[PROP_VISIBLE_CHILD_NAME]);

      break;

    case CHILD_PROP_TITLE:
      g_free (info->title);
      info->title = g_value_dup_string (value);
      gtk_container_child_notify (container, child, "title");
      break;

    case CHILD_PROP_ICON_NAME:
      g_free (info->icon_name);
      info->icon_name = g_value_dup_string (value);
      gtk_container_child_notify (container, child, "icon-name");
      break;

    case CHILD_PROP_POSITION:
      reorder_child (stack, child, g_value_get_int (value));
      break;

    case CHILD_PROP_NEEDS_ATTENTION:
      info->needs_attention = g_value_get_boolean (value);
      gtk_container_child_notify (container, child, "needs-attention");
      break;

    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

/* From clutter-easing.c, based on Robert Penner's
 * infamous easing equations, MIT license.
 */
static double
ease_out_cubic (double t)
{
  double p = t - 1;
  return p * p * p + 1;
}

static inline gboolean
is_left_transition (GtkStackTransitionType transition_type)
{
  return (transition_type == GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_LEFT);
}

static inline gboolean
is_right_transition (GtkStackTransitionType transition_type)
{
  return (transition_type == GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_RIGHT);
}

static inline gboolean
is_up_transition (GtkStackTransitionType transition_type)
{
  return (transition_type == GTK_STACK_TRANSITION_TYPE_SLIDE_UP ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_UP);
}

static inline gboolean
is_down_transition (GtkStackTransitionType transition_type)
{
  return (transition_type == GTK_STACK_TRANSITION_TYPE_SLIDE_DOWN ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_DOWN);
}

/* Transitions that cause the bin window to move */
static inline gboolean
is_window_moving_transition (GtkStackTransitionType transition_type)
{
  return (transition_type == GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT ||
          transition_type == GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT ||
          transition_type == GTK_STACK_TRANSITION_TYPE_SLIDE_UP ||
          transition_type == GTK_STACK_TRANSITION_TYPE_SLIDE_DOWN ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_UP ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_DOWN ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_LEFT ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_RIGHT);
}

/* Transitions that change direction depending on the relative order of the
old and new child */
static inline gboolean
is_direction_dependent_transition (GtkStackTransitionType transition_type)
{
  return (transition_type == GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT ||
          transition_type == GTK_STACK_TRANSITION_TYPE_SLIDE_UP_DOWN ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_UP_DOWN ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_DOWN_UP ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_LEFT_RIGHT ||
          transition_type == GTK_STACK_TRANSITION_TYPE_OVER_RIGHT_LEFT);
}

/* Returns simple transition type for a direction dependent transition, given
whether the new child (the one being switched to) is first in the stacking order
(added earlier). */
static inline GtkStackTransitionType
get_simple_transition_type (gboolean               new_child_first,
                            GtkStackTransitionType transition_type)
{
  switch (transition_type)
    {
    case GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT:
      return new_child_first ? GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT : GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT;
    case GTK_STACK_TRANSITION_TYPE_SLIDE_UP_DOWN:
      return new_child_first ? GTK_STACK_TRANSITION_TYPE_SLIDE_DOWN : GTK_STACK_TRANSITION_TYPE_SLIDE_UP;
    case GTK_STACK_TRANSITION_TYPE_OVER_UP_DOWN:
      return new_child_first ? GTK_STACK_TRANSITION_TYPE_UNDER_DOWN : GTK_STACK_TRANSITION_TYPE_OVER_UP;
    case GTK_STACK_TRANSITION_TYPE_OVER_DOWN_UP:
      return new_child_first ? GTK_STACK_TRANSITION_TYPE_UNDER_UP : GTK_STACK_TRANSITION_TYPE_OVER_DOWN;
    case GTK_STACK_TRANSITION_TYPE_OVER_LEFT_RIGHT:
      return new_child_first ? GTK_STACK_TRANSITION_TYPE_UNDER_RIGHT : GTK_STACK_TRANSITION_TYPE_OVER_LEFT;
    case GTK_STACK_TRANSITION_TYPE_OVER_RIGHT_LEFT:
      return new_child_first ? GTK_STACK_TRANSITION_TYPE_UNDER_LEFT : GTK_STACK_TRANSITION_TYPE_OVER_RIGHT;
    default: ;
    }
  return transition_type;
}

static gint
get_bin_window_x (GtkStack      *stack,
                  GtkAllocation *allocation)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  int x = 0;

  if (priv->transition_pos < 1.0)
    {
      if (is_left_transition (priv->active_transition_type))
        x = allocation->width * (1 - ease_out_cubic (priv->transition_pos));
      if (is_right_transition (priv->active_transition_type))
        x = -allocation->width * (1 - ease_out_cubic (priv->transition_pos));
    }

  return x;
}

static gint
get_bin_window_y (GtkStack      *stack,
                  GtkAllocation *allocation)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  int y = 0;

  if (priv->transition_pos < 1.0)
    {
      if (is_up_transition (priv->active_transition_type))
        y = allocation->height * (1 - ease_out_cubic (priv->transition_pos));
      if (is_down_transition(priv->active_transition_type))
        y = -allocation->height * (1 - ease_out_cubic (priv->transition_pos));
    }

  return y;
}

static gboolean
gtk_stack_set_transition_position (GtkStack *stack,
                                   gdouble   pos)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  gboolean done;

  priv->transition_pos = pos;
  gtk_widget_queue_draw (GTK_WIDGET (stack));

  if (priv->bin_window != NULL &&
      is_window_moving_transition (priv->active_transition_type))
    {
      GtkAllocation allocation;
      gtk_widget_get_allocation (GTK_WIDGET (stack), &allocation);
      gdk_window_move (priv->bin_window,
                       get_bin_window_x (stack, &allocation), get_bin_window_y (stack, &allocation));
    }

  done = pos >= 1.0;

  if (done || priv->last_visible_surface != NULL)
    {
      if (priv->last_visible_child)
        {
          gtk_widget_set_child_visible (priv->last_visible_child->widget, FALSE);
          priv->last_visible_child = NULL;
        }
    }

  if (done)
    {
      if (priv->last_visible_surface != NULL)
        {
          cairo_surface_destroy (priv->last_visible_surface);
          priv->last_visible_surface = NULL;
        }

      gtk_widget_queue_resize (GTK_WIDGET (stack));
    }

  return done;
}

static gboolean
gtk_stack_transition_cb (GtkWidget     *widget,
                         GdkFrameClock *frame_clock,
                         gpointer       user_data)
{
  GtkStack *stack = GTK_STACK (widget);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  gint64 now;
  gdouble t;

  now = gdk_frame_clock_get_frame_time (frame_clock);

  t = 1.0;
  if (now < priv->end_time)
    t = (now - priv->start_time) / (double) (priv->end_time - priv->start_time);

  /* Finish animation early if not mapped anymore */
  if (!gtk_widget_get_mapped (GTK_WIDGET (stack)))
    t = 1.0;

  if (gtk_stack_set_transition_position (stack, t))
    {
      priv->tick_id = 0;
      g_object_notify_by_pspec (G_OBJECT (stack), stack_props[PROP_TRANSITION_RUNNING]);

      return FALSE;
    }

  return TRUE;
}

static void
gtk_stack_schedule_ticks (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  if (priv->tick_id == 0)
    {
      priv->tick_id =
        gtk_widget_add_tick_callback (GTK_WIDGET (stack), gtk_stack_transition_cb, stack, NULL);
      g_object_notify_by_pspec (G_OBJECT (stack), stack_props[PROP_TRANSITION_RUNNING]);
    }
}

static void
gtk_stack_unschedule_ticks (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  if (priv->tick_id != 0)
    {
      gtk_widget_remove_tick_callback (GTK_WIDGET (stack), priv->tick_id);
      priv->tick_id = 0;
      g_object_notify_by_pspec (G_OBJECT (stack), stack_props[PROP_TRANSITION_RUNNING]);
    }
}

static GtkStackTransitionType
effective_transition_type (GtkStack               *stack,
                           GtkStackTransitionType  transition_type)
{
  if (gtk_widget_get_direction (GTK_WIDGET (stack)) == GTK_TEXT_DIR_RTL)
    {
      switch (transition_type)
        {
        case GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT:
          return GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT;
        case GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT:
          return GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT;
        case GTK_STACK_TRANSITION_TYPE_OVER_LEFT:
          return GTK_STACK_TRANSITION_TYPE_OVER_RIGHT;
        case GTK_STACK_TRANSITION_TYPE_OVER_RIGHT:
          return GTK_STACK_TRANSITION_TYPE_OVER_LEFT;
        case GTK_STACK_TRANSITION_TYPE_UNDER_LEFT:
          return GTK_STACK_TRANSITION_TYPE_UNDER_RIGHT;
        case GTK_STACK_TRANSITION_TYPE_UNDER_RIGHT:
          return GTK_STACK_TRANSITION_TYPE_UNDER_LEFT;
        default: ;
        }
    }

  return transition_type;
}

static void
gtk_stack_start_transition (GtkStack               *stack,
                            GtkStackTransitionType  transition_type,
                            guint                   transition_duration)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkWidget *widget = GTK_WIDGET (stack);
  gboolean animations_enabled;

  g_object_get (gtk_widget_get_settings (widget),
                "gtk-enable-animations", &animations_enabled,
                NULL);

  if (gtk_widget_get_mapped (widget) &&
      animations_enabled &&
      transition_type != GTK_STACK_TRANSITION_TYPE_NONE &&
      transition_duration != 0 &&
      priv->last_visible_child != NULL)
    {
      priv->transition_pos = 0.0;
      priv->start_time = gdk_frame_clock_get_frame_time (gtk_widget_get_frame_clock (widget));
      priv->end_time = priv->start_time + (transition_duration * 1000);
      priv->active_transition_type = effective_transition_type (stack, transition_type);
      gtk_stack_schedule_ticks (stack);
    }
  else
    {
      gtk_stack_unschedule_ticks (stack);
      priv->active_transition_type = GTK_STACK_TRANSITION_TYPE_NONE;
      gtk_stack_set_transition_position (stack, 1.0);
    }
}

static void
set_visible_child (GtkStack               *stack,
                   GtkStackChildInfo      *child_info,
                   GtkStackTransitionType  transition_type,
                   guint                   transition_duration)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkStackChildInfo *info;
  GtkWidget *widget = GTK_WIDGET (stack);
  GList *l;
  GtkWidget *toplevel;
  GtkWidget *focus;
  gboolean contains_focus = FALSE;

  /* If none, pick first visible */
  if (child_info == NULL)
    {
      for (l = priv->children; l != NULL; l = l->next)
        {
          info = l->data;
          if (gtk_widget_get_visible (info->widget))
            {
              child_info = info;
              break;
            }
        }
    }

  if (child_info == priv->visible_child)
    return;

  toplevel = gtk_widget_get_toplevel (widget);
  if (GTK_IS_WINDOW (toplevel))
    {
      focus = gtk_window_get_focus (GTK_WINDOW (toplevel));
      if (focus &&
          priv->visible_child &&
          priv->visible_child->widget &&
          gtk_widget_is_ancestor (focus, priv->visible_child->widget))
        {
          contains_focus = TRUE;

          if (priv->visible_child->last_focus)
            g_object_remove_weak_pointer (G_OBJECT (priv->visible_child->last_focus),
                                          (gpointer *)&priv->visible_child->last_focus);
          priv->visible_child->last_focus = focus;
          g_object_add_weak_pointer (G_OBJECT (priv->visible_child->last_focus),
                                     (gpointer *)&priv->visible_child->last_focus);
        }
    }

  if (priv->last_visible_child)
    gtk_widget_set_child_visible (priv->last_visible_child->widget, FALSE);
  priv->last_visible_child = NULL;

  if (priv->last_visible_surface != NULL)
    cairo_surface_destroy (priv->last_visible_surface);
  priv->last_visible_surface = NULL;

  if (priv->visible_child && priv->visible_child->widget)
    {
      if (gtk_widget_is_visible (widget))
        priv->last_visible_child = priv->visible_child;
      else
        gtk_widget_set_child_visible (priv->visible_child->widget, FALSE);
    }

  priv->visible_child = child_info;

  if (child_info)
    {
      gtk_widget_set_child_visible (child_info->widget, TRUE);

      if (contains_focus)
        {
          if (child_info->last_focus)
            gtk_widget_grab_focus (child_info->last_focus);
          else
            gtk_widget_child_focus (child_info->widget, GTK_DIR_TAB_FORWARD);
        }
    }

  if ((child_info == NULL || priv->last_visible_child == NULL) &&
      is_direction_dependent_transition (transition_type))
    {
      transition_type = GTK_STACK_TRANSITION_TYPE_NONE;
    }
  else if (is_direction_dependent_transition (transition_type))
    {
      gboolean i_first = FALSE;
      for (l = priv->children; l != NULL; l = l->next)
        {
	  if (child_info == l->data)
	    {
	      i_first = TRUE;
	      break;
	    }
	  if (priv->last_visible_child == l->data)
	    break;
        }

      transition_type = get_simple_transition_type (i_first, transition_type);
    }

  gtk_widget_queue_resize (GTK_WIDGET (stack));
  gtk_widget_queue_draw (GTK_WIDGET (stack));

  g_object_notify_by_pspec (G_OBJECT (stack), stack_props[PROP_VISIBLE_CHILD]);
  g_object_notify_by_pspec (G_OBJECT (stack),
                            stack_props[PROP_VISIBLE_CHILD_NAME]);

  gtk_stack_start_transition (stack, transition_type, transition_duration);
}

static void
stack_child_visibility_notify_cb (GObject    *obj,
                                  GParamSpec *pspec,
                                  gpointer    user_data)
{
  GtkStack *stack = GTK_STACK (user_data);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkWidget *child = GTK_WIDGET (obj);
  GtkStackChildInfo *child_info;

  child_info = find_child_info_for_widget (stack, child);

  if (priv->visible_child == NULL &&
      gtk_widget_get_visible (child))
    set_visible_child (stack, child_info, priv->transition_type, priv->transition_duration);
  else if (priv->visible_child == child_info &&
           !gtk_widget_get_visible (child))
    set_visible_child (stack, NULL, priv->transition_type, priv->transition_duration);

  if (child_info == priv->last_visible_child)
    {
      gtk_widget_set_child_visible (priv->last_visible_child->widget, FALSE);
      priv->last_visible_child = NULL;
    }
}

/**
 * gtk_stack_add_titled:
 * @stack: a #GtkStack
 * @child: the widget to add
 * @name: the name for @child
 * @title: a human-readable title for @child
 *
 * Adds a child to @stack.
 * The child is identified by the @name. The @title
 * will be used by #GtkStackSwitcher to represent
 * @child in a tab bar, so it should be short.
 *
 * Since: 3.10
 */
void
gtk_stack_add_titled (GtkStack   *stack,
                     GtkWidget   *child,
                     const gchar *name,
                     const gchar *title)
{
  g_return_if_fail (GTK_IS_STACK (stack));
  g_return_if_fail (GTK_IS_WIDGET (child));

  gtk_container_add_with_properties (GTK_CONTAINER (stack),
                                     child,
                                     "name", name,
                                     "title", title,
                                     NULL);
}

/**
 * gtk_stack_add_named:
 * @stack: a #GtkStack
 * @child: the widget to add
 * @name: the name for @child
 *
 * Adds a child to @stack.
 * The child is identified by the @name.
 *
 * Since: 3.10
 */
void
gtk_stack_add_named (GtkStack   *stack,
                    GtkWidget   *child,
                    const gchar *name)
{
  g_return_if_fail (GTK_IS_STACK (stack));
  g_return_if_fail (GTK_IS_WIDGET (child));

  gtk_container_add_with_properties (GTK_CONTAINER (stack),
                                     child,
                                     "name", name,
                                     NULL);
}

static void
gtk_stack_add (GtkContainer *container,
              GtkWidget     *child)
{
  GtkStack *stack = GTK_STACK (container);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkStackChildInfo *child_info;

  g_return_if_fail (child != NULL);

  child_info = g_slice_new (GtkStackChildInfo);
  child_info->widget = child;
  child_info->name = NULL;
  child_info->title = NULL;
  child_info->icon_name = NULL;
  child_info->needs_attention = FALSE;
  child_info->last_focus = NULL;

  priv->children = g_list_append (priv->children, child_info);

  gtk_widget_set_parent_window (child, priv->bin_window);
  gtk_widget_set_parent (child, GTK_WIDGET (stack));

  if (priv->bin_window)
    gdk_window_set_events (priv->bin_window,
                           gdk_window_get_events (priv->bin_window) |
                           gtk_widget_get_events (child));

  g_signal_connect (child, "notify::visible",
                    G_CALLBACK (stack_child_visibility_notify_cb), stack);

  gtk_widget_child_notify (child, "position");

  if (priv->visible_child == NULL &&
      gtk_widget_get_visible (child))
    set_visible_child (stack, child_info, priv->transition_type, priv->transition_duration);
  else
    gtk_widget_set_child_visible (child, FALSE);

  if (priv->hhomogeneous || priv->vhomogeneous || priv->visible_child == child_info)
    gtk_widget_queue_resize (GTK_WIDGET (stack));
}

static void
gtk_stack_remove (GtkContainer *container,
                  GtkWidget    *child)
{
  GtkStack *stack = GTK_STACK (container);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkStackChildInfo *child_info;
  gboolean was_visible;

  child_info = find_child_info_for_widget (stack, child);
  if (child_info == NULL)
    return;

  priv->children = g_list_remove (priv->children, child_info);

  g_signal_handlers_disconnect_by_func (child,
                                        stack_child_visibility_notify_cb,
                                        stack);

  was_visible = gtk_widget_get_visible (child);

  child_info->widget = NULL;

  if (priv->visible_child == child_info)
    set_visible_child (stack, NULL, priv->transition_type, priv->transition_duration);

  if (priv->last_visible_child == child_info)
    priv->last_visible_child = NULL;

  gtk_widget_unparent (child);

  g_free (child_info->name);
  g_free (child_info->title);
  g_free (child_info->icon_name);

  if (child_info->last_focus)
    g_object_remove_weak_pointer (G_OBJECT (child_info->last_focus),
                                  (gpointer *)&child_info->last_focus);

  g_slice_free (GtkStackChildInfo, child_info);

  if ((priv->hhomogeneous || priv->vhomogeneous) && was_visible)
    gtk_widget_queue_resize (GTK_WIDGET (stack));
}

/**
 * gtk_stack_get_child_by_name:
 * @stack: a #GtkStack
 * @name: the name of the child to find
 *
 * Finds the child of the #GtkStack with the name given as
 * the argument. Returns %NULL if there is no child with this
 * name.
 *
 * Returns: (transfer none): the requested child of the #GtkStack
 *
 * Since: 3.12
 */
GtkWidget *
gtk_stack_get_child_by_name (GtkStack    *stack,
                             const gchar *name)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkStackChildInfo *info;
  GList *l;

  g_return_val_if_fail (GTK_IS_STACK (stack), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  for (l = priv->children; l != NULL; l = l->next)
    {
      info = l->data;
      if (info->name && strcmp (info->name, name) == 0)
        return info->widget;
    }

  return NULL;
}

/**
 * gtk_stack_set_homogeneous:
 * @stack: a #GtkStack
 * @homogeneous: %TRUE to make @stack homogeneous
 *
 * Sets the #GtkStack to be homogeneous or not. If it
 * is homogeneous, the #GtkStack will request the same
 * size for all its children. If it isn't, the stack
 * may change size when a different child becomes visible.
 *
 * Since 3.16, homogeneity can be controlled separately
 * for horizontal and vertical size, with the
 * #GtkStack:hhomogeneous and #GtkStack:vhomogeneous.
 *
 * Since: 3.10
 */
void
gtk_stack_set_homogeneous (GtkStack *stack,
                           gboolean  homogeneous)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_if_fail (GTK_IS_STACK (stack));

  homogeneous = !!homogeneous;

  if ((priv->hhomogeneous && priv->vhomogeneous) == homogeneous)
    return;

  g_object_freeze_notify (G_OBJECT (stack));

  if (priv->hhomogeneous != homogeneous)
    {
      priv->hhomogeneous = homogeneous;
      g_object_notify_by_pspec (G_OBJECT (stack), stack_props[PROP_HHOMOGENEOUS]);
    }

  if (priv->vhomogeneous != homogeneous)
    {
      priv->vhomogeneous = homogeneous;
      g_object_notify_by_pspec (G_OBJECT (stack), stack_props[PROP_VHOMOGENEOUS]);
    }

  if (gtk_widget_get_visible (GTK_WIDGET(stack)))
    gtk_widget_queue_resize (GTK_WIDGET (stack));

  g_object_notify_by_pspec (G_OBJECT (stack), stack_props[PROP_HOMOGENEOUS]);
  g_object_thaw_notify (G_OBJECT (stack));
}

/**
 * gtk_stack_get_homogeneous:
 * @stack: a #GtkStack
 *
 * Gets whether @stack is homogeneous.
 * See gtk_stack_set_homogeneous().
 *
 * Returns: whether @stack is homogeneous.
 *
 * Since: 3.10
 */
gboolean
gtk_stack_get_homogeneous (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_val_if_fail (GTK_IS_STACK (stack), FALSE);

  return priv->hhomogeneous && priv->vhomogeneous;
}

/**
 * gtk_stack_set_hhomogeneous:
 * @stack: a #GtkStack
 * @hhomogeneous: %TRUE to make @stack horizontally homogeneous
 *
 * Sets the #GtkStack to be horizontally homogeneous or not.
 * If it is homogeneous, the #GtkStack will request the same
 * width for all its children. If it isn't, the stack
 * may change width when a different child becomes visible.
 *
 * Since: 3.16
 */
void
gtk_stack_set_hhomogeneous (GtkStack *stack,
                            gboolean  hhomogeneous)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_if_fail (GTK_IS_STACK (stack));

  hhomogeneous = !!hhomogeneous;

  if (priv->hhomogeneous == hhomogeneous)
    return;
  
  priv->hhomogeneous = hhomogeneous;

  if (gtk_widget_get_visible (GTK_WIDGET(stack)))
    gtk_widget_queue_resize (GTK_WIDGET (stack));

  g_object_notify_by_pspec (G_OBJECT (stack), stack_props[PROP_HHOMOGENEOUS]);
}

/**
 * gtk_stack_get_hhomogeneous:
 * @stack: a #GtkStack
 *
 * Gets whether @stack is horizontally homogeneous.
 * See gtk_stack_set_hhomogeneous().
 *
 * Returns: whether @stack is horizontally homogeneous.
 *
 * Since: 3.16
 */
gboolean
gtk_stack_get_hhomogeneous (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_val_if_fail (GTK_IS_STACK (stack), FALSE);

  return priv->hhomogeneous;
}

/**
 * gtk_stack_set_vhomogeneous:
 * @stack: a #GtkStack
 * @vhomogeneous: %TRUE to make @stack vertically homogeneous
 *
 * Sets the #GtkStack to be vertically homogeneous or not.
 * If it is homogeneous, the #GtkStack will request the same
 * height for all its children. If it isn't, the stack
 * may change height when a different child becomes visible.
 *
 * Since: 3.16
 */
void
gtk_stack_set_vhomogeneous (GtkStack *stack,
                            gboolean  vhomogeneous)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_if_fail (GTK_IS_STACK (stack));

  vhomogeneous = !!vhomogeneous;

  if (priv->vhomogeneous == vhomogeneous)
    return;
  
  priv->vhomogeneous = vhomogeneous;

  if (gtk_widget_get_visible (GTK_WIDGET(stack)))
    gtk_widget_queue_resize (GTK_WIDGET (stack));

  g_object_notify_by_pspec (G_OBJECT (stack), stack_props[PROP_VHOMOGENEOUS]);
}

/**
 * gtk_stack_get_vhomogeneous:
 * @stack: a #GtkStack
 *
 * Gets whether @stack is vertically homogeneous.
 * See gtk_stack_set_vhomogeneous().
 *
 * Returns: whether @stack is vertically homogeneous.
 *
 * Since: 3.16
 */
gboolean
gtk_stack_get_vhomogeneous (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_val_if_fail (GTK_IS_STACK (stack), FALSE);

  return priv->vhomogeneous;
}

/**
 * gtk_stack_get_transition_duration:
 * @stack: a #GtkStack
 *
 * Returns the amount of time (in milliseconds) that
 * transitions between pages in @stack will take.
 *
 * Returns: the transition duration
 *
 * Since: 3.10
 */
guint
gtk_stack_get_transition_duration (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_val_if_fail (GTK_IS_STACK (stack), 0);

  return priv->transition_duration;
}

/**
 * gtk_stack_set_transition_duration:
 * @stack: a #GtkStack
 * @duration: the new duration, in milliseconds
 *
 * Sets the duration that transitions between pages in @stack
 * will take.
 *
 * Since: 3.10
 */
void
gtk_stack_set_transition_duration (GtkStack *stack,
                                   guint     duration)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_if_fail (GTK_IS_STACK (stack));

  if (priv->transition_duration == duration)
    return;

  priv->transition_duration = duration;
  g_object_notify_by_pspec (G_OBJECT (stack),
                            stack_props[PROP_TRANSITION_DURATION]);
}

/**
 * gtk_stack_get_transition_type:
 * @stack: a #GtkStack
 *
 * Gets the type of animation that will be used
 * for transitions between pages in @stack.
 *
 * Returns: the current transition type of @stack
 *
 * Since: 3.10
 */
GtkStackTransitionType
gtk_stack_get_transition_type (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_val_if_fail (GTK_IS_STACK (stack), GTK_STACK_TRANSITION_TYPE_NONE);

  return priv->transition_type;
}

/**
 * gtk_stack_set_transition_type:
 * @stack: a #GtkStack
 * @transition: the new transition type
 *
 * Sets the type of animation that will be used for
 * transitions between pages in @stack. Available
 * types include various kinds of fades and slides.
 *
 * The transition type can be changed without problems
 * at runtime, so it is possible to change the animation
 * based on the page that is about to become current.
 *
 * Since: 3.10
 */
void
gtk_stack_set_transition_type (GtkStack              *stack,
                              GtkStackTransitionType  transition)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_if_fail (GTK_IS_STACK (stack));

  if (priv->transition_type == transition)
    return;

  priv->transition_type = transition;
  g_object_notify_by_pspec (G_OBJECT (stack),
                            stack_props[PROP_TRANSITION_TYPE]);
}

/**
 * gtk_stack_get_transition_running:
 * @stack: a #GtkStack
 *
 * Returns whether the @stack is currently in a transition from one page to
 * another.
 *
 * Returns: %TRUE if the transition is currently running, %FALSE otherwise.
 *
 * Since: 3.12
 */
gboolean
gtk_stack_get_transition_running (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_val_if_fail (GTK_IS_STACK (stack), FALSE);

  return (priv->tick_id != 0);
}

/**
 * gtk_stack_get_visible_child:
 * @stack: a #GtkStack
 *
 * Gets the currently visible child of @stack, or %NULL if
 * there are no visible children.
 *
 * Returns: (transfer none): the visible child of the #GtkStack
 *
 * Since: 3.10
 */
GtkWidget *
gtk_stack_get_visible_child (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_val_if_fail (GTK_IS_STACK (stack), NULL);

  return priv->visible_child ? priv->visible_child->widget : NULL;
}

/**
 * gtk_stack_get_visible_child_name:
 * @stack: a #GtkStack
 *
 * Returns the name of the currently visible child of @stack, or
 * %NULL if there is no visible child.
 *
 * Returns: (transfer none): the name of the visible child of the #GtkStack
 *
 * Since: 3.10
 */
const gchar *
gtk_stack_get_visible_child_name (GtkStack *stack)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  g_return_val_if_fail (GTK_IS_STACK (stack), NULL);

  if (priv->visible_child)
    return priv->visible_child->name;

  return NULL;
}

/**
 * gtk_stack_set_visible_child:
 * @stack: a #GtkStack
 * @child: a child of @stack
 *
 * Makes @child the visible child of @stack.
 *
 * If @child is different from the currently
 * visible child, the transition between the
 * two will be animated with the current
 * transition type of @stack.
 *
 * Note that the @child widget has to be visible itself
 * (see gtk_widget_show()) in order to become the visible
 * child of @stack.
 *
 * Since: 3.10
 */
void
gtk_stack_set_visible_child (GtkStack  *stack,
                             GtkWidget *child)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkStackChildInfo *child_info;

  g_return_if_fail (GTK_IS_STACK (stack));
  g_return_if_fail (GTK_IS_WIDGET (child));

  child_info = find_child_info_for_widget (stack, child);
  if (child_info == NULL)
    {
      g_warning ("Given child of type '%s' not found in GtkStack",
                 G_OBJECT_TYPE_NAME (child));
      return;
    }

  if (gtk_widget_get_visible (child_info->widget))
    set_visible_child (stack, child_info,
                       priv->transition_type,
                       priv->transition_duration);
}

/**
 * gtk_stack_set_visible_child_name:
 * @stack: a #GtkStack
 * @name: the name of the child to make visible
 *
 * Makes the child with the given name visible.
 *
 * If @child is different from the currently
 * visible child, the transition between the
 * two will be animated with the current
 * transition type of @stack.
 *
 * Note that the child widget has to be visible itself
 * (see gtk_widget_show()) in order to become the visible
 * child of @stack.
 *
 * Since: 3.10
 */
void
gtk_stack_set_visible_child_name (GtkStack   *stack,
                                 const gchar *name)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  gtk_stack_set_visible_child_full (stack, name, priv->transition_type);
}

/**
 * gtk_stack_set_visible_child_full:
 * @stack: a #GtkStack
 * @name: the name of the child to make visible
 * @transition: the transition type to use
 *
 * Makes the child with the given name visible.
 *
 * Note that the child widget has to be visible itself
 * (see gtk_widget_show()) in order to become the visible
 * child of @stack.
 *
 * Since: 3.10
 */
void
gtk_stack_set_visible_child_full (GtkStack               *stack,
                                  const gchar            *name,
                                  GtkStackTransitionType  transition)
{
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkStackChildInfo *child_info, *info;
  GList *l;

  g_return_if_fail (GTK_IS_STACK (stack));

  if (name == NULL)
    return;

  child_info = NULL;
  for (l = priv->children; l != NULL; l = l->next)
    {
      info = l->data;
      if (info->name != NULL &&
          strcmp (info->name, name) == 0)
        {
          child_info = info;
          break;
        }
    }

  if (child_info == NULL)
    {
      g_warning ("Child name '%s' not found in GtkStack", name);
      return;
    }

  if (gtk_widget_get_visible (child_info->widget))
    set_visible_child (stack, child_info, transition, priv->transition_duration);
}

static void
gtk_stack_forall (GtkContainer *container,
                  gboolean      include_internals,
                  GtkCallback   callback,
                  gpointer      callback_data)
{
  GtkStack *stack = GTK_STACK (container);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkStackChildInfo *child_info;
  GList *l;

  l = priv->children;
  while (l)
    {
      child_info = l->data;
      l = l->next;

      (* callback) (child_info->widget, callback_data);
    }
}

static void
gtk_stack_compute_expand (GtkWidget *widget,
                          gboolean  *hexpand_p,
                          gboolean  *vexpand_p)
{
  GtkStack *stack = GTK_STACK (widget);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  gboolean hexpand, vexpand;
  GtkStackChildInfo *child_info;
  GtkWidget *child;
  GList *l;

  hexpand = FALSE;
  vexpand = FALSE;
  for (l = priv->children; l != NULL; l = l->next)
    {
      child_info = l->data;
      child = child_info->widget;

      if (!hexpand &&
          gtk_widget_compute_expand (child, GTK_ORIENTATION_HORIZONTAL))
        hexpand = TRUE;

      if (!vexpand &&
          gtk_widget_compute_expand (child, GTK_ORIENTATION_VERTICAL))
        vexpand = TRUE;

      if (hexpand && vexpand)
        break;
    }

  *hexpand_p = hexpand;
  *vexpand_p = vexpand;
}

static void
gtk_stack_draw_crossfade (GtkWidget *widget,
                          cairo_t   *cr)
{
  GtkStack *stack = GTK_STACK (widget);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  cairo_push_group (cr);
  gtk_container_propagate_draw (GTK_CONTAINER (stack),
                                priv->visible_child->widget,
                                cr);
  cairo_save (cr);

  /* Multiply alpha by transition pos */
  cairo_set_source_rgba (cr, 1, 1, 1, priv->transition_pos);
  cairo_set_operator (cr, CAIRO_OPERATOR_DEST_IN);
  cairo_paint (cr);

  if (priv->last_visible_surface)
    {
      cairo_set_source_surface (cr, priv->last_visible_surface,
                                priv->last_visible_surface_allocation.x,
                                priv->last_visible_surface_allocation.y);
      cairo_set_operator (cr, CAIRO_OPERATOR_ADD);
      cairo_paint_with_alpha (cr, MAX (1.0 - priv->transition_pos, 0));
    }

  cairo_restore (cr);

  cairo_pop_group_to_source (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
  cairo_paint (cr);
}

static void
gtk_stack_draw_under (GtkWidget *widget,
                      cairo_t   *cr)
{
  GtkStack *stack = GTK_STACK (widget);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkAllocation allocation;
  gint x, y, width, height, pos_x, pos_y;

  gtk_widget_get_allocation (widget, &allocation);
  x = y = 0;
  width = allocation.width;
  height = allocation.height;
  pos_x = pos_y = 0;

  switch (priv->active_transition_type)
    {
    case GTK_STACK_TRANSITION_TYPE_UNDER_DOWN:
      y = 0;
      height = allocation.height * (ease_out_cubic (priv->transition_pos));
      pos_y = height;
      break;
    case GTK_STACK_TRANSITION_TYPE_UNDER_UP:
      y = allocation.height * (1 - ease_out_cubic (priv->transition_pos));
      height = allocation.height - y;
      pos_y = y - allocation.height;
      break;
    case GTK_STACK_TRANSITION_TYPE_UNDER_LEFT:
      x = allocation.width * (1 - ease_out_cubic (priv->transition_pos));
      width = allocation.width - x;
      pos_x = x - allocation.width;
      break;
    case GTK_STACK_TRANSITION_TYPE_UNDER_RIGHT:
      x = 0;
      width = allocation.width * (ease_out_cubic (priv->transition_pos));
      pos_x = width;
      break;
    default:
      g_assert_not_reached ();
    }

  cairo_save (cr);
  cairo_rectangle (cr, x, y, width, height);
  cairo_clip (cr);

  gtk_container_propagate_draw (GTK_CONTAINER (stack),
                                priv->visible_child->widget,
                                cr);

  cairo_restore (cr);

  if (priv->last_visible_surface)
    {
      cairo_set_source_surface (cr, priv->last_visible_surface, pos_x, pos_y);
      cairo_paint (cr);
    }
}

static void
gtk_stack_draw_slide (GtkWidget *widget,
                      cairo_t   *cr)
{
  GtkStack *stack = GTK_STACK (widget);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);

  if (priv->last_visible_surface &&
      gtk_cairo_should_draw_window (cr, priv->view_window))
    {
      GtkAllocation allocation;
      int x, y;

      gtk_widget_get_allocation (widget, &allocation);

      x = get_bin_window_x (stack, &allocation);
      y = get_bin_window_y (stack, &allocation);

      switch (priv->active_transition_type)
        {
        case GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT:
          x -= allocation.width;
          break;
        case GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT:
          x += allocation.width;
          break;
        case GTK_STACK_TRANSITION_TYPE_SLIDE_UP:
          y -= allocation.height;
          break;
        case GTK_STACK_TRANSITION_TYPE_SLIDE_DOWN:
          y += allocation.height;
          break;
        case GTK_STACK_TRANSITION_TYPE_OVER_UP:
        case GTK_STACK_TRANSITION_TYPE_OVER_DOWN:
          y = 0;
          break;
        case GTK_STACK_TRANSITION_TYPE_OVER_LEFT:
        case GTK_STACK_TRANSITION_TYPE_OVER_RIGHT:
          x = 0;
          break;
        default:
          g_assert_not_reached ();
          break;
        }

      x += priv->last_visible_surface_allocation.x;
      y += priv->last_visible_surface_allocation.y;

      cairo_save (cr);
      cairo_set_source_surface (cr, priv->last_visible_surface, x, y);
      cairo_paint (cr);
      cairo_restore (cr);
     }

  if (gtk_cairo_should_draw_window (cr, priv->bin_window))
    gtk_container_propagate_draw (GTK_CONTAINER (stack),
				  priv->visible_child->widget,
				  cr);
}

static gboolean
gtk_stack_draw (GtkWidget *widget,
                cairo_t   *cr)
{
  GtkStack *stack = GTK_STACK (widget);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  cairo_t *pattern_cr;

  if (gtk_cairo_should_draw_window (cr, priv->view_window))
    {
      GtkStyleContext *context;
          
      context = gtk_widget_get_style_context (widget);
      gtk_render_background (context,
                             cr,
                             0, 0,
                             gtk_widget_get_allocated_width (widget),
                             gtk_widget_get_allocated_height (widget));
    }

  if (priv->visible_child)
    {
      if (priv->transition_pos < 1.0)
        {
          if (priv->last_visible_surface == NULL &&
              priv->last_visible_child != NULL)
            {
              gtk_widget_get_allocation (priv->last_visible_child->widget,
                                         &priv->last_visible_surface_allocation);
              priv->last_visible_surface =
                gdk_window_create_similar_surface (gtk_widget_get_window (widget),
                                                   CAIRO_CONTENT_COLOR_ALPHA,
                                                   priv->last_visible_surface_allocation.width,
                                                   priv->last_visible_surface_allocation.height);
              pattern_cr = cairo_create (priv->last_visible_surface);
              /* We don't use propagate_draw here, because we don't want to apply
               * the bin_window offset
               */
              gtk_widget_draw (priv->last_visible_child->widget, pattern_cr);
              cairo_destroy (pattern_cr);
            }

          switch (priv->active_transition_type)
            {
            case GTK_STACK_TRANSITION_TYPE_CROSSFADE:
	      if (gtk_cairo_should_draw_window (cr, priv->bin_window))
		gtk_stack_draw_crossfade (widget, cr);
              break;
            case GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT:
            case GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT:
            case GTK_STACK_TRANSITION_TYPE_SLIDE_UP:
            case GTK_STACK_TRANSITION_TYPE_SLIDE_DOWN:
            case GTK_STACK_TRANSITION_TYPE_OVER_UP:
            case GTK_STACK_TRANSITION_TYPE_OVER_DOWN:
            case GTK_STACK_TRANSITION_TYPE_OVER_LEFT:
            case GTK_STACK_TRANSITION_TYPE_OVER_RIGHT:
              gtk_stack_draw_slide (widget, cr);
              break;
            case GTK_STACK_TRANSITION_TYPE_UNDER_UP:
            case GTK_STACK_TRANSITION_TYPE_UNDER_DOWN:
            case GTK_STACK_TRANSITION_TYPE_UNDER_LEFT:
            case GTK_STACK_TRANSITION_TYPE_UNDER_RIGHT:
	      if (gtk_cairo_should_draw_window (cr, priv->bin_window))
		gtk_stack_draw_under (widget, cr);
              break;
            default:
              g_assert_not_reached ();
            }

        }
      else if (gtk_cairo_should_draw_window (cr, priv->bin_window))
        gtk_container_propagate_draw (GTK_CONTAINER (stack),
                                      priv->visible_child->widget,
                                      cr);
    }

  return TRUE;
}

static void
gtk_stack_size_allocate (GtkWidget     *widget,
                         GtkAllocation *allocation)
{
  GtkStack *stack = GTK_STACK (widget);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkAllocation child_allocation;

  g_return_if_fail (allocation != NULL);

  gtk_widget_set_allocation (widget, allocation);

  child_allocation = *allocation;
  child_allocation.x = 0;
  child_allocation.y = 0;

  if (priv->last_visible_child)
    gtk_widget_size_allocate (priv->last_visible_child->widget, &child_allocation);

  if (priv->visible_child)
    gtk_widget_size_allocate (priv->visible_child->widget, &child_allocation);

   if (gtk_widget_get_realized (widget))
    {
      gdk_window_move_resize (priv->view_window,
                              allocation->x, allocation->y,
                              allocation->width, allocation->height);
      gdk_window_move_resize (priv->bin_window,
                              get_bin_window_x (stack, allocation), get_bin_window_y (stack, allocation),
                              allocation->width, allocation->height);
    }
}

static void
gtk_stack_get_preferred_height (GtkWidget *widget,
                                gint      *minimum_height,
                                gint      *natural_height)
{
  GtkStack *stack = GTK_STACK (widget);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkStackChildInfo *child_info;
  GtkWidget *child;
  gint child_min, child_nat;
  GList *l;

  *minimum_height = 0;
  *natural_height = 0;

  for (l = priv->children; l != NULL; l = l->next)
    {
      child_info = l->data;
      child = child_info->widget;

      if (!priv->vhomogeneous &&
          (priv->visible_child != child_info &&
           priv->last_visible_child != child_info))
        continue;
      if (gtk_widget_get_visible (child))
        {
          gtk_widget_get_preferred_height (child, &child_min, &child_nat);

          *minimum_height = MAX (*minimum_height, child_min);
          *natural_height = MAX (*natural_height, child_nat);
        }
    }

  if (priv->last_visible_surface != NULL)
    {
      *minimum_height = MAX (*minimum_height, priv->last_visible_surface_allocation.height);
      *natural_height = MAX (*natural_height, priv->last_visible_surface_allocation.height);
    }
}

static void
gtk_stack_get_preferred_height_for_width (GtkWidget *widget,
                                          gint       width,
                                          gint      *minimum_height,
                                          gint      *natural_height)
{
  GtkStack *stack = GTK_STACK (widget);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkStackChildInfo *child_info;
  GtkWidget *child;
  gint child_min, child_nat;
  GList *l;

  *minimum_height = 0;
  *natural_height = 0;

  for (l = priv->children; l != NULL; l = l->next)
    {
      child_info = l->data;
      child = child_info->widget;

      if (!priv->vhomogeneous &&
          (priv->visible_child != child_info &&
           priv->last_visible_child != child_info))
        continue;
      if (gtk_widget_get_visible (child))
        {
          gtk_widget_get_preferred_height_for_width (child, width, &child_min, &child_nat);

          *minimum_height = MAX (*minimum_height, child_min);
          *natural_height = MAX (*natural_height, child_nat);
        }
    }

  if (priv->last_visible_surface != NULL)
    {
      *minimum_height = MAX (*minimum_height, priv->last_visible_surface_allocation.height);
      *natural_height = MAX (*natural_height, priv->last_visible_surface_allocation.height);
    }
}

static void
gtk_stack_get_preferred_width (GtkWidget *widget,
                               gint      *minimum_width,
                               gint      *natural_width)
{
  GtkStack *stack = GTK_STACK (widget);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkStackChildInfo *child_info;
  GtkWidget *child;
  gint child_min, child_nat;
  GList *l;

  *minimum_width = 0;
  *natural_width = 0;

  for (l = priv->children; l != NULL; l = l->next)
    {
      child_info = l->data;
      child = child_info->widget;

      if (!priv->hhomogeneous &&
          (priv->visible_child != child_info &&
           priv->last_visible_child != child_info))
        continue;
      if (gtk_widget_get_visible (child))
        {
          gtk_widget_get_preferred_width (child, &child_min, &child_nat);

          *minimum_width = MAX (*minimum_width, child_min);
          *natural_width = MAX (*natural_width, child_nat);
        }
    }

  if (priv->last_visible_surface != NULL)
    {
      *minimum_width = MAX (*minimum_width, priv->last_visible_surface_allocation.width);
      *natural_width = MAX (*natural_width, priv->last_visible_surface_allocation.width);
    }
}

static void
gtk_stack_get_preferred_width_for_height (GtkWidget *widget,
                                          gint       height,
                                          gint      *minimum_width,
                                          gint      *natural_width)
{
  GtkStack *stack = GTK_STACK (widget);
  GtkStackPrivate *priv = gtk_stack_get_instance_private (stack);
  GtkStackChildInfo *child_info;
  GtkWidget *child;
  gint child_min, child_nat;
  GList *l;

  *minimum_width = 0;
  *natural_width = 0;

  for (l = priv->children; l != NULL; l = l->next)
    {
      child_info = l->data;
      child = child_info->widget;

      if (!priv->hhomogeneous &&
          (priv->visible_child != child_info &&
           priv->last_visible_child != child_info))
        continue;
      if (gtk_widget_get_visible (child))
        {
          gtk_widget_get_preferred_width_for_height (child, height, &child_min, &child_nat);

          *minimum_width = MAX (*minimum_width, child_min);
          *natural_width = MAX (*natural_width, child_nat);
        }
    }

  if (priv->last_visible_surface != NULL)
    {
      *minimum_width = MAX (*minimum_width, priv->last_visible_surface_allocation.width);
      *natural_width = MAX (*natural_width, priv->last_visible_surface_allocation.width);
    }
}
