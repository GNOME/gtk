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
#include "gtkcanvasbox.h"
#include "gtkintl.h"
#include "gtklistitemfactoryprivate.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
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
  GtkCanvasBox allocation;

  guint has_allocation : 1;
};

enum
{
  PROP_0,
  PROP_CANVAS,
  PROP_ITEM,
  PROP_WIDGET,

  N_PROPS
};

enum {
  COMPUTE_BOUNDS,
  LAST_SIGNAL
};

G_DEFINE_FINAL_TYPE (GtkCanvasItem, gtk_canvas_item, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS] = { NULL, };
static guint signals[LAST_SIGNAL] = { 0 };

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
gtk_canvas_item_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkCanvasItem *self = GTK_CANVAS_ITEM (object);

  switch (property_id)
    {
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
  gobject_class->get_property = gtk_canvas_item_get_property;
  gobject_class->set_property = gtk_canvas_item_set_property;

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

  /**
   * GtkCanvasItem::compute-bounds
   * @self: the `GtkCanvasItem`
   * @bounds: (type Gtk.CanvasBox) (out caller-allocates): return
   *   location for the bounds
   *
   * Emitted to determine the bounds for the widget of this canvasitem
   * during a size allocation cycle.
   *
   * A handler for this signal should fill @bounds with
   * the desired box to place the widget in.
   *
   * If the size depends on other items and cannot be computed yet,
   * handlers should return %FALSE and the signal will then be emitted
   * again once more items have been allocated.
   *
   * Because of that signal handlers are expected to be pure - not set
   * any properties or have other side effects - and idempotent - 
   * return the same result if called multiple times in order.
   *
   * returns: %TRUE if @bounds was set successfully
   */
  signals[COMPUTE_BOUNDS] =
    g_signal_new (I_("compute-bounds"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__BOXED,
                  G_TYPE_BOOLEAN, 1,
                  GTK_TYPE_CANVAS_BOX | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (signals[COMPUTE_BOUNDS],
                              G_TYPE_FROM_CLASS (gobject_class),
                              _gtk_marshal_BOOLEAN__BOXEDv);
}

static void
gtk_canvas_item_init (GtkCanvasItem *self)
{
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
gtk_canvas_item_invalidate_allocation (GtkCanvasItem *self)
{
  self->has_allocation = FALSE;
}

gboolean
gtk_canvas_item_allocate (GtkCanvasItem *self,
                          gboolean       force)
{
  gboolean result;
  int w, h;

  g_assert (!self->has_allocation);

  g_signal_emit (self, signals[COMPUTE_BOUNDS], 0, &self->bounds, &result);

  if (!result)
    {
      if (!force)
        return FALSE;
      gtk_canvas_box_init (&self->bounds, 0, 0, 0, 0, 0.5, 0.5);
    }

  if (self->widget)
    {
      if (gtk_widget_get_request_mode (self->widget) == GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH)
        {
          gtk_widget_measure (self->widget, GTK_ORIENTATION_HORIZONTAL, -1, &w, NULL, NULL, NULL);
          w = MAX (w, ceil (ABS (self->bounds.size.width)));
          gtk_widget_measure (self->widget, GTK_ORIENTATION_VERTICAL, w, &h, NULL, NULL, NULL);
          h = MAX (h, ceil (ABS (self->bounds.size.height)));
        }
      else
        {
          gtk_widget_measure (self->widget, GTK_ORIENTATION_VERTICAL, -1, &h, NULL, NULL, NULL);
          h = MAX (h, ceil (ABS (self->bounds.size.height)));
          gtk_widget_measure (self->widget, GTK_ORIENTATION_HORIZONTAL, h, &w, NULL, NULL, NULL);
          w = MAX (w, ceil (ABS (self->bounds.size.width)));
        }

      if (self->bounds.size.width >= 0)
        w = MAX (self->bounds.size.width, w);
      else
        w = MIN (self->bounds.size.width, -w);
      if (self->bounds.size.height >= 0)
        h = MAX (self->bounds.size.height, h);
      else
        h = MIN (self->bounds.size.height, -h);
    }
  else
    {
      w = 0;
      h = 0;
    }

  gtk_canvas_box_init (&self->allocation,
                       round (self->bounds.point.x - self->bounds.origin.horizontal * w)
                         + self->bounds.origin.horizontal * w,
                       round (self->bounds.point.y - self->bounds.origin.vertical * h)
                         + self->bounds.origin.vertical * h,
                       w, h,
                       self->bounds.origin.horizontal, self->bounds.origin.vertical);
  self->has_allocation = TRUE;

  return TRUE;
}

void
gtk_canvas_item_allocate_widget (GtkCanvasItem *self,
                                 float          dx,
                                 float          dy)
{
  graphene_rect_t allocation;
  GskTransform *transform;

  if (self->widget == NULL)
    return;

  gtk_canvas_box_to_rect (&self->allocation, &allocation);

  transform = gsk_transform_translate (NULL,
                                       &GRAPHENE_POINT_INIT (
                                         allocation.origin.x - dx
                                         + (signbit (self->allocation.size.width) ? allocation.size.width : 0),
                                         allocation.origin.y - dy
                                         + (signbit (self->allocation.size.height) ? allocation.size.height : 0)));
  transform = gsk_transform_scale (transform,
                                   signbit (self->allocation.size.width) ? -1 : 1,
                                   signbit (self->allocation.size.height) ? -1 : 1);

  gtk_widget_allocate (self->widget,
                       allocation.size.width,
                       allocation.size.height,
                       -1,
                       transform);
}

gboolean
gtk_canvas_item_has_allocation (GtkCanvasItem *self)
{
  return self->has_allocation;
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

void
gtk_canvas_item_invalidate_bounds (GtkCanvasItem *self)
{
  g_return_if_fail (GTK_IS_CANVAS_ITEM (self));

  if (self->canvas)
    gtk_widget_queue_allocate (GTK_WIDGET (self->canvas));
}

/**
 * gtk_canvas_item_get_bounds:
 * @self: a `GtkCanvasItem`
 *
 * Gets the bounds that are used to allocate the widget
 *
 * If the bounds are not known yet - for example when called during the size
 * allocation phase before this item has succesfully computed its bounds -
 * this function returns %NULL.
 *
 * See also gtk_canvas_item_get_allocation().
 *
 * Returns: (transfer none) (nullable): The bounds
 */
const GtkCanvasBox *
gtk_canvas_item_get_bounds (GtkCanvasItem *self)
{
  g_return_val_if_fail (GTK_IS_CANVAS_ITEM (self), NULL);

  if (!self->has_allocation)
    return NULL;

  return &self->bounds;
}

/**
 * gtk_canvas_item_get_allocation:
 * @self: a `GtkCanvasItem`
 *
 * Gets the allocation assigned to the widget.
 *
 * If the bounds are not known yet - for example when called during the size
 * allocation phase before this item has succesfully computed its bounds -
 * this function returns %NULL.
 *
 * Compared with gtk_canvas_item_get_bounds(), this function returns the actual
 * box used to allocate the widget, which may be different from the bounds
 * to conform to its size requirements.
 *
 * Returns: (transfer none) (nullable): The allocation
 */
const GtkCanvasBox *
gtk_canvas_item_get_allocation (GtkCanvasItem *self)
{
  g_return_val_if_fail (GTK_IS_CANVAS_ITEM (self), NULL);

  if (!self->has_allocation)
    return NULL;

  return &self->allocation;
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
