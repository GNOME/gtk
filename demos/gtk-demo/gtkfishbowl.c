/* GTK - The GIMP Toolkit
 * Copyright (C) 2017 Benjamin Otte <otte@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkfishbowl.h"

typedef struct _GtkFishbowlPrivate       GtkFishbowlPrivate;
typedef struct _GtkFishbowlChild         GtkFishbowlChild;

struct _GtkFishbowlPrivate
{
  GList *children;
  guint count;

  gint64 last_frame_time;
  guint tick_id;
};

struct _GtkFishbowlChild
{
  GtkWidget *widget;
  double x;
  double y;
  double dx;
  double dy;
};

enum {
   PROP_0,
   PROP_ANIMATING,
   PROP_COUNT,
   NUM_PROPERTIES
};

static GParamSpec *props[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE_WITH_PRIVATE (GtkFishbowl, gtk_fishbowl, GTK_TYPE_CONTAINER)

static void
gtk_fishbowl_init (GtkFishbowl *fishbowl)
{
  gtk_widget_set_has_window (GTK_WIDGET (fishbowl), FALSE);
}

/**
 * gtk_fishbowl_new:
 *
 * Creates a new #GtkFishbowl.
 *
 * Returns: a new #GtkFishbowl.
 */
GtkWidget*
gtk_fishbowl_new (void)
{
  return g_object_new (GTK_TYPE_FISHBOWL, NULL);
}

static void
gtk_fishbowl_measure (GtkWidget      *widget,
                      GtkOrientation  orientation,
                      int             for_size,
                      int            *minimum,
                      int            *natural,
                      int            *minimum_baseline,
                      int            *natural_baseline)
{
  GtkFishbowl *fishbowl = GTK_FISHBOWL (widget);
  GtkFishbowlPrivate *priv = gtk_fishbowl_get_instance_private (fishbowl);
  GtkFishbowlChild *child;
  GList *children;
  gint child_min, child_nat;

  *minimum = 0;
  *natural = 0;

  for (children = priv->children; children; children = children->next)
    {
      child = children->data;

      if (!gtk_widget_get_visible (child->widget))
        continue;

      gtk_widget_measure (child->widget, orientation, -1, &child_min, &child_nat, NULL, NULL);

      *minimum = MAX (*minimum, child_min);
      *natural = MAX (*natural, child_nat);
    }
}

static void
gtk_fishbowl_size_allocate (GtkWidget     *widget,
                            GtkAllocation *allocation)
{
  GtkFishbowl *fishbowl = GTK_FISHBOWL (widget);
  GtkFishbowlPrivate *priv = gtk_fishbowl_get_instance_private (fishbowl);
  GtkFishbowlChild *child;
  GtkAllocation child_allocation;
  GtkRequisition child_requisition;
  GList *children;

  gtk_widget_set_allocation (widget, allocation);

  for (children = priv->children; children; children = children->next)
    {
      child = children->data;

      if (!gtk_widget_get_visible (child->widget))
        continue;

      gtk_widget_get_preferred_size (child->widget, &child_requisition, NULL);
      child_allocation.x = allocation->x + round (child->x * (allocation->width - child_requisition.width));
      child_allocation.y = allocation->y + round (child->y * (allocation->height - child_requisition.height));
      child_allocation.width = child_requisition.width;
      child_allocation.height = child_requisition.height;

      gtk_widget_size_allocate (child->widget, &child_allocation);
    }
}

static double
new_speed (void)
{
  /* 5s to 50s to cross screen seems fair */
  return g_random_double_range (0.02, 0.2);
}

static void
gtk_fishbowl_add (GtkContainer *container,
                  GtkWidget    *widget)
{
  GtkFishbowl *fishbowl = GTK_FISHBOWL (container);
  GtkFishbowlPrivate *priv = gtk_fishbowl_get_instance_private (fishbowl);
  GtkFishbowlChild *child_info;

  g_return_if_fail (GTK_IS_FISHBOWL (fishbowl));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  child_info = g_new0 (GtkFishbowlChild, 1);
  child_info->widget = widget;
  child_info->x = 0;
  child_info->y = 0;
  child_info->dx = new_speed ();
  child_info->dy = new_speed ();

  gtk_widget_set_parent (widget, GTK_WIDGET (fishbowl));

  priv->children = g_list_prepend (priv->children, child_info);
  priv->count++;
  g_object_notify_by_pspec (G_OBJECT (fishbowl), props[PROP_COUNT]);
}

static void
gtk_fishbowl_remove (GtkContainer *container,
                     GtkWidget    *widget)
{
  GtkFishbowl *fishbowl = GTK_FISHBOWL (container);
  GtkFishbowlPrivate *priv = gtk_fishbowl_get_instance_private (fishbowl);
  GtkFishbowlChild *child;
  GtkWidget *widget_container = GTK_WIDGET (container);
  GList *children;

  for (children = priv->children; children; children = children->next)
    {
      child = children->data;

      if (child->widget == widget)
        {
          gboolean was_visible = gtk_widget_get_visible (widget);

          gtk_widget_unparent (widget);

          priv->children = g_list_remove_link (priv->children, children);
          g_list_free (children);
          g_free (child);

          if (was_visible && gtk_widget_get_visible (widget_container))
            gtk_widget_queue_resize (widget_container);

          priv->count--;
          g_object_notify_by_pspec (G_OBJECT (fishbowl), props[PROP_COUNT]);
          break;
        }
    }
}

static void
gtk_fishbowl_forall (GtkContainer *container,
                     GtkCallback   callback,
                     gpointer      callback_data)
{
  GtkFishbowl *fishbowl = GTK_FISHBOWL (container);
  GtkFishbowlPrivate *priv = gtk_fishbowl_get_instance_private (fishbowl);
  GtkFishbowlChild *child;
  GList *children;

  children = priv->children;
  while (children)
    {
      child = children->data;
      children = children->next;

      (* callback) (child->widget, callback_data);
    }
}

static void 
gtk_fishbowl_snapshot (GtkWidget   *widget,
                       GtkSnapshot *snapshot)
{
  GtkFishbowl *fishbowl = GTK_FISHBOWL (widget);
  GtkFishbowlPrivate *priv = gtk_fishbowl_get_instance_private (fishbowl);
  GtkFishbowlChild *child;
  GList *list;

  for (list = priv->children;
       list;
       list = list->next)
    {
      child = list->data;

      gtk_widget_snapshot_child (widget,
                                 child->widget,
                                 snapshot);
    }
}

static void
gtk_fishbowl_dispose (GObject *object)
{
  GtkFishbowl *fishbowl = GTK_FISHBOWL (object);

  gtk_fishbowl_set_animating (fishbowl, FALSE);
  gtk_fishbowl_set_count (fishbowl, 0);

  G_OBJECT_CLASS (gtk_fishbowl_parent_class)->dispose (object);
}

static void
gtk_fishbowl_set_property (GObject         *object,
                           guint            prop_id,
                           const GValue    *value,
                           GParamSpec      *pspec)
{
  GtkFishbowl *fishbowl = GTK_FISHBOWL (object);

  switch (prop_id)
    {
    case PROP_ANIMATING:
      gtk_fishbowl_set_animating (fishbowl, g_value_get_boolean (value));
      break;

    case PROP_COUNT:
      gtk_fishbowl_set_count (fishbowl, g_value_get_uint (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_fishbowl_get_property (GObject         *object,
                           guint            prop_id,
                           GValue          *value,
                           GParamSpec      *pspec)
{
  GtkFishbowl *fishbowl = GTK_FISHBOWL (object);

  switch (prop_id)
    {
    case PROP_ANIMATING:
      g_value_set_boolean (value, gtk_fishbowl_get_animating (fishbowl));
      break;

    case PROP_COUNT:
      g_value_set_uint (value, gtk_fishbowl_get_count (fishbowl));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_fishbowl_class_init (GtkFishbowlClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->dispose = gtk_fishbowl_dispose;
  object_class->set_property = gtk_fishbowl_set_property;
  object_class->get_property = gtk_fishbowl_get_property;

  widget_class->measure = gtk_fishbowl_measure;
  widget_class->size_allocate = gtk_fishbowl_size_allocate;
  widget_class->snapshot = gtk_fishbowl_snapshot;

  container_class->add = gtk_fishbowl_add;
  container_class->remove = gtk_fishbowl_remove;
  container_class->forall = gtk_fishbowl_forall;

  props[PROP_ANIMATING] =
      g_param_spec_boolean ("animating",
                            "animating",
                            "Whether children are moving around",
                            FALSE,
                            G_PARAM_READWRITE);

  props[PROP_COUNT] =
      g_param_spec_uint ("count",
                         "Count",
                         "Number of widgets",
                         0, G_MAXUINT,
                         0,
                         G_PARAM_READABLE);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, props);
}

guint
gtk_fishbowl_get_count (GtkFishbowl *fishbowl)
{
  GtkFishbowlPrivate *priv = gtk_fishbowl_get_instance_private (fishbowl);

  return priv->count;
}

char **icon_names = NULL;
gsize n_icon_names = 0;

static void
init_icon_names (GtkIconTheme *theme)
{
  GPtrArray *icons;
  GList *l, *icon_list;

  if (icon_names)
    return;

  icon_list = gtk_icon_theme_list_icons (theme, NULL);
  icons = g_ptr_array_new ();

  for (l = icon_list; l; l = l->next)
    {
      if (g_str_has_suffix (l->data, "symbolic"))
        continue;

      g_ptr_array_add (icons, g_strdup (l->data));
    }

  n_icon_names = icons->len;
  g_ptr_array_add (icons, NULL); /* NULL-terminate the array */
  icon_names = (char **) g_ptr_array_free (icons, FALSE);

  /* don't free strings, we assigned them to the array */
  g_list_free_full (icon_list, g_free);
}

static const char *
get_random_icon_name (GtkIconTheme *theme)
{
  init_icon_names (theme);

  return icon_names[g_random_int_range(0, n_icon_names)];
}

void
gtk_fishbowl_set_count (GtkFishbowl *fishbowl,
                        guint        count)
{
  GtkFishbowlPrivate *priv = gtk_fishbowl_get_instance_private (fishbowl);

  g_object_freeze_notify (G_OBJECT (fishbowl));

  while (priv->count > count)
    {
      gtk_container_remove (GTK_CONTAINER (fishbowl),
                            ((GtkFishbowlChild *) priv->children->data)->widget);
    }

  while (priv->count < count)
    {
      GtkWidget *new_widget;
        
      new_widget = gtk_image_new_from_icon_name (get_random_icon_name (gtk_icon_theme_get_default ()),
                                                 GTK_ICON_SIZE_DIALOG);
      gtk_widget_show (new_widget);
      gtk_container_add (GTK_CONTAINER (fishbowl), new_widget);
    }

  g_object_thaw_notify (G_OBJECT (fishbowl));
}

gboolean
gtk_fishbowl_get_animating (GtkFishbowl *fishbowl)
{
  GtkFishbowlPrivate *priv = gtk_fishbowl_get_instance_private (fishbowl);

  return priv->tick_id != 0;
}

static gboolean
gtk_fishbowl_tick (GtkWidget     *widget,
                   GdkFrameClock *frame_clock,
                   gpointer       unused)
{
  GtkFishbowl *fishbowl = GTK_FISHBOWL (widget);
  GtkFishbowlPrivate *priv = gtk_fishbowl_get_instance_private (fishbowl);
  GtkFishbowlChild *child;
  GList *l;
  gint64 frame_time, elapsed;

  frame_time = gdk_frame_clock_get_frame_time (gtk_widget_get_frame_clock (widget));
  elapsed = frame_time - priv->last_frame_time;
  priv->last_frame_time = frame_time;

  /* last frame was 0, so we're just starting to animate */
  if (elapsed == frame_time)
    return G_SOURCE_CONTINUE;

  for (l = priv->children; l; l = l->next)
    {
      child = l->data;

      child->x += child->dx * ((double) elapsed / G_USEC_PER_SEC);
      child->y += child->dy * ((double) elapsed / G_USEC_PER_SEC);

      if (child->x <= 0)
        {
          child->x = 0;
          child->dx = new_speed ();
        }
      else if (child->x >= 1)
        {
          child->x = 1;
          child->dx =  - new_speed ();
        }

      if (child->y <= 0)
        {
          child->y = 0;
          child->dy = new_speed ();
        }
      else if (child->y >= 1)
        {
          child->y = 1;
          child->dy =  - new_speed ();
        }
    }

  gtk_widget_queue_allocate (widget);

  return G_SOURCE_CONTINUE;
}

void
gtk_fishbowl_set_animating (GtkFishbowl *fishbowl,
                            gboolean     animating)
{
  GtkFishbowlPrivate *priv = gtk_fishbowl_get_instance_private (fishbowl);

  if (gtk_fishbowl_get_animating (fishbowl) == animating)
    return;

  if (animating)
    {
      priv->tick_id = gtk_widget_add_tick_callback (GTK_WIDGET (fishbowl),
                                                    gtk_fishbowl_tick,
                                                    NULL,
                                                    NULL);
    }
  else
    {
      priv->last_frame_time = 0;
      gtk_widget_remove_tick_callback (GTK_WIDGET (fishbowl), priv->tick_id);
      priv->tick_id = 0;
    }

  g_object_notify_by_pspec (G_OBJECT (fishbowl), props[PROP_ANIMATING]);
}

