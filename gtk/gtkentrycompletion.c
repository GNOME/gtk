/* gtkentrycompletion.c
 * Copyright (C) 2003  Kristian Rietveld  <kris@gtk.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include "gtkentrycompletion.h"
#include "gtkentryprivate.h"
#include "gtkcelllayout.h"

#include "gtkintl.h"
#include "gtkcellrenderertext.h"
#include "gtktreeselection.h"
#include "gtktreeview.h"
#include "gtkscrolledwindow.h"
#include "gtkvbox.h"
#include "gtkwindow.h"
#include "gtkentry.h"
#include "gtkmain.h"
#include "gtksignal.h"
#include "gtkmarshalers.h"

#include <string.h>


/* signals */
enum
{
  MATCH_SELECTED,
  ACTION_ACTIVATED,
  LAST_SIGNAL
};

/* properties */
enum
{
  PROP_0,
  PROP_MODEL,
  PROP_MINIMUM_KEY_LENGTH
};

#define GTK_ENTRY_COMPLETION_GET_PRIVATE(obj)(G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_ENTRY_COMPLETION, GtkEntryCompletionPrivate))

static void     gtk_entry_completion_class_init          (GtkEntryCompletionClass *klass);
static void     gtk_entry_completion_cell_layout_init    (GtkCellLayoutIface      *iface);
static void     gtk_entry_completion_init                (GtkEntryCompletion      *completion);
static void     gtk_entry_completion_set_property        (GObject                 *object,
                                                          guint                    prop_id,
                                                          const GValue            *value,
                                                          GParamSpec              *pspec);
static void     gtk_entry_completion_get_property        (GObject                 *object,
                                                          guint                    prop_id,
                                                          GValue                  *value,
                                                          GParamSpec              *pspec);
static void     gtk_entry_completion_finalize            (GObject                 *object);

static void     gtk_entry_completion_pack_start          (GtkCellLayout           *cell_layout,
                                                          GtkCellRenderer         *cell,
                                                          gboolean                 expand);
static void     gtk_entry_completion_pack_end            (GtkCellLayout           *cell_layout,
                                                          GtkCellRenderer         *cell,
                                                          gboolean                 expand);
static void     gtk_entry_completion_clear               (GtkCellLayout           *cell_layout);
static void     gtk_entry_completion_add_attribute       (GtkCellLayout           *cell_layout,
                                                          GtkCellRenderer         *cell,
                                                          const char              *attribute,
                                                          gint                     column);
static void     gtk_entry_completion_set_cell_data_func  (GtkCellLayout           *cell_layout,
                                                          GtkCellRenderer         *cell,
                                                          GtkCellLayoutDataFunc    func,
                                                          gpointer                 func_data,
                                                          GDestroyNotify           destroy);
static void     gtk_entry_completion_clear_attributes    (GtkCellLayout           *cell_layout,
                                                          GtkCellRenderer         *cell);
static void     gtk_entry_completion_reorder             (GtkCellLayout           *cell_layout,
                                                          GtkCellRenderer         *cell,
                                                          gint                     position);

static gboolean gtk_entry_completion_visible_func        (GtkTreeModel            *model,
                                                          GtkTreeIter             *iter,
                                                          gpointer                 data);
static gboolean gtk_entry_completion_popup_key_press     (GtkWidget               *widget,
                                                          GdkEventKey             *event,
                                                          gpointer                 user_data);
static gboolean gtk_entry_completion_popup_button_press  (GtkWidget               *widget,
                                                          GdkEventButton          *event,
                                                          gpointer                 user_data);
static gboolean gtk_entry_completion_list_button_press   (GtkWidget               *widget,
                                                          GdkEventButton          *event,
                                                          gpointer                 user_data);
static gboolean gtk_entry_completion_action_button_press (GtkWidget               *widget,
                                                          GdkEventButton          *event,
                                                          gpointer                 user_data);
static void     gtk_entry_completion_selection_changed   (GtkTreeSelection        *selection,
                                                          gpointer                 data);

static void     gtk_entry_completion_insert_action       (GtkEntryCompletion      *completion,
                                                          gint                     index,
                                                          const gchar             *string,
                                                          gboolean                 markup);
static void     gtk_entry_completion_action_data_func    (GtkTreeViewColumn       *tree_column,
                                                          GtkCellRenderer         *cell,
                                                          GtkTreeModel            *model,
                                                          GtkTreeIter             *iter,
                                                          gpointer                 data);


static GObjectClass *parent_class = NULL;
static guint entry_completion_signals[LAST_SIGNAL] = { 0 };


GType
gtk_entry_completion_get_type (void)
{
  static GType entry_completion_type = 0;

  if (!entry_completion_type)
    {
      static const GTypeInfo entry_completion_info =
      {
        sizeof (GtkEntryCompletionClass),
        NULL,
        NULL,
        (GClassInitFunc) gtk_entry_completion_class_init,
        NULL,
        NULL,
        sizeof (GtkEntryCompletion),
        0,
        (GInstanceInitFunc) gtk_entry_completion_init
      };

      static const GInterfaceInfo cell_layout_info =
      {
        (GInterfaceInitFunc) gtk_entry_completion_cell_layout_init,
        NULL,
        NULL
      };

      entry_completion_type =
        g_type_register_static (G_TYPE_OBJECT, "GtkEntryCompletion",
                                &entry_completion_info, 0);

      g_type_add_interface_static (entry_completion_type,
                                   GTK_TYPE_CELL_LAYOUT,
                                   &cell_layout_info);
    }

  return entry_completion_type;
}

static void
gtk_entry_completion_class_init (GtkEntryCompletionClass *klass)
{
  GObjectClass *object_class;

  parent_class = g_type_class_peek_parent (klass);
  object_class = (GObjectClass *)klass;

  object_class->set_property = gtk_entry_completion_set_property;
  object_class->get_property = gtk_entry_completion_get_property;
  object_class->finalize = gtk_entry_completion_finalize;

  entry_completion_signals[MATCH_SELECTED] =
    g_signal_new ("match_selected",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkEntryCompletionClass, match_selected),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__OBJECT_BOXED,
                  G_TYPE_BOOLEAN, 2,
                  GTK_TYPE_TREE_MODEL,
                  GTK_TYPE_TREE_ITER);
  entry_completion_signals[ACTION_ACTIVATED] =
    g_signal_new ("action_activated",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkEntryCompletionClass, action_activated),
                  NULL, NULL,
                  _gtk_marshal_NONE__INT,
                  G_TYPE_NONE, 1,
                  G_TYPE_INT);

  g_object_class_install_property (object_class,
                                   PROP_MODEL,
                                   g_param_spec_object ("model",
                                                        P_("Completion Model"),
                                                        P_("The model to find matches in"),
                                                        GTK_TYPE_TREE_MODEL,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_MINIMUM_KEY_LENGTH,
                                   g_param_spec_int ("minimum_key_length",
                                                     P_("Minimum Key Length"),
                                                     P_("Minimum length of the search key in order to look up matches"),
                                                     -1,
                                                     G_MAXINT,
                                                     1,
                                                     G_PARAM_READWRITE));

  g_type_class_add_private (object_class, sizeof (GtkEntryCompletionPrivate));
}

static void
gtk_entry_completion_cell_layout_init (GtkCellLayoutIface *iface)
{
  iface->pack_start = gtk_entry_completion_pack_start;
  iface->pack_end = gtk_entry_completion_pack_end;
  iface->clear = gtk_entry_completion_clear;
  iface->add_attribute = gtk_entry_completion_add_attribute;
  iface->set_cell_data_func = gtk_entry_completion_set_cell_data_func;
  iface->clear_attributes = gtk_entry_completion_clear_attributes;
  iface->reorder = gtk_entry_completion_reorder;
}

static void
gtk_entry_completion_init (GtkEntryCompletion *completion)
{
  GtkCellRenderer *cell;
  GtkTreeSelection *sel;
  GtkEntryCompletionPrivate *priv;

  /* yes, also priv, need to keep the code readable */
  priv = completion->priv = GTK_ENTRY_COMPLETION_GET_PRIVATE (completion);

  priv->minimum_key_length = 1;
  priv->text_column = -1;

  /* completions */
  priv->filter_model = NULL;

  priv->tree_view = gtk_tree_view_new ();
  g_signal_connect (priv->tree_view, "button_press_event",
                    G_CALLBACK (gtk_entry_completion_list_button_press),
                    completion);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (priv->tree_view), FALSE);

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view));
  gtk_tree_selection_set_mode (sel, GTK_SELECTION_SINGLE);
  gtk_tree_selection_unselect_all (sel);
  g_signal_connect (sel, "changed",
                    G_CALLBACK (gtk_entry_completion_selection_changed),
                    completion);
  priv->first_sel_changed = TRUE;

  priv->column = gtk_tree_view_column_new ();
  gtk_tree_view_append_column (GTK_TREE_VIEW (priv->tree_view), priv->column);

  priv->scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->scrolled_window),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (priv->scrolled_window),
                                       GTK_SHADOW_ETCHED_IN);

  /* a nasty hack to get the completions treeview to size nicely */
  gtk_widget_set_size_request (GTK_SCROLLED_WINDOW (priv->scrolled_window)->vscrollbar, -1, 0);

  /* actions */
  priv->actions = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_BOOLEAN);

  priv->action_view =
    gtk_tree_view_new_with_model (GTK_TREE_MODEL (priv->actions));
  g_signal_connect (priv->action_view, "button_press_event",
                    G_CALLBACK (gtk_entry_completion_action_button_press),
                    completion);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (priv->action_view), FALSE);

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->action_view));
  gtk_tree_selection_set_mode (sel, GTK_SELECTION_SINGLE);
  gtk_tree_selection_unselect_all (sel);

  cell = gtk_cell_renderer_text_new ();
  g_object_set (cell, "cell_background_gdk",
                &priv->tree_view->style->bg[GTK_STATE_NORMAL],
                NULL);
  gtk_tree_view_insert_column_with_data_func (GTK_TREE_VIEW (priv->action_view),
                                              0, "",
                                              cell,
                                              gtk_entry_completion_action_data_func,
                                              NULL,
                                              NULL);

  /* pack it all */
  priv->popup_window = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_window_set_resizable (GTK_WINDOW (priv->popup_window), FALSE);
  g_signal_connect (priv->popup_window, "key_press_event",
                    G_CALLBACK (gtk_entry_completion_popup_key_press),
                    completion);
  g_signal_connect (priv->popup_window, "button_press_event",
                    G_CALLBACK (gtk_entry_completion_popup_button_press),
                    completion);

  priv->vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (priv->popup_window), priv->vbox);

  gtk_container_add (GTK_CONTAINER (priv->scrolled_window), priv->tree_view);
  gtk_box_pack_start (GTK_BOX (priv->vbox), priv->scrolled_window,
                      TRUE, TRUE, 0);

  /* we don't want to see the action treeview when no actions have
   * been inserted, so we pack the action treeview after the first
   * action has been added
   */
}

static void
gtk_entry_completion_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (object);

  switch (prop_id)
    {
      case PROP_MODEL:
        gtk_entry_completion_set_model (completion,
                                        g_value_get_object (value));
        break;

      case PROP_MINIMUM_KEY_LENGTH:
        gtk_entry_completion_set_minimum_key_length (completion,
                                                     g_value_get_int (value));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gtk_entry_completion_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (object);

  switch (prop_id)
    {
      case PROP_MODEL:
        g_value_set_object (value,
                            gtk_entry_completion_get_model (completion));
        break;

      case PROP_MINIMUM_KEY_LENGTH:
        g_value_set_int (value, gtk_entry_completion_get_minimum_key_length (completion));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gtk_entry_completion_finalize (GObject *object)
{
  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (object);

  if (completion->priv->tree_view)
    gtk_widget_destroy (completion->priv->tree_view);

  if (completion->priv->entry)
    gtk_entry_set_completion (GTK_ENTRY (completion->priv->entry), NULL);

  if (completion->priv->actions)
    g_object_unref (completion->priv->actions);

  if (completion->priv->case_normalized_key)
    g_free (completion->priv->case_normalized_key);

  if (completion->priv->popup_window)
    gtk_widget_destroy (completion->priv->popup_window);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/* implement cell layout interface */
static void
gtk_entry_completion_pack_start (GtkCellLayout   *cell_layout,
                                 GtkCellRenderer *cell,
                                 gboolean         expand)
{
  GtkEntryCompletionPrivate *priv;

  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (cell_layout));

  priv = GTK_ENTRY_COMPLETION_GET_PRIVATE (cell_layout);

  gtk_tree_view_column_pack_start (priv->column, cell, expand);
}

static void
gtk_entry_completion_pack_end (GtkCellLayout   *cell_layout,
                               GtkCellRenderer *cell,
                               gboolean         expand)
{
  GtkEntryCompletionPrivate *priv;

  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (cell_layout));

  priv = GTK_ENTRY_COMPLETION_GET_PRIVATE (cell_layout);

  gtk_tree_view_column_pack_end (priv->column, cell, expand);
}

static void
gtk_entry_completion_clear (GtkCellLayout *cell_layout)
{
  GtkEntryCompletionPrivate *priv;

  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (cell_layout));

  priv = GTK_ENTRY_COMPLETION_GET_PRIVATE (cell_layout);

  gtk_tree_view_column_clear (priv->column);
}

static void
gtk_entry_completion_add_attribute (GtkCellLayout   *cell_layout,
                                    GtkCellRenderer *cell,
                                    const gchar     *attribute,
                                    gint             column)
{
  GtkEntryCompletionPrivate *priv;

  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (cell_layout));

  priv = GTK_ENTRY_COMPLETION_GET_PRIVATE (cell_layout);

  gtk_tree_view_column_add_attribute (priv->column, cell, attribute, column);
}

static void
gtk_entry_completion_set_cell_data_func (GtkCellLayout          *cell_layout,
                                         GtkCellRenderer        *cell,
                                         GtkCellLayoutDataFunc   func,
                                         gpointer                func_data,
                                         GDestroyNotify          destroy)
{
  GtkEntryCompletionPrivate *priv;

  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (cell_layout));

  priv = GTK_ENTRY_COMPLETION_GET_PRIVATE (cell_layout);

  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (priv->column),
                                      cell, func, func_data, destroy);
}

static void
gtk_entry_completion_clear_attributes (GtkCellLayout   *cell_layout,
                                       GtkCellRenderer *cell)
{
  GtkEntryCompletionPrivate *priv;

  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (cell_layout));

  priv = GTK_ENTRY_COMPLETION_GET_PRIVATE (cell_layout);

  gtk_tree_view_column_clear_attributes (priv->column, cell);
}

static void
gtk_entry_completion_reorder (GtkCellLayout   *cell_layout,
                              GtkCellRenderer *cell,
                              gint             position)
{
  GtkEntryCompletionPrivate *priv;

  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (cell_layout));

  priv = GTK_ENTRY_COMPLETION_GET_PRIVATE (cell_layout);

  gtk_cell_layout_reorder (GTK_CELL_LAYOUT (priv->column), cell, position);
}

/* all those callbacks */
static gboolean
gtk_entry_completion_default_completion_func (GtkEntryCompletion *completion,
                                              const gchar        *key,
                                              GtkTreeIter        *iter,
                                              gpointer            user_data)
{
  gchar *item = NULL;
  gchar *normalized_string;
  gchar *case_normalized_string;

  gboolean ret = FALSE;

  GtkTreeModel *model;

  model = gtk_tree_model_filter_get_model (completion->priv->filter_model);

  gtk_tree_model_get (model, iter,
                      completion->priv->text_column, &item,
                      -1);

  if (item != NULL)
    {
      normalized_string = g_utf8_normalize (item, -1, G_NORMALIZE_ALL);
      case_normalized_string = g_utf8_casefold (normalized_string, -1);
      
      if (!strncmp (key, case_normalized_string, strlen (key)))
	ret = TRUE;
      
      g_free (item);
      g_free (normalized_string);
      g_free (case_normalized_string);
    }

  return ret;
}

static gboolean
gtk_entry_completion_visible_func (GtkTreeModel *model,
                                   GtkTreeIter  *iter,
                                   gpointer      data)
{
  gboolean ret = FALSE;

  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (data);

  if (!completion->priv->case_normalized_key)
    return ret;

  if (completion->priv->text_column >= 0)
    ret = gtk_entry_completion_default_completion_func (completion,
                                                        completion->priv->case_normalized_key,
                                                        iter,
                                                        NULL);

  else if (completion->priv->match_func)
    ret = (* completion->priv->match_func) (completion,
                                            completion->priv->case_normalized_key,
                                            iter,
                                            completion->priv->match_data);

  return ret;
}

static gboolean
gtk_entry_completion_popup_key_press (GtkWidget   *widget,
                                      GdkEventKey *event,
                                      gpointer     user_data)
{
  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (user_data);

  if (!GTK_WIDGET_MAPPED (completion->priv->popup_window))
    return FALSE;

  /* propagate event to the entry */
  gtk_widget_event (completion->priv->entry, (GdkEvent *)event);

  return TRUE;
}

static gboolean
gtk_entry_completion_popup_button_press (GtkWidget      *widget,
                                         GdkEventButton *event,
                                         gpointer        user_data)
{
  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (user_data);

  if (!GTK_WIDGET_MAPPED (completion->priv->popup_window))
    return FALSE;

  /* if we come here, it's usually time to popdown */
  _gtk_entry_completion_popdown (completion);

  return TRUE;
}

static gboolean
gtk_entry_completion_list_button_press (GtkWidget      *widget,
                                        GdkEventButton *event,
                                        gpointer        user_data)
{
  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (user_data);
  GtkTreePath *path = NULL;

  if (!GTK_WIDGET_MAPPED (completion->priv->popup_window))
    return FALSE;

  if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (widget),
                                     event->x, event->y,
                                     &path, NULL, NULL, NULL))
    {
      gboolean entry_set;
      GtkTreeIter iter;

      gtk_tree_model_get_iter (GTK_TREE_MODEL (completion->priv->filter_model),
                               &iter, path);
      gtk_tree_path_free (path);

      g_signal_emit (completion, entry_completion_signals[MATCH_SELECTED],
                     0, GTK_TREE_MODEL (completion->priv->filter_model),
                     &iter, &entry_set);

      if (!entry_set)
        {
          gchar *str = NULL;

          gtk_tree_model_get (GTK_TREE_MODEL (completion->priv->filter_model),
                              &iter,
                              completion->priv->text_column, &str,
                              -1);

          g_signal_handler_block (completion->priv->entry,
                                  completion->priv->changed_id);
          gtk_entry_set_text (GTK_ENTRY (completion->priv->entry), str);
          g_signal_handler_unblock (completion->priv->entry,
                                    completion->priv->changed_id);

          /* move cursor to the end */
          gtk_editable_set_position (GTK_EDITABLE (completion->priv->entry),
                                     -1);

          g_free (str);
        }

      _gtk_entry_completion_popdown (completion);
      return TRUE;
    }

  return FALSE;
}

static gboolean
gtk_entry_completion_action_button_press (GtkWidget      *widget,
                                          GdkEventButton *event,
                                          gpointer        user_data)
{
  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (user_data);
  GtkTreePath *path = NULL;

  if (!GTK_WIDGET_MAPPED (completion->priv->popup_window))
    return FALSE;

  if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (widget),
                                     event->x, event->y,
                                     &path, NULL, NULL, NULL))
    {
      g_signal_emit (completion, entry_completion_signals[ACTION_ACTIVATED],
                     0, gtk_tree_path_get_indices (path)[0]);
      gtk_tree_path_free (path);

      _gtk_entry_completion_popdown (completion);
      return TRUE;
    }

  return FALSE;
}

static void
gtk_entry_completion_action_data_func (GtkTreeViewColumn *tree_column,
                                       GtkCellRenderer   *cell,
                                       GtkTreeModel      *model,
                                       GtkTreeIter       *iter,
                                       gpointer           data)
{
  gchar *string = NULL;
  gboolean markup;

  gtk_tree_model_get (model, iter,
                      0, &string,
                      1, &markup,
                      -1);

  if (!string)
    return;

  if (markup)
    g_object_set (G_OBJECT (cell),
                  "text", NULL,
                  "markup", string,
                  NULL);
  else
    g_object_set (G_OBJECT (cell),
                  "markup", NULL,
                  "text", string,
                  NULL);

  g_free (string);
}

static void
gtk_entry_completion_selection_changed (GtkTreeSelection *selection,
                                        gpointer          data)
{
  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (data);

  if (completion->priv->first_sel_changed)
    {
      completion->priv->first_sel_changed = FALSE;
      if (gtk_widget_is_focus (completion->priv->tree_view))
        gtk_tree_selection_unselect_all (selection);
    }
}

/* public API */

/**
 * gtk_entry_completion_new:
 *
 * Creates a new #GtkEntryCompletion object.
 *
 * Return value: A newly created #GtkEntryCompletion object.
 *
 * Since: 2.4
 */
GtkEntryCompletion *
gtk_entry_completion_new (void)
{
  GtkEntryCompletion *completion;

  completion = g_object_new (GTK_TYPE_ENTRY_COMPLETION, NULL);

  return completion;
}

/**
 * gtk_entry_completion_get_entry:
 * @completion: A #GtkEntryCompletion.
 *
 * Gets the entry @completion has been attached to.
 *
 * Return value: The entry @completion has been attached to.
 *
 * Since: 2.4
 */
GtkWidget *
gtk_entry_completion_get_entry (GtkEntryCompletion *completion)
{
  g_return_val_if_fail (GTK_IS_ENTRY_COMPLETION (completion), NULL);

  return completion->priv->entry;
}

/**
 * gtk_entry_completion_set_model:
 * @completion: A #GtkEntryCompletion.
 * @model: The #GtkTreeModel.
 *
 * Sets the model for a #GtkEntryCompletion. If @completion already has
 * a model set, it will remove it before setting the new model.
 *
 * Since: 2.4
 */
void
gtk_entry_completion_set_model (GtkEntryCompletion *completion,
                                GtkTreeModel       *model)
{
  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (completion));
  g_return_if_fail (GTK_IS_TREE_MODEL (model));

  /* code will unref the old filter model (if any) */
  completion->priv->filter_model =
    GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (model, NULL));
  gtk_tree_model_filter_set_visible_func (completion->priv->filter_model,
                                          gtk_entry_completion_visible_func,
                                          completion,
                                          NULL);
  gtk_tree_view_set_model (GTK_TREE_VIEW (completion->priv->tree_view),
                           GTK_TREE_MODEL (completion->priv->filter_model));
  g_object_unref (G_OBJECT (completion->priv->filter_model));
}

/**
 * gtk_entry_completion_get_model:
 * @completion: A #GtkEntryCompletion.
 *
 * Returns the model the #GtkEntryCompletion is using as data source.
 * Returns %NULL if the model is unset.
 *
 * Return value: A #GtkTreeModel, or %NULL if none is currently being used.
 *
 * Since: 2.4
 */
GtkTreeModel *
gtk_entry_completion_get_model (GtkEntryCompletion *completion)
{
  g_return_val_if_fail (GTK_IS_ENTRY_COMPLETION (completion), NULL);

  return gtk_tree_model_filter_get_model (completion->priv->filter_model);
}

/**
 * gtk_entry_completion_set_match_func:
 * @completion: A #GtkEntryCompletion.
 * @func: The #GtkEntryCompletionMatchFunc to use.
 * @func_data: The user data for @func.
 * @func_notify: Destroy notifier for @func_data.
 *
 * Sets the match function for @completion to be @func. The match function
 * is used to determine if a row should or should not be in the completion
 * list.
 *
 * Since: 2.4.
 */
void
gtk_entry_completion_set_match_func (GtkEntryCompletion          *completion,
                                     GtkEntryCompletionMatchFunc  func,
                                     gpointer                     func_data,
                                     GDestroyNotify               func_notify)
{
  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (completion));

  if (completion->priv->match_notify)
    (* completion->priv->match_notify) (completion->priv->match_data);

  completion->priv->match_func = func;
  completion->priv->match_data = func_data;
  completion->priv->match_notify = func_notify;
}

/**
 * gtk_entry_completion_set_minimum_key_length:
 * @completion: A #GtkEntryCompletion.
 * @length: The minimum length of the key in order to start completing.
 *
 * Requires the length of the search key for @completion to be at least
 * @length. This is useful for long lists, where completing using a small
 * key takes a lot of time and will come up with meaningless results anyway
 * (ie, a too large dataset).
 *
 * Since: 2.4
 */
void
gtk_entry_completion_set_minimum_key_length (GtkEntryCompletion *completion,
                                             gint                length)
{
  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (completion));
  g_return_if_fail (length >= 1);

  completion->priv->minimum_key_length = length;
}

/**
 * gtk_entry_completion_get_minimum_key_length:
 * @completion: A #GtkEntryCompletion.
 *
 * Returns the minimum key length as set for @completion.
 *
 * Return value: The currently used minimum key length.
 *
 * Since: 2.4
 */
gint
gtk_entry_completion_get_minimum_key_length (GtkEntryCompletion *completion)
{
  g_return_val_if_fail (GTK_IS_ENTRY_COMPLETION (completion), 0);

  return completion->priv->minimum_key_length;
}

/**
 * gtk_entry_completion_complete:
 * @completion: A #GtkEntryCompletion.
 *
 * Requests a completion operation, or in other words a refiltering of the
 * current list with completions, using the current key. The completion list
 * view will be updated accordingly.
 *
 * Since: 2.4
 */
void
gtk_entry_completion_complete (GtkEntryCompletion *completion)
{
  gchar *tmp;

  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (completion));
  g_return_if_fail (completion->priv->filter_model != NULL);

  if (completion->priv->case_normalized_key)
    g_free (completion->priv->case_normalized_key);

  tmp = g_utf8_normalize (gtk_entry_get_text (GTK_ENTRY (completion->priv->entry)),
                          -1, G_NORMALIZE_ALL);
  completion->priv->case_normalized_key = g_utf8_casefold (tmp, -1);
  g_free (tmp);

  gtk_tree_model_filter_refilter (completion->priv->filter_model);
}

static void
gtk_entry_completion_insert_action (GtkEntryCompletion *completion,
                                    gint                index,
                                    const gchar        *string,
                                    gboolean            markup)
{
  GtkTreeIter iter;

  gtk_list_store_insert (completion->priv->actions, &iter, index);
  gtk_list_store_set (completion->priv->actions, &iter,
                      0, string,
                      1, markup,
                      -1);

  if (!completion->priv->action_view->parent)
    {
      GtkTreePath *path = gtk_tree_path_new_from_indices (0, -1);

      gtk_tree_view_set_cursor (GTK_TREE_VIEW (completion->priv->action_view),
                                path, NULL, FALSE);
      gtk_tree_path_free (path);

      gtk_box_pack_start (GTK_BOX (completion->priv->vbox),
                          completion->priv->action_view, FALSE, FALSE, 0);
      gtk_widget_show (completion->priv->action_view);
    }
}

/**
 * gtk_entry_completion_insert_action_text:
 * @completion: A #GtkEntryCompletion.
 * @index: The index of the item to insert.
 * @text: Text of the item to insert.
 *
 * Inserts an action in @completion's action item list at position @index
 * with text @text. If you want the action item to have markup, use
 * gtk_entry_completion_insert_action_markup().
 *
 * Since: 2.4
 */
void
gtk_entry_completion_insert_action_text (GtkEntryCompletion *completion,
                                         gint                index,
                                         const gchar        *text)
{
  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (completion));
  g_return_if_fail (text != NULL);

  gtk_entry_completion_insert_action (completion, index, text, FALSE);
}

/**
 * gtk_entry_completion_insert_action_markup:
 * @completion: A #GtkEntryCompletion.
 * @index: The index of the item to insert.
 * @markup: Markup of the item to insert.
 *
 * Inserts an action in @completion's action item list at position @index
 * with markup @markup.
 *
 * Since: 2.4
 */
void
gtk_entry_completion_insert_action_markup (GtkEntryCompletion *completion,
                                           gint                index,
                                           const gchar        *markup)
{
  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (completion));
  g_return_if_fail (markup != NULL);

  gtk_entry_completion_insert_action (completion, index, markup, TRUE);
}

/**
 * gtk_entry_completion_delete_action:
 * @completion: A #GtkEntryCompletion.
 * @index: The index of the item to Delete.
 *
 * Deletes the action at @index from @completion's action list.
 *
 * Since: 2.4
 */
void
gtk_entry_completion_delete_action (GtkEntryCompletion *completion,
                                    gint                index)
{
  GtkTreeIter iter;

  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (completion));
  g_return_if_fail (index >= 0);

  gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (completion->priv->actions),
                                 &iter, NULL, index);
  gtk_list_store_remove (completion->priv->actions, &iter);
}

/**
 * gtk_entry_completion_set_text_column:
 * @completion: A #GtkEntryCompletion.
 * @column: The column in the model of @completion to get strings from.
 *
 * Convenience function for setting up the most used case of this code: a
 * completion list with just strings. This function will set up @completion
 * to have a list displaying all (and just) strings in the completion list,
 * and to get those strings from @column in the model of @completion.
 *
 * This functions creates and adds a GtkCellRendererText for the selected column.

 * Since: 2.4
 */
void
gtk_entry_completion_set_text_column (GtkEntryCompletion *completion,
                                      gint                column)
{
  GtkCellRenderer *cell;

  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (completion));
  g_return_if_fail (column >= 0);

  completion->priv->text_column = column;

  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (completion),
                              cell, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (completion),
                                 cell,
                                 "text", column);
}

/* private */

/* lame copy from gtkentry.c */
static void
get_borders (GtkEntry *entry,
             gint     *xborder,
             gint     *yborder)
{
  GtkWidget *widget = GTK_WIDGET (entry);
  gint focus_width;
  gboolean interior_focus;

  gtk_widget_style_get (widget,
                        "interior-focus", &interior_focus,
                        "focus-line-width", &focus_width,
                        NULL);

  if (entry->has_frame)
    {
      *xborder = widget->style->xthickness;
      *yborder = widget->style->ythickness;
    }
  else
    {
      *xborder = 0;
      *yborder = 0;
    }

  if (!interior_focus)
    {
      *xborder += focus_width;
      *yborder += focus_width;
    }
}

/* some nasty size requisition */
gint
_gtk_entry_completion_resize_popup (GtkEntryCompletion *completion)
{
  gint items, height, x_border, y_border;

  get_borders (GTK_ENTRY (completion->priv->entry), &x_border, &y_border);

  items = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (completion->priv->filter_model), NULL);

  items = MIN (items, 15);

  gtk_tree_view_column_cell_get_size (completion->priv->column, NULL,
                                      NULL, NULL, NULL, &height);

  if (items <= 0)
    gtk_widget_hide (completion->priv->scrolled_window);
  else
    gtk_widget_show (completion->priv->scrolled_window);

  gtk_widget_set_size_request (completion->priv->tree_view,
                               completion->priv->entry->allocation.width - 2 * x_border,
                               items * height);

  /* default on no match */
  completion->priv->current_selected = -1;

  items = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (completion->priv->actions), NULL);

  if (items)
    {
      gtk_widget_show (completion->priv->action_view);

      gtk_tree_view_column_cell_get_size (gtk_tree_view_get_column (GTK_TREE_VIEW (completion->priv->action_view), 0),
                                          NULL, NULL, NULL, NULL,
                                          &height);

      gtk_widget_set_size_request (completion->priv->action_view,
                                   completion->priv->entry->allocation.width - 2 * x_border,
                                   items * height);
    }
  else
    gtk_widget_hide (completion->priv->action_view);

  return height;
}

void
_gtk_entry_completion_popup (GtkEntryCompletion *completion)
{
  gint x, y, x_border, y_border;
  gint height;
  GdkScreen *screen;
  gint monitor_num;
  GdkRectangle monitor;
  GtkRequisition popup_req;

  if (GTK_WIDGET_MAPPED (completion->priv->popup_window))
    return;

  gtk_widget_show_all (completion->priv->vbox);

  gdk_window_get_origin (completion->priv->entry->window, &x, &y);
  get_borders (GTK_ENTRY (completion->priv->entry), &x_border, &y_border);

  x += x_border;
  y += 2 * y_border;

  height = _gtk_entry_completion_resize_popup (completion);

  gtk_widget_size_request (completion->priv->popup_window, &popup_req);

  screen = gtk_widget_get_screen (GTK_WIDGET (completion->priv->entry));
  monitor_num = gdk_screen_get_monitor_at_window (screen, 
						  GTK_WIDGET (completion->priv->entry)->window);
  gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);
  
  if (x < monitor.x)
    x = monitor.x;
  else if (x + popup_req.width > monitor.x + monitor.width)
    x = monitor.x + monitor.width - popup_req.width;
  
  if (y + height + popup_req.height <= monitor.y + monitor.height)
    y += height;
  else
    y -= popup_req.height;

  gtk_window_move (GTK_WINDOW (completion->priv->popup_window), x, y);

  gtk_widget_show (completion->priv->popup_window);

  gtk_grab_add (completion->priv->popup_window);
  gdk_pointer_grab (completion->priv->popup_window->window, TRUE,
                    GDK_BUTTON_PRESS_MASK |
                    GDK_BUTTON_RELEASE_MASK |
                    GDK_POINTER_MOTION_MASK,
                    NULL, NULL, GDK_CURRENT_TIME);
}

void
_gtk_entry_completion_popdown (GtkEntryCompletion *completion)
{
  gdk_pointer_ungrab (GDK_CURRENT_TIME);
  gtk_grab_remove (completion->priv->popup_window);

  gtk_widget_hide (completion->priv->popup_window);
}
