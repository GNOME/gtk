/*
 * Copyright © 2019 Red Hat, Inc.
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

#include "gtkmultiselection.h"

#include "gtkintl.h"
#include "gtkselectionmodel.h"
#include "gtkset.h"

/**
 * SECTION:gtkmultiselection
 * @Short_description: A selection model that allows selecting a multiple items
 * @Title: GtkMultiSelection
 * @see_also: #GtkSelectionModel
 *
 * GtkMultiSelection is an implementation of the #GtkSelectionModel interface
 * that allows selecting multiple elements.
 *
 * Note that due to the way the selection is stored, newly added items are
 * always unselected, even if they were just removed from the model, and were
 * selected before. In particular this means that changing the sort order of
 * an underlying sort model will clear the selection. In other words, the
 * selection is *not persistent*.
 */

struct _GtkMultiSelection
{
  GObject parent_instance;

  GListModel *model;

  GtkSet *selected;
};

struct _GtkMultiSelectionClass
{
  GObjectClass parent_class;
};

enum {
  PROP_0,
  PROP_MODEL,

  N_PROPS,
};

static GParamSpec *properties[N_PROPS] = { NULL, };

static GType
gtk_multi_selection_get_item_type (GListModel *list)
{
  GtkMultiSelection *self = GTK_MULTI_SELECTION (list);

  return g_list_model_get_item_type (self->model);
}

static guint
gtk_multi_selection_get_n_items (GListModel *list)
{
  GtkMultiSelection *self = GTK_MULTI_SELECTION (list);

  return g_list_model_get_n_items (self->model);
}

static gpointer
gtk_multi_selection_get_item (GListModel *list,
                              guint       position)
{
  GtkMultiSelection *self = GTK_MULTI_SELECTION (list);

  return g_list_model_get_item (self->model, position);
}

static void
gtk_multi_selection_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_multi_selection_get_item_type;
  iface->get_n_items = gtk_multi_selection_get_n_items;
  iface->get_item = gtk_multi_selection_get_item;
}

static gboolean
gtk_multi_selection_is_selected (GtkSelectionModel *model,
                                 guint              position)
{
  GtkMultiSelection *self = GTK_MULTI_SELECTION (model);

  return gtk_set_contains (self->selected, position);
}

static gboolean
gtk_multi_selection_select_range (GtkSelectionModel *model,
                                  guint              position,
                                  guint              n_items,
                                  gboolean           exclusive)
{
  GtkMultiSelection *self = GTK_MULTI_SELECTION (model);
  guint min = G_MAXUINT;
  guint max = 0;

  if (exclusive)
    {
      min = gtk_set_get_min (self->selected);
      max = gtk_set_get_max (self->selected);
      gtk_set_remove_all (self->selected);
    }

  gtk_set_add_range (self->selected, position, n_items);

  min = MIN (position, min);
  max = MAX (max, position + n_items - 1);

  gtk_selection_model_selection_changed (model, min, max - min + 1);

  return TRUE;
}

static gboolean
gtk_multi_selection_unselect_range (GtkSelectionModel *model,
                                    guint              position,
                                    guint              n_items)
{
  GtkMultiSelection *self = GTK_MULTI_SELECTION (model);

  gtk_set_remove_range (self->selected, position, n_items);
  gtk_selection_model_selection_changed (model, position, n_items);

  return TRUE;
}

static gboolean
gtk_multi_selection_select_item (GtkSelectionModel *model,
                                 guint              position,
                                 gboolean           exclusive)
{
  return gtk_multi_selection_select_range (model, position, 1, exclusive);
}

static gboolean
gtk_multi_selection_unselect_item (GtkSelectionModel *model,
                                   guint              position)
{
  return gtk_multi_selection_unselect_range (model, position, 1);
}

static gboolean
gtk_multi_selection_select_all (GtkSelectionModel *model)
{
  return gtk_multi_selection_select_range (model, 0, g_list_model_get_n_items (G_LIST_MODEL (model)), FALSE);
}

static gboolean
gtk_multi_selection_unselect_all (GtkSelectionModel *model)
{
  return gtk_multi_selection_unselect_range (model, 0, g_list_model_get_n_items (G_LIST_MODEL (model)));
}

static gboolean
gtk_multi_selection_add_or_remove (GtkSelectionModel    *model,
                                   gboolean              add,
                                   GtkSelectionCallback  callback,
                                   gpointer              data)
{
  GtkMultiSelection *self = GTK_MULTI_SELECTION (model);
  guint pos, start, n;
  gboolean in;
  guint min, max;

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

          if (add)
            gtk_set_add_range (self->selected, start, n);
          else
            gtk_set_remove_range (self->selected, start, n);
        }
      pos = start + n;
    }
  while (n > 0);

  if (min <= max)
    gtk_selection_model_selection_changed (model, min, max - min + 1);

  return TRUE;
}

static gboolean
gtk_multi_selection_select_callback (GtkSelectionModel    *model,
                                     GtkSelectionCallback  callback,
                                     gpointer              data)
{
  return gtk_multi_selection_add_or_remove (model, TRUE, callback, data);
}

static gboolean
gtk_multi_selection_unselect_callback (GtkSelectionModel    *model,
                                       GtkSelectionCallback  callback,
                                       gpointer              data)
{
  return gtk_multi_selection_add_or_remove (model, FALSE, callback, data);
}

static void
gtk_multi_selection_query_range (GtkSelectionModel *model,
                                 guint              position,
                                 guint             *start_range,
                                 guint             *n_items,
                                 gboolean          *selected)
{
  GtkMultiSelection *self = GTK_MULTI_SELECTION (model);
  guint upper_bound = g_list_model_get_n_items (self->model);

  gtk_set_find_range (self->selected, position, upper_bound, start_range, n_items, selected);
}

static void
gtk_multi_selection_selection_model_init (GtkSelectionModelInterface *iface)
{
  iface->is_selected = gtk_multi_selection_is_selected;
  iface->select_item = gtk_multi_selection_select_item;
  iface->unselect_item = gtk_multi_selection_unselect_item;
  iface->select_range = gtk_multi_selection_select_range;
  iface->unselect_range = gtk_multi_selection_unselect_range;
  iface->select_all = gtk_multi_selection_select_all;
  iface->unselect_all = gtk_multi_selection_unselect_all;
  iface->select_callback = gtk_multi_selection_select_callback;
  iface->unselect_callback = gtk_multi_selection_unselect_callback;
  iface->query_range = gtk_multi_selection_query_range;
}

G_DEFINE_TYPE_EXTENDED (GtkMultiSelection, gtk_multi_selection, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL,
                                               gtk_multi_selection_list_model_init)
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_SELECTION_MODEL,
                                               gtk_multi_selection_selection_model_init))

static void
gtk_multi_selection_items_changed_cb (GListModel        *model,
                                      guint              position,
                                      guint              removed,
                                      guint              added,
                                      GtkMultiSelection *self)
{
  gtk_set_remove_range (self->selected, position, removed);
  gtk_set_shift (self->selected, position, (int)added - (int)removed);
  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
}

static void
gtk_multi_selection_clear_model (GtkMultiSelection *self)
{
  if (self->model == NULL)
    return;

  g_signal_handlers_disconnect_by_func (self->model,
                                        gtk_multi_selection_items_changed_cb,
                                        self);
  g_clear_object (&self->model);
}

static void
gtk_multi_selection_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)

{
  GtkMultiSelection *self = GTK_MULTI_SELECTION (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      self->model = g_value_dup_object (value);
      g_warn_if_fail (self->model != NULL);
      g_signal_connect (self->model,
                        "items-changed",
                        G_CALLBACK (gtk_multi_selection_items_changed_cb),
                        self);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_multi_selection_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GtkMultiSelection *self = GTK_MULTI_SELECTION (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_multi_selection_dispose (GObject *object)
{
  GtkMultiSelection *self = GTK_MULTI_SELECTION (object);

  gtk_multi_selection_clear_model (self);

  g_clear_pointer (&self->selected, gtk_set_free);

  G_OBJECT_CLASS (gtk_multi_selection_parent_class)->dispose (object);
}

static void
gtk_multi_selection_class_init (GtkMultiSelectionClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = gtk_multi_selection_get_property;
  gobject_class->set_property = gtk_multi_selection_set_property;
  gobject_class->dispose = gtk_multi_selection_dispose;

  /**
   * GtkMultiSelection:model
   *
   * The list managed by this selection
   */
  properties[PROP_MODEL] =
    g_param_spec_object ("model",
                         P_("Model"),
                         P_("List managed by this selection"),
                         G_TYPE_LIST_MODEL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gtk_multi_selection_init (GtkMultiSelection *self)
{
  self->selected = gtk_set_new ();
}

/**
 * gtk_multi_selection_new:
 * @model: (transfer none): the #GListModel to manage
 *
 * Creates a new selection to handle @model.
 *
 * Returns: (transfer full) (type GtkMultiSelection): a new #GtkMultiSelection
 **/
GListModel *
gtk_multi_selection_new (GListModel *model)
{
  g_return_val_if_fail (G_IS_LIST_MODEL (model), NULL);

  return g_object_new (GTK_TYPE_MULTI_SELECTION,
                       "model", model,
                       NULL);
}
