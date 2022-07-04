/*
 * Copyright © 2022 Benjamin Otte
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

#include "gtkcanvasitemprivate.h"

#include "gtkcanvas.h"
#include "gtkcanvasboxprivate.h"
#include "gtkintl.h"
#include "gtklistitemfactoryprivate.h"
#include "gtkwidget.h"

/**
 * GtkCanvasItem:
 *
 * `GtkCanvasItem` holds all information relevant for placing a widget
 * onto the canvas.
 */

struct _GtkCanvasItem
{
  GObject parent_instance;

  GtkCanvas *canvas;
  gpointer item;
  GtkWidget *widget;
  GtkCanvasBox bounds;
  GtkCanvasBox bounds_var;
  GtkCanvasBox allocation_var;

  GtkCanvasVector size_vecs[4];
};

enum
{
  PROP_0,
  PROP_BOUNDS,
  PROP_CANVAS,
  PROP_ITEM,
  PROP_WIDGET,

  N_PROPS
};

G_DEFINE_FINAL_TYPE (GtkCanvasItem, gtk_canvas_item, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
gtk_canvas_item_dispose (GObject *object)
{
  GtkCanvasItem *self = GTK_CANVAS_ITEM (object);

  /* holds a reference */
  g_assert (self->canvas == NULL);
  /* must have been deleted in teardown */
  g_assert (self->item == NULL);
  g_assert (self->widget == NULL);

  G_OBJECT_CLASS (gtk_canvas_item_parent_class)->dispose (object);
}

static void
gtk_canvas_item_finalize (GObject *object)
{
  GtkCanvasItem *self = GTK_CANVAS_ITEM (object);
  int i;

  for (i = 0; i < 4; i++)
    gtk_canvas_vector_finish (&self->size_vecs[i]);

  gtk_canvas_box_finish (&self->bounds);
  gtk_canvas_box_finish (&self->bounds_var);
  gtk_canvas_box_finish (&self->allocation_var);

  G_OBJECT_CLASS (gtk_canvas_item_parent_class)->finalize (object);
}

static void
gtk_canvas_item_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkCanvasItem *self = GTK_CANVAS_ITEM (object);

  switch (property_id)
    {
    case PROP_BOUNDS:
      g_value_set_boxed (value, &self->bounds);
      break;

    case PROP_CANVAS:
      g_value_set_object (value, self->canvas);
      break;

    case PROP_ITEM:
      g_value_set_object (value, self->item);
      break;

    case PROP_WIDGET:
      g_value_set_object (value, self->widget);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_canvas_item_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkCanvasItem *self = GTK_CANVAS_ITEM (object);

  switch (property_id)
    {
    case PROP_BOUNDS:
      gtk_canvas_item_set_bounds (self, g_value_get_boxed (value));
      break;

    case PROP_WIDGET:
      gtk_canvas_item_set_widget (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_canvas_item_class_init (GtkCanvasItemClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gtk_canvas_item_dispose;
  gobject_class->finalize = gtk_canvas_item_finalize;
  gobject_class->get_property = gtk_canvas_item_get_property;
  gobject_class->set_property = gtk_canvas_item_set_property;

  /**
   * GtkCanvasItem:bounds: (attributes org.gtk.Property.get=gtk_canvas_item_get_bounds org.gtk.Property.set=gtk_canvas_item_set_bounds)
   *
   * The bounds to place the widget into.
   */
  properties[PROP_BOUNDS] =
    g_param_spec_boxed ("bounds", NULL, NULL,
                        GTK_TYPE_CANVAS_BOX,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkCanvasItem:canvas: (attributes org.gtk.Property.get=gtk_canvas_item_get_canvas org.gtk.Property.set=gtk_canvas_item_set_canvas)
   *
   * The canvas this item belongs to or %NULL if the canvas has been destroyed
   */
  properties[PROP_CANVAS] =
    g_param_spec_object ("canvas", NULL, NULL,
                         GTK_TYPE_CANVAS,
                         G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkCanvasItem:item: (attributes org.gtk.Property.get=gtk_canvas_item_get_item org.gtk.Property.set=gtk_canvas_item_set_item)
   *
   * The item represented by this canvas item.
   */
  properties[PROP_ITEM] =
    g_param_spec_object ("item", NULL, NULL,
                         G_TYPE_OBJECT,
                         G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkCanvasItem:widget: (attributes org.gtk.Property.get=gtk_canvas_item_get_widget org.gtk.Property.set=gtk_canvas_item_set_widget)
   *
   * The widget managed.
   */
  properties[PROP_WIDGET] =
    g_param_spec_object ("widget", NULL, NULL,
                         GTK_TYPE_WIDGET,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gtk_canvas_item_init (GtkCanvasItem *self)
{
  int i;

  for (i = 0; i < 4; i++)
    gtk_canvas_vector_init_variable (&self->size_vecs[i]);

  gtk_canvas_vector_init_constant (&self->bounds.point, 0, 0);
  gtk_canvas_vector_init_copy (&self->bounds.size, &self->size_vecs[GTK_CANVAS_ITEM_MEASURE_NAT_FOR_NAT]);
  gtk_canvas_vector_init_constant (&self->bounds.origin, 0.5, 0.5);
  gtk_canvas_box_init_variable (&self->bounds_var);
  gtk_canvas_box_update_variable (&self->bounds_var, &self->bounds);
  gtk_canvas_box_init_variable (&self->allocation_var);
}

GtkCanvasItem *
gtk_canvas_item_new (GtkCanvas *canvas,
                     gpointer   item)
{
  GtkCanvasItem *self;

  self = g_object_new (GTK_TYPE_CANVAS_ITEM,
                       NULL);

  /* no reference, the canvas references us */
  self->canvas = canvas;
  /* transfer full */
  self->item = item;

  return self;
}

void
gtk_canvas_item_validate_variables (GtkCanvasItem *self)
{
  int w[4], h[4], i;

  if (self->widget == NULL)
    {
      memset (w, 0, sizeof (w));
      memset (h, 0, sizeof (h));
    }
  else
    {
      if (gtk_widget_get_request_mode (self->widget) == GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH)
        {
          gtk_widget_measure (self->widget, GTK_ORIENTATION_HORIZONTAL, -1, &w[0], &w[2], NULL, NULL);
          w[1] = w[0];
          gtk_widget_measure (self->widget, GTK_ORIENTATION_VERTICAL, w[0], &h[0], &h[1], NULL, NULL);
          w[3] = w[2];
          gtk_widget_measure (self->widget, GTK_ORIENTATION_VERTICAL, w[2], &h[2], &h[3], NULL, NULL);
        }
      else
        {
          gtk_widget_measure (self->widget, GTK_ORIENTATION_VERTICAL, -1, &h[0], &h[2], NULL, NULL);
          h[1] = h[0];
          gtk_widget_measure (self->widget, GTK_ORIENTATION_HORIZONTAL, h[0], &w[0], &w[1], NULL, NULL);
          h[3] = h[2];
          gtk_widget_measure (self->widget, GTK_ORIENTATION_HORIZONTAL, h[2], &w[2], &w[3], NULL, NULL);
        }
    }

  for (i = 0; i < 4; i++)
    {
      gtk_canvas_vector_init_constant (
          gtk_canvas_vector_get_variable (&self->size_vecs[i]),
          0, 0);
    }

  gtk_canvas_vector_init_invalid (
      gtk_canvas_vector_get_variable (&self->allocation_var.point));
  gtk_canvas_vector_init_invalid (
      gtk_canvas_vector_get_variable (&self->allocation_var.size));
  gtk_canvas_vector_init_invalid (
      gtk_canvas_vector_get_variable (&self->allocation_var.origin));
}

void
gtk_canvas_item_allocate (GtkCanvasItem   *self,
                          graphene_rect_t *rect)
{
  graphene_vec2_t origin;

  if (!gtk_canvas_vector_eval (&self->bounds.origin, &origin))
    graphene_vec2_init_from_vec2 (&origin, graphene_vec2_zero ());

  gtk_canvas_vector_init_constant (
      gtk_canvas_vector_get_variable (&self->allocation_var.point),
      rect->origin.x + graphene_vec2_get_x (&origin) * rect->size.width,
      rect->origin.y + graphene_vec2_get_y (&origin) * rect->size.height);
  gtk_canvas_vector_init_constant (
      gtk_canvas_vector_get_variable (&self->allocation_var.size),
      rect->size.width, rect->size.height);
  gtk_canvas_vector_init_constant (
      gtk_canvas_vector_get_variable (&self->allocation_var.origin),
      graphene_vec2_get_x (&origin),
      graphene_vec2_get_y (&origin));
}

void
gtk_canvas_item_allocate_widget (GtkCanvasItem *self,
                                 float          dx,
                                 float          dy)
{
  graphene_rect_t allocation;

  if (self->widget == NULL)
    return;

  if (!gtk_canvas_box_eval (&self->allocation_var, &allocation))
    {
      /* gtkcanvas.c will not call this function otherwise */
      g_assert_not_reached ();
    }

  graphene_rect_normalize (&allocation);
  gtk_widget_size_allocate (self->widget,
                            &(GtkAllocation) {
                              allocation.origin.x - dx,
                              allocation.origin.y - dy,
                              allocation.size.width,
                              allocation.size.height
                            }, -1);
}

gboolean
gtk_canvas_item_has_allocation (GtkCanvasItem *self)
{
  return !gtk_canvas_vector_is_invalid (gtk_canvas_vector_get_variable (&self->allocation_var.point));
}

const GtkCanvasVector *
gtk_canvas_vector_get_item_measure (GtkCanvasItem        *item,
                                    GtkCanvasItemMeasure  measure)
{
  g_return_val_if_fail (GTK_IS_CANVAS_ITEM (item), NULL);

  return &item->size_vecs[measure];
}

const GtkCanvasBox *
gtk_canvas_box_get_item_bounds (GtkCanvasItem *item)
{
  g_return_val_if_fail (GTK_IS_CANVAS_ITEM (item), NULL);

  return &item->bounds_var;
}

const GtkCanvasBox *
gtk_canvas_box_get_item_allocation (GtkCanvasItem *item)
{
  g_return_val_if_fail (GTK_IS_CANVAS_ITEM (item), NULL);

  return &item->allocation_var;
}

/**
 * gtk_canvas_item_get_canvas: (attributes org.gtk.Method.get_property=canvas)
 * @self: a `GtkCanvasItem`
 *
 * Gets the canvas this item belongs to.
 *
 * If the canvas has discarded this item, this property willbe set to %NULL.
 *
 * Returns: (nullable) (transfer none): The canvas
 */
GtkCanvas *
gtk_canvas_item_get_canvas (GtkCanvasItem *self)
{
  g_return_val_if_fail (GTK_IS_CANVAS_ITEM (self), NULL);

  return self->canvas;
}

/**
 * gtk_canvas_item_get_item: (attributes org.gtk.Method.get_property=item)
 * @self: a `GtkCanvasItem`
 *
 * Gets the item that is associated with this canvasitem or %NULL if the canvas has
 * discarded this canvasitem.
 *
 * Returns: (transfer none) (nullable) (type GObject): The item.
 */
gpointer
gtk_canvas_item_get_item (GtkCanvasItem *self)
{
  g_return_val_if_fail (GTK_IS_CANVAS_ITEM (self), NULL);

  return self->item;
}

/**
 * gtk_canvas_item_set_bounds: (attributes org.gtk.Method.set_property=bounds)
 * @self: a `GtkCanvasItem`
 * @bounds: (transfer none): the bounds to allocate the widget in
 *
 * Sets the box to allocate the widget into.
 */
void
gtk_canvas_item_set_bounds (GtkCanvasItem      *self,
                            const GtkCanvasBox *bounds)
{
  g_return_if_fail (GTK_IS_CANVAS_ITEM (self));
  g_return_if_fail (bounds != NULL);

  gtk_canvas_box_init_copy (&self->bounds, bounds);
  gtk_canvas_box_update_variable (&self->bounds_var, bounds);

  if (self->canvas)
    gtk_widget_queue_allocate (GTK_WIDGET (self->canvas));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BOUNDS]);
}

/**
 * gtk_canvas_item_get_bounds: (attributes org.gtk.Method.get_property=bounds)
 * @self: a `GtkCanvasItem`
 *
 * Gets the bounds that are used to allocate the widget
 *
 * Returns: (transfer none): The bounds
 */
const GtkCanvasBox *
gtk_canvas_item_get_bounds (GtkCanvasItem *self)
{
  g_return_val_if_fail (GTK_IS_CANVAS_ITEM (self), NULL);

  return &self->bounds;
}

/**
 * gtk_canvas_item_set_widget: (attributes org.gtk.Method.set_property=widget)
 * @self: a `GtkCanvasItem`
 * @widget: (nullable) (transfer none): the widget to use
 *
 * Sets the widget to be displayed by this item.
 */
void
gtk_canvas_item_set_widget (GtkCanvasItem  *self,
                            GtkWidget      *widget)
{
  g_return_if_fail (GTK_IS_CANVAS_ITEM (self));
  g_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget));

  if (self->widget == widget)
    return;

  if (self->widget)
    {
      if (self->canvas)
        gtk_widget_unparent (self->widget);
      g_object_unref (self->widget);
    }

  self->widget = g_object_ref_sink (widget);

  if (self->canvas)
    {
      /* FIXME: Put in right spot */
      gtk_widget_set_parent (widget, GTK_WIDGET (self->canvas));
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_WIDGET]);
}

/**
 * gtk_canvas_item_get_widget: (attributes org.gtk.Method.get_property=widget)
 * @self: a `GtkCanvasItem`
 *
 * Gets the widget that's currently displayed by this canvasitem
 *
 * Returns: (nullable) (transfer none): The widget in use
 */
GtkWidget *
gtk_canvas_item_get_widget (GtkCanvasItem *self)
{
  g_return_val_if_fail (GTK_IS_CANVAS_ITEM (self), NULL);

  return self->widget;
}

void
gtk_canvas_item_setup (GtkCanvasItem      *self,
                       GtkListItemFactory *factory)
{
  gtk_list_item_factory_setup (factory, G_OBJECT (self), TRUE, NULL, NULL);
}

void
gtk_canvas_item_teardown (GtkCanvasItem      *self,
                          GtkListItemFactory *factory)
{
  gtk_list_item_factory_teardown (factory, G_OBJECT (self), TRUE, NULL, NULL);
}
