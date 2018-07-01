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

#include "gtkzoom.h"

#include "gtkintl.h"
#include "gtksnapshot.h"

/**
 * SECTION:gtkzoom
 * @title: GtkZoom
 * @short_description: A widget for zooming
 * @see_also: #GtkAspectFrame, #GtkMagnifier
 *
 * GtkZoom is a widget that zooms its contents to the available size
 * user a way to control the playback.
 */

struct _GtkZoom
{
  GtkWidget parent_instance;

  GtkWidget *child;
  double zoom;
  graphene_point3d_t offset;
};

enum
{
  PROP_0,
  PROP_CHILD,

  N_PROPS
};

G_DEFINE_TYPE (GtkZoom, gtk_zoom, GTK_TYPE_CONTAINER)

static GParamSpec *properties[N_PROPS] = { NULL, };

static GType
gtk_zoom_child_type (GtkContainer *container)
{
  GtkZoom *zoom = GTK_ZOOM (container);

  if (!zoom->child)
    return GTK_TYPE_WIDGET;
  else
    return G_TYPE_NONE;
}

static void
gtk_zoom_add (GtkContainer *container,
	      GtkWidget    *child)
{
  GtkZoom *zoom = GTK_ZOOM (container);

  if (zoom->child != NULL)
    {
      g_warning ("Attempting to add a widget with type %s to a %s, "
                 "but as a GtkZoom subclass a %s can only contain one widget at a time; "
                 "it already contains a widget of type %s",
                 g_type_name (G_OBJECT_TYPE (child)),
                 g_type_name (G_OBJECT_TYPE (zoom)),
                 g_type_name (G_OBJECT_TYPE (zoom)),
                 g_type_name (G_OBJECT_TYPE (zoom->child)));
      return;
    }

  gtk_zoom_set_child (zoom, child);
}

static void
gtk_zoom_remove (GtkContainer *container,
                 GtkWidget    *child)
{
  GtkZoom *zoom = GTK_ZOOM (container);

  g_return_if_fail (zoom->child == child);

  gtk_zoom_set_child (zoom, NULL);
}

static void
gtk_zoom_forall (GtkContainer *container,
		GtkCallback   callback,
		gpointer      callback_data)
{
  GtkZoom *zoom = GTK_ZOOM (container);

  if (zoom->child)
    (* callback) (zoom->child, callback_data);
}

static void
gtk_zoom_measure (GtkWidget      *widget,
                  GtkOrientation  orientation,
                  int             for_size,
                  int            *minimum,
                  int            *natural,
                  int            *minimum_baseline,
                  int            *natural_baseline)
{
  GtkZoom *self = GTK_ZOOM (widget);

  if (self->child)
    {
      gtk_widget_measure (self->child,
                          orientation,
                          for_size,
                          minimum, natural,
                          minimum_baseline, natural_baseline);
    }
}

static void
gtk_zoom_size_allocate (GtkWidget           *widget,
                        const GtkAllocation *allocation,
                        int                  baseline)
{
  GtkZoom *self = GTK_ZOOM (widget);
  GtkAllocation child_allocation;

  if (self->child == NULL)
    {
      self->zoom = 1.0;
      graphene_point3d_init (&self->offset, 0.0, 0.0, 0.0);
    }
  else
    {
      gtk_widget_measure (self->child, GTK_ORIENTATION_HORIZONTAL, -1, &child_allocation.width, NULL, NULL, NULL);
      gtk_widget_measure (self->child, GTK_ORIENTATION_VERTICAL, child_allocation.width, &child_allocation.height, NULL, NULL, NULL);
      self->zoom = MIN ((double) allocation->width / child_allocation.width,
                        (double) allocation->height / child_allocation.height);
      child_allocation.x = 0;
      child_allocation.y = 0;
      gtk_widget_size_allocate (self->child, &child_allocation, -1);
      graphene_point3d_init (&self->offset,
                             /* use int math here, so we get a pixel offset */
                             (allocation->width - child_allocation.width * self->zoom) / 2,
                             (allocation->height - child_allocation.height * self->zoom) / 2,
                             0.0);
    }
}

static void
gtk_zoom_snapshot (GtkWidget   *widget,
                   GtkSnapshot *snapshot)
{
  GtkZoom *self = GTK_ZOOM (widget);

  if (self->child)
    {
      graphene_matrix_t transform;

      graphene_matrix_init_scale (&transform, self->zoom, self->zoom, 1.0);
      graphene_matrix_translate (&transform, &self->offset);

      gtk_snapshot_push_transform (snapshot, &transform);

      gtk_widget_snapshot_child (widget, self->child, snapshot);

      gtk_snapshot_pop (snapshot);
    }
}

static void
gtk_zoom_dispose (GObject *object)
{
  GtkZoom *self = GTK_ZOOM (object);

  g_clear_pointer (&self->child, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_zoom_parent_class)->dispose (object);
}

static void
gtk_zoom_get_property (GObject    *object,
                       guint       property_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  GtkZoom *self = GTK_ZOOM (object);

  switch (property_id)
    {
    case PROP_CHILD:
      g_value_set_object (value, self->child);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_zoom_set_property (GObject      *object,
                       guint         property_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  GtkZoom *self = GTK_ZOOM (object);

  switch (property_id)
    {
    case PROP_CHILD:
      gtk_zoom_set_child (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_zoom_class_init (GtkZoomClass *klass)
{
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  container_class->add = gtk_zoom_add;
  container_class->remove = gtk_zoom_remove;
  container_class->forall = gtk_zoom_forall;
  container_class->child_type = gtk_zoom_child_type;

  widget_class->measure = gtk_zoom_measure;
  widget_class->size_allocate = gtk_zoom_size_allocate;
  widget_class->snapshot = gtk_zoom_snapshot;

  gobject_class->dispose = gtk_zoom_dispose;
  gobject_class->get_property = gtk_zoom_get_property;
  gobject_class->set_property = gtk_zoom_set_property;

  /**
   * GtkZoom:child:
   *
   * The displayed widget
   */
  properties[PROP_CHILD] =
    g_param_spec_object ("child",
                         P_("Child"),
                         P_("The displayed widget"),
                         GTK_TYPE_WIDGET,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, I_("zoom"));
}

static void
gtk_zoom_init (GtkZoom *self)
{
  gtk_widget_set_has_surface (GTK_WIDGET (self), FALSE);
}

/**
 * gtk_zoom_new:
 *
 * Creates a new empty #GtkZoom.
 *
 * Returns: a new #GtkZoom
 **/
GtkWidget *
gtk_zoom_new (void)
{
  return g_object_new (GTK_TYPE_ZOOM, NULL);
}

/**
 * gtk_zoom_get_child:
 * @self: a #GtkZoom
 *
 * Gets the child #GtkWidget displayed by @self or %NULL if no child was set.
 *
 * Returns: (nullable) (transfer none): The displayed widget
 **/
GtkWidget *
gtk_zoom_get_child (GtkZoom *self)
{
  g_return_val_if_fail (GTK_IS_ZOOM (self), NULL);

  return self->child;
}

/**
 * gtk_zoom_set_child:
 * @self: a #GtkZoom
 * @child: (allow-none): the #GtkWidget to display
 *
 * Makes @self display the given @child.
 **/
void
gtk_zoom_set_child (GtkZoom   *self,
                    GtkWidget *child)
{
  g_return_if_fail (GTK_IS_ZOOM (self));
  g_return_if_fail (child == NULL || GTK_IS_WIDGET (child));

  if (self->child == child)
    return;

  g_clear_pointer (&self->child, gtk_widget_unparent);

  if (child)
    {
      self->child = child;
      gtk_widget_set_parent (child, GTK_WIDGET (self));
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CHILD]);
}

