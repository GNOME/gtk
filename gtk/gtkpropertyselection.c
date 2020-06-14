/*
 * Copyright © 2020 Red Hat, Inc.
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
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gtkpropertyselection.h"

#include "gtkbitset.h"
#include "gtkintl.h"
#include "gtkselectionmodel.h"

/**
 * SECTION:gtkpropertyselection
 * @Short_description: A selection model that uses an item property
 * @Title: GtkPropertySelection
 * @see_also: #GtkSelectionModel
 *
 * GtkPropertySelection is an implementation of the #GtkSelectionModel
 * interface that stores the selected state for each item in a property
 * of the item.
 *
 * The property named by #GtkPropertySelection:property must be writable
 * boolean property of the item type. GtkPropertySelection preserves the
 * selected state of items when they are added to the model, but it does
 * not listen to changes of the property while the item is a part of the
 * model. It assumes that it has *exclusive* access to the property.
 *
 * The advantage of storing the selected state in item properties is that
 * the state is *persistent* -- when an item is removed and re-added to
 * the model, it will still have the same selection state. In particular,
 * this makes the selection persist across changes of the sort order if
 * the underlying model is a #GtkSortListModel.
 */

struct _GtkPropertySelection
{
  GObject parent_instance;

  GListModel *model;
  char *property;
};

struct _GtkPropertySelectionClass
{
  GObjectClass parent_class;
};

enum {
  PROP_0,
  PROP_MODEL,
  PROP_PROPERTY,

  N_PROPS,
};

static GParamSpec *properties[N_PROPS] = { NULL, };

static GType
gtk_property_selection_get_item_type (GListModel *list)
{
  GtkPropertySelection *self = GTK_PROPERTY_SELECTION (list);

  return g_list_model_get_item_type (self->model);
}

static guint
gtk_property_selection_get_n_items (GListModel *list)
{
  GtkPropertySelection *self = GTK_PROPERTY_SELECTION (list);

  return g_list_model_get_n_items (self->model);
}

static gpointer
gtk_property_selection_get_item (GListModel *list,
                                 guint       position)
{
  GtkPropertySelection *self = GTK_PROPERTY_SELECTION (list);

  return g_list_model_get_item (self->model, position);
}

static void
gtk_property_selection_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_property_selection_get_item_type;
  iface->get_n_items = gtk_property_selection_get_n_items;
  iface->get_item = gtk_property_selection_get_item;
}

static gboolean
is_selected (GtkPropertySelection *self,
             guint                 position)
{
  gpointer item;
  gboolean ret;

  item = g_list_model_get_item (self->model, position);
  g_object_get (item, self->property, &ret, NULL);
  g_object_unref (item);

  return ret;
}

static void
set_selected (GtkPropertySelection *self,
              guint                 position,
              gboolean              selected)
{
  gpointer item;

  item = g_list_model_get_item (self->model, position);
  g_object_set (item, self->property, selected, NULL);
  g_object_unref (item);
}

static gboolean
gtk_property_selection_is_selected (GtkSelectionModel *model,
                                    guint              position)
{
  return is_selected (GTK_PROPERTY_SELECTION (model), position);
}

static gboolean
gtk_property_selection_set_selection (GtkSelectionModel *model,
                                      GtkBitset         *selected,
                                      GtkBitset         *mask)
{
  GtkPropertySelection *self = GTK_PROPERTY_SELECTION (model);
  GtkBitsetIter iter;
  guint i, n, min, max;
  gboolean has_value;

  n = g_list_model_get_n_items (G_LIST_MODEL (self));
  min = G_MAXUINT;
  max = 0;

  for (has_value = gtk_bitset_iter_init_first (&iter, mask, &i);
       has_value && i < n;
       has_value = gtk_bitset_iter_next (&iter, &i))
    {
      set_selected (self, i, gtk_bitset_contains (selected, i));
      /* XXX: Check if something changed? */
      min = MIN (min, i);
      max = MAX (max, i);
    }

  if (min <= max)
    gtk_selection_model_selection_changed (model, min, max - min + 1);

  return TRUE;
}

static gboolean
gtk_property_selection_add_or_remove (GtkSelectionModel    *model,
                                      gboolean              unselect_rest,
                                      gboolean              add,
                                      GtkSelectionCallback  callback,
                                      gpointer              data)
{
  GtkPropertySelection *self = GTK_PROPERTY_SELECTION (model);
  guint pos, start, n, n_items;
  gboolean in;
  guint min, max;
  guint i;

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self));
  if (unselect_rest)
    {
      for (i = 0; i < n_items; i++)
        set_selected (self, i, FALSE);
    }

  min = G_MAXUINT;
  max = 0;

  pos = 0;
  do
    {
      callback (pos, &start, &n, &in, data);
      if (in)
        {
          if (start < min)
            min = start;
          if (start + n - 1 > max)
            max = start + n - 1;

          for (i = start; i < start + n; i++)
            set_selected (self, i, add);
        }
      pos = start + n;
    }
  while (n > 0);

  /* FIXME: do better here */
  if (unselect_rest)
    gtk_selection_model_selection_changed (model, 0, n_items);
  else if (min <= max)
    gtk_selection_model_selection_changed (model, min, max - min + 1);

  return TRUE;
}

static gboolean
gtk_property_selection_select_callback (GtkSelectionModel    *model,
                                        gboolean              unselect_rest,
                                        GtkSelectionCallback  callback,
                                        gpointer              data)
{
  return gtk_property_selection_add_or_remove (model, unselect_rest, TRUE, callback, data);
}

static gboolean
gtk_property_selection_unselect_callback (GtkSelectionModel    *model,
                                          GtkSelectionCallback  callback,
                                          gpointer              data)
{
  return gtk_property_selection_add_or_remove (model, FALSE, FALSE, callback, data);
}

static void
gtk_property_selection_selection_model_init (GtkSelectionModelInterface *iface)
{
  iface->is_selected = gtk_property_selection_is_selected;
  iface->set_selection = gtk_property_selection_set_selection;
  iface->select_callback = gtk_property_selection_select_callback;
  iface->unselect_callback = gtk_property_selection_unselect_callback;
}

G_DEFINE_TYPE_EXTENDED (GtkPropertySelection, gtk_property_selection, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL,
                                               gtk_property_selection_list_model_init)
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_SELECTION_MODEL,
                                               gtk_property_selection_selection_model_init))

static void
gtk_property_selection_items_changed_cb (GListModel           *model,
                                         guint                 position,
                                         guint                 removed,
                                         guint                 added,
                                         GtkPropertySelection *self)
{
  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
}

static void
gtk_property_selection_clear_model (GtkPropertySelection *self)
{
  if (self->model == NULL)
    return;

  g_signal_handlers_disconnect_by_func (self->model,
                                        gtk_property_selection_items_changed_cb,
                                        self);
  g_clear_object (&self->model);
}

static void
gtk_property_selection_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)

{
  GtkPropertySelection *self = GTK_PROPERTY_SELECTION (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      self->model = g_value_dup_object (value);
      g_warn_if_fail (self->model != NULL);
      g_signal_connect (self->model,
                        "items-changed",
                        G_CALLBACK (gtk_property_selection_items_changed_cb),
                        self);
      break;

    case PROP_PROPERTY:
      {
        GObjectClass *class;
        GParamSpec *prop;

        self->property = g_value_dup_string (value);
        g_warn_if_fail (self->property != NULL);

        class = g_type_class_ref (g_list_model_get_item_type (self->model));
        prop = g_object_class_find_property (class, self->property);
        g_warn_if_fail (prop != NULL &&
                        prop->value_type == G_TYPE_BOOLEAN &&
                        ((prop->flags & (G_PARAM_READABLE|G_PARAM_WRITABLE|G_PARAM_CONSTRUCT_ONLY)) ==
                        (G_PARAM_READABLE|G_PARAM_WRITABLE)));
        g_type_class_unref (class);
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_property_selection_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GtkPropertySelection *self = GTK_PROPERTY_SELECTION (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;

    case PROP_PROPERTY:
      g_value_set_string (value, self->property);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_property_selection_dispose (GObject *object)
{
  GtkPropertySelection *self = GTK_PROPERTY_SELECTION (object);

  gtk_property_selection_clear_model (self);

  g_free (self->property);

  G_OBJECT_CLASS (gtk_property_selection_parent_class)->dispose (object);
}

static void
gtk_property_selection_class_init (GtkPropertySelectionClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = gtk_property_selection_get_property;
  gobject_class->set_property = gtk_property_selection_set_property;
  gobject_class->dispose = gtk_property_selection_dispose;

  /**
   * GtkPropertySelection:model
   *
   * The list managed by this selection
   */
  properties[PROP_MODEL] =
    g_param_spec_object ("model",
                         P_("Model"),
                         P_("List managed by this selection"),
                         G_TYPE_LIST_MODEL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_PROPERTY] =
    g_param_spec_string ("property",
                         P_("Property"),
                         P_("Item property to store selection state in"),
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gtk_property_selection_init (GtkPropertySelection *self)
{
}

/**
 * gtk_property_selection_new:
 * @model: (transfer none): the #GListModel to manage
 * @property: the item property to use
 *
 * Creates a new property selection to handle @model.
 *
 * @property must be the name of a writable boolean property
 * of the item type of @model.
 *
 * Note that GtkPropertySelection does not monitor the property
 * for changes while the item is part of the model, but it does
 * inherit the initial value when an item is added to the model.
 *
 * Returns: (transfer full) (type GtkPropertySelection): a new #GtkPropertySelection
 **/
GListModel *
gtk_property_selection_new (GListModel *model,
                            const char *property)
{
  g_return_val_if_fail (G_IS_LIST_MODEL (model), NULL);

  return g_object_new (GTK_TYPE_PROPERTY_SELECTION,
                       "model", model,
                       "property", property,
                       NULL);
}
