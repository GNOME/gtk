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

#include "gtksuggestionentry.h"

#include "gtkintl.h"
#include "gtklistview.h"
#include "gtklistitemfactory.h"
#include "gtksignallistitemfactory.h"
#include "gtkpopover.h"
#include "gtkprivate.h"
#include "gtksingleselection.h"
#include "gtkfilterlistmodel.h"
#include "gtkstringfilter.h"
#include "gtkmultifilter.h"
#include "gtkwidgetprivate.h"
#include "gtknative.h"
#include "gtktogglebutton.h"
#include "gtkexpression.h"
#include "gtklabel.h"
#include "gtklistitem.h"
#include "gtkbuildable.h"
#include "gtkstringlist.h"
#include "gtkboxlayout.h"
#include "gtktext.h"
#include "gtkscrolledwindow.h"
#include "gtkeventcontrollerkey.h"
#include "gtkeventcontrollerfocus.h"
#include "gtkbox.h"
#include "gtkgizmoprivate.h"
#include "gtkactionable.h"


/**
 * SECTION:gtksuggestionentry:
 * @Title: GtkSuggestionEntry
 * @Short_description: An entry with a completion popup
 * @See_also: #GtkComboBoxText, GtkEntryCompletion, #GtkDropDown
 *
 * GtkSuggestionEntry is an entry that allows the user to enter
 * a string manually, or choose from a list of suggestions.
 *
 * The options are given to GtkSuggestionEntry in the form of #GListModel,
 * and how the individual options are represented is determined by
 * a #GtkListItemFactory. The default factory displays simple strings,
 * and expects to obtain these from the model by evaluating an expression
 * that has to be provided via gtk_suggestion_entry_set_expression().
 *
 * There are some variations in the way GtkSuggestionEntry can handle
 * the suggestions in the model, controlled by a few properties. If
 * #GtkSuggestionEntry:use-filter is set to %FALSE, the popup will not
 * be filtered against the entry contents, and will always show all
 * suggestions (unless you do your own filtering). The filtering that
 * is done by GtkSuggestionEntry if use-filter is %TRUE is case-insensitive
 * and matches a prefix of the strings returned by the expression given
 * in #GtkSuggestionEntry:expression.
 *
 * If #GtkSuggestionEntry:insert-prefix is set to %TRUE,
 * GtkSuggestionEntry will automatically inserted the longest common
 * prefix of all matching suggestions into the entry as you type. You
 * probably want to turn this off when you are doing your own filtering,
 * since it makes the assumption that the filtering is doing prefix
 * matching.
 *
 * If #GtkSuggestionEntry:insert-selection is set to %TRUE,
 * GtkSuggestionEntry will automatically insert the current selection
 * from the popup into the entry.
 *
 * # CSS Nodes
 *
 * |[<!-- language="plain" -->
 * widget
 * ╰── box
 *     ├── entry.suggestion
 *     │   ├── text
 *     │   ╰── popover
 *     ╰── [button]
 * ]|
 *
 * GtkSuggestionEntry has a single CSS node with name entry that carries
 * a .sugggestion style class, and the text and popover nodes are children
 * of that. The parent of the entry node is a box node, which also contains
 * the CSS node for the button (which may be hidden). The parent of the box
 * node is a widget node.
 */

struct _GtkSuggestionEntry
{
  GtkWidget parent_instance;

  GListModel *model;
  GtkListItemFactory *factory;
  GtkExpression *expression;

  GtkFilterListModel *filter_model;
  GtkSingleSelection *selection;

  GtkWidget *box;
  GtkWidget *gizmo;
  GtkWidget *button;
  GtkWidget *entry;
  GtkWidget *popup;
  GtkWidget *list;

  char *search;
  char *prefix;

  gulong changed_id;

  guint minimum_length;

  guint use_filter       : 1;
  guint insert_selection : 1;
  guint insert_prefix    : 1;
  guint show_button      : 1;
};

typedef struct _GtkSuggestionEntryClass GtkSuggestionEntryClass;

struct _GtkSuggestionEntryClass
{
  GtkWidgetClass parent_class;
};

enum
{
  PROP_0,
  PROP_MODEL,
  PROP_FACTORY,
  PROP_EXPRESSION,
  PROP_PLACEHOLDER_TEXT,
  PROP_POPUP_VISIBLE,
  PROP_USE_FILTER,
  PROP_INSERT_PREFIX,
  PROP_INSERT_SELECTION,
  PROP_MINIMUM_LENGTH,
  PROP_SHOW_BUTTON,

  N_PROPERTIES,
};

static void gtk_suggestion_entry_set_popup_visible (GtkSuggestionEntry *self,
                                                    gboolean            visible);

static GtkEditable *
gtk_suggestion_entry_get_delegate (GtkEditable *editable)
{
  return GTK_EDITABLE (GTK_SUGGESTION_ENTRY (editable)->entry);
}

static void
gtk_suggestion_entry_editable_init (GtkEditableInterface *iface)
{
  iface->get_delegate = gtk_suggestion_entry_get_delegate;
}

G_DEFINE_TYPE_WITH_CODE (GtkSuggestionEntry, gtk_suggestion_entry, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE,
                                                gtk_suggestion_entry_editable_init))

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static void
gtk_suggestion_entry_dispose (GObject *object)
{
  GtkSuggestionEntry *self = GTK_SUGGESTION_ENTRY (object);

  if (self->changed_id)
    {
      g_signal_handler_disconnect (self->entry, self->changed_id);
      self->changed_id = 0;
    }
  g_clear_pointer (&self->box, gtk_widget_unparent);

  g_clear_pointer (&self->expression, gtk_expression_unref);
  g_clear_object (&self->factory);

  g_clear_object (&self->model);
  g_clear_object (&self->filter_model);
  g_clear_object (&self->selection);

  g_clear_pointer (&self->search, g_free);
  g_clear_pointer (&self->prefix, g_free);

  G_OBJECT_CLASS (gtk_suggestion_entry_parent_class)->dispose (object);
}

static void
gtk_suggestion_entry_get_property (GObject    *object,
                                   guint       property_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GtkSuggestionEntry *self = GTK_SUGGESTION_ENTRY (object);

  if (gtk_editable_delegate_get_property (object, property_id, value, pspec))
    return;

  switch (property_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, gtk_suggestion_entry_get_model (self));
      break;

    case PROP_FACTORY:
      g_value_set_object (value, gtk_suggestion_entry_get_factory (self));
      break;

    case PROP_EXPRESSION:
      gtk_value_set_expression (value, gtk_suggestion_entry_get_expression (self));
      break;

    case PROP_PLACEHOLDER_TEXT:
      g_value_set_string (value, gtk_text_get_placeholder_text (GTK_TEXT (self->entry)));
      break;

    case PROP_POPUP_VISIBLE:
      g_value_set_boolean (value, self->popup && gtk_widget_get_visible (self->popup));
      break;

    case PROP_USE_FILTER:
      g_value_set_boolean (value, gtk_suggestion_entry_get_use_filter (self));
      break;

    case PROP_INSERT_SELECTION:
      g_value_set_boolean (value, gtk_suggestion_entry_get_insert_selection (self));
      break;

    case PROP_INSERT_PREFIX:
      g_value_set_boolean (value, gtk_suggestion_entry_get_insert_prefix (self));
      break;

    case PROP_MINIMUM_LENGTH:
      g_value_set_uint (value, gtk_suggestion_entry_get_minimum_length (self));
      break;

    case PROP_SHOW_BUTTON:
      g_value_set_boolean (value, gtk_suggestion_entry_get_show_button (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_suggestion_entry_set_property (GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GtkSuggestionEntry *self = GTK_SUGGESTION_ENTRY (object);

  if (gtk_editable_delegate_set_property (object, property_id, value, pspec))
    return;

  switch (property_id)
    {
    case PROP_MODEL:
      gtk_suggestion_entry_set_model (self, g_value_get_object (value));
      break;

    case PROP_FACTORY:
      gtk_suggestion_entry_set_factory (self, g_value_get_object (value));
      break;

    case PROP_EXPRESSION:
      gtk_suggestion_entry_set_expression (self, gtk_value_get_expression (value));
      break;

    case PROP_PLACEHOLDER_TEXT:
      gtk_text_set_placeholder_text (GTK_TEXT (self->entry), g_value_get_string (value));
      break;

    case PROP_POPUP_VISIBLE:
      gtk_suggestion_entry_set_popup_visible (self, g_value_get_boolean (value));
      break;

    case PROP_USE_FILTER:
      gtk_suggestion_entry_set_use_filter (self, g_value_get_boolean (value));
      break;

    case PROP_INSERT_SELECTION:
      gtk_suggestion_entry_set_insert_selection (self, g_value_get_boolean (value));
      break;

    case PROP_INSERT_PREFIX:
      gtk_suggestion_entry_set_insert_prefix (self, g_value_get_boolean (value));
      break;

    case PROP_MINIMUM_LENGTH:
      gtk_suggestion_entry_set_minimum_length (self, g_value_get_uint (value));
      break;

    case PROP_SHOW_BUTTON:
      gtk_suggestion_entry_set_show_button (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_suggestion_entry_measure (GtkWidget      *widget,
                              GtkOrientation  orientation,
                              int             size,
                              int            *minimum,
                              int            *natural,
                              int            *minimum_baseline,
                              int            *natural_baseline)
{
  GtkSuggestionEntry *self = GTK_SUGGESTION_ENTRY (widget);

  gtk_widget_measure (self->box,
                      orientation,
                      size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);
}

static void
gtk_suggestion_entry_size_allocate (GtkWidget *widget,
                                    int        width,
                                    int        height,
                                    int        baseline)
{
  GtkSuggestionEntry *self = GTK_SUGGESTION_ENTRY (widget);

  gtk_widget_size_allocate (self->box, &(GtkAllocation) { 0, 0, width, height }, baseline);

}

static gboolean
gtk_suggestion_entry_grab_focus (GtkWidget *widget)
{
  GtkSuggestionEntry *self = GTK_SUGGESTION_ENTRY (widget);

  return gtk_widget_grab_focus (self->entry);
}

static void
gtk_suggestion_entry_class_init (GtkSuggestionEntryClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_suggestion_entry_dispose;
  object_class->get_property = gtk_suggestion_entry_get_property;
  object_class->set_property = gtk_suggestion_entry_set_property;

  widget_class->measure = gtk_suggestion_entry_measure;
  widget_class->size_allocate = gtk_suggestion_entry_size_allocate;
  widget_class->focus = gtk_widget_focus_child;
  widget_class->grab_focus = gtk_suggestion_entry_grab_focus;

  /**
   * GtkSuggestionEntry:model:
   *
   * Model for the displayed items.
   */
  properties[PROP_MODEL] =
    g_param_spec_object ("model",
                         P_("Model"),
                         P_("Model for the displayed items"),
                         G_TYPE_LIST_MODEL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkSuggestionEntry:factory:
   *
   * Factory for populating list items.
   */
  properties[PROP_FACTORY] =
    g_param_spec_object ("factory",
                         P_("Factory"),
                         P_("Factory for populating list items"),
                         GTK_TYPE_LIST_ITEM_FACTORY,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkSuggestionEntry:expression: (type GtkExpression)
   *
   * An expression to evaluate to obtain strings to match against the text
   * in the entry. If #GtkSuggestionEntry:factory is not set, the expression
   * is also used to bind strings to labels produced by a default factory.
   */
  properties[PROP_EXPRESSION] =
    gtk_param_spec_expression ("expression",
                               P_("Expression"),
                               P_("Expression to determine strings to search for"),
                               G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_PLACEHOLDER_TEXT] =
      g_param_spec_string ("placeholder-text",
                           P_("Placeholder text"),
                           P_("Show text in the entry when it’s empty and unfocused"),
                           NULL,
                           G_PARAM_READWRITE);

  /**
   * GtkSuggestionEntry:popup-visible:
   *
   * %TRUE if the popup with suggestions is currently visible.
   */
  properties[PROP_POPUP_VISIBLE] =
      g_param_spec_boolean ("popup-visible",
                            P_("Popup visible"),
                            P_("Whether the popup with suggestions is currently visible"),
                            FALSE,
                            G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkSuggestionEntry:use-filter:
   *
   * Whether to filter the suggestions by matching them against the
   * current text in the entry.
   */
  properties[PROP_USE_FILTER] =
      g_param_spec_boolean ("use-filter",
                            P_("Use filter"),
                            P_("Whether to filter the list for matches"),
                            TRUE,
                            G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkSuggestionEntry:insert-selection:
   *
   * Determines whether the currently selected items should be
   * inserted automatically in the entry.
   **/
  properties[PROP_INSERT_SELECTION] =
      g_param_spec_boolean ("insert-selection",
                            P_("Insert selection"),
                            P_("Whether to insert the selected item automatically"),
                            FALSE,
                            G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkSuggestionEntry:insert-prefix:
   *
   * Determines whether the common prefix of the matching suggestions
   * should be inserted automatically in the entry. If filtering is
   * not used, this property has no effect.
   **/
  properties[PROP_INSERT_PREFIX] =
      g_param_spec_boolean ("insert-prefix",
                            P_("Insert prefix"),
                            P_("Whether to insert the common prefix automatically"),
                            FALSE,
                            G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_SHOW_BUTTON] =
      g_param_spec_boolean ("show-button",
                            P_("Show button"),
                            P_("Whether to show a button for presenting the popup"),
                            FALSE,
                            G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_MINIMUM_LENGTH] =
      g_param_spec_uint ("minimum-length",
                         P_("Minimum Length"),
                         P_("Minimum length for matches when filtering"),
                         0, G_MAXUINT, 1,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
  gtk_editable_install_properties (object_class, N_PROPERTIES);

  /**
   * GtkSuggestionEntry|popup.show:
   *
   * Toggles the #GtkSuggestionEntry:popup-visible property
   * and thereby shows or hides the popup with suggestions.
   */
  gtk_widget_class_install_property_action (widget_class, "popup.show", "popup-visible");

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Down, GDK_ALT_MASK,
                                       "popup.show", NULL);
}

static void
setup_item (GtkSignalListItemFactory *factory,
            GtkListItem              *list_item,
            gpointer                  data)
{
  GtkWidget *label;

  label = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_list_item_set_child (list_item, label);
}

static void
bind_item (GtkSignalListItemFactory *factory,
           GtkListItem              *list_item,
           gpointer                  data)
{
  GtkSuggestionEntry *self = data;
  gpointer item;
  GtkWidget *label;
  GValue value = G_VALUE_INIT;

  item = gtk_list_item_get_item (list_item);
  label = gtk_list_item_get_child (list_item);

  if (self->expression)
    {
      gtk_expression_evaluate (self->expression, item, &value);
    }
  else if (GTK_IS_STRING_OBJECT (item))
    {
      g_object_get_property (G_OBJECT (item), "string", &value);
    }
  else
    {
      g_critical ("Either GtkSuggestionEntry:factory or GtkSuggestionEntry:expression "
                  "must be set");
      g_value_set_string (&value, "No value");
    }

  gtk_label_set_label (GTK_LABEL (label), g_value_get_string (&value));
  g_value_unset (&value);
}

static void
gtk_suggestion_entry_set_popup_visible (GtkSuggestionEntry *self,
                                        gboolean            visible)
{
  if (gtk_widget_get_visible (self->popup) == visible)
    return;

  if (visible)
    {
      if (!gtk_widget_has_focus (self->entry))
        gtk_text_grab_focus_without_selecting (GTK_TEXT (self->entry));

      gtk_single_selection_set_selected (self->selection, GTK_INVALID_LIST_POSITION);
      gtk_popover_popup (GTK_POPOVER (self->popup));
    }
  else
    {
      gtk_popover_popdown (GTK_POPOVER (self->popup));
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_POPUP_VISIBLE]);
}

static gboolean
text_changed_idle (gpointer data)
{
  GtkSuggestionEntry *self = data;
  const char *text;
  glong len;
  guint matches;
  GtkFilter *filter;

  if (!self->filter_model)
    return G_SOURCE_REMOVE;

  text = gtk_editable_get_text (GTK_EDITABLE (self->entry));
  len = g_utf8_strlen (text, -1);

  g_free (self->search);
  self->search = g_strdup (text);

  filter = gtk_filter_list_model_get_filter (self->filter_model);
  if (GTK_IS_STRING_FILTER (filter))
    gtk_string_filter_set_search (GTK_STRING_FILTER (filter), self->search);

  matches = g_list_model_get_n_items (G_LIST_MODEL (self->filter_model));

  if (len < self->minimum_length)
    gtk_suggestion_entry_set_popup_visible (self, FALSE);
  else
    gtk_suggestion_entry_set_popup_visible (self, matches > 0);

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "popup.show", matches > 0);

  return G_SOURCE_REMOVE;
}

static void
text_changed (GtkEditable        *editable,
              GParamSpec         *pspec,
              GtkSuggestionEntry *self)
{
  /* We need to defer to an idle since GtkText sets selection bounds
   * after notify::text
   */
  g_idle_add (text_changed_idle, self);
}

static void
accept_current_selection (GtkSuggestionEntry *self)
{
  gpointer item;
  GValue value = G_VALUE_INIT;

  item = gtk_single_selection_get_selected_item (self->selection);
  if (!item)
    return;

  g_signal_handler_block (self->entry, self->changed_id);

  if (self->expression)
    {
      gtk_expression_evaluate (self->expression, item, &value);
    }
  else if (GTK_IS_STRING_OBJECT (item))
    {
      g_object_get_property (G_OBJECT (item), "string", &value);
    }
  else
    {
      g_critical ("Either GtkSuggestionEntry:factory or GtkSuggestionEntry:expression "
                  "must be set");
      g_value_set_string (&value, "No value");
    }

  gtk_editable_set_text (GTK_EDITABLE (self->entry), g_value_get_string (&value));

  if (self->prefix && *self->prefix && self->use_filter)
    {
      int len = g_utf8_strlen (self->prefix, -1);
      gtk_editable_select_region (GTK_EDITABLE (self->entry), len, -1);
    }
  else
    gtk_editable_set_position (GTK_EDITABLE (self->entry), -1);

  g_value_unset (&value);

  g_signal_handler_unblock (self->entry, self->changed_id);
}

static void
gtk_suggestion_entry_row_activated (GtkListView        *listview,
                                    guint               position,
                                    GtkSuggestionEntry *self)
{
  gtk_suggestion_entry_set_popup_visible (self, FALSE);
  accept_current_selection (self);
}

static inline gboolean
keyval_is_cursor_move (guint keyval)
{
  if (keyval == GDK_KEY_Up || keyval == GDK_KEY_KP_Up)
    return TRUE;

  if (keyval == GDK_KEY_Down || keyval == GDK_KEY_KP_Down)
    return TRUE;

  if (keyval == GDK_KEY_Page_Up || keyval == GDK_KEY_Page_Down)
    return TRUE;

  return FALSE;
}

#define PAGE_STEP 10

static gboolean
gtk_suggestion_entry_key_pressed (GtkEventControllerKey *controller,
                                  guint                  keyval,
                                  guint                  keycode,
                                  GdkModifierType        state,
                                  GtkSuggestionEntry    *self)
{
  guint matches;
  guint selected;

  if (!gtk_widget_get_mapped (self->popup) &&
      !self->insert_selection)
    return FALSE;

  if (state & (GDK_SHIFT_MASK | GDK_ALT_MASK | GDK_CONTROL_MASK))
    return FALSE;

  if (keyval == GDK_KEY_Return ||
      keyval == GDK_KEY_KP_Enter ||
      keyval == GDK_KEY_ISO_Enter)
    {
      gtk_suggestion_entry_set_popup_visible (self, FALSE);
      g_clear_pointer (&self->prefix, g_free);
      accept_current_selection (self);
      return TRUE;
    }
  else if (keyval == GDK_KEY_Escape)
    {
      if (gtk_widget_get_mapped (self->popup))
        {
          GtkFilter *filter;
          const char *text;

          gtk_suggestion_entry_set_popup_visible (self, FALSE);

          g_signal_handler_block (self->entry, self->changed_id);

          text = self->prefix ? self->prefix : "";
          gtk_editable_set_text (GTK_EDITABLE (self->entry), text);

          g_free (self->search);
          self->search = g_strdup (text);

          filter = gtk_filter_list_model_get_filter (self->filter_model);
          if (GTK_IS_STRING_FILTER (filter))
            gtk_string_filter_set_search (GTK_STRING_FILTER (filter), self->search);

          g_clear_pointer (&self->prefix, g_free);
          gtk_editable_set_position (GTK_EDITABLE (self->entry), -1);

          g_signal_handler_unblock (self->entry, self->changed_id);
          return TRUE;
       }
    }
  else if (keyval == GDK_KEY_Right ||
           keyval == GDK_KEY_KP_Right)
    {
      g_clear_pointer (&self->prefix, g_free);
      gtk_editable_set_position (GTK_EDITABLE (self->entry), -1);
      return TRUE;
    }
  else if (keyval == GDK_KEY_Left ||
           keyval == GDK_KEY_KP_Left)
    {
      g_clear_pointer (&self->prefix, g_free);
      return FALSE;
    }
  else if (keyval == GDK_KEY_Tab ||
           keyval == GDK_KEY_KP_Tab ||
           keyval == GDK_KEY_ISO_Left_Tab)
    {
      gtk_suggestion_entry_set_popup_visible (self, FALSE);
      g_clear_pointer (&self->prefix, g_free);
      return FALSE; /* don't disrupt normal focus handling */
    }

  matches = g_list_model_get_n_items (G_LIST_MODEL (self->selection));
  selected = gtk_single_selection_get_selected (self->selection);

  if (keyval_is_cursor_move (keyval))
    {
      if (keyval == GDK_KEY_Up || keyval == GDK_KEY_KP_Up)
        {
          if (selected == 0)
            selected = GTK_INVALID_LIST_POSITION;
          else if (selected == GTK_INVALID_LIST_POSITION)
            selected = matches - 1;
          else
            selected--;
        }
      else if (keyval == GDK_KEY_Down || keyval == GDK_KEY_KP_Down)
        {
          if (selected == matches - 1)
            selected = GTK_INVALID_LIST_POSITION;
          else if (selected == GTK_INVALID_LIST_POSITION)
            selected = 0;
          else
            selected++;
        }
      else if (keyval == GDK_KEY_Page_Up)
        {
          if (selected == 0)
            selected = GTK_INVALID_LIST_POSITION;
          else if (selected == GTK_INVALID_LIST_POSITION)
            selected = matches - 1;
          else if (selected >= PAGE_STEP)
            selected -= PAGE_STEP;
          else
            selected = 0;
        }
      else if (keyval == GDK_KEY_Page_Down)
        {
          if (selected == matches - 1)
            selected = GTK_INVALID_LIST_POSITION;
          else if (selected == GTK_INVALID_LIST_POSITION)
            selected = 0;
          else if (selected + PAGE_STEP < matches)
            selected += PAGE_STEP;
          else
            selected = matches - 1;
        }

      if (!self->prefix)
        self->prefix = g_strdup (gtk_editable_get_text (GTK_EDITABLE (self->entry)));
      gtk_single_selection_set_selected (self->selection, selected);
      return TRUE;
    }

  g_clear_pointer (&self->prefix, g_free);

  return FALSE;
}

static void
gtk_suggestion_entry_focus_out (GtkEventController *controller,
                                GtkSuggestionEntry *self)
{
  if (!gtk_widget_get_mapped (self->popup))
    return;

  gtk_suggestion_entry_set_popup_visible (self, FALSE);
  g_clear_pointer (&self->prefix, g_free);
  accept_current_selection (self);
}

static void
set_default_factory (GtkSuggestionEntry *self)
{
  GtkListItemFactory *factory;

  factory = gtk_signal_list_item_factory_new ();

  g_signal_connect (factory, "setup", G_CALLBACK (setup_item), self);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_item), self);

  gtk_suggestion_entry_set_factory (self, factory);

  g_object_unref (factory);
}

static void
measure_entry (GtkGizmo       *gizmo,
               GtkOrientation  orientation,
               gint            size,
               gint           *minimum,
               gint           *natural,
               gint           *minimum_baseline,
               gint           *natural_baseline)
{
  gtk_widget_measure (gtk_widget_get_first_child (GTK_WIDGET (gizmo)),
                      orientation, size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);
}

static void
allocate_entry (GtkGizmo *gizmo,
                int       width,
                int       height,
                int       baseline)
{
  GtkSuggestionEntry *self;

  self = GTK_SUGGESTION_ENTRY (gtk_widget_get_parent (gtk_widget_get_parent (GTK_WIDGET (gizmo))));

  gtk_widget_size_allocate (gtk_widget_get_first_child (GTK_WIDGET (gizmo)),
                            &(GtkAllocation){ 0, 0, width, height },
                            baseline);

  gtk_widget_set_size_request (self->popup, gtk_widget_get_allocated_width (GTK_WIDGET (gizmo)), -1);

  gtk_native_check_resize (GTK_NATIVE (self->popup));
}

static gboolean
grab_focus_entry (GtkGizmo *gizmo)
{
  return gtk_widget_grab_focus (gtk_widget_get_first_child (GTK_WIDGET (gizmo)));
}

static void
gtk_suggestion_entry_init (GtkSuggestionEntry *self)
{
  GtkWidget *sw;
  GtkEventController *controller;

  self->minimum_length = 1;
  self->use_filter = TRUE;
  self->insert_selection = FALSE;
  self->insert_prefix = FALSE;
  self->show_button = FALSE;

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "popup.show", FALSE);

  self->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_add_css_class (self->box, "linked");
  gtk_widget_set_hexpand (self->box, TRUE);
  gtk_widget_set_parent (self->box, GTK_WIDGET (self));

  self->gizmo = gtk_gizmo_new ("entry", measure_entry, allocate_entry, NULL, NULL,
                               (GtkGizmoFocusFunc)gtk_widget_focus_child,
                               grab_focus_entry);
  gtk_widget_add_css_class (self->gizmo, "suggestion");
  gtk_widget_set_hexpand (self->gizmo, TRUE);
  gtk_box_append (GTK_BOX (self->box), self->gizmo);

  self->entry = gtk_text_new ();
  gtk_widget_set_parent (self->entry, self->gizmo);
  gtk_widget_set_hexpand (self->entry, TRUE);
  gtk_editable_init_delegate (GTK_EDITABLE (self));
  self->changed_id = g_signal_connect (self->entry, "notify::text", G_CALLBACK (text_changed), self);

  self->button = gtk_toggle_button_new ();
  gtk_button_set_icon_name (GTK_BUTTON (self->button), "pan-down-symbolic");
  gtk_widget_set_focus_on_click (self->button, FALSE);
  gtk_actionable_set_action_name (GTK_ACTIONABLE (self->button), "popup.show");
  gtk_box_append (GTK_BOX (self->box), self->button);
  gtk_widget_hide (self->button);

  self->popup = gtk_popover_new ();
  gtk_popover_set_position (GTK_POPOVER (self->popup), GTK_POS_BOTTOM);
  gtk_popover_set_autohide (GTK_POPOVER (self->popup), FALSE);
  gtk_popover_set_has_arrow (GTK_POPOVER (self->popup), FALSE);
  gtk_widget_set_halign (self->popup, GTK_ALIGN_START);
  gtk_widget_add_css_class (self->popup, "menu");
  gtk_widget_set_parent (self->popup, self->gizmo);
  sw = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (sw), 400);
  gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (sw), TRUE);

  gtk_popover_set_child (GTK_POPOVER (self->popup), sw);
  self->list = gtk_list_view_new ();
  gtk_list_view_set_single_click_activate (GTK_LIST_VIEW (self->list), TRUE);
  g_signal_connect (self->list, "activate",
                    G_CALLBACK (gtk_suggestion_entry_row_activated), self);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), self->list);

  set_default_factory (self);

  controller = gtk_event_controller_key_new ();
  gtk_event_controller_set_name (controller, "gtk-suggestion-entry");
  g_signal_connect (controller, "key-pressed",
                    G_CALLBACK (gtk_suggestion_entry_key_pressed), self);
  gtk_widget_prepend_controller (self->entry, controller);

  controller = gtk_event_controller_focus_new ();
  gtk_event_controller_set_name (controller, "gtk-suggestion-entry");
  g_signal_connect (controller, "leave",
                    G_CALLBACK (gtk_suggestion_entry_focus_out), self);
  gtk_widget_add_controller (self->entry, controller);
}

/**
 * gtk_suggestion_entry_new:
 *
 * Creates a new empty #GtkSuggestionEntry.
 *
 * You most likely want to call gtk_suggestion_entry_set_factory()
 * to set up a way to map its items to widgets and
 * gtk_suggestion_entry_set_model() to set a model to provide
 * items next.
 *
 * Returns: a new #GtkSuggestionEntry
 **/
GtkWidget *
gtk_suggestion_entry_new (void)
{
  return g_object_new (GTK_TYPE_SUGGESTION_ENTRY, NULL);
}

/**
 * gtk_suggestion_entry_get_model:
 * @self: a #GtkSuggestionEntry
 *
 * Gets the model that provides the displayed items.
 *
 * Returns: (nullable) (transfer none): The model in use
 **/
GListModel *
gtk_suggestion_entry_get_model (GtkSuggestionEntry *self)
{
  g_return_val_if_fail (GTK_IS_SUGGESTION_ENTRY (self), NULL);

  return self->model;
}

static void update_prefix (GtkSuggestionEntry *self);

static void
search_changed (GObject            *object,
                GParamSpec         *pspec,
                GtkSuggestionEntry *self)
{
  update_prefix (self);
}

static void
update_filter (GtkSuggestionEntry *self)
{
  GtkFilter *filter;
  GtkExpression *expression;

  if (!self->filter_model)
    return;

  if (!self->use_filter)
    expression = NULL;
  else if (self->expression)
    expression = gtk_expression_ref (self->expression);
  else if (GTK_IS_STRING_LIST (self->model))
    expression = gtk_property_expression_new (GTK_TYPE_STRING_OBJECT, NULL, "string");
  else
    expression = NULL;

  if (expression)
    {
      filter = gtk_string_filter_new ();
      gtk_string_filter_set_match_mode (GTK_STRING_FILTER (filter), GTK_STRING_FILTER_MATCH_MODE_PREFIX);
      gtk_string_filter_set_ignore_case (GTK_STRING_FILTER (filter), TRUE);
      gtk_string_filter_set_expression (GTK_STRING_FILTER (filter), expression);
      g_signal_connect (filter, "notify::search", G_CALLBACK (search_changed), self);
    }
  else
    filter = NULL;

  gtk_filter_list_model_set_filter (self->filter_model, filter);

  g_clear_pointer (&expression, gtk_expression_unref);
  g_clear_object (&filter);
}

static void
update_popup_action (GtkSuggestionEntry *self)
{
  guint n_items;

  if (self->filter_model)
    n_items = g_list_model_get_n_items (G_LIST_MODEL (self->filter_model));
  else
    n_items = 0;

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "popup.show", n_items > 0);
}

static void
selection_changed (GtkSingleSelection *selection,
                   GParamSpec         *pspec,
                   GtkSuggestionEntry *self)
{
  if (self->insert_selection)
    accept_current_selection (self);
}

/* This hardcodes our filter configuration, and needs
 * updates when we allow case-sensitive matching
 */
static gboolean
match (const char *search,
       const char *text)
{
  char *s;
  char *t;
  gboolean result;

  s = g_utf8_casefold (search, -1);
  t = g_utf8_casefold (text, -1);

  result = g_str_has_prefix (t, s);

  g_free (s);
  g_free (t);

  return result;
}

/* Assume that search matches all the items in the model,
 * and that search matches text, find the longest prefix
 * of text that still matches all items in the model.
 */
static char *
compute_prefix (GtkSuggestionEntry *self,
                const char         *search,
                const char         *text)
{
  char *prefix = NULL;
  guint i, n;
  gulong search_len;
  gulong prefix_len;

  search_len = g_utf8_strlen (search, -1);

  n = g_list_model_get_n_items (G_LIST_MODEL (self->selection));
  for (i = 0; i < n; i++)
    {
      gpointer item = g_list_model_get_item (G_LIST_MODEL (self->selection), i);
      GValue value = G_VALUE_INIT;
      char *string;

      if (self->expression)
        {
          gtk_expression_evaluate (self->expression, item, &value);
          string = g_utf8_normalize (g_value_get_string (&value), -1, G_NORMALIZE_ALL);
          g_value_unset (&value);
        }
      else if (GTK_IS_STRING_OBJECT (item))
        {
          string = g_utf8_normalize (gtk_string_object_get_string (GTK_STRING_OBJECT (item)), -1, G_NORMALIZE_ALL);
        }
      else
        {
          g_critical ("Either GtkSuggestionEntry:factory or GtkSuggestionEntry:expression "
                      "must be set");
          g_object_unref (item);
          g_free (prefix);
          return NULL;
        }

      g_object_unref (item);

      if (!prefix)
        {
          prefix = string;
          prefix_len = g_utf8_strlen (prefix, -1);
        }
      else
        {
          /* Not very efficient, but easy to understand:
           * shorten the prefix until we match again
           */
          while (!match (prefix, string))
            {
              char *tmp;

              tmp = g_utf8_substring (prefix, 0, prefix_len - 1);
              prefix = tmp;
              prefix_len = prefix_len - 1;

              /* Not strictly correct, since normalization and
               * casefolding might affect lengths, but in the
               * worst case, we'll do more matching than necessary.
               */
              if (prefix_len <= search_len)
                {
                  g_free (string);
                  return prefix;
                }
            }

          g_free (string);
        }
    }

  return prefix;
}

static void
insert_prefix (GtkSuggestionEntry *self,
               const char         *prefix)
{
  const char *key;
  int key_len;
  int prefix_len;

  if (!prefix)
    return;

  key = gtk_editable_get_text (GTK_EDITABLE (self->entry));
  key_len = g_utf8_strlen (key, -1);

  prefix_len = g_utf8_strlen (prefix, -1);

  if (prefix_len > key_len)
    {
      int pos = prefix_len;

      gtk_editable_insert_text (GTK_EDITABLE (self->entry),
                                prefix + strlen (key), -1, &pos);
      gtk_editable_select_region (GTK_EDITABLE (self->entry),
                                  key_len, prefix_len);
    }
}

static void
update_prefix (GtkSuggestionEntry *self)
{
  const char *text;
  char *prefix;

  if (!self->insert_prefix)
    return;

  g_signal_handler_block (self->entry, self->changed_id);

  text = gtk_editable_get_text (GTK_EDITABLE (self->entry));

  prefix = compute_prefix (self, self->search, text);
  insert_prefix (self, prefix);

  g_free (prefix);

  g_signal_handler_unblock (self->entry, self->changed_id);
}

static void
items_changed (GListModel         *model,
               guint               position,
               guint               removed,
               guint               added,
               GtkSuggestionEntry *self)
{
  update_prefix (self);
  update_popup_action (self);
}

/**
 * gtk_suggestion_entry_set_model:
 * @self: a #GtkSuggestionEntry
 * @model: (allow-none) (transfer none): the model to use or %NULL for none
 *
 * Sets the #GListModel to use.
 */
void
gtk_suggestion_entry_set_model (GtkSuggestionEntry *self,
                                GListModel  *model)
{
  g_return_if_fail (GTK_IS_SUGGESTION_ENTRY (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));

  if (!g_set_object (&self->model, model))
    return;

  if (self->selection)
    g_signal_handlers_disconnect_by_func (self->selection, selection_changed, self);

  if (model == NULL)
    {
      gtk_list_view_set_model (GTK_LIST_VIEW (self->list), NULL);
      g_clear_object (&self->selection);
      g_clear_object (&self->filter_model);
    }
  else
    {
      GtkFilterListModel *filter_model;
      GtkSingleSelection *selection;

      filter_model = gtk_filter_list_model_new (model, NULL);
      g_set_object (&self->filter_model, filter_model);
      g_object_unref (filter_model);

      update_filter (self);

      selection = gtk_single_selection_new (G_LIST_MODEL (filter_model));
      gtk_single_selection_set_autoselect (selection, FALSE);
      gtk_single_selection_set_can_unselect (selection, TRUE);
      g_set_object (&self->selection, selection);
      gtk_list_view_set_model (GTK_LIST_VIEW (self->list), G_LIST_MODEL (selection));
      g_object_unref (selection);
    }

  if (self->selection)
    {
      g_signal_connect (self->selection, "notify::selected",
                        G_CALLBACK (selection_changed), self);
      selection_changed (self->selection, NULL, self);
      g_signal_connect (self->selection, "items-changed",
                        G_CALLBACK (items_changed), self);
    }

  update_popup_action (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

/**
 * gtk_suggestion_entry_set_from_strings:
 * @self: a #GtkSuggestionEntry
 * @strings: a %NULL-terminated string array
 *
 * Populates @self with the strings in @strings,
 * by creating a suitable model and factory.
 */
void
gtk_suggestion_entry_set_from_strings (GtkSuggestionEntry  *self,
                                       const char         **strings)
{
  GListModel *model;

  g_return_if_fail (GTK_IS_SUGGESTION_ENTRY (self));
  g_return_if_fail (strings != NULL);

  set_default_factory (self);

  model = G_LIST_MODEL (gtk_string_list_new (strings));
  gtk_suggestion_entry_set_model (self, model);
  g_object_unref (model);
}

/**
 * gtk_suggestion_entry_get_factory:
 * @self: a #GtkSuggestionEntry
 *
 * Gets the factory that's currently used to populate list items.
 *
 * Returns: (nullable) (transfer none): The factory in use
 **/
GtkListItemFactory *
gtk_suggestion_entry_get_factory (GtkSuggestionEntry *self)
{
  g_return_val_if_fail (GTK_IS_SUGGESTION_ENTRY (self), NULL);

  return self->factory;
}

/**
 * gtk_suggestion_entry_set_factory:
 * @self: a #GtkSuggestionEntry
 * @factory: (nullable) (transfer none): the factory to use or %NULL for none
 *
 * Sets the #GtkListItemFactory to use for populating list items.
 **/
void
gtk_suggestion_entry_set_factory (GtkSuggestionEntry *self,
                                  GtkListItemFactory *factory)
{
  g_return_if_fail (GTK_IS_SUGGESTION_ENTRY (self));
  g_return_if_fail (factory == NULL || GTK_LIST_ITEM_FACTORY (factory));

  if (!g_set_object (&self->factory, factory))
    return;

  if (self->list)
    gtk_list_view_set_factory (GTK_LIST_VIEW (self->list), factory);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FACTORY]);
}

/**
 * gtk_suggestion_entry_set_expression:
 * @self: a #GtkSuggestionEntry
 * @expression: (nullable): a #GtkExpression, or %NULL
 *
 * Sets the expression that gets evaluated to obtain strings from items
 * when searching in the popup. The expression must have a value type of
 * #GTK_TYPE_STRING.
 */
void
gtk_suggestion_entry_set_expression (GtkSuggestionEntry *self,
                                     GtkExpression      *expression)
{
  g_return_if_fail (GTK_IS_SUGGESTION_ENTRY (self));
  g_return_if_fail (expression == NULL ||
                    gtk_expression_get_value_type (expression) == G_TYPE_STRING);

  if (self->expression == expression)
    return;

  if (self->expression)
    gtk_expression_unref (self->expression);

  self->expression = expression;

  if (self->expression)
    gtk_expression_ref (self->expression);

  update_filter (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EXPRESSION]);
}

/**
 * gtk_suggestion_entry_get_expression:
 * @self: a #GtkSuggestionEntry
 *
 * Gets the expression set with gtk_suggestion_entry_set_expression().
 *
 * Returns: (nullable) (transfer none): a #GtkExpression or %NULL
 */
GtkExpression *
gtk_suggestion_entry_get_expression (GtkSuggestionEntry *self)
{
  g_return_val_if_fail (GTK_IS_SUGGESTION_ENTRY (self), NULL);

  return self->expression;
}

/**
 * gtk_suggestion_entry_set_use_filter:
 * @self: a #GtkSuggestionEntry
 * @use_filter: whether to filter the suggestions
 *
 * Sets whether the suggestions will be filtered by matching
 * them against the text in the entry.
 *
 * The filtering done by #GtkSuggestionEntry is case-insensitive
 * and matches a prefix. If you need different filtering (or no
 * filtering at all), you can set @use_filter to %FALSE and use
 * a #GtkFilterListModel to do your own filtering.
 */
void
gtk_suggestion_entry_set_use_filter (GtkSuggestionEntry *self,
                                     gboolean            use_filter)
{
  g_return_if_fail (GTK_IS_SUGGESTION_ENTRY (self));

  if (self->use_filter == use_filter)
    return;

  self->use_filter = use_filter;

  update_filter (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_USE_FILTER]);
}

/**
 * gtk_suggestion_entry_get_use_filter:
 * @self: a #GtkSuggestionEntry
 *
 * Gets the value set by gtk_suggestion_entry_set_use_filter().
 *
 * Returns: %TRUE if the suggestions are filtered
 */
gboolean
gtk_suggestion_entry_get_use_filter (GtkSuggestionEntry *self)
{
  g_return_val_if_fail (GTK_IS_SUGGESTION_ENTRY (self), TRUE);

  return self->use_filter;
}

/**
 * gtk_suggestion_entry_set_insert_selection:
 * @self: a #GtkSuggestionEntry
 * @insert_selection: %TRUE to insert the selected text
 *
 * Sets whether the currently selected item should be
 * automatically inserted in the entry as you navigate
 * the popup. If #GtkSuggestionEntry:use-filter is %TRUE,
 * the inserted text will be selected, similar to how
 * #GtkSuggestionEntry:insert-prefix works. A suggestion
 * can be accepted with the right arrow or Enter keys.
 */
void
gtk_suggestion_entry_set_insert_selection (GtkSuggestionEntry *self,
                                           gboolean            insert_selection)
{
  g_return_if_fail (GTK_IS_SUGGESTION_ENTRY (self));

  if (self->insert_selection == insert_selection)
    return;

  self->insert_selection = insert_selection;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INSERT_SELECTION]);
}

/**
 * gtk_suggestion_entry_get_insert_selection:
 * @self: a #GtkSuggestionEntry
 *
 * Gets the value set by gtk_suggestion_entry_set_insert_selection().
 *
 * Returns: whether selected items are automatically inserted
 */
gboolean
gtk_suggestion_entry_get_insert_selection (GtkSuggestionEntry *self)
{
  g_return_val_if_fail (GTK_IS_SUGGESTION_ENTRY (self), FALSE);

  return self->insert_selection;
}

/**
 * gtk_suggestion_entry_set_insert_prefix:
 * @self: a #GtkSuggestionEntry
 * @insert_prefix: %TRUE to insert the common prefix
 *
 * Sets whether the common prefix of the matching selections
 * should be automatically inserted in the entry as you type.
 *
 * The inserted prefix is selected, and it can be accepted
 * by using the right arrow key, or by just continuing to
 * type.
 */
void
gtk_suggestion_entry_set_insert_prefix (GtkSuggestionEntry *self,
                                        gboolean            insert_prefix)
{
  g_return_if_fail (GTK_IS_SUGGESTION_ENTRY (self));

  if (self->insert_prefix == insert_prefix)
    return;

  self->insert_prefix = insert_prefix;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INSERT_PREFIX]);
}

/**
 * gtk_suggestion_entry_get_insert_prefix:
 * @self: a #GtkSuggestionEntry
 *
 * Gets the value set by gtk_suggestion_entry_set_insert_prefix().
 *
 * Returns: whether the common prefix is automatically inserted
 */
gboolean
gtk_suggestion_entry_get_insert_prefix (GtkSuggestionEntry *self)
{
  g_return_val_if_fail (GTK_IS_SUGGESTION_ENTRY (self), FALSE);

  return self->insert_prefix;
}

/**
 * gtk_suggestion_entry_set_show_button:
 * @self: a #GtkSuggestionEntry
 * @show_button: %TRUE to show a button
 *
 * Sets whether the GtkSuggestionEntry should show a button
 * for opening the popup with suggestions.
 */
void
gtk_suggestion_entry_set_show_button (GtkSuggestionEntry *self,
                                      gboolean            show_button)
{
  g_return_if_fail (GTK_IS_SUGGESTION_ENTRY (self));

  if (self->show_button == show_button)
    return;

  self->show_button = show_button;

  if (self->button)
    gtk_widget_set_visible (self->button, show_button);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_BUTTON]);
}

/**
 * gtk_suggestion_entry_get_show_button:
 * @self: a #GtkSuggestionEntry
 *
 * Gets the value set by gtk_suggestion_entry_set_show_button().
 *
 * Returns: %TRUE if @self is showing a button for suggestions
 */
gboolean
gtk_suggestion_entry_get_show_button (GtkSuggestionEntry *self)
{
  g_return_val_if_fail (GTK_IS_SUGGESTION_ENTRY (self), FALSE);

  return self->show_button;
}

/**
 * gtk_suggestion_entry_set_minimum_length:
 * @self: a #GtkSuggestionEntry
 * @minimum_length: the minimum length of matches when filtering
 *
 * Sets the minimum number of characters the user has to enter
 * before the GtkSuggestionEntry presents the suggestion popup.
 */
void
gtk_suggestion_entry_set_minimum_length (GtkSuggestionEntry *self,
                                         guint               minimum_length)
{
  g_return_if_fail (GTK_IS_SUGGESTION_ENTRY (self));

  if (self->minimum_length == minimum_length)
    return;

  self->minimum_length = minimum_length;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MINIMUM_LENGTH]);
}

/**
 * gtk_suggestion_entry_get_minimum_length:
 * @self: a #GtkSuggestionEntry
 *
 * Gets the value set by gtk_suggestion_entry_set_minimum_length().
 *
 * Returns: the minimum length of matches when filtering
 */
guint
gtk_suggestion_entry_get_minimum_length (GtkSuggestionEntry *self)
{
  g_return_val_if_fail (GTK_IS_SUGGESTION_ENTRY (self), 1);

  return self->minimum_length;
}
