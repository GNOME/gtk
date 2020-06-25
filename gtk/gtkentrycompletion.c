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
 * case-insensitively against the text obtained from the model using
 * an expression set with gtk_entry_completion_set_expression().
 *
 * When the user selects a completion, the content of the entry is
 * updated. By default, the content of the entry is replaced by the
 * text column of the model, but this can be overridden by connecting
 * to the #GtkEntryCompletion::match-selected signal and updating the
 * entry in the signal handler. Note that you should return %TRUE from
 * the signal handler to suppress the default behaviour.
 *
 * To add completion functionality to an entry, use gtk_entry_set_completion().
 */

#include "config.h"

#include "gtkentrycompletion.h"

#include "gtkentryprivate.h"
#include "gtktextprivate.h"

#include "gtkintl.h"
#include "gtkscrolledwindow.h"
#include "gtksizerequest.h"
#include "gtkbox.h"
#include "gtkpopover.h"
#include "gtkentry.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkeventcontrollerfocus.h"
#include "gtkeventcontrollerkey.h"
#include "gtkgestureclick.h"

#include "gtkprivate.h"
#include "gtkwindowprivate.h"
#include "gtkwidgetprivate.h"
#include "gtknative.h"
#include "gtkstylecontext.h"
#include "gtkstringfilter.h"
#include "gtkbuildable.h"
#include "gtkfilterlistmodel.h"
#include "gtklistview.h"
#include "gtkstringlist.h"
#include "gtksignallistitemfactory.h"
#include "gtklistitem.h"
#include "gtklabel.h"
#include "gtksingleselection.h"
#include "gtkselectionmodel.h"

#include <string.h>

#define PAGE_STEP 14
#define COMPLETION_TIMEOUT 100

/* signals */
enum
{
  INSERT_PREFIX,
  MATCH_SELECTED,
  CURSOR_ON_MATCH,
  NO_MATCHES,
  LAST_SIGNAL
};

/* properties */
enum
{
  PROP_0,
  PROP_MODEL,
  PROP_EXPRESSION,
  PROP_FACTORY,
  PROP_MINIMUM_KEY_LENGTH,
  PROP_INLINE_COMPLETION,
  PROP_POPUP_COMPLETION,
  PROP_POPUP_SET_WIDTH,
  PROP_POPUP_SINGLE_MATCH,
  PROP_INLINE_SELECTION,
  NUM_PROPERTIES
};


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

static void gtk_entry_completion_list_activated          (GtkListView *listview,
                                                          guint        position,
                                                          gpointer     user_data);
static gboolean gtk_entry_completion_match_selected      (GtkEntryCompletion *completion,
                                                          guint               position);
static gboolean gtk_entry_completion_real_insert_prefix  (GtkEntryCompletion *completion,
                                                          const char         *prefix);
static gboolean gtk_entry_completion_cursor_on_match     (GtkEntryCompletion *completion,
                                                          guint               position);
static gboolean gtk_entry_completion_insert_completion   (GtkEntryCompletion *completion,
                                                          guint               position);
static void     connect_completion_signals                  (GtkEntryCompletion *completion);
static void     disconnect_completion_signals               (GtkEntryCompletion *completion);


static GParamSpec *entry_completion_props[NUM_PROPERTIES] = { NULL, };

static guint entry_completion_signals[LAST_SIGNAL] = { 0 };

/* GtkBuildable */
static void
gtk_entry_completion_buildable_init (GtkBuildableIface  *iface)
{
}

G_DEFINE_TYPE_WITH_CODE (GtkEntryCompletion, gtk_entry_completion, G_TYPE_OBJECT,
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
   * @selected: the position of the selected item
   *
   * Gets emitted when a match from the list is selected.
   * The default behaviour is to replace the contents of the
   * entry with the contents of the selected item.
   *
   * Returns: %TRUE if the signal has been handled
   */
  entry_completion_signals[MATCH_SELECTED] =
    g_signal_new (I_("match-selected"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkEntryCompletionClass, match_selected),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__UINT,
                  G_TYPE_BOOLEAN, 1,
                  G_TYPE_UINT);

  /**
   * GtkEntryCompletion::cursor-on-match:
   * @widget: the object which received the signal
   * @selected: the position of the selected item
   *
   * Gets emitted when a match from the cursor is on a match
   * of the list. The default behaviour is to replace the contents
   * of the entry with the contents of the text column in the row
   * pointed to by @iter.
   *
   * Returns: %TRUE if the signal has been handled
   */
  entry_completion_signals[CURSOR_ON_MATCH] =
    g_signal_new (I_("cursor-on-match"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkEntryCompletionClass, cursor_on_match),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__UINT,
                  G_TYPE_BOOLEAN, 1,
                  G_TYPE_UINT);

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
   * GtkEntryCompletion:model:
   *
   * Model for the displayed list items.
   */
  entry_completion_props[PROP_MODEL] =
      g_param_spec_object ("model",
                           P_("Completion Model"),
                           P_("The model to find matches in"),
                           G_TYPE_LIST_MODEL,
                           GTK_PARAM_READWRITE);

  /**
   * GtkEntryCompletion:factory:
   *
   * Factory for populating list items.
   */
  entry_completion_props[PROP_FACTORY] =
      g_param_spec_object ("factory",
                           P_("Factory"),
                           P_("The factory for populating list items"),
                           GTK_TYPE_LIST_ITEM_FACTORY,
                           GTK_PARAM_READWRITE);

  /**
   * GtkEntryCompletion:expression: (type GtkExpression)
   *
   * An expression to evaluate to obtain strings to match against the text
   * of the entry. If #GtkEntryCompletion:factory is not set, the expression
   * is also used to bind strings to labels produced by a default factory.
   */
  entry_completion_props[PROP_EXPRESSION] =
      gtk_param_spec_expression ("expression",
                                 P_("Expression to use"),
                                 P_("Expression to evaluate to get strings"),
                                 GTK_PARAM_READWRITE);

  entry_completion_props[PROP_MINIMUM_KEY_LENGTH] =
      g_param_spec_int ("minimum-key-length",
                        P_("Minimum Key Length"),
                        P_("Minimum length of the search key in order to look up matches"),
                        0, G_MAXINT, 1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntryCompletion:inline-completion:
   *
   * Determines whether the common prefix of the possible completions
   * should be inserted automatically in the entry. Note that this
   * requires #GtkEntryCompletion:expression to be set, even if you
   * are using a custom match function.
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

  g_object_class_install_properties (object_class, NUM_PROPERTIES, entry_completion_props);
}

static void
setup_item (GtkSignalListItemFactory *factory,
            GtkListItem              *item)
{
  GtkWidget *label;

  label = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_list_item_set_child (item, label);
}

static void
bind_item (GtkSignalListItemFactory *factory,
           GtkListItem              *item,
           GtkEntryCompletion       *completion)
{
  GtkWidget *label;
  gpointer obj;
  GValue value = G_VALUE_INIT;

  label = gtk_list_item_get_child (item);
  obj = gtk_list_item_get_item (item);

  if (completion->expression &&
      gtk_expression_evaluate (completion->expression, obj, &value))
    {
      gtk_label_set_label (GTK_LABEL (label), g_value_get_string (&value));
      g_value_unset (&value);
    }
  else if (GTK_IS_STRING_OBJECT (obj))
    {
      const char *string;

      string = gtk_string_object_get_string (GTK_STRING_OBJECT (obj));
      gtk_label_set_label (GTK_LABEL (label), string);
    }
  else
    {
      g_critical ("Either GtkEntryCompletion:factory or GtkEntryCompletion:expression must be set");
    }
}

static void
gtk_entry_completion_init (GtkEntryCompletion *completion)
{
  GListModel *model;
  GtkFilter *filter;

  completion->minimum_key_length = 1;
  completion->has_completion = FALSE;
  completion->inline_completion = FALSE;
  completion->popup_completion = TRUE;
  completion->popup_set_width = TRUE;
  completion->popup_single_match = TRUE;
  completion->inline_selection = FALSE;

  completion->expression = gtk_property_expression_new (GTK_TYPE_STRING_OBJECT, NULL, "string");

  filter = gtk_string_filter_new ();
  gtk_string_filter_set_expression (GTK_STRING_FILTER (filter), completion->expression);
  gtk_string_filter_set_match_mode (GTK_STRING_FILTER (filter), GTK_STRING_FILTER_MATCH_MODE_PREFIX);
  gtk_string_filter_set_ignore_case (GTK_STRING_FILTER (filter), TRUE);

  model = G_LIST_MODEL (gtk_string_list_new ((const char *[]){ NULL }));
  completion->filter_model = G_LIST_MODEL (gtk_filter_list_model_new (model, filter));
  g_object_unref (model);
  g_object_unref (filter);

  completion->factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (completion->factory, "setup", G_CALLBACK (setup_item), NULL);
  g_signal_connect (completion->factory, "bind", G_CALLBACK (bind_item), completion);
}

static gboolean
propagate_to_entry (GtkEventControllerKey *key,
                    guint                  keyval,
                    guint                  keycode,
                    GdkModifierType        modifiers,
                    GtkEntryCompletion    *completion)
{
  GtkText *text = gtk_entry_get_text_widget (GTK_ENTRY (completion->entry));

  return gtk_event_controller_key_forward (key, GTK_WIDGET (text));
}

static void
gtk_entry_completion_constructed (GObject *object)
{
  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (object);
  GtkEventController *controller;
  GListModel *selection;

  G_OBJECT_CLASS (gtk_entry_completion_parent_class)->constructed (object);

  completion->list_view = gtk_list_view_new ();
  gtk_list_view_set_single_click_activate (GTK_LIST_VIEW (completion->list_view), TRUE);
  selection = G_LIST_MODEL (gtk_single_selection_new (completion->filter_model));
  gtk_single_selection_set_autoselect (GTK_SINGLE_SELECTION (selection), FALSE);
  gtk_single_selection_set_can_unselect (GTK_SINGLE_SELECTION (selection), TRUE);
  gtk_list_view_set_model (GTK_LIST_VIEW (completion->list_view), selection);
  g_object_unref (selection);

  gtk_list_view_set_factory (GTK_LIST_VIEW (completion->list_view), completion->factory);

  g_signal_connect (completion->list_view, "activate",
                    G_CALLBACK (gtk_entry_completion_list_activated),
                    completion);

  completion->scrolled_window = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (completion->scrolled_window),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (completion->scrolled_window), 400);
  gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (completion->scrolled_window), TRUE);

  /* pack it all */
  completion->popup_window = gtk_popover_new ();
  gtk_popover_set_position (GTK_POPOVER (completion->popup_window), GTK_POS_BOTTOM);
  gtk_popover_set_autohide (GTK_POPOVER (completion->popup_window), FALSE);
  gtk_popover_set_has_arrow (GTK_POPOVER (completion->popup_window), FALSE);
  gtk_widget_add_css_class (completion->popup_window, "entry-completion");

  controller = gtk_event_controller_key_new ();
  g_signal_connect (controller, "key-pressed",
                    G_CALLBACK (propagate_to_entry), completion);
  g_signal_connect (controller, "key-released",
                    G_CALLBACK (propagate_to_entry), completion);
  gtk_widget_add_controller (completion->popup_window, controller);

  controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
  g_signal_connect_swapped (controller, "released",
                            G_CALLBACK (gtk_entry_completion_popdown),
                            completion);
  gtk_widget_add_controller (completion->popup_window, controller);

  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (completion->scrolled_window),
                                 completion->list_view);
  gtk_widget_set_hexpand (completion->scrolled_window, TRUE);
  gtk_widget_set_vexpand (completion->scrolled_window, TRUE);
  gtk_popover_set_child (GTK_POPOVER (completion->popup_window),
                         completion->scrolled_window);
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

      case PROP_FACTORY:
        gtk_entry_completion_set_factory (completion,
                                          g_value_get_object (value));
        break;

      case PROP_EXPRESSION:
        gtk_entry_completion_set_expression (completion,
                                             gtk_value_get_expression (value));
        break;

      case PROP_MINIMUM_KEY_LENGTH:
        gtk_entry_completion_set_minimum_key_length (completion,
                                                     g_value_get_int (value));
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
        g_value_set_object (value, gtk_entry_completion_get_model (completion));
        break;

      case PROP_FACTORY:
        g_value_set_object (value, gtk_entry_completion_get_factory (completion));
        break;

      case PROP_EXPRESSION:
        gtk_value_set_expression (value, gtk_entry_completion_get_expression (completion));
        break;

      case PROP_MINIMUM_KEY_LENGTH:
        g_value_set_int (value, gtk_entry_completion_get_minimum_key_length (completion));
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

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gtk_entry_completion_finalize (GObject *object)
{
  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (object);

  g_free (completion->case_normalized_key);
  g_free (completion->completion_prefix);

  G_OBJECT_CLASS (gtk_entry_completion_parent_class)->finalize (object);
}

static void
gtk_entry_completion_dispose (GObject *object)
{
  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (object);

  if (completion->entry)
    gtk_entry_set_completion (GTK_ENTRY (completion->entry), NULL);

  g_clear_object (&completion->filter_model);
  g_clear_object (&completion->expression);
  g_clear_object (&completion->factory);

  G_OBJECT_CLASS (gtk_entry_completion_parent_class)->dispose (object);
}

/* all those callbacks */

static void
gtk_entry_completion_list_activated (GtkListView *listview,
                                     guint        position,
                                     gpointer     user_data)
{
  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (user_data);
  gboolean entry_set;
  GtkText *text = gtk_entry_get_text_widget (GTK_ENTRY (completion->entry));

  g_signal_handler_block (text, completion->changed_id);
  g_signal_emit (completion, entry_completion_signals[MATCH_SELECTED],
                 0, position, &entry_set);
  g_signal_handler_unblock (text, completion->changed_id);

  gtk_entry_completion_popdown (completion);
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

  return completion->entry;
}

/**
 * gtk_entry_completion_set_model:
 * @completion: a #GtkEntryCompletion
 * @model: (allow-none): the #GListModel
 *
 * Sets the model for a #GtkEntryCompletion. If @completion already has
 * a model set, it will remove it before setting the new model.
 * If model is %NULL, then it will unset the model.
 */
void
gtk_entry_completion_set_model (GtkEntryCompletion *completion,
                                GListModel         *model)
{
  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (completion));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));

  gtk_filter_list_model_set_model (GTK_FILTER_LIST_MODEL (completion->filter_model), model);

  if (!model)
    {
      gtk_entry_completion_popdown (completion);
      return;
    }

  g_object_notify_by_pspec (G_OBJECT (completion), entry_completion_props[PROP_MODEL]);

  if (gtk_widget_get_visible (completion->popup_window))
    gtk_entry_completion_resize_popup (completion);
}

/**
 * gtk_entry_completion_get_model:
 * @completion: a #GtkEntryCompletion
 *
 * Returns the model the #GtkEntryCompletion is using as data source.
 * Returns %NULL if the model is unset.
 *
 * Returns: (nullable) (transfer none): A #GListModel, or %NULL if none
 *     is currently being used
 */
GListModel *
gtk_entry_completion_get_model (GtkEntryCompletion *completion)
{
  g_return_val_if_fail (GTK_IS_ENTRY_COMPLETION (completion), NULL);

  return gtk_filter_list_model_get_model (GTK_FILTER_LIST_MODEL (completion->filter_model));
}

/**
 * gtk_entry_completion_set_expression:
 * @completion: a #GtkEntryCompletion
 * @expression: (nullable): a #GtkExpression, or %NULL
 *
 * Sets the expression that gets evaluated to obtain strings from items
 * when finding completions. The expression must have a value type of
 * #GTK_TYPE_STRING.
 */
void
gtk_entry_completion_set_expression (GtkEntryCompletion *completion,
                                     GtkExpression      *expression)
{
  GtkFilter *filter;

  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (completion));
  g_return_if_fail (expression == NULL || GTK_IS_EXPRESSION (expression));

  if (completion->expression)
    gtk_expression_unref (completion->expression);

  completion->expression = expression;

  if (completion->expression)
    gtk_expression_ref (completion->expression);

  filter = gtk_filter_list_model_get_filter (GTK_FILTER_LIST_MODEL (completion->filter_model));
  gtk_string_filter_set_expression (GTK_STRING_FILTER (filter), completion->expression);

  g_object_notify_by_pspec (G_OBJECT (completion), entry_completion_props[PROP_EXPRESSION]);
}

/**
 * gtk_entry_completion_get_expression:
 * @completion: a #GtkEntryCompletion
 *
 * Gets the expression set with gtk_entry_completion_set_expression().
 *
 * Returns: (nullable) (transfer none): a #GtkExpression or %NULL
 */
GtkExpression *
gtk_entry_completion_get_expression (GtkEntryCompletion *completion)
{
  g_return_val_if_fail (GTK_IS_ENTRY_COMPLETION (completion), NULL);

  return completion->expression;
}

/**
 * gtk_entry_completion_set_factory:
 * @completion: a #GtkEntryCompletion
 * @factory: (allow-none) (transfer none): the factory to use or %NULL for none
 *
 * Sets the #GtkListItemFactory to use for populating list items.
 **/
void
gtk_entry_completion_set_factory (GtkEntryCompletion *completion,
                                  GtkListItemFactory *factory)
{
  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (completion));
  g_return_if_fail (factory == NULL || GTK_IS_LIST_ITEM_FACTORY (factory));

  if (!g_set_object (&completion->factory, factory))
    return;

  if (completion->list_view)
    gtk_list_view_set_factory (GTK_LIST_VIEW (completion->list_view), factory);

  g_object_notify_by_pspec (G_OBJECT (completion), entry_completion_props[PROP_FACTORY]);
}

/**
 * gtk_entry_completion_set_factory:
 * @completion: a #GtkEntryCompletion
 * @self: a #GtkDropDown
 *
 * Gets the factory that's currently used to populate list items in the popup.
 *
 * Returns: (nullable) (transfer none): The factory in use
 **/
GtkListItemFactory *
gtk_entry_completion_get_factory (GtkEntryCompletion *completion)
{
  g_return_val_if_fail (GTK_IS_ENTRY_COMPLETION (completion), NULL);

  return completion->factory;
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

  if (completion->minimum_key_length != length)
    {
      completion->minimum_key_length = length;

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

  return completion->minimum_key_length;
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
  GtkFilter *filter;
  const char *text;

  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (completion));
  g_return_if_fail (GTK_IS_ENTRY (completion->entry));

  filter = gtk_filter_list_model_get_filter (GTK_FILTER_LIST_MODEL (completion->filter_model));
  if (!filter)
    return;

  text = gtk_editable_get_text (GTK_EDITABLE (completion->entry));
  gtk_string_filter_set_search (GTK_STRING_FILTER (filter), text);

  if (g_list_model_get_n_items (completion->filter_model) == 0)
    g_signal_emit (completion, entry_completion_signals[NO_MATCHES], 0);

  if (gtk_widget_get_visible (completion->popup_window))
    gtk_entry_completion_resize_popup (completion);
}

/* private */

/* some nasty size requisition */
void
gtk_entry_completion_resize_popup (GtkEntryCompletion *completion)
{
  GtkAllocation allocation;
  GtkRequisition entry_req;
  GtkRequisition list_req;
  int width;

  if (!gtk_native_get_surface (gtk_widget_get_native (completion->entry)))
    return;

  gtk_widget_get_surface_allocation (completion->entry, &allocation);
  gtk_widget_get_preferred_size (completion->entry, &entry_req, NULL);

  /* Call get preferred size on the on the tree view to force it to validate its
   * cells before calling into the cell size functions.
   */
  gtk_widget_get_preferred_size (completion->list_view, &list_req, NULL);
  gtk_widget_realize (completion->list_view);

  if (completion->popup_set_width)
    width = allocation.width;
  else
    width = -1;

  gtk_scrolled_window_set_min_content_width (GTK_SCROLLED_WINDOW (completion->scrolled_window), width);
  gtk_widget_set_size_request (completion->popup_window, width, -1);
  gtk_native_check_resize (GTK_NATIVE (completion->popup_window));
}

static void
gtk_entry_completion_popup (GtkEntryCompletion *completion)
{
  GtkText *text = gtk_entry_get_text_widget (GTK_ENTRY (completion->entry));

  if (gtk_widget_get_mapped (completion->popup_window))
    return;

  if (!gtk_widget_get_mapped (GTK_WIDGET (text)))
    return;

  if (!gtk_widget_has_focus (GTK_WIDGET (text)))
    return;

  /* default on no match */
  completion->current_selected = -1;

  gtk_widget_realize (completion->popup_window);

  gtk_entry_completion_resize_popup (completion);

  gtk_popover_popup (GTK_POPOVER (completion->popup_window));
}

void
gtk_entry_completion_popdown (GtkEntryCompletion *completion)
{
  if (!gtk_widget_get_mapped (completion->popup_window))
    return;

  gtk_popover_popdown (GTK_POPOVER (completion->popup_window));
}

static gboolean
gtk_entry_completion_get_text (GtkEntryCompletion *completion,
                               guint               position,
                               GValue             *value)
{
  gpointer item;

  item = g_list_model_get_item (completion->filter_model, position);

  if (completion->expression == NULL ||
      !gtk_expression_evaluate (completion->expression, item, value))
    {
      g_object_unref (item);
      return FALSE;
    }

  g_object_unref (item);
  return TRUE;
}

static gboolean
gtk_entry_completion_match_selected (GtkEntryCompletion *completion,
                                     guint               position)
{
  GValue value = G_VALUE_INIT;

  if (!gtk_entry_completion_get_text (completion, position, &value))
    return FALSE;

  gtk_editable_set_text (GTK_EDITABLE (completion->entry),
                         g_value_get_string (&value));
  gtk_editable_set_position (GTK_EDITABLE (completion->entry), -1);

  g_value_unset (&value);

  return TRUE;
}

static gboolean
gtk_entry_completion_cursor_on_match (GtkEntryCompletion *completion,
                                      guint               position)
{
  gtk_entry_completion_insert_completion (completion, position);
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
  char *prefix = NULL;
  guint i, n;

  n = g_list_model_get_n_items (completion->filter_model);
  for (i = 0; i < n; i++)
    {
      gpointer item = g_list_model_get_item (completion->filter_model, i);
      GValue value = G_VALUE_INIT;
      const char *text;

      if (completion->expression &&
          gtk_expression_evaluate (completion->expression, item, &value))
        {
          text = g_value_get_string (&value);
        }
      else if (GTK_IS_STRING_OBJECT (item))
        {
          text = gtk_string_object_get_string (GTK_STRING_OBJECT (item));
        }
      else
        {
          g_object_unref (item);
          continue;
        }

      if (text && g_str_has_prefix (text, key))
        {
          if (!prefix)
            prefix = g_strdup (text);
          else
            {
              char *p = prefix;
              char *q = (char *)text;

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
                      break;
                    default: ;
                    }
                }
            }
        }

      g_value_unset (&value);
      g_object_unref (item);
    }

  return prefix;
}


static gboolean
gtk_entry_completion_real_insert_prefix (GtkEntryCompletion *completion,
                                         const char         *prefix)
{
  if (prefix)
    {
      gint key_len;
      gint prefix_len;
      const char *key;

      prefix_len = g_utf8_strlen (prefix, -1);

      key = gtk_editable_get_text (GTK_EDITABLE (completion->entry));
      key_len = g_utf8_strlen (key, -1);

      if (prefix_len > key_len)
        {
          gint pos = prefix_len;

          gtk_editable_insert_text (GTK_EDITABLE (completion->entry),
                                    prefix + strlen (key), -1, &pos);
          gtk_editable_select_region (GTK_EDITABLE (completion->entry),
                                      key_len, prefix_len);

          completion->has_completion = TRUE;
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

  return completion->completion_prefix;
}

static gboolean
gtk_entry_completion_insert_completion (GtkEntryCompletion *completion,
                                        guint               position)
{
  GValue value = G_VALUE_INIT;
  int len;
  GtkText *text = gtk_entry_get_text_widget (GTK_ENTRY (completion->entry));
  GtkEntryBuffer *buffer = gtk_text_get_buffer (text);

  if (!gtk_entry_completion_get_text (completion, position, &value))
    return FALSE;

  if (completion->changed_id > 0)
    g_signal_handler_block (text, completion->changed_id);

  if (completion->insert_text_id > 0)
    g_signal_handler_block (buffer, completion->insert_text_id);

  gtk_editable_set_text (GTK_EDITABLE (completion->entry), g_value_get_string (&value));

  len = g_utf8_strlen (completion->completion_prefix, -1);
  gtk_editable_select_region (GTK_EDITABLE (completion->entry), len, -1);

  if (completion->changed_id > 0)
    g_signal_handler_unblock (text, completion->changed_id);

  if (completion->insert_text_id > 0)
    g_signal_handler_unblock (buffer, completion->insert_text_id);

  g_value_unset (&value);

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
  GtkText *text = gtk_entry_get_text_widget (GTK_ENTRY (completion->entry));
  GtkEntryBuffer *buffer = gtk_text_get_buffer (text);

  if (completion->insert_text_id > 0)
    g_signal_handler_block (buffer, completion->insert_text_id);

  prefix = gtk_entry_completion_compute_prefix (completion,
                                                gtk_editable_get_text (GTK_EDITABLE (completion->entry)));

  if (prefix)
    {
      g_signal_emit (completion, entry_completion_signals[INSERT_PREFIX],
                     0, prefix, &done);
      g_free (prefix);
    }

  if (completion->insert_text_id > 0)
    g_signal_handler_unblock (buffer, completion->insert_text_id);
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

  if (completion->inline_completion == inline_completion)
    return;

  completion->inline_completion = inline_completion;

  g_object_notify_by_pspec (G_OBJECT (completion), entry_completion_props[PROP_INLINE_COMPLETION]);
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

  return completion->inline_completion;
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

  if (completion->popup_completion == popup_completion)
    return;

  completion->popup_completion = popup_completion;

  g_object_notify_by_pspec (G_OBJECT (completion), entry_completion_props[PROP_POPUP_COMPLETION]);
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

  return completion->popup_completion;
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

  if (completion->popup_set_width != popup_set_width)
    {
      completion->popup_set_width = popup_set_width;

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

  return completion->popup_set_width;
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

  if (completion->popup_single_match == popup_single_match)
    return;

  completion->popup_single_match = popup_single_match;

  g_object_notify_by_pspec (G_OBJECT (completion), entry_completion_props[PROP_POPUP_SINGLE_MATCH]);
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

  return completion->popup_single_match;
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

  if (completion->inline_selection == inline_selection)
    return;

  completion->inline_selection = inline_selection;

  g_object_notify_by_pspec (G_OBJECT (completion), entry_completion_props[PROP_INLINE_SELECTION]);
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

  return completion->inline_selection;
}


static gint
gtk_entry_completion_timeout (gpointer data)
{
  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (data);
  const char *text;

  completion->completion_timeout = 0;

  text = gtk_editable_get_text (GTK_EDITABLE (completion->entry));
  if (completion->filter_model &&
      g_utf8_strlen (text, -1) >= completion->minimum_key_length)
    {
      int matches;
      gboolean popup_single;

      gtk_entry_completion_complete (completion);
      matches = g_list_model_get_n_items (completion->filter_model);
      gtk_selection_model_unselect_all (GTK_SELECTION_MODEL (gtk_list_view_get_model (GTK_LIST_VIEW (completion->list_view))));

      g_object_get (completion, "popup-single-match", &popup_single, NULL);
      if ((completion->popup_single_match && matches > 0) || matches > 1)
        {
          if (gtk_widget_get_visible (completion->popup_window))
            gtk_entry_completion_resize_popup (completion);
          else
            gtk_entry_completion_popup (completion);
        }
      else
        gtk_entry_completion_popdown (completion);
    }
  else if (gtk_widget_get_visible (completion->popup_window))
    gtk_entry_completion_popdown (completion);

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
  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (user_data);
  GtkWidget *widget = completion->entry;
  int matches;

  if (!completion->popup_completion)
    return FALSE;

  if (keyval == GDK_KEY_Return ||
      keyval == GDK_KEY_KP_Enter ||
      keyval == GDK_KEY_ISO_Enter ||
      keyval == GDK_KEY_Escape)
    {
      if (completion->completion_timeout)
        {
          g_source_remove (completion->completion_timeout);
          completion->completion_timeout = 0;
        }
    }

  if (!gtk_widget_get_mapped (completion->popup_window))
    return FALSE;

  matches = g_list_model_get_n_items (completion->filter_model);

  if (keyval_is_cursor_move (keyval))
    {
      if (keyval == GDK_KEY_Up || keyval == GDK_KEY_KP_Up)
        {
          if (completion->current_selected < 0)
            completion->current_selected = matches - 1;
          else
            completion->current_selected--;
        }
      else if (keyval == GDK_KEY_Down || keyval == GDK_KEY_KP_Down)
        {
          if (completion->current_selected < matches - 1)
            completion->current_selected++;
          else
            completion->current_selected = -1;
        }
      else if (keyval == GDK_KEY_Page_Up)
        {
          if (completion->current_selected < 0)
            completion->current_selected = matches - 1;
          else if (completion->current_selected == 0)
            completion->current_selected = -1;
          else if (completion->current_selected < matches)
            {
              completion->current_selected -= PAGE_STEP;
              if (completion->current_selected < 0)
                completion->current_selected = 0;
            }
          else
            {
              completion->current_selected -= PAGE_STEP;
              if (completion->current_selected < matches - 1)
                completion->current_selected = matches - 1;
            }
        }
      else if (keyval == GDK_KEY_Page_Down)
        {
          if (completion->current_selected < 0)
            completion->current_selected = 0;
          else if (completion->current_selected < matches - 1)
            {
              completion->current_selected += PAGE_STEP;
              if (completion->current_selected > matches - 1)
                completion->current_selected = matches - 1;
            }
          else if (completion->current_selected == matches - 1)
            {
              completion->current_selected = -1;
            }
          else
            {
              completion->current_selected += PAGE_STEP;
              if (completion->current_selected > matches - 1)
                completion->current_selected = matches - 1;
            }
        }

      if (completion->current_selected < 0)
        {
          gtk_selection_model_unselect_all (GTK_SELECTION_MODEL (gtk_list_view_get_model (GTK_LIST_VIEW (completion->list_view))));

          if (completion->inline_selection &&
              completion->completion_prefix)
            {
              gtk_editable_set_text (GTK_EDITABLE (completion->entry),
                                     completion->completion_prefix);
              gtk_editable_set_position (GTK_EDITABLE (widget), -1);
            }
        }
      else if (completion->current_selected < matches)
        {
          gtk_selection_model_select_item (GTK_SELECTION_MODEL (gtk_list_view_get_model (GTK_LIST_VIEW (completion->list_view))),
                                           completion->current_selected,
                                           TRUE);

          if (completion->inline_selection)
            {
              gpointer obj = gtk_single_selection_get_selected_item (GTK_SINGLE_SELECTION (gtk_list_view_get_model (GTK_LIST_VIEW (completion->list_view))));

              gboolean entry_set;

              if (obj == NULL)
                return FALSE;

              if (completion->completion_prefix == NULL)
                completion->completion_prefix = g_strdup (gtk_editable_get_text (GTK_EDITABLE (completion->entry)));

              g_signal_emit_by_name (completion, "cursor-on-match",
                                     completion->current_selected, &entry_set);
            }
        }
      else if (completion->current_selected - matches >= 0)
        {
          gtk_selection_model_select_item (GTK_SELECTION_MODEL (gtk_list_view_get_model (GTK_LIST_VIEW (completion->list_view))),
                                           completion->current_selected - matches,
                                           TRUE);

          if (completion->inline_selection &&
              completion->completion_prefix)
            {
              gtk_editable_set_text (GTK_EDITABLE (completion->entry),
                                     completion->completion_prefix);
              gtk_editable_set_position (GTK_EDITABLE (widget), -1);
            }
        }

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
      gtk_entry_completion_popdown (completion);

      if (completion->current_selected < 0)
        {
          retval = FALSE;
          goto keypress_completion_out;
        }
      else if (completion->inline_selection)
        {
          /* Escape rejects the tentative completion */
          if (keyval == GDK_KEY_Escape)
            {
              if (completion->completion_prefix)
                gtk_editable_set_text (GTK_EDITABLE (completion->entry),
                                       completion->completion_prefix);
              else
                gtk_editable_set_text (GTK_EDITABLE (completion->entry), "");
            }

          /* Move the cursor to the end for Right/Esc */
          if (keyval == GDK_KEY_Right ||
              keyval == GDK_KEY_KP_Right ||
              keyval == GDK_KEY_Escape)
            gtk_editable_set_position (GTK_EDITABLE (widget), -1);
          /* Let the default keybindings run for Left, i.e. either move
           * to the previous character or select word if a modifier is used
           */
          else
            retval = FALSE;
        }

keypress_completion_out:
      if (completion->inline_selection)
        g_clear_pointer (&completion->completion_prefix, g_free);

      return retval;
    }
  else if (keyval == GDK_KEY_Tab ||
           keyval == GDK_KEY_KP_Tab ||
           keyval == GDK_KEY_ISO_Left_Tab)
    {
      gtk_entry_reset_im_context (GTK_ENTRY (widget));
      gtk_entry_completion_popdown (completion);

      g_clear_pointer (&completion->completion_prefix, g_free);

      return FALSE;
    }
  else if (keyval == GDK_KEY_ISO_Enter ||
           keyval == GDK_KEY_KP_Enter ||
           keyval == GDK_KEY_Return)
    {
      gboolean retval = TRUE;

      gtk_entry_reset_im_context (GTK_ENTRY (widget));
      gtk_entry_completion_popdown (completion);

      if (completion->current_selected < matches)
        {
          gpointer obj = gtk_single_selection_get_selected_item (GTK_SINGLE_SELECTION (gtk_list_view_get_model (GTK_LIST_VIEW (completion->list_view))));
          GValue value = G_VALUE_INIT;

          if (completion->expression &&
              gtk_expression_evaluate (completion->expression, obj, &value))
            {
              GtkText *text = gtk_entry_get_text_widget (GTK_ENTRY (completion->entry));
              gboolean entry_set;

              g_signal_handler_block (text, completion->changed_id);
              g_signal_emit_by_name (completion, "match-selected",
                                     completion->current_selected, &entry_set);
              g_signal_handler_unblock (text, completion->changed_id);

              if (!entry_set)
                {
                  gtk_editable_set_text (GTK_EDITABLE (widget), g_value_get_string (&value));
                  gtk_editable_set_position (GTK_EDITABLE (widget), -1);
                }
            }
          else
            retval = FALSE;
        }

      g_clear_pointer (&completion->completion_prefix, g_free);

      return retval;
    }

  g_clear_pointer (&completion->completion_prefix, g_free);

  return FALSE;
}

static void
gtk_entry_completion_changed (GtkWidget *widget,
                              gpointer   user_data)
{
  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (user_data);
  const char *text;

  if (!completion->popup_completion)
    return;

  /* (re)install completion timeout */
  if (completion->completion_timeout)
    {
      g_source_remove (completion->completion_timeout);
      completion->completion_timeout = 0;
    }

  text = gtk_editable_get_text (GTK_EDITABLE (widget));
  if (!text)
    return;

  /* no need to normalize for this test */
  if (completion->minimum_key_length > 0 &&
      strcmp ("", text) == 0)
    {
      if (gtk_widget_get_visible (completion->popup_window))
        gtk_entry_completion_popdown (completion);
      return;
    }

  completion->completion_timeout = g_timeout_add (COMPLETION_TIMEOUT,
                                                  gtk_entry_completion_timeout,
                                                  completion);
  g_source_set_name_by_id (completion->completion_timeout,
                           "[gtk] gtk_entry_completion_timeout");
}

static gboolean
check_completion_callback (GtkEntryCompletion *completion)
{
  completion->check_completion_idle = NULL;

  gtk_entry_completion_complete (completion);
  gtk_entry_completion_insert_prefix (completion);

  return FALSE;
}

static void
clear_completion_callback (GObject            *text,
                           GParamSpec         *pspec,
                           GtkEntryCompletion *completion)
{
  if (!completion->inline_completion)
    return;

  if (pspec->name == I_("cursor-position") ||
      pspec->name == I_("selection-bound"))
    completion->has_completion = FALSE;
}

static gboolean
accept_completion_callback (GtkEntryCompletion *completion)
{
  if (!completion->inline_completion)
    return FALSE;

  if (completion->has_completion)
    gtk_editable_set_position (GTK_EDITABLE (completion->entry),
                               gtk_entry_buffer_get_length (gtk_entry_get_buffer (GTK_ENTRY (completion->entry))));

  return FALSE;
}

static void
text_focus_out (GtkEntryCompletion *completion)
{
  if (!gtk_widget_get_mapped (completion->popup_window))
    accept_completion_callback (completion);
}

static void
completion_inserted_text_callback (GtkEntryBuffer     *buffer,
                                   guint               position,
                                   const char         *text,
                                   guint               length,
                                   GtkEntryCompletion *completion)
{
  if (!completion->inline_completion)
    return;

  /* idle to update the selection based on the file list */
  if (completion->check_completion_idle == NULL)
    {
      completion->check_completion_idle = g_idle_source_new ();
      g_source_set_priority (completion->check_completion_idle, G_PRIORITY_HIGH);
      g_source_set_closure (completion->check_completion_idle,
                            g_cclosure_new_object (G_CALLBACK (check_completion_callback),
                                                   G_OBJECT (completion)));
      g_source_attach (completion->check_completion_idle, NULL);
      g_source_set_name (completion->check_completion_idle, "[gtk] check_completion_callback");
    }
}

static void
connect_completion_signals (GtkEntryCompletion *completion)
{
  GtkEventController *controller;
  GtkText *text = gtk_entry_get_text_widget (GTK_ENTRY (completion->entry));
  GtkEntryBuffer *buffer = gtk_text_get_buffer (text);

  controller = completion->entry_key_controller = gtk_event_controller_key_new ();
  gtk_event_controller_set_name (controller, "gtk-entry-completion");
  g_signal_connect (controller, "key-pressed",
                    G_CALLBACK (gtk_entry_completion_key_pressed), completion);
  gtk_widget_prepend_controller (GTK_WIDGET (text), controller);
  controller = completion->entry_focus_controller = gtk_event_controller_focus_new ();
  gtk_event_controller_set_name (controller, "gtk-entry-completion");
  g_signal_connect_swapped (controller, "leave", G_CALLBACK (text_focus_out), completion);
  gtk_widget_add_controller (GTK_WIDGET (text), controller);

  completion->changed_id =
    g_signal_connect (text, "changed", G_CALLBACK (gtk_entry_completion_changed), completion);

  completion->insert_text_id =
      g_signal_connect (buffer, "inserted-text", G_CALLBACK (completion_inserted_text_callback), completion);
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
  GtkText *text = gtk_entry_get_text_widget (GTK_ENTRY (completion->entry));
  GtkEntryBuffer *buffer = gtk_text_get_buffer (text);

  gtk_widget_remove_controller (GTK_WIDGET (text), completion->entry_key_controller);
  gtk_widget_remove_controller (GTK_WIDGET (text), completion->entry_focus_controller);

  if (completion->changed_id > 0 &&
      g_signal_handler_is_connected (text, completion->changed_id))
    {
      g_signal_handler_disconnect (text, completion->changed_id);
      completion->changed_id = 0;
    }
  if (completion->insert_text_id > 0 &&
      g_signal_handler_is_connected (buffer, completion->insert_text_id))
    {
      g_signal_handler_disconnect (buffer, completion->insert_text_id);
      completion->insert_text_id = 0;
    }
  g_signal_handlers_disconnect_by_func (text, G_CALLBACK (clear_completion_callback), completion);
  g_signal_handlers_disconnect_by_func (text, G_CALLBACK (accept_completion_callback), completion);
}

void
gtk_entry_completion_disconnect (GtkEntryCompletion *completion)
{
  if (completion->completion_timeout)
    {
      g_source_remove (completion->completion_timeout);
      completion->completion_timeout = 0;
    }
  if (completion->check_completion_idle)
    {
      g_source_destroy (completion->check_completion_idle);
      completion->check_completion_idle = NULL;
    }

  if (gtk_widget_get_mapped (completion->popup_window))
    gtk_entry_completion_popdown (completion);

  disconnect_completion_signals (completion);
  unset_accessible_relation (completion->popup_window, completion->entry);
  gtk_widget_unparent (completion->popup_window);
  completion->entry = NULL;
}

void
gtk_entry_completion_connect (GtkEntryCompletion *completion,
                              GtkEntry           *entry)
{
  completion->entry = GTK_WIDGET (entry);
  set_accessible_relation (completion->popup_window, completion->entry);
  gtk_widget_set_parent (completion->popup_window, GTK_WIDGET (entry));
  connect_completion_signals (completion);
}
