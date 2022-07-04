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

#include "gtkcanvas.h"

#include "gtkcanvasbox.h"
#include "gtkcanvasitemprivate.h"
#include "gtkcanvasvectorprivate.h"
#include "gtkintl.h"
#include "gtklistitemfactory.h"
#include "gtkwidgetprivate.h"

#define GDK_ARRAY_NAME gtk_canvas_items
#define GDK_ARRAY_TYPE_NAME GtkCanvasItems
#define GDK_ARRAY_ELEMENT_TYPE GtkCanvasItem * 
#define GDK_ARRAY_FREE_FUNC g_object_unref
#include "gdk/gdkarrayimpl.c"

/**
 * GtkCanvas:
 *
 * `GtkCanvas` is a widget that allows developers to place a list of items
 * using their own method.
 *
 * ![An example GtkCanvas](canvas.png)
 */

struct _GtkCanvas
{
  GtkWidget parent_instance;

  GListModel *model;
  GtkListItemFactory *factory;

  GtkCanvasItems items;
  GHashTable *item_lookup;

  GtkCanvasVector viewport_size;
};

enum
{
  PROP_0,
  PROP_FACTORY,
  PROP_MODEL,

  N_PROPS
};

G_DEFINE_FINAL_TYPE (GtkCanvas, gtk_canvas, GTK_TYPE_WIDGET)

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
gtk_canvas_clear_factory (GtkCanvas *self)
{
  if (self->factory == NULL)
    return;

  g_clear_object (&self->factory);
}

static void
gtk_canvas_remove_items (GtkCanvas *self,
                         guint      pos,
                         guint      n_items)
{
  guint i;

  /* We first remove the items, and then do teardown. That way we have a consistent
   * state in the canvas.
   */
  for (i = pos; i < pos + n_items; i++)
    {
      GtkCanvasItem *ci = gtk_canvas_items_get (&self->items, i);
      g_hash_table_remove (self->item_lookup, gtk_canvas_item_get_item (ci));
    }
  for (i = pos; i < pos + n_items; i++)
    {
      gtk_canvas_item_teardown (gtk_canvas_items_get (&self->items, i), self->factory);
    }
}

static void
gtk_canvas_add_items (GtkCanvas *self,
                      guint      pos,
                      guint      n_items)
{
  guint i;

  /* We first create all the items and then run the factory code
   * on them, so that the factory code can reference the items.
   */
  for (i = pos; i < pos + n_items; i++)
    {
      gpointer item = g_list_model_get_item (self->model, i);
      *gtk_canvas_items_index (&self->items, i) = gtk_canvas_item_new (self, item);
      g_hash_table_insert (self->item_lookup, item, gtk_canvas_items_get (&self->items, i));
    }
  for (i = pos; i < pos + n_items; i++)
    {
      gtk_canvas_item_setup (gtk_canvas_items_get (&self->items, i), self->factory);
    }
}

static void
gtk_canvas_items_changed_cb (GListModel *model,
                             guint       pos,
                             guint       removed,
                             guint       added,
                             GtkCanvas  *self)
{
  gtk_canvas_remove_items (self, pos, removed);

  gtk_canvas_items_splice (&self->items, pos, removed, FALSE, NULL, added);

  gtk_canvas_add_items (self, pos, added);
}

static void
gtk_canvas_clear_model (GtkCanvas *self)
{
  if (self->model == NULL)
    return;

  g_signal_handlers_disconnect_by_func (self->model,
                                        gtk_canvas_items_changed_cb,
                                        self);
  g_clear_object (&self->model);
}

static void
gtk_canvas_dispose (GObject *object)
{
  GtkCanvas *self = GTK_CANVAS (object);

  gtk_canvas_clear_model (self);
  gtk_canvas_clear_factory (self);

  G_OBJECT_CLASS (gtk_canvas_parent_class)->dispose (object);
}

static void
gtk_canvas_finalize (GObject *object)
{
  GtkCanvas *self = GTK_CANVAS (object);

  g_hash_table_unref (self->item_lookup);
  gtk_canvas_vector_finish (&self->viewport_size);

  G_OBJECT_CLASS (gtk_canvas_parent_class)->finalize (object);
}

static void
gtk_canvas_get_property (GObject    *object,
                         guint       property_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  GtkCanvas *self = GTK_CANVAS (object);

  switch (property_id)
    {
    case PROP_FACTORY:
      g_value_set_object (value, self->factory);
      break;

    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_canvas_set_property (GObject      *object,
                         guint         property_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GtkCanvas *self = GTK_CANVAS (object);

  switch (property_id)
    {
    case PROP_FACTORY:
      gtk_canvas_set_factory (self, g_value_get_object (value));
      break;

    case PROP_MODEL:
      gtk_canvas_set_model (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_canvas_validate_variables (GtkCanvas *self)
{
  int i;

  for (i = 0; i < gtk_canvas_items_get_size (&self->items); i++)
    {
      GtkCanvasItem *ci = gtk_canvas_items_get (&self->items, i);

      gtk_canvas_item_validate_variables (ci);
    }
}

static void
gtk_canvas_allocate (GtkWidget *widget,
                     int        width,
                     int        height,
                     int        baseline)
{
  GtkCanvas *self = GTK_CANVAS (widget);
  gboolean missing, force, success;
  gsize i;

  gtk_canvas_validate_variables (self);

  gtk_canvas_vector_init_constant (gtk_canvas_vector_get_variable (&self->viewport_size), width, height);

  force = FALSE;
  do
    {
      /* Try to allocate items in a loop */
      success = FALSE;
      missing = FALSE;

      for (i = 0; i < gtk_canvas_items_get_size (&self->items); i++)
        {
          GtkCanvasItem *ci = gtk_canvas_items_get (&self->items, i);
          GtkWidget *child = gtk_canvas_item_get_widget (ci);
          const GtkCanvasBox *bounds;
          graphene_rect_t rect;
          int x, y, w, h;

          if (child == NULL || gtk_canvas_item_has_allocation (ci))
            continue;

          bounds = gtk_canvas_item_get_bounds (ci);
          if (!gtk_canvas_box_eval (bounds, &rect))
            {
              if (force)
                {
                  rect = *graphene_rect_zero ();
                  /* XXX: set force to FALSE? */
                }
              else
                {
                  missing = TRUE;
                  continue;
                }
            }

          if (gtk_widget_get_request_mode (child) == GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH)
            {
              gtk_widget_measure (child, GTK_ORIENTATION_HORIZONTAL, -1, &w, NULL, NULL, NULL);
              w = MAX (w, ceil (ABS (rect.size.width)));
              gtk_widget_measure (child, GTK_ORIENTATION_VERTICAL, w, &h, NULL, NULL, NULL);
              h = MAX (h, ceil (ABS (rect.size.height)));
            }
          else
            {
              gtk_widget_measure (child, GTK_ORIENTATION_VERTICAL, -1, &h, NULL, NULL, NULL);
              h = MAX (h, ceil (ABS (rect.size.height)));
              gtk_widget_measure (child, GTK_ORIENTATION_HORIZONTAL, h, &w, NULL, NULL, NULL);
              w = MAX (w, ceil (ABS (rect.size.width)));
            }

          if (rect.size.width < 0)
            w = -w;
          if (rect.size.height < 0)
            h = -h;
          if (ABS (w) > ABS (rect.size.width) || ABS (h) > ABS (rect.size.height))
            {
              graphene_vec2_t origin;

              if (!gtk_canvas_vector_eval (gtk_canvas_box_get_origin (bounds), &origin))
                graphene_vec2_init_from_vec2 (&origin, graphene_vec2_zero ());

              if (ABS (w) > ABS (rect.size.width))
                x = round (rect.origin.x + graphene_vec2_get_x (&origin) * (rect.size.width - w));
              else
                x = round (rect.origin.x);
              if (ABS (h) > ABS (rect.size.height))
                y = round (rect.origin.y + graphene_vec2_get_y (&origin) * (rect.size.height - h));
              else
                y = round (rect.origin.y);
            }
          else
            {
              x = round (rect.origin.x);
              y = round (rect.origin.y);
            }

          gtk_canvas_item_allocate (ci, &GRAPHENE_RECT_INIT (x, y, w, h));
          success = TRUE;
        }
      if (!success)
        {
          /* We didn't allocate a single widget in this loop, do something! */
          g_warning ("Could not allocate all Canvas items");
          force = TRUE;
        }
    }
  while (missing);
  
  for (i = 0; i < gtk_canvas_items_get_size (&self->items); i++)
    {
      GtkCanvasItem *ci = gtk_canvas_items_get (&self->items, i);

      gtk_canvas_item_allocate_widget (ci, 0, 0);
    }
}

static void
gtk_canvas_class_init (GtkCanvasClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  widget_class->size_allocate = gtk_canvas_allocate;

  gobject_class->dispose = gtk_canvas_dispose;
  gobject_class->finalize = gtk_canvas_finalize;
  gobject_class->get_property = gtk_canvas_get_property;
  gobject_class->set_property = gtk_canvas_set_property;

  /**
   * GtkCanvas:factory: (attributes org.gtk.Property.get=gtk_canvas_get_factory org.gtk.Property.set=gtk_canvas_set_factory)
   *
   * The factory used to set up canvasitems from items in the model.
   */
  properties[PROP_FACTORY] =
    g_param_spec_object ("factory", NULL, NULL,
                         GTK_TYPE_LIST_ITEM_FACTORY,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkCanvas:model: (attributes org.gtk.Property.get=gtk_canvas_get_model org.gtk.Property.set=gtk_canvas_set_model)
   *
   * The model with the items to display
   */
  properties[PROP_MODEL] =
    g_param_spec_object ("model", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, I_("canvas"));
}

static void
gtk_canvas_init (GtkCanvas *self)
{
  self->item_lookup = g_hash_table_new (g_direct_hash, g_direct_equal);

  gtk_canvas_vector_init_variable (&self->viewport_size);
}

/**
 * gtk_canvas_new:
 * @model: (nullable) (transfer full): the model to use
 * @factory: (nullable) (transfer full): The factory to populate items with
 *
 * Creates a new `GtkCanvas` that uses the given @factory for
 * mapping items to widgets.
 *
 * The function takes ownership of the
 * arguments, so you can write code like
 * ```c
 * canvas = gtk_canvas_new (create_model (),
 *   gtk_builder_list_item_factory_new_from_resource ("/resource.ui"));
 * ```
 *
 * Returns: a new `GtkCanvas` using the given @model and @factory
 */
GtkWidget *
gtk_canvas_new (GListModel         *model,
                GtkListItemFactory *factory)
{
  GtkWidget *result;

  g_return_val_if_fail (model == NULL || G_IS_LIST_MODEL (model), NULL);
  g_return_val_if_fail (factory == NULL || GTK_IS_LIST_ITEM_FACTORY (factory), NULL);

  result = g_object_new (GTK_TYPE_CANVAS,
                         "factory", factory,
                         "model", model,
                         NULL);

  g_clear_object (&model);
  g_clear_object (&factory);

  return result;
}

/**
 * gtk_canvas_set_factory: (attributes org.gtk.Method.set_property=factory)
 * @self: a `GtkCanvas`
 * @factory: (nullable) (transfer none): the factory to use
 *
 * Sets the `GtkListItemFactory` to use for populating canvas items.
 */
void
gtk_canvas_set_factory (GtkCanvas          *self,
                        GtkListItemFactory *factory)
{
  guint n_items;

  g_return_if_fail (GTK_IS_CANVAS (self));
  g_return_if_fail (factory == NULL || GTK_IS_LIST_ITEM_FACTORY (factory));

  if (self->factory == factory)
    return;

  n_items = self->model ? g_list_model_get_n_items (G_LIST_MODEL (self->model)) : 0;
  gtk_canvas_remove_items (self, 0, n_items);

  g_set_object (&self->factory, factory);
  gtk_canvas_items_splice (&self->items, 0, n_items, FALSE, NULL, n_items);

  gtk_canvas_add_items (self, 0, n_items);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FACTORY]);
}

/**
 * gtk_canvas_get_factory: (attributes org.gtk.Method.get_property=factory)
 * @self: a `GtkCanvas`
 *
 * Gets the factory that's currently used to populate canvas items.
 *
 * Returns: (nullable) (transfer none): The factory in use
 */
GtkListItemFactory *
gtk_canvas_get_factory (GtkCanvas *self)
{
  g_return_val_if_fail (GTK_IS_CANVAS (self), NULL);

  return self->factory;
}

/**
 * gtk_canvas_set_model: (attributes org.gtk.Method.set_property=model)
 * @self: a `GtkCanvas`
 * @model: (nullable) (transfer none): the model to use
 *
 * Sets the model containing the items to populate the canvas with.
 */
void
gtk_canvas_set_model (GtkCanvas  *self,
                      GListModel *model)
{
  g_return_if_fail (GTK_IS_CANVAS (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));

  if (self->model == model)
    return;

  gtk_canvas_clear_model (self);

  if (model)
    {
      guint added;

      self->model = g_object_ref (model);

      g_signal_connect (model,
                        "items-changed",
                        G_CALLBACK (gtk_canvas_items_changed_cb),
                        self);

      added = g_list_model_get_n_items (G_LIST_MODEL (model));
      gtk_canvas_items_splice (&self->items, 0, gtk_canvas_items_get_size (&self->items), FALSE, NULL, added);
      gtk_canvas_add_items (self, 0, added);
    }
  else
    {
      gtk_canvas_items_clear (&self->items);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

/**
 * gtk_canvas_get_model: (attributes org.gtk.Method.get_property=model)
 * @self: a `GtkCanvas`
 *
 * Gets the model that's currently used for the displayed items.
 *
 * Returns: (nullable) (transfer none): The model in use
 */
GListModel *
gtk_canvas_get_model (GtkCanvas *self)
{
  g_return_val_if_fail (GTK_IS_CANVAS (self), NULL);

  return self->model;
}

/**
 * gtk_canvas_lookup_item:
 * @self: a `GtkCanvas`
 * @item: an item in the model
 *
 * Gets the `GtkCanvasItem` that manages the given item.
 * If the item is not part of the model, %NULL will be returned.
 *
 * The resulting canvas item will return @item from a call to
 * [method@Gtk.CanvasItem.get_item].
 *
 * During addition of multiple items, this function will work
 * but may return potentially uninitialized canvasitems when the
 * factory has not run on them yet.
 * During item removal, all removed items can not be queried with
 * this function, even if the factory has not unbound the yet.
 *
 * Returns: (nullable) (transfer none): The canvasitem for item
 **/
GtkCanvasItem *
gtk_canvas_lookup_item (GtkCanvas *self,
                        gpointer   item)
{
  g_return_val_if_fail (GTK_IS_CANVAS (self), NULL);
  g_return_val_if_fail (G_IS_OBJECT (item), NULL);

  return g_hash_table_lookup (self->item_lookup, item);
}

const GtkCanvasVector *
gtk_canvas_get_viewport_size (GtkCanvas *self)
{
  return &self->viewport_size;
}
