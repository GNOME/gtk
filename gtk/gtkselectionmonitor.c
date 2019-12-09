/*
 * Copyright © 2019 Matthias Clasen
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

#include "gtkselectionmonitor.h"

#include "gtkintl.h"
#include "gtkselectionmodel.h"
#include "gtksingleselection.h"

struct _GtkSelectionMonitor
{
  GObject parent_instance;

  guint n_selected;
  GtkSelectionModel *model;

  /* Keep an internal iter to speed up the case where
   * we iterate over all items in the model from 0 to n.
   */
  guint last_in;
  guint last_out;
  guint start_range;
  guint n_items;
  guint n_before;
};

struct _GtkSelectionMonitorClass
{
  GObjectClass parent_class;
};

enum {
  PROP_0,
  PROP_MODEL,
  N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

static GType
gtk_selection_monitor_get_item_type (GListModel *list)
{
  GtkSelectionMonitor *self = GTK_SELECTION_MONITOR (list);

  return g_list_model_get_item_type (G_LIST_MODEL (self->model));
}

static guint
gtk_selection_monitor_get_n_items (GListModel *list)
{
  GtkSelectionMonitor *self = GTK_SELECTION_MONITOR (list);

  return self->n_selected;
}

static gpointer
gtk_selection_monitor_get_item (GListModel *list,
                                guint       position)
{
  GtkSelectionMonitor *self = GTK_SELECTION_MONITOR (list);

  if (GTK_IS_SINGLE_SELECTION (self->model))
    {
      gpointer item;

      if (position != 0)
        return NULL;

      item = gtk_single_selection_get_selected_item (GTK_SINGLE_SELECTION (self->model));
      if (item == NULL)
        return NULL;

      return g_object_ref (item);
    }
  else
    {
      guint n_before;
      guint pos;

      if (self->last_in != GTK_INVALID_LIST_POSITION && position > self->last_in)
        {
          if (self->last_out + (position - self->last_in) < self->start_range + self->n_items)
            {
              /* still inside the last queried range */
              self->last_out = self->last_out + (position - self->last_in);
              self->last_in = position;

              return g_list_model_get_item (G_LIST_MODEL (self->model), self->last_out);
            }
          else
            {
              /* query the next range */
              pos = self->start_range + self->n_items;
              n_before = self->n_before + self->n_items;
            }
        }
      else
        {
          /* query from the beginning */
          pos = 0;
          n_before = 0;
        }

      while (TRUE)
        {
          guint start_range;
          guint n_items;
          gboolean selected;

          gtk_selection_model_query_range (self->model, pos, &start_range, &n_items, &selected);

          if (selected)
            {
              if (position - n_before < n_items)
                {
                  self->last_in = position;
                  self->last_out = start_range + (position - n_before);
                  self->start_range = start_range;
                  self->n_items = n_items;
                  self->n_before = n_before;

                  return g_list_model_get_item (G_LIST_MODEL (self->model), self->last_out);
                }

              n_before += n_items;
            }

          pos = start_range + n_items;

          if (n_items == 0)
            break;
        }

      self->last_in = GTK_INVALID_LIST_POSITION;
      self->last_out = GTK_INVALID_LIST_POSITION;
    }

  return NULL;
}

static void
gtk_selection_monitor_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_selection_monitor_get_item_type;
  iface->get_n_items = gtk_selection_monitor_get_n_items;
  iface->get_item = gtk_selection_monitor_get_item;
}

static gboolean
gtk_selection_monitor_is_selected (GtkSelectionModel *model,
                                   guint              position)
{
  return FALSE;
}

static void
gtk_selection_monitor_query_range (GtkSelectionModel *model,
                                   guint              position,
                                   guint             *start_range,
                                   guint             *n_range,
                                   gboolean          *selected)
{
  GtkSelectionMonitor *self = GTK_SELECTION_MONITOR (model);

  *start_range = 0;
  *n_range = g_list_model_get_n_items (G_LIST_MODEL (self));
  *selected = FALSE;
}

static void
gtk_selection_monitor_selection_model_init (GtkSelectionModelInterface *iface)
{
  iface->is_selected = gtk_selection_monitor_is_selected; 
  iface->query_range = gtk_selection_monitor_query_range;
}

G_DEFINE_TYPE_EXTENDED (GtkSelectionMonitor, gtk_selection_monitor, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL,
                                               gtk_selection_monitor_list_model_init)
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_SELECTION_MODEL,
                                               gtk_selection_monitor_selection_model_init))

static guint
get_n_selected (GtkSelectionMonitor *self)
{
  if (GTK_IS_SINGLE_SELECTION (self->model))
    {
      if (gtk_single_selection_get_selected (GTK_SINGLE_SELECTION (self->model)) == GTK_INVALID_LIST_POSITION)
        return 0;
      else
        return 1;
    }
  else
    {
      guint n_selected;
      guint pos;

      n_selected = 0;
      pos = 0;
      while (TRUE)
        {
          guint start_range;
          guint n_items;
          gboolean selected;

          gtk_selection_model_query_range (self->model, pos, &start_range, &n_items, &selected);

          if (selected)
            n_selected += n_items;

          pos = start_range + n_items;

          if (n_items == 0)
            break;
        }

      return n_selected;
    }
}

static void
selection_changed (GtkSelectionModel *selection,
                   guint position,
                   guint n_items,
                   GtkSelectionMonitor *self)
{
  guint old_selected;

  self->last_in = GTK_INVALID_LIST_POSITION;

  old_selected = self->n_selected;
  self->n_selected = get_n_selected (self);
  g_list_model_items_changed (G_LIST_MODEL (self), 0, old_selected, self->n_selected);
}

static void
gtk_selection_monitor_clear_model (GtkSelectionMonitor *self)
{
  if (self->model == NULL)
    return;

  self->last_in = GTK_INVALID_LIST_POSITION;

  g_signal_handlers_disconnect_by_func (self->model, selection_changed, self);
  g_clear_object (&self->model);
}

static void
gtk_selection_monitor_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)

{
  GtkSelectionMonitor *self = GTK_SELECTION_MONITOR (object);
  guint old_selected;

  switch (prop_id)
    {
    case PROP_MODEL:
      gtk_selection_monitor_clear_model (self);
      self->model = g_value_dup_object (value);
      old_selected = self->n_selected;
      self->n_selected = get_n_selected (self);
      g_list_model_items_changed (G_LIST_MODEL (self), 0, old_selected, self->n_selected);
      g_signal_connect (self->model, "selection-changed", G_CALLBACK (selection_changed), self);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_selection_monitor_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GtkSelectionMonitor *self = GTK_SELECTION_MONITOR (object);

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
gtk_selection_monitor_dispose (GObject *object)
{
  GtkSelectionMonitor *self = GTK_SELECTION_MONITOR (object);

  gtk_selection_monitor_clear_model (self);

  G_OBJECT_CLASS (gtk_selection_monitor_parent_class)->dispose (object);
}

static void
gtk_selection_monitor_class_init (GtkSelectionMonitorClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = gtk_selection_monitor_get_property;
  gobject_class->set_property = gtk_selection_monitor_set_property;
  gobject_class->dispose = gtk_selection_monitor_dispose;

  /**
   * GtkSelectionMonitor:model:
   *
   * The selection model
   */
  properties[PROP_MODEL] =
    g_param_spec_object ("model",
                       P_("The selection model"),
                       P_("The selection model"),
                       GTK_TYPE_SELECTION_MODEL,
                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gtk_selection_monitor_init (GtkSelectionMonitor *self)
{
  self->last_in = GTK_INVALID_LIST_POSITION;
}

/**
 * gtk_selection_monitor_new:
 * @model: (transfer none): the #GtkSelectionModel to monitor
 *
 * Creates a new selection monitor to observe @model.
 *
 * Currently, only #GtkSingleSelection can be monitored
 *
 * Returns: (transfer full) (type GtkSelectionMonitor): a new #GtkSelectionMonitor
 **/
GtkSelectionMonitor *
gtk_selection_monitor_new (GtkSelectionModel *model)
{
  g_return_val_if_fail (GTK_IS_SELECTION_MODEL (model), NULL);

  return g_object_new (GTK_TYPE_SELECTION_MONITOR,
                       "model", model,
                       NULL);
}

/**
 * gtk_selection_monitor_get_model:
 * @self: a #GtkSelectionMonitor
 *
 * Gets the model that @self is wrapping.
 *
 * Returns: (transfer none): The model being wrapped
 **/
GtkSelectionModel *
gtk_selection_monitor_get_model (GtkSelectionMonitor *self)
{
  g_return_val_if_fail (GTK_IS_SELECTION_MONITOR (self), NULL);

  return self->model;
}

