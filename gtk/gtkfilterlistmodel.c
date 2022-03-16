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

#include "gtkfilterlistmodel.h"

#include "gtkbitset.h"
#include "gtkintl.h"
#include "gtkprivate.h"

/**
 * GtkFilterListModel:
 *
 * `GtkFilterListModel` is a list model that filters the elements of
 * the underlying model according to a `GtkFilter`.
 *
 * It hides some elements from the other model according to
 * criteria given by a `GtkFilter`.
 *
 * The model can be set up to do incremental searching, so that
 * filtering long lists doesn't block the UI. See
 * [method@Gtk.FilterListModel.set_incremental] for details.
 */

enum {
  PROP_0,
  PROP_FILTER,
  PROP_INCREMENTAL,
  PROP_MODEL,
  PROP_PENDING,
  NUM_PROPERTIES
};

struct _GtkFilterListModel
{
  GObject parent_instance;

  GListModel *model;
  GtkFilter *filter;
  GtkFilterMatch strictness;
  gboolean incremental;

  GtkBitset *matches; /* NULL if strictness != GTK_FILTER_MATCH_SOME */
  GtkBitset *pending; /* not yet filtered items or NULL if all filtered */
  guint pending_cb; /* idle callback handle */
};

struct _GtkFilterListModelClass
{
  GObjectClass parent_class;
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static GType
gtk_filter_list_model_get_item_type (GListModel *list)
{
  return G_TYPE_OBJECT;
}

static guint
gtk_filter_list_model_get_n_items (GListModel *list)
{
  GtkFilterListModel *self = GTK_FILTER_LIST_MODEL (list);

  switch (self->strictness)
    {
    case GTK_FILTER_MATCH_NONE:
      return 0;

    case GTK_FILTER_MATCH_ALL:
      return g_list_model_get_n_items (self->model);

    case GTK_FILTER_MATCH_SOME:
      return gtk_bitset_get_size (self->matches);

    default:
      g_assert_not_reached ();
      return 0;
    }
}

static gpointer
gtk_filter_list_model_get_item (GListModel *list,
                                guint       position)
{
  GtkFilterListModel *self = GTK_FILTER_LIST_MODEL (list);
  guint unfiltered;

  switch (self->strictness)
    {
    case GTK_FILTER_MATCH_NONE:
      return NULL;

    case GTK_FILTER_MATCH_ALL:
      unfiltered = position;
      break;

    case GTK_FILTER_MATCH_SOME:
      unfiltered = gtk_bitset_get_nth (self->matches, position);
      if (unfiltered == 0 && position >= gtk_bitset_get_size (self->matches))
        return NULL;
      break;

    default:
      g_assert_not_reached ();
    }

  return g_list_model_get_item (self->model, unfiltered);
}

static void
gtk_filter_list_model_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_filter_list_model_get_item_type;
  iface->get_n_items = gtk_filter_list_model_get_n_items;
  iface->get_item = gtk_filter_list_model_get_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GtkFilterListModel, gtk_filter_list_model, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_filter_list_model_model_init))

static gboolean
gtk_filter_list_model_run_filter_on_item (GtkFilterListModel *self,
                                          guint               position)
{
  gpointer item;
  gboolean visible;

  /* all other cases should have beeen optimized away */
  g_assert (self->strictness == GTK_FILTER_MATCH_SOME);

  item = g_list_model_get_item (self->model, position);
  visible = gtk_filter_match (self->filter, item);
  g_object_unref (item);

  return visible;
}

static void
gtk_filter_list_model_run_filter (GtkFilterListModel *self,
                                  guint               n_steps)
{
  GtkBitsetIter iter;
  guint i, pos;
  gboolean more;

  g_return_if_fail (GTK_IS_FILTER_LIST_MODEL (self));

  if (self->pending == NULL)
    return;

  for (i = 0, more = gtk_bitset_iter_init_first (&iter, self->pending, &pos);
       i < n_steps && more;
       i++, more = gtk_bitset_iter_next (&iter, &pos))
    {
      if (gtk_filter_list_model_run_filter_on_item (self, pos))
        gtk_bitset_add (self->matches, pos);
    }

  if (more)
    gtk_bitset_remove_range_closed (self->pending, 0, pos - 1);
  else
    g_clear_pointer (&self->pending, gtk_bitset_unref);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PENDING]);

  return;
}

static void
gtk_filter_list_model_stop_filtering (GtkFilterListModel *self)
{
  gboolean notify_pending = self->pending != NULL;

  g_clear_pointer (&self->pending, gtk_bitset_unref);
  g_clear_handle_id (&self->pending_cb, g_source_remove);

  if (notify_pending)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PENDING]);
}

static void
gtk_filter_list_model_emit_items_changed_for_changes (GtkFilterListModel *self,
                                                      GtkBitset          *old)
{
  GtkBitset *changes;

  changes = gtk_bitset_copy (self->matches);
  gtk_bitset_difference (changes, old);
  if (!gtk_bitset_is_empty (changes))
    {
      guint min, max;

      min = gtk_bitset_get_minimum (changes);
      max = gtk_bitset_get_maximum (changes);
      g_list_model_items_changed (G_LIST_MODEL (self),
                                  min > 0 ? gtk_bitset_get_size_in_range (self->matches, 0, min - 1) : 0,
                                  gtk_bitset_get_size_in_range (old, min, max),
                                  gtk_bitset_get_size_in_range (self->matches, min, max));
    }
  gtk_bitset_unref (changes);
  gtk_bitset_unref (old);
}

static gboolean
gtk_filter_list_model_run_filter_cb (gpointer data)
{
  GtkFilterListModel *self = data;
  GtkBitset *old;

  old = gtk_bitset_copy (self->matches);
  gtk_filter_list_model_run_filter (self, 512);

  if (self->pending == NULL)
    gtk_filter_list_model_stop_filtering (self);

  gtk_filter_list_model_emit_items_changed_for_changes (self, old);

  return G_SOURCE_CONTINUE;
}

/* NB: bitset is (transfer full) */
static void
gtk_filter_list_model_start_filtering (GtkFilterListModel *self,
                                       GtkBitset          *items)
{
  if (self->pending)
    {
      gtk_bitset_union (self->pending, items);
      gtk_bitset_unref (items);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PENDING]);
      return;
    }

  if (gtk_bitset_is_empty (items))
    {
      gtk_bitset_unref (items);
      return;
    }

  self->pending = items;

  if (!self->incremental)
    {
      gtk_filter_list_model_run_filter (self, G_MAXUINT);
      g_assert (self->pending == NULL);
      return;
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PENDING]);
  g_assert (self->pending_cb == 0);
  self->pending_cb = g_idle_add (gtk_filter_list_model_run_filter_cb, self);
  gdk_source_set_static_name_by_id (self->pending_cb, "[gtk] gtk_filter_list_model_run_filter_cb");
}

static void
gtk_filter_list_model_items_changed_cb (GListModel         *model,
                                        guint               position,
                                        guint               removed,
                                        guint               added,
                                        GtkFilterListModel *self)
{
  guint filter_removed, filter_added;

  switch (self->strictness)
    {
    case GTK_FILTER_MATCH_NONE:
      return;

    case GTK_FILTER_MATCH_ALL:
      g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
      return;

    case GTK_FILTER_MATCH_SOME:
      break;

    default:
      g_assert_not_reached ();
    }

  if (removed > 0)
    filter_removed = gtk_bitset_get_size_in_range (self->matches, position, position + removed - 1);
  else
    filter_removed = 0;

  gtk_bitset_splice (self->matches, position, removed, added);
  if (self->pending)
    gtk_bitset_splice (self->pending, position, removed, added);

  if (added > 0)
    {
      gtk_filter_list_model_start_filtering (self, gtk_bitset_new_range (position, added));
      filter_added = gtk_bitset_get_size_in_range (self->matches, position, position + added - 1);
    }
  else
    filter_added = 0;

  if (filter_removed > 0 || filter_added > 0)
    g_list_model_items_changed (G_LIST_MODEL (self),
                                position > 0 ? gtk_bitset_get_size_in_range (self->matches, 0, position - 1) : 0,
                                filter_removed, filter_added);
}

static void
gtk_filter_list_model_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GtkFilterListModel *self = GTK_FILTER_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_FILTER:
      gtk_filter_list_model_set_filter (self, g_value_get_object (value));
      break;

    case PROP_INCREMENTAL:
      gtk_filter_list_model_set_incremental (self, g_value_get_boolean (value));
      break;

    case PROP_MODEL:
      gtk_filter_list_model_set_model (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_filter_list_model_get_property (GObject     *object,
                                    guint        prop_id,
                                    GValue      *value,
                                    GParamSpec  *pspec)
{
  GtkFilterListModel *self = GTK_FILTER_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_FILTER:
      g_value_set_object (value, self->filter);
      break;

    case PROP_INCREMENTAL:
      g_value_set_boolean (value, self->incremental);
      break;

    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;

    case PROP_PENDING:
      g_value_set_uint (value, gtk_filter_list_model_get_pending (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_filter_list_model_clear_model (GtkFilterListModel *self)
{
  if (self->model == NULL)
    return;

  gtk_filter_list_model_stop_filtering (self);
  g_signal_handlers_disconnect_by_func (self->model, gtk_filter_list_model_items_changed_cb, self);
  g_clear_object (&self->model);
  if (self->matches)
    gtk_bitset_remove_all (self->matches);
}

static void
gtk_filter_list_model_refilter (GtkFilterListModel *self,
                                GtkFilterChange     change)
{
  GtkFilterMatch new_strictness;

  if (self->model == NULL)
    new_strictness = GTK_FILTER_MATCH_NONE;
  else if (self->filter == NULL)
    new_strictness = GTK_FILTER_MATCH_ALL;
  else
    new_strictness = gtk_filter_get_strictness (self->filter);

  /* don't set self->strictness yet so get_n_items() and friends return old values */

  switch (new_strictness)
    {
    case GTK_FILTER_MATCH_NONE:
      {
        guint n_before = g_list_model_get_n_items (G_LIST_MODEL (self));
        g_clear_pointer (&self->matches, gtk_bitset_unref);
        self->strictness = new_strictness;
        gtk_filter_list_model_stop_filtering (self);
        if (n_before > 0)
          g_list_model_items_changed (G_LIST_MODEL (self), 0, n_before, 0);
      }
      break;

    case GTK_FILTER_MATCH_ALL:
      switch (self->strictness)
        {
        case GTK_FILTER_MATCH_NONE:
          self->strictness = new_strictness;
          g_list_model_items_changed (G_LIST_MODEL (self), 0, 0, g_list_model_get_n_items (self->model));
          break;
        case GTK_FILTER_MATCH_ALL:
          self->strictness = new_strictness;
          break;
        default:
        case GTK_FILTER_MATCH_SOME:
          {
            guint n_before, n_after;

            gtk_filter_list_model_stop_filtering (self);
            self->strictness = new_strictness;
            n_after = g_list_model_get_n_items (G_LIST_MODEL (self));
            n_before = gtk_bitset_get_size (self->matches);
            if (n_before == n_after)
              {
                g_clear_pointer (&self->matches, gtk_bitset_unref);
              }
            else
              {
                GtkBitset *inverse;
                guint start, end;

                inverse = gtk_bitset_new_range (0, n_after);
                gtk_bitset_subtract (inverse, self->matches);
                /* otherwise all items would be visible */
                g_assert (!gtk_bitset_is_empty (inverse));

                /* find first filtered */
                start = gtk_bitset_get_minimum (inverse);
                end = n_after - gtk_bitset_get_maximum (inverse) - 1;

                gtk_bitset_unref (inverse);

                g_clear_pointer (&self->matches, gtk_bitset_unref);
                g_list_model_items_changed (G_LIST_MODEL (self), start, n_before - end - start, n_after - end - start);
              }
          }
          break;
        }
      break;

    default:
      g_assert_not_reached ();
      break;

    case GTK_FILTER_MATCH_SOME:
      {
        GtkBitset *old, *pending;

        if (self->matches == NULL)
          {
            if (self->strictness == GTK_FILTER_MATCH_ALL)
              old = gtk_bitset_new_range (0, g_list_model_get_n_items (self->model));
            else
              old = gtk_bitset_new_empty ();
          }
        else
          {
            old = self->matches;
          }
        self->strictness = new_strictness;
        switch (change)
          {
          default:
            g_assert_not_reached ();
            /* fall thru */
          case GTK_FILTER_CHANGE_DIFFERENT:
            self->matches = gtk_bitset_new_empty ();
            pending = gtk_bitset_new_range (0, g_list_model_get_n_items (self->model));
            break;
          case GTK_FILTER_CHANGE_LESS_STRICT:
            self->matches = gtk_bitset_copy (old);
            pending = gtk_bitset_new_range (0, g_list_model_get_n_items (self->model));
            gtk_bitset_subtract (pending, self->matches);
            break;
          case GTK_FILTER_CHANGE_MORE_STRICT:
            self->matches = gtk_bitset_new_empty ();
            pending = gtk_bitset_copy (old);
            break;
          }
        gtk_filter_list_model_start_filtering (self, pending);

        gtk_filter_list_model_emit_items_changed_for_changes (self, old);
      }
    }
}

static void
gtk_filter_list_model_filter_changed_cb (GtkFilter          *filter,
                                         GtkFilterChange     change,
                                         GtkFilterListModel *self)
{
  gtk_filter_list_model_refilter (self, change);
}

static void
gtk_filter_list_model_clear_filter (GtkFilterListModel *self)
{
  if (self->filter == NULL)
    return;

  g_signal_handlers_disconnect_by_func (self->filter, gtk_filter_list_model_filter_changed_cb, self);
  g_clear_object (&self->filter);
}

static void
gtk_filter_list_model_dispose (GObject *object)
{
  GtkFilterListModel *self = GTK_FILTER_LIST_MODEL (object);

  gtk_filter_list_model_clear_model (self);
  gtk_filter_list_model_clear_filter (self);
  g_clear_pointer (&self->matches, gtk_bitset_unref);

  G_OBJECT_CLASS (gtk_filter_list_model_parent_class)->dispose (object);
}

static void
gtk_filter_list_model_class_init (GtkFilterListModelClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->set_property = gtk_filter_list_model_set_property;
  gobject_class->get_property = gtk_filter_list_model_get_property;
  gobject_class->dispose = gtk_filter_list_model_dispose;

  /**
   * GtkFilterListModel:filter: (attributes org.gtk.Property.get=gtk_filter_list_model_get_filter org.gtk.Property.set=gtk_filter_list_model_set_filter)
   *
   * The filter for this model.
   */
  properties[PROP_FILTER] =
      g_param_spec_object ("filter",
                           P_("Filter"),
                           P_("The filter set for this model"),
                           GTK_TYPE_FILTER,
                           GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkFilterListModel:incremental: (attributes org.gtk.Property.get=gtk_filter_list_model_get_incremental org.gtk.Property.set=gtk_filter_list_model_set_incremental)
   *
   * If the model should filter items incrementally.
   */
  properties[PROP_INCREMENTAL] =
      g_param_spec_boolean ("incremental",
                            P_("Incremental"),
                            P_("Filter items incrementally"),
                            FALSE,
                            GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkFilterListModel:model: (attributes org.gtk.Property.get=gtk_filter_list_model_get_model org.gtk.Property.set=gtk_filter_list_model_set_model)
   *
   * The model being filtered.
   */
  properties[PROP_MODEL] =
      g_param_spec_object ("model",
                           P_("Model"),
                           P_("The model being filtered"),
                           G_TYPE_LIST_MODEL,
                           GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkFilterListModel:pending: (attributes org.gtk.Property.get=gtk_filter_list_model_get_pending)
   *
   * Number of items not yet filtered.
   */
  properties[PROP_PENDING] =
      g_param_spec_uint ("pending",
                         P_("Pending"),
                         P_("Number of items not yet filtered"),
                         0, G_MAXUINT, 0,
                         GTK_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, properties);
}

static void
gtk_filter_list_model_init (GtkFilterListModel *self)
{
  self->strictness = GTK_FILTER_MATCH_NONE;
}

/**
 * gtk_filter_list_model_new:
 * @model: (nullable) (transfer full): the model to sort
 * @filter: (nullable) (transfer full): filter
 *
 * Creates a new `GtkFilterListModel` that will filter @model using the given
 * @filter.
 *
 * Returns: a new `GtkFilterListModel`
 **/
GtkFilterListModel *
gtk_filter_list_model_new (GListModel *model,
                           GtkFilter  *filter)
{
  GtkFilterListModel *result;

  g_return_val_if_fail (model == NULL || G_IS_LIST_MODEL (model), NULL);
  g_return_val_if_fail (filter == NULL || GTK_IS_FILTER (filter), NULL);

  result = g_object_new (GTK_TYPE_FILTER_LIST_MODEL,
                         "model", model,
                         "filter", filter,
                         NULL);

  /* consume the references */
  g_clear_object (&model);
  g_clear_object (&filter);

  return result;
}

/**
 * gtk_filter_list_model_set_filter: (attributes org.gtk.Method.set_property=filter)
 * @self: a `GtkFilterListModel`
 * @filter: (nullable) (transfer none): filter to use
 *
 * Sets the filter used to filter items.
 **/
void
gtk_filter_list_model_set_filter (GtkFilterListModel *self,
                                  GtkFilter          *filter)
{
  g_return_if_fail (GTK_IS_FILTER_LIST_MODEL (self));
  g_return_if_fail (filter == NULL || GTK_IS_FILTER (filter));

  if (self->filter == filter)
    return;

  gtk_filter_list_model_clear_filter (self);

  if (filter)
    {
      self->filter = g_object_ref (filter);
      g_signal_connect (filter, "changed", G_CALLBACK (gtk_filter_list_model_filter_changed_cb), self);
      gtk_filter_list_model_filter_changed_cb (filter, GTK_FILTER_CHANGE_DIFFERENT, self);
    }
  else
    {
      gtk_filter_list_model_refilter (self, GTK_FILTER_CHANGE_LESS_STRICT);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FILTER]);
}

/**
 * gtk_filter_list_model_get_filter: (attributes org.gtk.Method.get_property=filter)
 * @self: a `GtkFilterListModel`
 *
 * Gets the `GtkFilter` currently set on @self.
 *
 * Returns: (nullable) (transfer none): The filter currently in use
 */
GtkFilter *
gtk_filter_list_model_get_filter (GtkFilterListModel *self)
{
  g_return_val_if_fail (GTK_IS_FILTER_LIST_MODEL (self), FALSE);

  return self->filter;
}

/**
 * gtk_filter_list_model_set_model: (attributes org.gtk.Method.set_property=model)
 * @self: a `GtkFilterListModel`
 * @model: (nullable): The model to be filtered
 *
 * Sets the model to be filtered.
 *
 * Note that GTK makes no effort to ensure that @model conforms to
 * the item type of @self. It assumes that the caller knows what they
 * are doing and have set up an appropriate filter to ensure that item
 * types match.
 */
void
gtk_filter_list_model_set_model (GtkFilterListModel *self,
                                 GListModel         *model)
{
  guint removed, added;

  g_return_if_fail (GTK_IS_FILTER_LIST_MODEL (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));
  /* Note: We don't check for matching item type here, we just assume the
   * filter func takes care of filtering wrong items. */

  if (self->model == model)
    return;

  removed = g_list_model_get_n_items (G_LIST_MODEL (self));
  gtk_filter_list_model_clear_model (self);

  if (model)
    {
      self->model = g_object_ref (model);
      g_signal_connect (model, "items-changed", G_CALLBACK (gtk_filter_list_model_items_changed_cb), self);
      if (removed == 0)
        {
          self->strictness = GTK_FILTER_MATCH_NONE;
          gtk_filter_list_model_refilter (self, GTK_FILTER_CHANGE_LESS_STRICT);
          added = 0;
        }
      else if (self->matches)
        {
          gtk_filter_list_model_start_filtering (self, gtk_bitset_new_range (0, g_list_model_get_n_items (model)));
          added = gtk_bitset_get_size (self->matches);
        }
      else
        {
          added = g_list_model_get_n_items (model);
        }
    }
  else
    {
      self->strictness = GTK_FILTER_MATCH_NONE;
      added = 0;
    }

  if (removed > 0 || added > 0)
    g_list_model_items_changed (G_LIST_MODEL (self), 0, removed, added);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

/**
 * gtk_filter_list_model_get_model: (attributes org.gtk.Method.get_property=model)
 * @self: a `GtkFilterListModel`
 *
 * Gets the model currently filtered or %NULL if none.
 *
 * Returns: (nullable) (transfer none): The model that gets filtered
 */
GListModel *
gtk_filter_list_model_get_model (GtkFilterListModel *self)
{
  g_return_val_if_fail (GTK_IS_FILTER_LIST_MODEL (self), NULL);

  return self->model;
}

/**
 * gtk_filter_list_model_set_incremental: (attributes org.gtk.Method.set_property=incremental)
 * @self: a `GtkFilterListModel`
 * @incremental: %TRUE to enable incremental filtering
 *
 * Sets the filter model to do an incremental sort.
 *
 * When incremental filtering is enabled, the `GtkFilterListModel` will not
 * run filters immediately, but will instead queue an idle handler that
 * incrementally filters the items and adds them to the list. This of course
 * means that items are not instantly added to the list, but only appear
 * incrementally.
 *
 * When your filter blocks the UI while filtering, you might consider
 * turning this on. Depending on your model and filters, this may become
 * interesting around 10,000 to 100,000 items.
 *
 * By default, incremental filtering is disabled.
 *
 * See [method@Gtk.FilterListModel.get_pending] for progress information
 * about an ongoing incremental filtering operation.
 **/
void
gtk_filter_list_model_set_incremental (GtkFilterListModel *self,
                                       gboolean            incremental)
{
  g_return_if_fail (GTK_IS_FILTER_LIST_MODEL (self));

  if (self->incremental == incremental)
    return;

  self->incremental = incremental;

  if (!incremental)
    {
      GtkBitset *old;
      gtk_filter_list_model_run_filter (self, G_MAXUINT);

      old = gtk_bitset_copy (self->matches);
      gtk_filter_list_model_run_filter (self, 512);

      gtk_filter_list_model_stop_filtering (self);

      gtk_filter_list_model_emit_items_changed_for_changes (self, old);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INCREMENTAL]);
}

/**
 * gtk_filter_list_model_get_incremental: (attributes org.gtk.Method.get_property=incremental)
 * @self: a `GtkFilterListModel`
 *
 * Returns whether incremental filtering is enabled.
 *
 * See [method@Gtk.FilterListModel.set_incremental].
 *
 * Returns: %TRUE if incremental filtering is enabled
 */
gboolean
gtk_filter_list_model_get_incremental (GtkFilterListModel *self)
{
  g_return_val_if_fail (GTK_IS_FILTER_LIST_MODEL (self), FALSE);

  return self->incremental;
}

/**
 * gtk_filter_list_model_get_pending: (attributes org.gtk.Method.get_property=pending)
 * @self: a `GtkFilterListModel`
 *
 * Returns the number of items that have not been filtered yet.
 *
 * You can use this value to check if @self is busy filtering by
 * comparing the return value to 0 or you can compute the percentage
 * of the filter remaining by dividing the return value by the total
 * number of items in the underlying model:
 *
 * ```c
 * pending = gtk_filter_list_model_get_pending (self);
 * model = gtk_filter_list_model_get_model (self);
 * percentage = pending / (double) g_list_model_get_n_items (model);
 * ```
 *
 * If no filter operation is ongoing - in particular when
 * [property@Gtk.FilterListModel:incremental] is %FALSE - this
 * function returns 0.
 *
 * Returns: The number of items not yet filtered
 */
guint
gtk_filter_list_model_get_pending (GtkFilterListModel *self)
{
  g_return_val_if_fail (GTK_IS_FILTER_LIST_MODEL (self), FALSE);

  if (self->pending == NULL)
    return 0;

  return gtk_bitset_get_size (self->pending);
}
