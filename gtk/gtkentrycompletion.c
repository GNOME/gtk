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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:gtkentrycompletion
 * @Short_description: Completion functionality for GtkEntry
 * @Title: GtkEntryCompletion
 *
 * #GtkEntryCompletion is an auxiliary object to be used in conjunction with
 * #GtkEntry to provide the completion functionality. It implements the
 * #GtkCellLayout interface, to allow the user to add extra cells to the
 * #GtkTreeView with completion matches.
 *
 * “Completion functionality” means that when the user modifies the text
 * in the entry, #GtkEntryCompletion checks which rows in the model match
 * the current content of the entry, and displays a list of matches.
 * By default, the matching is done by comparing the entry text
 * case-insensitively against the text column of the model (see
 * gtk_entry_completion_set_text_column()), but this can be overridden
 * with a custom match function (see gtk_entry_completion_set_match_func()).
 *
 * When the user selects a completion, the content of the entry is
 * updated. By default, the content of the entry is replaced by the
 * text column of the model, but this can be overridden by connecting
 * to the #GtkEntryCompletion::match-selected signal and updating the
 * entry in the signal handler. Note that you should return %TRUE from
 * the signal handler to suppress the default behaviour.
 *
 * To add completion functionality to an entry, use gtk_entry_set_completion().
 *
 * In addition to regular completion matches, which will be inserted into the
 * entry when they are selected, #GtkEntryCompletion also allows to display
 * “actions” in the popup window. Their appearance is similar to menuitems,
 * to differentiate them clearly from completion strings. When an action is
 * selected, the #GtkEntryCompletion::action-activated signal is emitted.
 *
 * GtkEntryCompletion uses a #GtkTreeModelFilter model to represent the
 * subset of the entire model that is currently matching. While the
 * GtkEntryCompletion signals #GtkEntryCompletion::match-selected and
 * #GtkEntryCompletion::cursor-on-match take the original model and an
 * iter pointing to that model as arguments, other callbacks and signals
 * (such as #GtkCellLayoutDataFuncs or #GtkCellArea::apply-attributes)
 * will generally take the filter model as argument. As long as you are
 * only calling gtk_tree_model_get(), this will make no difference to
 * you. If for some reason, you need the original model, use
 * gtk_tree_model_filter_get_model(). Don’t forget to use
 * gtk_tree_model_filter_convert_iter_to_child_iter() to obtain a
 * matching iter.
 */

#include "config.h"

#include "gtkentrycompletion.h"

#include "gtkentryprivate.h"
#include "gtktextprivate.h"
#include "gtkcelllayout.h"
#include "gtkcellareabox.h"

#include "gtkintl.h"
#include "gtkcellrenderertext.h"
#include "gtkframe.h"
#include "gtktreeselection.h"
#include "gtktreeview.h"
#include "gtkscrolledwindow.h"
#include "gtksizerequest.h"
#include "gtkbox.h"
#include "gtkpopover.h"
#include "gtkentry.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkeventcontrollerkey.h"
#include "gtkgestureclick.h"

#include "gtkprivate.h"
#include "gtkwindowprivate.h"
#include "gtkwidgetprivate.h"
#include "gtknative.h"
#include "gtkstylecontext.h"

#include <string.h>

#define PAGE_STEP 14
#define COMPLETION_TIMEOUT 100

/* signals */
enum
{
  INSERT_PREFIX,
  MATCH_SELECTED,
  ACTION_ACTIVATED,
  CURSOR_ON_MATCH,
  NO_MATCHES,
  LAST_SIGNAL
};

/* properties */
enum
{
  PROP_0,
  PROP_MODEL,
  PROP_MINIMUM_KEY_LENGTH,
  PROP_TEXT_COLUMN,
  PROP_INLINE_COMPLETION,
  PROP_POPUP_COMPLETION,
  PROP_POPUP_SET_WIDTH,
  PROP_POPUP_SINGLE_MATCH,
  PROP_INLINE_SELECTION,
  PROP_CELL_AREA,
  NUM_PROPERTIES
};


static void     gtk_entry_completion_cell_layout_init    (GtkCellLayoutIface      *iface);
static GtkCellArea* gtk_entry_completion_get_area        (GtkCellLayout           *cell_layout);

static void     gtk_entry_completion_constructed         (GObject      *object);
static void     gtk_entry_completion_set_property        (GObject      *object,
                                                          guint         prop_id,
                                                          const GValue *value,
                                                          GParamSpec   *pspec);
static void     gtk_entry_completion_get_property        (GObject      *object,
                                                          guint         prop_id,
                                                          GValue       *value,
                                                          GParamSpec   *pspec);
static void     gtk_entry_completion_finalize            (GObject      *object);
static void     gtk_entry_completion_dispose             (GObject      *object);

static gboolean gtk_entry_completion_visible_func        (GtkTreeModel       *model,
                                                          GtkTreeIter        *iter,
                                                          gpointer            data);
static void     gtk_entry_completion_list_activated      (GtkTreeView        *treeview,
                                                          GtkTreePath        *path,
                                                          GtkTreeViewColumn  *column,
                                                          gpointer            user_data);
static void     gtk_entry_completion_action_activated    (GtkTreeView        *treeview,
                                                          GtkTreePath        *path,
                                                          GtkTreeViewColumn  *column,
                                                          gpointer            user_data);
static void     gtk_entry_completion_selection_changed   (GtkTreeSelection   *selection,
                                                          gpointer            data);
static void     gtk_entry_completion_insert_action       (GtkEntryCompletion *completion,
                                                          gint                index,
                                                          const gchar        *string,
                                                          gboolean            markup);
static void     gtk_entry_completion_action_data_func    (GtkTreeViewColumn  *tree_column,
                                                          GtkCellRenderer    *cell,
                                                          GtkTreeModel       *model,
                                                          GtkTreeIter        *iter,
                                                          gpointer            data);

static gboolean gtk_entry_completion_match_selected      (GtkEntryCompletion *completion,
                                                          GtkTreeModel       *model,
                                                          GtkTreeIter        *iter);
static gboolean gtk_entry_completion_real_insert_prefix  (GtkEntryCompletion *completion,
                                                          const gchar        *prefix);
static gboolean gtk_entry_completion_cursor_on_match     (GtkEntryCompletion *completion,
                                                          GtkTreeModel       *model,
                                                          GtkTreeIter        *iter);
static gboolean gtk_entry_completion_insert_completion   (GtkEntryCompletion *completion,
                                                          GtkTreeModel       *model,
                                                          GtkTreeIter        *iter);
static void     gtk_entry_completion_insert_completion_text (GtkEntryCompletion *completion,
                                                             const gchar *text);
static void     connect_completion_signals                  (GtkEntryCompletion *completion);
static void     disconnect_completion_signals               (GtkEntryCompletion *completion);


static GParamSpec *entry_completion_props[NUM_PROPERTIES] = { NULL, };

static guint entry_completion_signals[LAST_SIGNAL] = { 0 };

/* GtkBuildable */
static void     gtk_entry_completion_buildable_init      (GtkBuildableIface  *iface);

G_DEFINE_TYPE_WITH_CODE (GtkEntryCompletion, gtk_entry_completion, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GtkEntryCompletion)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_LAYOUT,
                                                gtk_entry_completion_cell_layout_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_entry_completion_buildable_init))


static void
gtk_entry_completion_class_init (GtkEntryCompletionClass *klass)
{
  GObjectClass *object_class;

  object_class = (GObjectClass *)klass;

  object_class->constructed = gtk_entry_completion_constructed;
  object_class->set_property = gtk_entry_completion_set_property;
  object_class->get_property = gtk_entry_completion_get_property;
  object_class->dispose = gtk_entry_completion_dispose;
  object_class->finalize = gtk_entry_completion_finalize;

  klass->match_selected = gtk_entry_completion_match_selected;
  klass->insert_prefix = gtk_entry_completion_real_insert_prefix;
  klass->cursor_on_match = gtk_entry_completion_cursor_on_match;
  klass->no_matches = NULL;

  /**
   * GtkEntryCompletion::insert-prefix:
   * @widget: the object which received the signal
   * @prefix: the common prefix of all possible completions
   *
   * Gets emitted when the inline autocompletion is triggered.
   * The default behaviour is to make the entry display the
   * whole prefix and select the newly inserted part.
   *
   * Applications may connect to this signal in order to insert only a
   * smaller part of the @prefix into the entry - e.g. the entry used in
   * the #GtkFileChooser inserts only the part of the prefix up to the
   * next '/'.
   *
   * Returns: %TRUE if the signal has been handled
   */
  entry_completion_signals[INSERT_PREFIX] =
    g_signal_new (I_("insert-prefix"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkEntryCompletionClass, insert_prefix),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__STRING,
                  G_TYPE_BOOLEAN, 1,
                  G_TYPE_STRING);

  /**
   * GtkEntryCompletion::match-selected:
   * @widget: the object which received the signal
   * @model: the #GtkTreeModel containing the matches
   * @iter: a #GtkTreeIter positioned at the selected match
   *
   * Gets emitted when a match from the list is selected.
   * The default behaviour is to replace the contents of the
   * entry with the contents of the text column in the row
   * pointed to by @iter.
   *
   * Note that @model is the model that was passed to
   * gtk_entry_completion_set_model().
   *
   * Returns: %TRUE if the signal has been handled
   */
  entry_completion_signals[MATCH_SELECTED] =
    g_signal_new (I_("match-selected"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkEntryCompletionClass, match_selected),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__OBJECT_BOXED,
                  G_TYPE_BOOLEAN, 2,
                  GTK_TYPE_TREE_MODEL,
                  GTK_TYPE_TREE_ITER);

  /**
   * GtkEntryCompletion::cursor-on-match:
   * @widget: the object which received the signal
   * @model: the #GtkTreeModel containing the matches
   * @iter: a #GtkTreeIter positioned at the selected match
   *
   * Gets emitted when a match from the cursor is on a match
   * of the list. The default behaviour is to replace the contents
   * of the entry with the contents of the text column in the row
   * pointed to by @iter.
   *
   * Note that @model is the model that was passed to
   * gtk_entry_completion_set_model().
   *
   * Returns: %TRUE if the signal has been handled
   */
  entry_completion_signals[CURSOR_ON_MATCH] =
    g_signal_new (I_("cursor-on-match"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkEntryCompletionClass, cursor_on_match),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__OBJECT_BOXED,
                  G_TYPE_BOOLEAN, 2,
                  GTK_TYPE_TREE_MODEL,
                  GTK_TYPE_TREE_ITER);

  /**
   * GtkEntryCompletion::no-matches:
   * @widget: the object which received the signal
   *
   * Gets emitted when the filter model has zero
   * number of rows in completion_complete method.
   * (In other words when GtkEntryCompletion is out of
   *  suggestions)
   */
  entry_completion_signals[NO_MATCHES] =
    g_signal_new (I_("no-matches"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkEntryCompletionClass, no_matches),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkEntryCompletion::action-activated:
   * @widget: the object which received the signal
   * @index: the index of the activated action
   *
   * Gets emitted when an action is activated.
   */
  entry_completion_signals[ACTION_ACTIVATED] =
    g_signal_new (I_("action-activated"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkEntryCompletionClass, action_activated),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_INT);

  entry_completion_props[PROP_MODEL] =
      g_param_spec_object ("model",
                           P_("Completion Model"),
                           P_("The model to find matches in"),
                           GTK_TYPE_TREE_MODEL,
                           GTK_PARAM_READWRITE);

  entry_completion_props[PROP_MINIMUM_KEY_LENGTH] =
      g_param_spec_int ("minimum-key-length",
                        P_("Minimum Key Length"),
                        P_("Minimum length of the search key in order to look up matches"),
                        0, G_MAXINT, 1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntryCompletion:text-column:
   *
   * The column of the model containing the strings.
   * Note that the strings must be UTF-8.
   */
  entry_completion_props[PROP_TEXT_COLUMN] =
    g_param_spec_int ("text-column",
                      P_("Text column"),
                      P_("The column of the model containing the strings."),
                      -1, G_MAXINT, -1,
                      GTK_PARAM_READWRITE);

  /**
   * GtkEntryCompletion:inline-completion:
   *
   * Determines whether the common prefix of the possible completions
   * should be inserted automatically in the entry. Note that this
   * requires text-column to be set, even if you are using a custom
   * match function.
   **/
  entry_completion_props[PROP_INLINE_COMPLETION] =
      g_param_spec_boolean ("inline-completion",
                            P_("Inline completion"),
                            P_("Whether the common prefix should be inserted automatically"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntryCompletion:popup-completion:
   *
   * Determines whether the possible completions should be
   * shown in a popup window.
   **/
  entry_completion_props[PROP_POPUP_COMPLETION] =
      g_param_spec_boolean ("popup-completion",
                            P_("Popup completion"),
                            P_("Whether the completions should be shown in a popup window"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntryCompletion:popup-set-width:
   *
   * Determines whether the completions popup window will be
   * resized to the width of the entry.
   */
  entry_completion_props[PROP_POPUP_SET_WIDTH] =
      g_param_spec_boolean ("popup-set-width",
                            P_("Popup set width"),
                            P_("If TRUE, the popup window will have the same size as the entry"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntryCompletion:popup-single-match:
   *
   * Determines whether the completions popup window will shown
   * for a single possible completion. You probably want to set
   * this to %FALSE if you are using
   * [inline completion][GtkEntryCompletion--inline-completion].
   */
  entry_completion_props[PROP_POPUP_SINGLE_MATCH] =
      g_param_spec_boolean ("popup-single-match",
                            P_("Popup single match"),
                            P_("If TRUE, the popup window will appear for a single match."),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntryCompletion:inline-selection:
   *
   * Determines whether the possible completions on the popup
   * will appear in the entry as you navigate through them.
   */
  entry_completion_props[PROP_INLINE_SELECTION] =
      g_param_spec_boolean ("inline-selection",
                            P_("Inline selection"),
                            P_("Your description here"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntryCompletion:cell-area:
   *
   * The #GtkCellArea used to layout cell renderers in the treeview column.
   *
   * If no area is specified when creating the entry completion with
   * gtk_entry_completion_new_with_area() a horizontally oriented
   * #GtkCellAreaBox will be used.
   */
  entry_completion_props[PROP_CELL_AREA] =
      g_param_spec_object ("cell-area",
                           P_("Cell Area"),
                           P_("The GtkCellArea used to layout cells"),
                           GTK_TYPE_CELL_AREA,
                           GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, entry_completion_props);
}


static void
gtk_entry_completion_buildable_custom_tag_end (GtkBuildable *buildable,
                                                GtkBuilder   *builder,
                                                GObject      *child,
                                                const gchar  *tagname,
                                                gpointer      data)
{
  /* Just ignore the boolean return from here */
  _gtk_cell_layout_buildable_custom_tag_end (buildable, builder, child, tagname, data);
}

static void
gtk_entry_completion_buildable_init (GtkBuildableIface *iface)
{
  iface->add_child = _gtk_cell_layout_buildable_add_child;
  iface->custom_tag_start = _gtk_cell_layout_buildable_custom_tag_start;
  iface->custom_tag_end = gtk_entry_completion_buildable_custom_tag_end;
}

static void
gtk_entry_completion_cell_layout_init (GtkCellLayoutIface *iface)
{
  iface->get_area = gtk_entry_completion_get_area;
}

static void
gtk_entry_completion_init (GtkEntryCompletion *completion)
{
  GtkEntryCompletionPrivate *priv;

  /* yes, also priv, need to keep the code readable */
  completion->priv = gtk_entry_completion_get_instance_private (completion);
  priv = completion->priv;

  priv->minimum_key_length = 1;
  priv->text_column = -1;
  priv->has_completion = FALSE;
  priv->inline_completion = FALSE;
  priv->popup_completion = TRUE;
  priv->popup_set_width = TRUE;
  priv->popup_single_match = TRUE;
  priv->inline_selection = FALSE;

  priv->filter_model = NULL;
}

static gboolean
propagate_to_entry (GtkEventControllerKey *key,
                    guint                  keyval,
                    guint                  keycode,
                    GdkModifierType        modifiers,
                    GtkEntryCompletion    *completion)
{
  GtkEntryCompletionPrivate *priv = completion->priv;
  GtkText *text = gtk_entry_get_text_widget (GTK_ENTRY (priv->entry));

  return gtk_event_controller_key_forward (key, GTK_WIDGET (text));
}

static void
gtk_entry_completion_constructed (GObject *object)
{
  GtkEntryCompletion        *completion = GTK_ENTRY_COMPLETION (object);
  GtkEntryCompletionPrivate *priv = completion->priv;
  GtkCellRenderer           *cell;
  GtkTreeSelection          *sel;
  GtkWidget                 *popup_frame;
  GtkEventController        *controller;

  G_OBJECT_CLASS (gtk_entry_completion_parent_class)->constructed (object);

  if (!priv->cell_area)
    {
      priv->cell_area = gtk_cell_area_box_new ();
      g_object_ref_sink (priv->cell_area);
    }

  /* completions */
  priv->tree_view = gtk_tree_view_new ();
  g_signal_connect (priv->tree_view, "row-activated",
                    G_CALLBACK (gtk_entry_completion_list_activated),
                    completion);

  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (priv->tree_view), FALSE);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (priv->tree_view), FALSE);
  gtk_tree_view_set_hover_selection (GTK_TREE_VIEW (priv->tree_view), TRUE);
  gtk_tree_view_set_activate_on_single_click (GTK_TREE_VIEW (priv->tree_view), TRUE);

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view));
  gtk_tree_selection_set_mode (sel, GTK_SELECTION_SINGLE);
  gtk_tree_selection_unselect_all (sel);
  g_signal_connect (sel, "changed",
                    G_CALLBACK (gtk_entry_completion_selection_changed),
                    completion);
  priv->first_sel_changed = TRUE;

  priv->column = gtk_tree_view_column_new_with_area (priv->cell_area);
  gtk_tree_view_append_column (GTK_TREE_VIEW (priv->tree_view), priv->column);

  priv->scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->scrolled_window),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (priv->scrolled_window),
                                       GTK_SHADOW_NONE);

  /* a nasty hack to get the completions treeview to size nicely */
  gtk_widget_set_size_request (gtk_scrolled_window_get_vscrollbar (GTK_SCROLLED_WINDOW (priv->scrolled_window)),
                               -1, 0);

  /* actions */
  priv->actions = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_BOOLEAN);

  priv->action_view =
    gtk_tree_view_new_with_model (GTK_TREE_MODEL (priv->actions));
  g_object_ref_sink (priv->action_view);
  g_signal_connect (priv->action_view, "row-activated",
                    G_CALLBACK (gtk_entry_completion_action_activated),
                    completion);
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (priv->action_view), FALSE);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (priv->action_view), FALSE);
  gtk_tree_view_set_hover_selection (GTK_TREE_VIEW (priv->action_view), TRUE);
  gtk_tree_view_set_activate_on_single_click (GTK_TREE_VIEW (priv->action_view), TRUE);

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->action_view));
  gtk_tree_selection_set_mode (sel, GTK_SELECTION_SINGLE);
  gtk_tree_selection_unselect_all (sel);

  cell = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_data_func (GTK_TREE_VIEW (priv->action_view),
                                              0, "",
                                              cell,
                                              gtk_entry_completion_action_data_func,
                                              NULL,
                                              NULL);

  /* pack it all */
  priv->popup_window = gtk_popover_new (NULL);
  gtk_popover_set_position (GTK_POPOVER (priv->popup_window), GTK_POS_BOTTOM);
  gtk_popover_set_autohide (GTK_POPOVER (priv->popup_window), FALSE);
  gtk_popover_set_has_arrow (GTK_POPOVER (priv->popup_window), FALSE);
  gtk_style_context_add_class (gtk_widget_get_style_context (priv->popup_window), "entry-completion");

  controller = gtk_event_controller_key_new ();
  g_signal_connect (controller, "key-pressed",
                    G_CALLBACK (propagate_to_entry), completion);
  g_signal_connect (controller, "key-released",
                    G_CALLBACK (propagate_to_entry), completion);
  gtk_widget_add_controller (priv->popup_window, controller);

  controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
  g_signal_connect_swapped (controller, "released",
                            G_CALLBACK (_gtk_entry_completion_popdown),
                            completion);
  gtk_widget_add_controller (priv->popup_window, controller);

  popup_frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (popup_frame),
                             GTK_SHADOW_ETCHED_IN);
  gtk_container_add (GTK_CONTAINER (priv->popup_window), popup_frame);

  priv->vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (popup_frame), priv->vbox);

  gtk_container_add (GTK_CONTAINER (priv->scrolled_window), priv->tree_view);
  gtk_widget_set_hexpand (priv->scrolled_window, TRUE);
  gtk_widget_set_vexpand (priv->scrolled_window, TRUE);
  gtk_container_add (GTK_CONTAINER (priv->vbox), priv->scrolled_window);

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
  GtkEntryCompletionPrivate *priv = completion->priv;
  GtkCellArea *area;

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

      case PROP_TEXT_COLUMN:
        priv->text_column = g_value_get_int (value);
        break;

      case PROP_INLINE_COMPLETION:
	gtk_entry_completion_set_inline_completion (completion,
						    g_value_get_boolean (value));
        break;

      case PROP_POPUP_COMPLETION:
	gtk_entry_completion_set_popup_completion (completion,
						   g_value_get_boolean (value));
        break;

      case PROP_POPUP_SET_WIDTH:
	gtk_entry_completion_set_popup_set_width (completion,
						  g_value_get_boolean (value));
        break;

      case PROP_POPUP_SINGLE_MATCH:
	gtk_entry_completion_set_popup_single_match (completion,
						     g_value_get_boolean (value));
        break;

      case PROP_INLINE_SELECTION:
	gtk_entry_completion_set_inline_selection (completion,
						   g_value_get_boolean (value));
        break;

      case PROP_CELL_AREA:
        /* Construct-only, can only be assigned once */
        area = g_value_get_object (value);
        if (area)
          {
            if (priv->cell_area != NULL)
              {
                g_warning ("cell-area has already been set, ignoring construct property");
                g_object_ref_sink (area);
                g_object_unref (area);
              }
            else
              priv->cell_area = g_object_ref_sink (area);
          }
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

      case PROP_TEXT_COLUMN:
        g_value_set_int (value, gtk_entry_completion_get_text_column (completion));
        break;

      case PROP_INLINE_COMPLETION:
        g_value_set_boolean (value, gtk_entry_completion_get_inline_completion (completion));
        break;

      case PROP_POPUP_COMPLETION:
        g_value_set_boolean (value, gtk_entry_completion_get_popup_completion (completion));
        break;

      case PROP_POPUP_SET_WIDTH:
        g_value_set_boolean (value, gtk_entry_completion_get_popup_set_width (completion));
        break;

      case PROP_POPUP_SINGLE_MATCH:
        g_value_set_boolean (value, gtk_entry_completion_get_popup_single_match (completion));
        break;

      case PROP_INLINE_SELECTION:
        g_value_set_boolean (value, gtk_entry_completion_get_inline_selection (completion));
        break;

      case PROP_CELL_AREA:
        g_value_set_object (value, completion->priv->cell_area);
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
  GtkEntryCompletionPrivate *priv = completion->priv;

  g_free (priv->case_normalized_key);
  g_free (priv->completion_prefix);

  if (priv->match_notify)
    (* priv->match_notify) (priv->match_data);

  G_OBJECT_CLASS (gtk_entry_completion_parent_class)->finalize (object);
}

static void
gtk_entry_completion_dispose (GObject *object)
{
  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (object);
  GtkEntryCompletionPrivate *priv = completion->priv;

  if (priv->entry)
    gtk_entry_set_completion (GTK_ENTRY (priv->entry), NULL);

  g_clear_object (&priv->actions);
  g_clear_object (&priv->action_view);
  g_clear_object (&priv->cell_area);

  G_OBJECT_CLASS (gtk_entry_completion_parent_class)->dispose (object);
}

/* implement cell layout interface (only need to return the underlying cell area) */
static GtkCellArea*
gtk_entry_completion_get_area (GtkCellLayout *cell_layout)
{
  GtkEntryCompletionPrivate *priv;

  priv = GTK_ENTRY_COMPLETION (cell_layout)->priv;

  if (G_UNLIKELY (!priv->cell_area))
    {
      priv->cell_area = gtk_cell_area_box_new ();
      g_object_ref_sink (priv->cell_area);
    }

  return priv->cell_area;
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

  g_return_val_if_fail (gtk_tree_model_get_column_type (model, completion->priv->text_column) == G_TYPE_STRING,
                        FALSE);

  gtk_tree_model_get (model, iter,
                      completion->priv->text_column, &item,
                      -1);

  if (item != NULL)
    {
      normalized_string = g_utf8_normalize (item, -1, G_NORMALIZE_ALL);

      if (normalized_string != NULL)
        {
          case_normalized_string = g_utf8_casefold (normalized_string, -1);

          if (!strncmp (key, case_normalized_string, strlen (key)))
            ret = TRUE;

          g_free (case_normalized_string);
        }
      g_free (normalized_string);
    }
  g_free (item);

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

  if (completion->priv->match_func)
    ret = (* completion->priv->match_func) (completion,
                                            completion->priv->case_normalized_key,
                                            iter,
                                            completion->priv->match_data);
  else if (completion->priv->text_column >= 0)
    ret = gtk_entry_completion_default_completion_func (completion,
                                                        completion->priv->case_normalized_key,
                                                        iter,
                                                        NULL);

  return ret;
}

static void
gtk_entry_completion_list_activated (GtkTreeView       *treeview,
                                     GtkTreePath       *path,
                                     GtkTreeViewColumn *column,
                                     gpointer           user_data)
{
  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (user_data);
  GtkTreeIter iter;
  gboolean entry_set;
  GtkTreeModel *model;
  GtkTreeIter child_iter;
  GtkText *text = gtk_entry_get_text_widget (GTK_ENTRY (completion->priv->entry));

  gtk_tree_model_get_iter (GTK_TREE_MODEL (completion->priv->filter_model), &iter, path);
  gtk_tree_model_filter_convert_iter_to_child_iter (completion->priv->filter_model,
                                                    &child_iter,
                                                    &iter);
  model = gtk_tree_model_filter_get_model (completion->priv->filter_model);

  g_signal_handler_block (text, completion->priv->changed_id);
  g_signal_emit (completion, entry_completion_signals[MATCH_SELECTED],
                 0, model, &child_iter, &entry_set);
  g_signal_handler_unblock (text, completion->priv->changed_id);

  _gtk_entry_completion_popdown (completion);
}

static void
gtk_entry_completion_action_activated (GtkTreeView       *treeview,
                                       GtkTreePath       *path,
                                       GtkTreeViewColumn *column,
                                       gpointer           user_data)
{
  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (user_data);

  gtk_entry_reset_im_context (GTK_ENTRY (completion->priv->entry));

  g_signal_emit (completion, entry_completion_signals[ACTION_ACTIVATED],
                 0, gtk_tree_path_get_indices (path)[0]);

  _gtk_entry_completion_popdown (completion);
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
    g_object_set (cell,
                  "text", NULL,
                  "markup", string,
                  NULL);
  else
    g_object_set (cell,
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
 * Returns: A newly created #GtkEntryCompletion object
 */
GtkEntryCompletion *
gtk_entry_completion_new (void)
{
  GtkEntryCompletion *completion;

  completion = g_object_new (GTK_TYPE_ENTRY_COMPLETION, NULL);

  return completion;
}

/**
 * gtk_entry_completion_new_with_area:
 * @area: the #GtkCellArea used to layout cells
 *
 * Creates a new #GtkEntryCompletion object using the
 * specified @area to layout cells in the underlying
 * #GtkTreeViewColumn for the drop-down menu.
 *
 * Returns: A newly created #GtkEntryCompletion object
 */
GtkEntryCompletion *
gtk_entry_completion_new_with_area (GtkCellArea *area)
{
  GtkEntryCompletion *completion;

  completion = g_object_new (GTK_TYPE_ENTRY_COMPLETION, "cell-area", area, NULL);

  return completion;
}

/**
 * gtk_entry_completion_get_entry:
 * @completion: a #GtkEntryCompletion
 *
 * Gets the entry @completion has been attached to.
 *
 * Returns: (transfer none): The entry @completion has been attached to
 */
GtkWidget *
gtk_entry_completion_get_entry (GtkEntryCompletion *completion)
{
  g_return_val_if_fail (GTK_IS_ENTRY_COMPLETION (completion), NULL);

  return completion->priv->entry;
}

/**
 * gtk_entry_completion_set_model:
 * @completion: a #GtkEntryCompletion
 * @model: (allow-none): the #GtkTreeModel
 *
 * Sets the model for a #GtkEntryCompletion. If @completion already has
 * a model set, it will remove it before setting the new model.
 * If model is %NULL, then it will unset the model.
 */
void
gtk_entry_completion_set_model (GtkEntryCompletion *completion,
                                GtkTreeModel       *model)
{
  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (completion));
  g_return_if_fail (model == NULL || GTK_IS_TREE_MODEL (model));

  if (!model)
    {
      gtk_tree_view_set_model (GTK_TREE_VIEW (completion->priv->tree_view),
                               NULL);
      _gtk_entry_completion_popdown (completion);
      completion->priv->filter_model = NULL;
      return;
    }

  /* code will unref the old filter model (if any) */
  completion->priv->filter_model =
    GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (model, NULL));
  gtk_tree_model_filter_set_visible_func (completion->priv->filter_model,
                                          gtk_entry_completion_visible_func,
                                          completion,
                                          NULL);

  gtk_tree_view_set_model (GTK_TREE_VIEW (completion->priv->tree_view),
                           GTK_TREE_MODEL (completion->priv->filter_model));
  g_object_unref (completion->priv->filter_model);

  g_object_notify_by_pspec (G_OBJECT (completion), entry_completion_props[PROP_MODEL]);

  if (gtk_widget_get_visible (completion->priv->popup_window))
    _gtk_entry_completion_resize_popup (completion);
}

/**
 * gtk_entry_completion_get_model:
 * @completion: a #GtkEntryCompletion
 *
 * Returns the model the #GtkEntryCompletion is using as data source.
 * Returns %NULL if the model is unset.
 *
 * Returns: (nullable) (transfer none): A #GtkTreeModel, or %NULL if none
 *     is currently being used
 */
GtkTreeModel *
gtk_entry_completion_get_model (GtkEntryCompletion *completion)
{
  g_return_val_if_fail (GTK_IS_ENTRY_COMPLETION (completion), NULL);

  if (!completion->priv->filter_model)
    return NULL;

  return gtk_tree_model_filter_get_model (completion->priv->filter_model);
}

/**
 * gtk_entry_completion_set_match_func:
 * @completion: a #GtkEntryCompletion
 * @func: the #GtkEntryCompletionMatchFunc to use
 * @func_data: user data for @func
 * @func_notify: destroy notify for @func_data.
 *
 * Sets the match function for @completion to be @func. The match function
 * is used to determine if a row should or should not be in the completion
 * list.
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
 * @completion: a #GtkEntryCompletion
 * @length: the minimum length of the key in order to start completing
 *
 * Requires the length of the search key for @completion to be at least
 * @length. This is useful for long lists, where completing using a small
 * key takes a lot of time and will come up with meaningless results anyway
 * (ie, a too large dataset).
 */
void
gtk_entry_completion_set_minimum_key_length (GtkEntryCompletion *completion,
                                             gint                length)
{
  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (completion));
  g_return_if_fail (length >= 0);

  if (completion->priv->minimum_key_length != length)
    {
      completion->priv->minimum_key_length = length;

      g_object_notify_by_pspec (G_OBJECT (completion),
                                entry_completion_props[PROP_MINIMUM_KEY_LENGTH]);
    }
}

/**
 * gtk_entry_completion_get_minimum_key_length:
 * @completion: a #GtkEntryCompletion
 *
 * Returns the minimum key length as set for @completion.
 *
 * Returns: The currently used minimum key length
 */
gint
gtk_entry_completion_get_minimum_key_length (GtkEntryCompletion *completion)
{
  g_return_val_if_fail (GTK_IS_ENTRY_COMPLETION (completion), 0);

  return completion->priv->minimum_key_length;
}

/**
 * gtk_entry_completion_complete:
 * @completion: a #GtkEntryCompletion
 *
 * Requests a completion operation, or in other words a refiltering of the
 * current list with completions, using the current key. The completion list
 * view will be updated accordingly.
 */
void
gtk_entry_completion_complete (GtkEntryCompletion *completion)
{
  gchar *tmp;
  GtkTreeIter iter;

  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (completion));
  g_return_if_fail (GTK_IS_ENTRY (completion->priv->entry));

  if (!completion->priv->filter_model)
    return;

  g_free (completion->priv->case_normalized_key);

  tmp = g_utf8_normalize (gtk_editable_get_text (GTK_EDITABLE (completion->priv->entry)),
                          -1, G_NORMALIZE_ALL);
  completion->priv->case_normalized_key = g_utf8_casefold (tmp, -1);
  g_free (tmp);

  gtk_tree_model_filter_refilter (completion->priv->filter_model);

  if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (completion->priv->filter_model), &iter))
    g_signal_emit (completion, entry_completion_signals[NO_MATCHES], 0);

  if (gtk_widget_get_visible (completion->priv->popup_window))
    _gtk_entry_completion_resize_popup (completion);
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

  if (!gtk_widget_get_parent (completion->priv->action_view))
    {
      GtkTreePath *path = gtk_tree_path_new_from_indices (0, -1);

      gtk_tree_view_set_cursor (GTK_TREE_VIEW (completion->priv->action_view),
                                path, NULL, FALSE);
      gtk_tree_path_free (path);

      gtk_container_add (GTK_CONTAINER (completion->priv->vbox),
                          completion->priv->action_view);
      gtk_widget_show (completion->priv->action_view);
    }
}

/**
 * gtk_entry_completion_insert_action_text:
 * @completion: a #GtkEntryCompletion
 * @index_: the index of the item to insert
 * @text: text of the item to insert
 *
 * Inserts an action in @completion’s action item list at position @index_
 * with text @text. If you want the action item to have markup, use
 * gtk_entry_completion_insert_action_markup().
 *
 * Note that @index_ is a relative position in the list of actions and
 * the position of an action can change when deleting a different action.
 */
void
gtk_entry_completion_insert_action_text (GtkEntryCompletion *completion,
                                         gint                index_,
                                         const gchar        *text)
{
  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (completion));
  g_return_if_fail (text != NULL);

  gtk_entry_completion_insert_action (completion, index_, text, FALSE);
}

/**
 * gtk_entry_completion_insert_action_markup:
 * @completion: a #GtkEntryCompletion
 * @index_: the index of the item to insert
 * @markup: markup of the item to insert
 *
 * Inserts an action in @completion’s action item list at position @index_
 * with markup @markup.
 */
void
gtk_entry_completion_insert_action_markup (GtkEntryCompletion *completion,
                                           gint                index_,
                                           const gchar        *markup)
{
  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (completion));
  g_return_if_fail (markup != NULL);

  gtk_entry_completion_insert_action (completion, index_, markup, TRUE);
}

/**
 * gtk_entry_completion_delete_action:
 * @completion: a #GtkEntryCompletion
 * @index_: the index of the item to delete
 *
 * Deletes the action at @index_ from @completion’s action list.
 *
 * Note that @index_ is a relative position and the position of an
 * action may have changed since it was inserted.
 */
void
gtk_entry_completion_delete_action (GtkEntryCompletion *completion,
                                    gint                index_)
{
  GtkTreeIter iter;

  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (completion));
  g_return_if_fail (index_ >= 0);

  gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (completion->priv->actions),
                                 &iter, NULL, index_);
  gtk_list_store_remove (completion->priv->actions, &iter);
}

/**
 * gtk_entry_completion_set_text_column:
 * @completion: a #GtkEntryCompletion
 * @column: the column in the model of @completion to get strings from
 *
 * Convenience function for setting up the most used case of this code: a
 * completion list with just strings. This function will set up @completion
 * to have a list displaying all (and just) strings in the completion list,
 * and to get those strings from @column in the model of @completion.
 *
 * This functions creates and adds a #GtkCellRendererText for the selected
 * column. If you need to set the text column, but don't want the cell
 * renderer, use g_object_set() to set the #GtkEntryCompletion:text-column
 * property directly.
 */
void
gtk_entry_completion_set_text_column (GtkEntryCompletion *completion,
                                      gint                column)
{
  GtkCellRenderer *cell;

  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (completion));
  g_return_if_fail (column >= 0);

  if (completion->priv->text_column == column)
    return;

  completion->priv->text_column = column;

  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (completion),
                              cell, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (completion),
                                 cell,
                                 "text", column);

  g_object_notify_by_pspec (G_OBJECT (completion), entry_completion_props[PROP_TEXT_COLUMN]);
}

/**
 * gtk_entry_completion_get_text_column:
 * @completion: a #GtkEntryCompletion
 *
 * Returns the column in the model of @completion to get strings from.
 *
 * Returns: the column containing the strings
 */
gint
gtk_entry_completion_get_text_column (GtkEntryCompletion *completion)
{
  g_return_val_if_fail (GTK_IS_ENTRY_COMPLETION (completion), -1);

  return completion->priv->text_column;
}

/* private */

/* some nasty size requisition */
void
_gtk_entry_completion_resize_popup (GtkEntryCompletion *completion)
{
  GtkAllocation allocation;
  gint matches, actions, items, height;
  GdkSurface *surface;
  GtkRequisition entry_req;
  GtkRequisition tree_req;
  GtkTreePath *path;
  gint width;
  GtkTreeViewColumn *action_column;
  gint action_height;

  surface = gtk_native_get_surface (gtk_widget_get_native (completion->priv->entry));

  if (!surface)
    return;

  if (!completion->priv->filter_model)
    return;

  gtk_widget_get_surface_allocation (completion->priv->entry, &allocation);
  gtk_widget_get_preferred_size (completion->priv->entry,
                                 &entry_req, NULL);

  matches = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (completion->priv->filter_model), NULL);
  actions = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (completion->priv->actions), NULL);
  action_column  = gtk_tree_view_get_column (GTK_TREE_VIEW (completion->priv->action_view), 0);

  /* Call get preferred size on the on the tree view to force it to validate its
   * cells before calling into the cell size functions.
   */
  gtk_widget_get_preferred_size (completion->priv->tree_view,
                                 &tree_req, NULL);
  gtk_tree_view_column_cell_get_size (completion->priv->column,
                                      NULL, NULL, NULL, &height);
  gtk_tree_view_column_cell_get_size (action_column,
                                      NULL, NULL, NULL, &action_height);

  gtk_widget_realize (completion->priv->tree_view);

  items = MIN (matches, 10);

  if (items <= 0)
    gtk_widget_hide (completion->priv->scrolled_window);
  else
    gtk_widget_show (completion->priv->scrolled_window);

  if (completion->priv->popup_set_width)
    width = allocation.width;
  else
    width = -1;

  gtk_tree_view_columns_autosize (GTK_TREE_VIEW (completion->priv->tree_view));
  gtk_scrolled_window_set_min_content_width (GTK_SCROLLED_WINDOW (completion->priv->scrolled_window), width);
  gtk_widget_set_size_request (completion->priv->popup_window, width, -1);
  gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (completion->priv->scrolled_window), items * height);

  if (actions)
    gtk_widget_show (completion->priv->action_view);
  else
    gtk_widget_hide (completion->priv->action_view);

  if (matches > 0)
    {
      path = gtk_tree_path_new_from_indices (0, -1);
      gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (completion->priv->tree_view), path,
                                    NULL, FALSE, 0.0, 0.0);
      gtk_tree_path_free (path);
    }
  gtk_native_check_resize (GTK_NATIVE (completion->priv->popup_window));
}

static void
gtk_entry_completion_popup (GtkEntryCompletion *completion)
{
  GtkText *text = gtk_entry_get_text_widget (GTK_ENTRY (completion->priv->entry));

  if (gtk_widget_get_mapped (completion->priv->popup_window))
    return;

  if (!gtk_widget_get_mapped (GTK_WIDGET (text)))
    return;

  if (!gtk_widget_has_focus (GTK_WIDGET (text)))
    return;

  /* default on no match */
  completion->priv->current_selected = -1;

  gtk_widget_realize (completion->priv->popup_window);

  _gtk_entry_completion_resize_popup (completion);

  gtk_popover_popup (GTK_POPOVER (completion->priv->popup_window));
}

void
_gtk_entry_completion_popdown (GtkEntryCompletion *completion)
{
  if (!gtk_widget_get_mapped (completion->priv->popup_window))
    return;

  gtk_popover_popdown (GTK_POPOVER (completion->priv->popup_window));
}

static gboolean
gtk_entry_completion_match_selected (GtkEntryCompletion *completion,
                                     GtkTreeModel       *model,
                                     GtkTreeIter        *iter)
{
  gchar *str = NULL;

  gtk_tree_model_get (model, iter, completion->priv->text_column, &str, -1);
  gtk_editable_set_text (GTK_EDITABLE (completion->priv->entry), str ? str : "");

  /* move cursor to the end */
  gtk_editable_set_position (GTK_EDITABLE (completion->priv->entry), -1);

  g_free (str);

  return TRUE;
}

static gboolean
gtk_entry_completion_cursor_on_match (GtkEntryCompletion *completion,
                                      GtkTreeModel       *model,
                                      GtkTreeIter        *iter)
{
  gtk_entry_completion_insert_completion (completion, model, iter);

  return TRUE;
}

/**
 * gtk_entry_completion_compute_prefix:
 * @completion: the entry completion
 * @key: The text to complete for
 *
 * Computes the common prefix that is shared by all rows in @completion
 * that start with @key. If no row matches @key, %NULL will be returned.
 * Note that a text column must have been set for this function to work,
 * see gtk_entry_completion_set_text_column() for details. 
 *
 * Returns: (nullable) (transfer full): The common prefix all rows starting with
 *   @key or %NULL if no row matches @key.
 **/
gchar *
gtk_entry_completion_compute_prefix (GtkEntryCompletion *completion,
                                     const char         *key)
{
  GtkTreeIter iter;
  gchar *prefix = NULL;
  gboolean valid;

  if (completion->priv->text_column < 0)
    return NULL;

  valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (completion->priv->filter_model),
                                         &iter);

  while (valid)
    {
      gchar *text;

      gtk_tree_model_get (GTK_TREE_MODEL (completion->priv->filter_model),
                          &iter, completion->priv->text_column, &text,
                          -1);

      if (text && g_str_has_prefix (text, key))
        {
          if (!prefix)
            prefix = g_strdup (text);
          else
            {
              gchar *p = prefix;
              gchar *q = text;

              while (*p && *p == *q)
                {
                  p++;
                  q++;
                }

              *p = '\0';

              if (p > prefix)
                {
                  /* strip a partial multibyte character */
                  q = g_utf8_find_prev_char (prefix, p);
                  switch (g_utf8_get_char_validated (q, p - q))
                    {
                    case (gunichar)-2:
                    case (gunichar)-1:
                      *q = 0;
                    default: ;
                    }
                }
            }
        }

      g_free (text);
      valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (completion->priv->filter_model),
                                        &iter);
    }

  return prefix;
}


static gboolean
gtk_entry_completion_real_insert_prefix (GtkEntryCompletion *completion,
                                         const gchar        *prefix)
{
  if (prefix)
    {
      gint key_len;
      gint prefix_len;
      const gchar *key;

      prefix_len = g_utf8_strlen (prefix, -1);

      key = gtk_editable_get_text (GTK_EDITABLE (completion->priv->entry));
      key_len = g_utf8_strlen (key, -1);

      if (prefix_len > key_len)
        {
          gint pos = prefix_len;

          gtk_editable_insert_text (GTK_EDITABLE (completion->priv->entry),
                                    prefix + strlen (key), -1, &pos);
          gtk_editable_select_region (GTK_EDITABLE (completion->priv->entry),
                                      key_len, prefix_len);

          completion->priv->has_completion = TRUE;
        }
    }

  return TRUE;
}

/**
 * gtk_entry_completion_get_completion_prefix:
 * @completion: a #GtkEntryCompletion
 *
 * Get the original text entered by the user that triggered
 * the completion or %NULL if there’s no completion ongoing.
 *
 * Returns: the prefix for the current completion
 */
const gchar*
gtk_entry_completion_get_completion_prefix (GtkEntryCompletion *completion)
{
  g_return_val_if_fail (GTK_IS_ENTRY_COMPLETION (completion), NULL);

  return completion->priv->completion_prefix;
}

static void
gtk_entry_completion_insert_completion_text (GtkEntryCompletion *completion,
                                             const gchar        *new_text)
{
  GtkEntryCompletionPrivate *priv = completion->priv;
  gint len;
  GtkText *text = gtk_entry_get_text_widget (GTK_ENTRY (priv->entry));

  priv = completion->priv;

  if (priv->changed_id > 0)
    g_signal_handler_block (text, priv->changed_id);

  if (priv->insert_text_id > 0)
    g_signal_handler_block (text, priv->insert_text_id);

  gtk_editable_set_text (GTK_EDITABLE (priv->entry), new_text);

  len = strlen (priv->completion_prefix);
  gtk_editable_select_region (GTK_EDITABLE (priv->entry), len, -1);

  if (priv->changed_id > 0)
    g_signal_handler_unblock (text, priv->changed_id);

  if (priv->insert_text_id > 0)
    g_signal_handler_unblock (text, priv->insert_text_id);
}

static gboolean
gtk_entry_completion_insert_completion (GtkEntryCompletion *completion,
                                        GtkTreeModel       *model,
                                        GtkTreeIter        *iter)
{
  gchar *str = NULL;

  if (completion->priv->text_column < 0)
    return FALSE;

  gtk_tree_model_get (model, iter,
                      completion->priv->text_column, &str,
                      -1);

  gtk_entry_completion_insert_completion_text (completion, str);

  g_free (str);

  return TRUE;
}

/**
 * gtk_entry_completion_insert_prefix:
 * @completion: a #GtkEntryCompletion
 *
 * Requests a prefix insertion.
 */
void
gtk_entry_completion_insert_prefix (GtkEntryCompletion *completion)
{
  gboolean done;
  gchar *prefix;
  GtkText *text = gtk_entry_get_text_widget (GTK_ENTRY (completion->priv->entry));

  if (completion->priv->insert_text_id > 0)
    g_signal_handler_block (text, completion->priv->insert_text_id);

  prefix = gtk_entry_completion_compute_prefix (completion,
                                                gtk_editable_get_text (GTK_EDITABLE (completion->priv->entry)));

  if (prefix)
    {
      g_signal_emit (completion, entry_completion_signals[INSERT_PREFIX],
                     0, prefix, &done);
      g_free (prefix);
    }

  if (completion->priv->insert_text_id > 0)
    g_signal_handler_unblock (text, completion->priv->insert_text_id);
}

/**
 * gtk_entry_completion_set_inline_completion:
 * @completion: a #GtkEntryCompletion
 * @inline_completion: %TRUE to do inline completion
 *
 * Sets whether the common prefix of the possible completions should
 * be automatically inserted in the entry.
 */
void
gtk_entry_completion_set_inline_completion (GtkEntryCompletion *completion,
                                            gboolean            inline_completion)
{
  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (completion));

  inline_completion = inline_completion != FALSE;

  if (completion->priv->inline_completion != inline_completion)
    {
      completion->priv->inline_completion = inline_completion;

      g_object_notify_by_pspec (G_OBJECT (completion), entry_completion_props[PROP_INLINE_COMPLETION]);
    }
}

/**
 * gtk_entry_completion_get_inline_completion:
 * @completion: a #GtkEntryCompletion
 *
 * Returns whether the common prefix of the possible completions should
 * be automatically inserted in the entry.
 *
 * Returns: %TRUE if inline completion is turned on
 */
gboolean
gtk_entry_completion_get_inline_completion (GtkEntryCompletion *completion)
{
  g_return_val_if_fail (GTK_IS_ENTRY_COMPLETION (completion), FALSE);

  return completion->priv->inline_completion;
}

/**
 * gtk_entry_completion_set_popup_completion:
 * @completion: a #GtkEntryCompletion
 * @popup_completion: %TRUE to do popup completion
 *
 * Sets whether the completions should be presented in a popup window.
 */
void
gtk_entry_completion_set_popup_completion (GtkEntryCompletion *completion,
                                           gboolean            popup_completion)
{
  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (completion));

  popup_completion = popup_completion != FALSE;

  if (completion->priv->popup_completion != popup_completion)
    {
      completion->priv->popup_completion = popup_completion;

      g_object_notify_by_pspec (G_OBJECT (completion), entry_completion_props[PROP_POPUP_COMPLETION]);
    }
}


/**
 * gtk_entry_completion_get_popup_completion:
 * @completion: a #GtkEntryCompletion
 *
 * Returns whether the completions should be presented in a popup window.
 *
 * Returns: %TRUE if popup completion is turned on
 */
gboolean
gtk_entry_completion_get_popup_completion (GtkEntryCompletion *completion)
{
  g_return_val_if_fail (GTK_IS_ENTRY_COMPLETION (completion), TRUE);

  return completion->priv->popup_completion;
}

/**
 * gtk_entry_completion_set_popup_set_width:
 * @completion: a #GtkEntryCompletion
 * @popup_set_width: %TRUE to make the width of the popup the same as the entry
 *
 * Sets whether the completion popup window will be resized to be the same
 * width as the entry.
 */
void
gtk_entry_completion_set_popup_set_width (GtkEntryCompletion *completion,
                                          gboolean            popup_set_width)
{
  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (completion));

  popup_set_width = popup_set_width != FALSE;

  if (completion->priv->popup_set_width != popup_set_width)
    {
      completion->priv->popup_set_width = popup_set_width;

      g_object_notify_by_pspec (G_OBJECT (completion), entry_completion_props[PROP_POPUP_SET_WIDTH]);
    }
}

/**
 * gtk_entry_completion_get_popup_set_width:
 * @completion: a #GtkEntryCompletion
 *
 * Returns whether the  completion popup window will be resized to the
 * width of the entry.
 *
 * Returns: %TRUE if the popup window will be resized to the width of
 *   the entry
 */
gboolean
gtk_entry_completion_get_popup_set_width (GtkEntryCompletion *completion)
{
  g_return_val_if_fail (GTK_IS_ENTRY_COMPLETION (completion), TRUE);

  return completion->priv->popup_set_width;
}


/**
 * gtk_entry_completion_set_popup_single_match:
 * @completion: a #GtkEntryCompletion
 * @popup_single_match: %TRUE if the popup should appear even for a single
 *     match
 *
 * Sets whether the completion popup window will appear even if there is
 * only a single match. You may want to set this to %FALSE if you
 * are using [inline completion][GtkEntryCompletion--inline-completion].
 */
void
gtk_entry_completion_set_popup_single_match (GtkEntryCompletion *completion,
                                             gboolean            popup_single_match)
{
  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (completion));

  popup_single_match = popup_single_match != FALSE;

  if (completion->priv->popup_single_match != popup_single_match)
    {
      completion->priv->popup_single_match = popup_single_match;

      g_object_notify_by_pspec (G_OBJECT (completion), entry_completion_props[PROP_POPUP_SINGLE_MATCH]);
    }
}

/**
 * gtk_entry_completion_get_popup_single_match:
 * @completion: a #GtkEntryCompletion
 *
 * Returns whether the completion popup window will appear even if there is
 * only a single match.
 *
 * Returns: %TRUE if the popup window will appear regardless of the
 *    number of matches
 */
gboolean
gtk_entry_completion_get_popup_single_match (GtkEntryCompletion *completion)
{
  g_return_val_if_fail (GTK_IS_ENTRY_COMPLETION (completion), TRUE);

  return completion->priv->popup_single_match;
}

/**
 * gtk_entry_completion_set_inline_selection:
 * @completion: a #GtkEntryCompletion
 * @inline_selection: %TRUE to do inline selection
 *
 * Sets whether it is possible to cycle through the possible completions
 * inside the entry.
 */
void
gtk_entry_completion_set_inline_selection (GtkEntryCompletion *completion,
                                           gboolean inline_selection)
{
  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (completion));

  inline_selection = inline_selection != FALSE;

  if (completion->priv->inline_selection != inline_selection)
    {
      completion->priv->inline_selection = inline_selection;

      g_object_notify_by_pspec (G_OBJECT (completion), entry_completion_props[PROP_INLINE_SELECTION]);
    }
}

/**
 * gtk_entry_completion_get_inline_selection:
 * @completion: a #GtkEntryCompletion
 *
 * Returns %TRUE if inline-selection mode is turned on.
 *
 * Returns: %TRUE if inline-selection mode is on
 */
gboolean
gtk_entry_completion_get_inline_selection (GtkEntryCompletion *completion)
{
  g_return_val_if_fail (GTK_IS_ENTRY_COMPLETION (completion), FALSE);

  return completion->priv->inline_selection;
}


static gint
gtk_entry_completion_timeout (gpointer data)
{
  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (data);

  completion->priv->completion_timeout = 0;

  if (completion->priv->filter_model &&
      g_utf8_strlen (gtk_editable_get_text (GTK_EDITABLE (completion->priv->entry)), -1)
      >= completion->priv->minimum_key_length)
    {
      gint matches;
      gint actions;
      GtkTreeSelection *s;
      gboolean popup_single;

      gtk_entry_completion_complete (completion);
      matches = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (completion->priv->filter_model), NULL);
      gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (GTK_TREE_VIEW (completion->priv->tree_view)));

      s = gtk_tree_view_get_selection (GTK_TREE_VIEW (completion->priv->action_view));

      gtk_tree_selection_unselect_all (s);

      actions = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (completion->priv->actions), NULL);

      g_object_get (completion, "popup-single-match", &popup_single, NULL);
      if ((matches > (popup_single ? 0: 1)) || actions > 0)
        {
          if (gtk_widget_get_visible (completion->priv->popup_window))
            _gtk_entry_completion_resize_popup (completion);
          else
            gtk_entry_completion_popup (completion);
        }
      else
        _gtk_entry_completion_popdown (completion);
    }
  else if (gtk_widget_get_visible (completion->priv->popup_window))
    _gtk_entry_completion_popdown (completion);
  return G_SOURCE_REMOVE;
}

static inline gboolean
keyval_is_cursor_move (guint keyval)
{
  if (keyval == GDK_KEY_Up || keyval == GDK_KEY_KP_Up)
    return TRUE;

  if (keyval == GDK_KEY_Down || keyval == GDK_KEY_KP_Down)
    return TRUE;

  if (keyval == GDK_KEY_Page_Up)
    return TRUE;

  if (keyval == GDK_KEY_Page_Down)
    return TRUE;

  return FALSE;
}

static gboolean
gtk_entry_completion_key_pressed (GtkEventControllerKey *controller,
                                  guint                  keyval,
                                  guint                  keycode,
                                  GdkModifierType        state,
                                  gpointer               user_data)
{
  gint matches, actions = 0;
  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (user_data);
  GtkWidget *widget = completion->priv->entry;
  GtkText *text = gtk_entry_get_text_widget (GTK_ENTRY (widget));

  if (!completion->priv->popup_completion)
    return FALSE;

  if (keyval == GDK_KEY_Return ||
      keyval == GDK_KEY_KP_Enter ||
      keyval == GDK_KEY_ISO_Enter ||
      keyval == GDK_KEY_Escape)
    {
      if (completion->priv->completion_timeout)
        {
          g_source_remove (completion->priv->completion_timeout);
          completion->priv->completion_timeout = 0;
        }
    }

  if (!gtk_widget_get_mapped (completion->priv->popup_window))
    return FALSE;

  matches = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (completion->priv->filter_model), NULL);

  if (completion->priv->actions)
    actions = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (completion->priv->actions), NULL);

  if (keyval_is_cursor_move (keyval))
    {
      GtkTreePath *path = NULL;

      if (keyval == GDK_KEY_Up || keyval == GDK_KEY_KP_Up)
        {
          if (completion->priv->current_selected < 0)
            completion->priv->current_selected = matches + actions - 1;
          else
            completion->priv->current_selected--;
        }
      else if (keyval == GDK_KEY_Down || keyval == GDK_KEY_KP_Down)
        {
          if (completion->priv->current_selected < matches + actions - 1)
            completion->priv->current_selected++;
          else
            completion->priv->current_selected = -1;
        }
      else if (keyval == GDK_KEY_Page_Up)
        {
          if (completion->priv->current_selected < 0)
            completion->priv->current_selected = matches + actions - 1;
          else if (completion->priv->current_selected == 0)
            completion->priv->current_selected = -1;
          else if (completion->priv->current_selected < matches)
            {
              completion->priv->current_selected -= PAGE_STEP;
              if (completion->priv->current_selected < 0)
                completion->priv->current_selected = 0;
            }
          else
            {
              completion->priv->current_selected -= PAGE_STEP;
              if (completion->priv->current_selected < matches - 1)
                completion->priv->current_selected = matches - 1;
            }
        }
      else if (keyval == GDK_KEY_Page_Down)
        {
          if (completion->priv->current_selected < 0)
            completion->priv->current_selected = 0;
          else if (completion->priv->current_selected < matches - 1)
            {
              completion->priv->current_selected += PAGE_STEP;
              if (completion->priv->current_selected > matches - 1)
                completion->priv->current_selected = matches - 1;
            }
          else if (completion->priv->current_selected == matches + actions - 1)
            {
              completion->priv->current_selected = -1;
            }
          else
            {
              completion->priv->current_selected += PAGE_STEP;
              if (completion->priv->current_selected > matches + actions - 1)
                completion->priv->current_selected = matches + actions - 1;
            }
        }

      if (completion->priv->current_selected < 0)
        {
          gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (GTK_TREE_VIEW (completion->priv->tree_view)));
          gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (GTK_TREE_VIEW (completion->priv->action_view)));

          if (completion->priv->inline_selection &&
              completion->priv->completion_prefix)
            {
              gtk_editable_set_text (GTK_EDITABLE (completion->priv->entry),
                                     completion->priv->completion_prefix);
              gtk_editable_set_position (GTK_EDITABLE (widget), -1);
            }
        }
      else if (completion->priv->current_selected < matches)
        {
          gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (GTK_TREE_VIEW (completion->priv->action_view)));

          path = gtk_tree_path_new_from_indices (completion->priv->current_selected, -1);
          gtk_tree_view_set_cursor (GTK_TREE_VIEW (completion->priv->tree_view),
                                    path, NULL, FALSE);

          if (completion->priv->inline_selection)
            {

              GtkTreeIter iter;
              GtkTreeIter child_iter;
              GtkTreeModel *model = NULL;
              GtkTreeSelection *sel;
              gboolean entry_set;

              sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (completion->priv->tree_view));
              if (!gtk_tree_selection_get_selected (sel, &model, &iter))
                return FALSE;
             gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model), &child_iter, &iter);
              model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));

              if (completion->priv->completion_prefix == NULL)
                completion->priv->completion_prefix = g_strdup (gtk_editable_get_text (GTK_EDITABLE (completion->priv->entry)));

              g_signal_emit_by_name (completion, "cursor-on-match", model,
                                     &child_iter, &entry_set);
            }
        }
      else if (completion->priv->current_selected - matches >= 0)
        {
          gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (GTK_TREE_VIEW (completion->priv->tree_view)));

          path = gtk_tree_path_new_from_indices (completion->priv->current_selected - matches, -1);
          gtk_tree_view_set_cursor (GTK_TREE_VIEW (completion->priv->action_view),
                                    path, NULL, FALSE);

          if (completion->priv->inline_selection &&
              completion->priv->completion_prefix)
            {
              gtk_editable_set_text (GTK_EDITABLE (completion->priv->entry),
                                     completion->priv->completion_prefix);
              gtk_editable_set_position (GTK_EDITABLE (widget), -1);
            }
        }

      gtk_tree_path_free (path);

      return TRUE;
    }
  else if (keyval == GDK_KEY_Escape ||
           keyval == GDK_KEY_Left ||
           keyval == GDK_KEY_KP_Left ||
           keyval == GDK_KEY_Right ||
           keyval == GDK_KEY_KP_Right)
    {
      gboolean retval = TRUE;

      gtk_entry_reset_im_context (GTK_ENTRY (widget));
      _gtk_entry_completion_popdown (completion);

      if (completion->priv->current_selected < 0)
        {
          retval = FALSE;
          goto keypress_completion_out;
        }
      else if (completion->priv->inline_selection)
        {
          /* Escape rejects the tentative completion */
          if (keyval == GDK_KEY_Escape)
            {
              if (completion->priv->completion_prefix)
                gtk_editable_set_text (GTK_EDITABLE (completion->priv->entry),
                                       completion->priv->completion_prefix);
              else
                gtk_editable_set_text (GTK_EDITABLE (completion->priv->entry), "");
            }

          /* Move the cursor to the end for Right/Esc */
          if (keyval == GDK_KEY_Right ||
              keyval == GDK_KEY_KP_Right ||
              keyval == GDK_KEY_Escape)
            gtk_editable_set_position (GTK_EDITABLE (widget), -1);
          /* Let the default keybindings run for Left, i.e. either move to the
 *            * previous character or select word if a modifier is used */
          else
            retval = FALSE;
        }

keypress_completion_out:
      if (completion->priv->inline_selection)
        {
          g_free (completion->priv->completion_prefix);
          completion->priv->completion_prefix = NULL;
        }

      return retval;
    }
  else if (keyval == GDK_KEY_Tab ||
           keyval == GDK_KEY_KP_Tab ||
           keyval == GDK_KEY_ISO_Left_Tab)
    {
      gtk_entry_reset_im_context (GTK_ENTRY (widget));
      _gtk_entry_completion_popdown (completion);

      g_free (completion->priv->completion_prefix);
      completion->priv->completion_prefix = NULL;

      return FALSE;
    }
  else if (keyval == GDK_KEY_ISO_Enter ||
           keyval == GDK_KEY_KP_Enter ||
           keyval == GDK_KEY_Return)
    {
      GtkTreeIter iter;
      GtkTreeModel *model = NULL;
      GtkTreeModel *child_model;
      GtkTreeIter child_iter;
      GtkTreeSelection *sel;
      gboolean retval = TRUE;

      gtk_entry_reset_im_context (GTK_ENTRY (widget));
      _gtk_entry_completion_popdown (completion);

      if (completion->priv->current_selected < matches)
        {
          gboolean entry_set;

          sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (completion->priv->tree_view));
          if (gtk_tree_selection_get_selected (sel, &model, &iter))
            {
              gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model), &child_iter, &iter);
              child_model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
              g_signal_handler_block (text, completion->priv->changed_id);
              g_signal_emit_by_name (completion, "match-selected",
                                     child_model, &child_iter, &entry_set);
              g_signal_handler_unblock (text, completion->priv->changed_id);

              if (!entry_set)
                {
                  gchar *str = NULL;

                  gtk_tree_model_get (model, &iter,
                                      completion->priv->text_column, &str,
                                      -1);

                  gtk_editable_set_text (GTK_EDITABLE (widget), str);

                  /* move the cursor to the end */
                  gtk_editable_set_position (GTK_EDITABLE (widget), -1);
                  g_free (str);
                }
            }
          else
            retval = FALSE;
        }
      else if (completion->priv->current_selected - matches >= 0)
        {
          sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (completion->priv->action_view));
          if (gtk_tree_selection_get_selected (sel, &model, &iter))
            {
              GtkTreePath *path;

              path = gtk_tree_path_new_from_indices (completion->priv->current_selected - matches, -1);
              g_signal_emit_by_name (completion, "action-activated",
                                     gtk_tree_path_get_indices (path)[0]);
              gtk_tree_path_free (path);
            }
          else
            retval = FALSE;
        }

      g_free (completion->priv->completion_prefix);
      completion->priv->completion_prefix = NULL;

      return retval;
    }

  return FALSE;
}

static void
gtk_entry_completion_changed (GtkWidget *widget,
                              gpointer   user_data)
{
  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (user_data);
  GdkDevice *device;

  if (!completion->priv->popup_completion)
    return;

  /* (re)install completion timeout */
  if (completion->priv->completion_timeout)
    {
      g_source_remove (completion->priv->completion_timeout);
      completion->priv->completion_timeout = 0;
    }

  if (!gtk_editable_get_text (GTK_EDITABLE (widget)))
    return;

  /* no need to normalize for this test */
  if (completion->priv->minimum_key_length > 0 &&
      strcmp ("", gtk_editable_get_text (GTK_EDITABLE (widget))) == 0)
    {
      if (gtk_widget_get_visible (completion->priv->popup_window))
        _gtk_entry_completion_popdown (completion);
      return;
    }

  device = gtk_get_current_event_device ();

  if (device && gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    device = gdk_device_get_associated_device (device);

  if (device)
    completion->priv->device = device;

  completion->priv->completion_timeout =
    g_timeout_add (COMPLETION_TIMEOUT,
                   gtk_entry_completion_timeout,
                   completion);
  g_source_set_name_by_id (completion->priv->completion_timeout, "[gtk] gtk_entry_completion_timeout");
}

static gboolean
check_completion_callback (GtkEntryCompletion *completion)
{
  completion->priv->check_completion_idle = NULL;
  
  gtk_entry_completion_complete (completion);
  gtk_entry_completion_insert_prefix (completion);

  return FALSE;
}

static void
clear_completion_callback (GObject            *text,
                           GParamSpec         *pspec,
                           GtkEntryCompletion *completion)
{
  if (!completion->priv->inline_completion)
    return;

  if (pspec->name == I_("cursor-position") ||
      pspec->name == I_("selection-bound"))
    completion->priv->has_completion = FALSE;
}

static gboolean
accept_completion_callback (GtkEntryCompletion *completion)
{
  if (!completion->priv->inline_completion)
    return FALSE;

  if (completion->priv->has_completion)
    gtk_editable_set_position (GTK_EDITABLE (completion->priv->entry),
                               gtk_entry_buffer_get_length (gtk_entry_get_buffer (GTK_ENTRY (completion->priv->entry))));

  return FALSE;
}

static gboolean
text_focus_out (GtkEntryCompletion *completion)
{
  if (gtk_widget_get_mapped (completion->priv->popup_window))
    return FALSE;

  return accept_completion_callback (completion);
}

static void
completion_insert_text_callback (GtkText            *entry,
                                 const gchar        *text,
                                 gint                length,
                                 gint                position,
                                 GtkEntryCompletion *completion)
{
  if (!completion->priv->inline_completion)
    return;

  /* idle to update the selection based on the file list */
  if (completion->priv->check_completion_idle == NULL)
    {
      completion->priv->check_completion_idle = g_idle_source_new ();
      g_source_set_priority (completion->priv->check_completion_idle, G_PRIORITY_HIGH);
      g_source_set_closure (completion->priv->check_completion_idle,
                            g_cclosure_new_object (G_CALLBACK (check_completion_callback),
                                                   G_OBJECT (completion)));
      g_source_attach (completion->priv->check_completion_idle, NULL);
      g_source_set_name (completion->priv->check_completion_idle, "[gtk] check_completion_callback");
    }
}

static void
connect_completion_signals (GtkEntryCompletion *completion)
{
  GtkEntryCompletionPrivate *priv = completion->priv;
  GtkEventController *controller;
  GtkText *text = gtk_entry_get_text_widget (GTK_ENTRY (priv->entry));

  controller = priv->entry_key_controller = gtk_event_controller_key_new ();
  g_signal_connect (controller, "key-pressed",
                    G_CALLBACK (gtk_entry_completion_key_pressed), completion);
  g_signal_connect_swapped (controller, "focus-out", G_CALLBACK (text_focus_out), completion);
  gtk_widget_add_controller (GTK_WIDGET (text), controller);

  completion->priv->changed_id =
    g_signal_connect (text, "changed", G_CALLBACK (gtk_entry_completion_changed), completion);

    completion->priv->insert_text_id =
      g_signal_connect (text, "insert-text", G_CALLBACK (completion_insert_text_callback), completion);
    g_signal_connect (text, "notify", G_CALLBACK (clear_completion_callback), completion);
    g_signal_connect_swapped (text, "activate", G_CALLBACK (accept_completion_callback), completion);
}

static void
set_accessible_relation (GtkWidget *window,
                         GtkWidget *entry)
{
  AtkObject *window_accessible;
  AtkObject *entry_accessible;

  window_accessible = gtk_widget_get_accessible (window);
  entry_accessible = gtk_widget_get_accessible (entry);

  atk_object_add_relationship (window_accessible,
                               ATK_RELATION_POPUP_FOR,
                               entry_accessible);
}

static void
unset_accessible_relation (GtkWidget *window,
                           GtkWidget *entry)
{
  AtkObject *window_accessible;
  AtkObject *entry_accessible;

  window_accessible = gtk_widget_get_accessible (window);
  entry_accessible = gtk_widget_get_accessible (entry);

  atk_object_remove_relationship (window_accessible,
                                  ATK_RELATION_POPUP_FOR,
                                  entry_accessible);
}

static void
disconnect_completion_signals (GtkEntryCompletion *completion)
{
  GtkText *text = gtk_entry_get_text_widget (GTK_ENTRY (completion->priv->entry));

  gtk_widget_remove_controller (GTK_WIDGET (text), completion->priv->entry_key_controller);

  if (completion->priv->changed_id > 0 &&
      g_signal_handler_is_connected (text, completion->priv->changed_id))
    {
      g_signal_handler_disconnect (text, completion->priv->changed_id);
      completion->priv->changed_id = 0;
    }
  if (completion->priv->insert_text_id > 0 &&
      g_signal_handler_is_connected (text, completion->priv->insert_text_id))
    {
      g_signal_handler_disconnect (text, completion->priv->insert_text_id);
      completion->priv->insert_text_id = 0;
    }
  g_signal_handlers_disconnect_by_func (text, G_CALLBACK (completion_insert_text_callback), completion);
  g_signal_handlers_disconnect_by_func (text, G_CALLBACK (clear_completion_callback), completion);
  g_signal_handlers_disconnect_by_func (text, G_CALLBACK (accept_completion_callback), completion);
}

void
_gtk_entry_completion_disconnect (GtkEntryCompletion *completion)
{
  if (completion->priv->completion_timeout)
    {
      g_source_remove (completion->priv->completion_timeout);
      completion->priv->completion_timeout = 0;
    }
  if (completion->priv->check_completion_idle)
    {
      g_source_destroy (completion->priv->check_completion_idle);
      completion->priv->check_completion_idle = NULL;
    }

  if (gtk_widget_get_mapped (completion->priv->popup_window))
    _gtk_entry_completion_popdown (completion);

  disconnect_completion_signals (completion);

  unset_accessible_relation (completion->priv->popup_window,
                             completion->priv->entry);
  gtk_popover_set_relative_to (GTK_POPOVER (completion->priv->popup_window),
                               NULL);

  completion->priv->entry = NULL;
}

void
_gtk_entry_completion_connect (GtkEntryCompletion *completion,
                               GtkEntry           *entry)
{
  completion->priv->entry = GTK_WIDGET (entry);

  set_accessible_relation (completion->priv->popup_window,
                           completion->priv->entry);
  gtk_popover_set_relative_to (GTK_POPOVER (completion->priv->popup_window),
                              completion->priv->entry);

  connect_completion_signals (completion);
}
