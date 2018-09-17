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

#include "gtklistview.h"

#include "gtkintl.h"
#include "gtklistitemfactoryprivate.h"

/**
 * SECTION:gtklistview
 * @title: GtkListView
 * @short_description: A widget for displaying lists
 * @see_also: #GListModel
 *
 * GtkListView is a widget to present a view into a large dynamic list of items.
 */

struct _GtkListView
{
  GtkWidget parent_instance;

  GListModel *model;
  GtkListItemFactory *item_factory;
};

enum
{
  PROP_0,
  PROP_MODEL,

  N_PROPS
};

G_DEFINE_TYPE (GtkListView, gtk_list_view, GTK_TYPE_WIDGET)

static GParamSpec *properties[N_PROPS] = { NULL, };

static gboolean
gtk_list_view_is_empty (GtkListView *self)
{
  return self->model == NULL;
}

static void
gtk_list_view_measure (GtkWidget      *widget,
                       GtkOrientation  orientation,
                       int             for_size,
                       int            *minimum,
                       int            *natural,
                       int            *minimum_baseline,
                       int            *natural_baseline)
{
  GtkListView *self = GTK_LIST_VIEW (widget);

  if (gtk_list_view_is_empty (self))
    {
      *minimum = 0;
      *natural = 0;
      return;
    }

  *minimum = 0;
  *natural = 0;
  return;
}

static void
gtk_list_view_size_allocate (GtkWidget *widget,
                             int        width,
                             int        height,
                             int        baseline)
{
  //GtkListView *self = GTK_LIST_VIEW (widget);
}

static void
gtk_list_view_model_items_changed_cb (GListModel  *model,
                                      guint        position,
                                      guint        removed,
                                      guint        added,
                                      GtkListView *self)
{
}

static void
gtk_list_view_clear_model (GtkListView *self)
{
  if (self->model == NULL)
    return;

  g_signal_handlers_disconnect_by_func (self->model, gtk_list_view_model_items_changed_cb, self);
  g_clear_object (&self->model);
}

static void
gtk_list_view_dispose (GObject *object)
{
  GtkListView *self = GTK_LIST_VIEW (object);

  gtk_list_view_clear_model (self);

  g_clear_object (&self->item_factory);

  G_OBJECT_CLASS (gtk_list_view_parent_class)->dispose (object);
}

static void
gtk_list_view_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkListView *self = GTK_LIST_VIEW (object);

  switch (property_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_list_view_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkListView *self = GTK_LIST_VIEW (object);

  switch (property_id)
    {
    case PROP_MODEL:
      gtk_list_view_set_model (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_list_view_class_init (GtkListViewClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  widget_class->measure = gtk_list_view_measure;
  widget_class->size_allocate = gtk_list_view_size_allocate;

  gobject_class->dispose = gtk_list_view_dispose;
  gobject_class->get_property = gtk_list_view_get_property;
  gobject_class->set_property = gtk_list_view_set_property;

  /**
   * GtkListView:model:
   *
   * Model for the items displayed
   */
  properties[PROP_MODEL] =
    g_param_spec_object ("model",
                         P_("Model"),
                         P_("Model for the items displayed"),
                         G_TYPE_LIST_MODEL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, I_("list"));
}

static void
gtk_list_view_init (GtkListView *self)
{
}

/**
 * gtk_list_view_new:
 *
 * Creates a new empty #GtkListView.
 *
 * You most likely want to call gtk_list_view_set_model() to set
 * a model and then set up a way to map its items to widgets next.
 *
 * Returns: a new #GtkListView
 **/
GtkWidget *
gtk_list_view_new (void)
{
  return g_object_new (GTK_TYPE_LIST_VIEW, NULL);
}

/**
 * gtk_list_view_get_model:
 * @self: a #GtkListView
 *
 * Gets the model that's currently used to read the items displayed.
 *
 * Returns: (nullable) (transfer none): The model in use
 **/
GListModel *
gtk_list_view_get_model (GtkListView *self)
{
  g_return_val_if_fail (GTK_IS_LIST_VIEW (self), NULL);

  return self->model;
}

/**
 * gtk_list_view_set_model:
 * @self: a #GtkListView
 * @file: (allow-none) (transfer none): the model to use or %NULL for none
 *
 * Sets the #GListModel to use for
 **/
void
gtk_list_view_set_model (GtkListView *self,
                         GListModel  *model)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));

  if (self->model == model)
    return;

  gtk_list_view_clear_model (self);

  if (model)
    {
      self->model = g_object_ref (model);

      g_signal_connect (model,
                        "items-changed", 
                        G_CALLBACK (gtk_list_view_model_items_changed_cb),
                        self);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

void
gtk_list_view_set_functions (GtkListView            *self,
                             GtkListCreateWidgetFunc create_func,
                             GtkListBindWidgetFunc   bind_func,
                             gpointer                user_data,
                             GDestroyNotify          user_destroy)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (self));
  g_return_if_fail (create_func);
  g_return_if_fail (bind_func);
  g_return_if_fail (user_data != NULL || user_destroy == NULL);

  g_clear_object (&self->item_factory);

  self->item_factory = gtk_list_item_factory_new (create_func, bind_func, user_data, user_destroy);
}

