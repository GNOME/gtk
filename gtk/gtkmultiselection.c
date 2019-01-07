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
#include "gtksingleselection.h"
#include "gtkset.h"

/**
 * SECTION:gtkmultiselection
 * @Short_description: A selection model that allows selecting a multiple items
 * @Title: GtkMultiSelection
 * @see_also: #GtkSelectionModel
 *
 * GtkMultiSelection is an implementation of the #GtkSelectionModel interface 
 * that allows selecting multiple elements.
 */

struct _GtkMultiSelection
{
  GObject parent_instance;

  GListModel *model;

  GtkSet *selected;
  guint last_selected;
};

/*
 * We store a set of positions for selected items. This can be maintained
 * efficiently as long as it consists of a small number of ranges. I
 * degenerate cases such as 'every second item in the list', it will
 * be O(|model|).
 *
 * To implement persistence across add/remove changes in the underlying
 * model (for example, resorting), we mark the selected objects, which
 * is also going to be O(|model|) in the 'select all' case.
 */

struct _GtkMultiSelectionClass
{
  GObjectClass parent_class;
};

enum {
  PROP_0,

  /* selectionmodel */
  PROP_MODEL,

  N_PROPS = PROP_MODEL
};

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

static void
mark_selected (GtkMultiSelection *self, gboolean in)
{
  GtkSetIter iter;
  guint pos;

  gtk_set_iter_init (&iter, self->selected);
  while (gtk_set_iter_next (&iter, &pos))
    {
      /* Mark the object as being selected in this multiselection.
       * See gtk_multi_selection_items_changed_cb, where this is
       * used to identify objects that were removed and readded.
       */
      GObject *obj = g_list_model_get_item (self->model, pos);
      g_object_set_data (obj, "GtkMultiSelection", in ? self : NULL);
      g_object_unref (obj);
    }
}

static void
mark_range (GtkMultiSelection *self, guint first, guint n_items, gboolean in)
{
  guint pos;

  for (pos = first; pos < first + n_items; pos++)
    {
      GObject *obj = g_list_model_get_item (self->model, pos);
      g_object_set_data (obj, "GtkMultiSelection", in ? self : NULL);
      g_object_unref (obj);
    }
}

static gboolean
gtk_multi_selection_select_range (GtkSelectionModel *model,
                                  guint              position,
                                  guint              n_items,
                                  gboolean           exclusive)
{
  GtkMultiSelection *self = GTK_MULTI_SELECTION (model);

  if (exclusive)
    {
      mark_selected (self, FALSE);
      gtk_set_remove_all (self->selected);
    }
  mark_range (self, position, n_items, TRUE);
  gtk_set_add_range (self->selected, position, n_items);
  gtk_selection_model_selection_changed (model, position, n_items);

  return TRUE;
}

static gboolean
gtk_multi_selection_unselect_range (GtkSelectionModel *model,
                                    guint              position,
                                    guint              n_items)
{
  GtkMultiSelection *self = GTK_MULTI_SELECTION (model);

  mark_range (self, position, n_items, FALSE);
  gtk_set_remove_range (self->selected, position, n_items);
  gtk_selection_model_selection_changed (model, position, n_items);

  return TRUE;
}

static gboolean
gtk_multi_selection_select_item (GtkSelectionModel *model,
                                 guint              position,
                                 gboolean           exclusive,
                                 gboolean           extend)
{
  GtkMultiSelection *self = GTK_MULTI_SELECTION (model);
  guint pos, n_items;

  if (extend && self->last_selected != GTK_INVALID_LIST_POSITION)
    {
      pos = MIN (position, self->last_selected);
      n_items = MAX (position, self->last_selected) - pos + 1;
    }
  else
    {
      pos = position;
      n_items = 1;
    }

  self->last_selected = position;
  return gtk_multi_selection_select_range (model, pos, n_items, exclusive);
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
  GtkMultiSelection *self = GTK_MULTI_SELECTION (model);
  self->last_selected = GTK_INVALID_LIST_POSITION;
  return gtk_multi_selection_unselect_range (model, 0, g_list_model_get_n_items (G_LIST_MODEL (model)));
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
  guint pos;

  gtk_set_remove_range (self->selected, position, removed);
  gtk_set_shift (self->selected, position, (int)added - (int)removed);
  for (pos = position; pos < position + added; pos++)
    {
      GObject *obj = g_list_model_get_item (self->model, pos);
      if (g_object_get_data (obj, "GtkMultiSelection") == self)
        gtk_set_add_item (self->selected, pos);
      g_object_unref (obj);
    }

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
      gtk_multi_selection_clear_model (self);
      self->model = g_value_dup_object (value);
      if (self->model)
        g_signal_connect (self->model, "items-changed",
                          G_CALLBACK (gtk_multi_selection_items_changed_cb), self);
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
  self->last_selected = GTK_INVALID_LIST_POSITION;

  G_OBJECT_CLASS (gtk_multi_selection_parent_class)->dispose (object);
}

static void
gtk_multi_selection_class_init (GtkMultiSelectionClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = gtk_multi_selection_get_property;
  gobject_class->set_property = gtk_multi_selection_set_property;
  gobject_class->dispose = gtk_multi_selection_dispose;

  g_object_class_override_property (gobject_class, PROP_MODEL, "model");
}

static void
gtk_multi_selection_init (GtkMultiSelection *self)
{
  self->selected = gtk_set_new ();
  self->last_selected = GTK_INVALID_LIST_POSITION;
}

/**
 * gtk_multi_selection_new:
 * @model: (transfer none): the #GListModel to manage
 *
 * Creates a new selection to handle @model.
 *
 * Returns: (transfer full) (type GtkMultiSelection): a new #GtkMultiSelection
 **/
GtkMultiSelection *
gtk_multi_selection_new (GListModel *model)
{
  g_return_val_if_fail (G_IS_LIST_MODEL (model), NULL);

  return g_object_new (GTK_TYPE_MULTI_SELECTION,
                       "model", model,
                       NULL);
}

