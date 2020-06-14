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

#include "gtkbitset.h"
#include "gtkintl.h"
#include "gtkselectionmodel.h"

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

  GtkBitset *selected;
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

  return gtk_bitset_contains (self->selected, position);
}

static gboolean
gtk_multi_selection_set_selection (GtkSelectionModel *model,
                                   GtkBitset         *selected,
                                   GtkBitset         *mask)
{
  GtkMultiSelection *self = GTK_MULTI_SELECTION (model);
  GtkBitset *changes;
  guint min, max, n_items;

  /* changes = (self->selected XOR selected) AND mask
   * But doing it this way avoids looking at all values outside the mask
   */
  changes = gtk_bitset_copy (selected);
  gtk_bitset_difference (changes, self->selected);
  gtk_bitset_intersect (changes, mask);

  min = gtk_bitset_get_minimum (changes);
  max = gtk_bitset_get_maximum (changes);

  /* sanity check */
  n_items = g_list_model_get_n_items (self->model);
  if (max >= n_items)
    {
      gtk_bitset_remove_range_closed (changes, n_items, max);
      max = gtk_bitset_get_maximum (changes);
    }

  /* actually do the change */
  gtk_bitset_difference (self->selected, changes);

  gtk_bitset_unref (changes);

  if (min <= max)
    gtk_selection_model_selection_changed (model, min, max - min + 1);

  return TRUE;
}

static void
gtk_multi_selection_selection_model_init (GtkSelectionModelInterface *iface)
{
  iface->is_selected = gtk_multi_selection_is_selected;
  iface->set_selection = gtk_multi_selection_set_selection;
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
  gtk_bitset_slice (self->selected, position, removed, added);

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

  g_clear_pointer (&self->selected, gtk_bitset_unref);

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
  self->selected = gtk_bitset_new_empty ();
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
