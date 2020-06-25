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
 */

struct _GtkSuggestionEntry
{
  GtkWidget parent_instance;

  GListModel *model;
  GtkFilterListModel *filter_model;
  GtkSingleSelection *selection;

  GtkListItemFactory *factory;
  GtkExpression *expression;

  GtkWidget *entry;
  GtkWidget *popup;
  GtkWidget *list;

  gulong changed_id;
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

  N_PROPERTIES,
};

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
  g_clear_pointer (&self->popup, gtk_widget_unparent);
  g_clear_pointer (&self->entry, gtk_widget_unparent);

  g_clear_pointer (&self->expression, gtk_expression_unref);
  g_clear_object (&self->factory);

  g_clear_object (&self->model);
  g_clear_object (&self->filter_model);
  g_clear_object (&self->selection);

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

  gtk_widget_measure (self->entry,
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

  gtk_widget_size_allocate (self->entry, &(GtkAllocation) { 0, 0, width, height }, baseline);

  gtk_widget_set_size_request (self->popup, gtk_widget_get_width (widget), -1);

  gtk_native_check_resize (GTK_NATIVE (self->popup));
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
   * An expression to evaluate to obtain strings to match against the search
   * term (see #GtkSuggestionEntry:enable-search). If #GtkDropDown:factory is not set,
   * the expression is also used to bind strings to labels produced by a
   * default factory.
   */
  properties[PROP_EXPRESSION] =
    gtk_param_spec_expression ("expression",
                               P_("Expression"),
                               P_("Expression to determine strings to search for"),
                               G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
  gtk_editable_install_properties (object_class, N_PROPERTIES);

  gtk_widget_class_set_css_name (widget_class, I_("entry"));
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

  if (self->expression &&
      gtk_expression_evaluate (self->expression, item, &value))
    {
      gtk_label_set_label (GTK_LABEL (label), g_value_get_string (&value));
      g_value_unset (&value);
    }
  else if (GTK_IS_STRING_OBJECT (item))
    {
      gtk_label_set_label (GTK_LABEL (label),
                           gtk_string_object_get_string (GTK_STRING_OBJECT (item)));
    }
  else
    {
      g_critical ("Either GtkSuggestionEntry:factory or GtkDropDown:expression "
                  "must be set");
    }
}

static void
text_changed (GtkEditable *editable,
              GParamSpec *pspec,
              GtkSuggestionEntry *self)
{
  const char *text;
  GtkFilter *filter;

  text = gtk_editable_get_text (editable);
  if (!text || !*text)
    {
      gtk_popover_popdown (GTK_POPOVER (self->popup));
      return;
    }

  if (!self->filter_model)
    return;

  filter = gtk_filter_list_model_get_filter (self->filter_model);
  if (filter)
    gtk_string_filter_set_search (GTK_STRING_FILTER (filter), text);

  if (g_list_model_get_n_items (G_LIST_MODEL (self->selection)) > 0)
    gtk_popover_popup (GTK_POPOVER (self->popup));
  else
    gtk_popover_popdown (GTK_POPOVER (self->popup));
}

static void
accept_current_selection (GtkSuggestionEntry *self)
{
  gpointer item;
  GValue value = G_VALUE_INIT;

  item = gtk_single_selection_get_selected_item (self->selection);

  g_signal_handler_block (self->entry, self->changed_id);

  if (self->expression &&
      gtk_expression_evaluate (self->expression, item, &value))
    {
      gtk_editable_set_text (GTK_EDITABLE (self->entry), g_value_get_string (&value));
      gtk_editable_set_position (GTK_EDITABLE (self->entry), -1);
      g_value_unset (&value);
    }
  else if (GTK_IS_STRING_OBJECT (item))
    {
      gtk_editable_set_text (GTK_EDITABLE (self->entry),
                             gtk_string_object_get_string (GTK_STRING_OBJECT (item)));
      gtk_editable_set_position (GTK_EDITABLE (self->entry), -1);
    }
  else
    {
      g_critical ("Either GtkSuggestionEntry:factory or GtkDropDown:expression "
                  "must be set");
    }

  g_signal_handler_unblock (self->entry, self->changed_id);
}

static void
gtk_suggestion_entry_row_activated (GtkListView        *listview,
                                    guint               position,
                                    GtkSuggestionEntry *self)
{
  gtk_popover_popdown (GTK_POPOVER (self->popup));
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

  if (!gtk_widget_get_mapped (self->popup))
    return FALSE;

  if (keyval == GDK_KEY_Return ||
      keyval == GDK_KEY_KP_Enter ||
      keyval == GDK_KEY_ISO_Enter)
    {
      gtk_popover_popdown (GTK_POPOVER (self->popup));
      accept_current_selection (self);
      return TRUE;
    }
  else if (keyval == GDK_KEY_Escape)
    {
      gtk_popover_popdown (GTK_POPOVER (self->popup));
      return TRUE;
    }
  else if (keyval == GDK_KEY_Tab ||
           keyval == GDK_KEY_KP_Tab ||
           keyval == GDK_KEY_ISO_Left_Tab)
    {
      gtk_popover_popdown (GTK_POPOVER (self->popup));
      return TRUE;
    }

  matches = g_list_model_get_n_items (G_LIST_MODEL (self->selection));
  selected = gtk_single_selection_get_selected (self->selection);

  if (keyval_is_cursor_move (keyval))
    {
      if (keyval == GDK_KEY_Up || keyval == GDK_KEY_KP_Up)
        {
          if (selected == 0 || selected == GTK_INVALID_LIST_POSITION)
            selected = matches - 1;
          else
            selected--;
        }
      else if (keyval == GDK_KEY_Down || keyval == GDK_KEY_KP_Down)
        {
          if (selected == matches - 1 || selected == GTK_INVALID_LIST_POSITION)
            selected = 0;
          else
            selected++;
        }
      else if (keyval == GDK_KEY_Page_Up)
        {
          if (selected == 0 || selected == GTK_INVALID_LIST_POSITION)
            selected = matches - 1;
          else if (selected >= PAGE_STEP)
            selected -= PAGE_STEP;
          else
            selected = 0;
        }
      else if (keyval == GDK_KEY_Page_Down)
        {
          if (selected == matches - 1 || selected == GTK_INVALID_LIST_POSITION)
            selected = 0;
          else if (selected + PAGE_STEP < matches)
            selected += PAGE_STEP;
          else
            selected = matches - 1;
        }

      gtk_single_selection_set_selected (self->selection, selected);
      return TRUE;
    }

  return FALSE;
}

static void
gtk_suggestion_entry_focus_out (GtkEventController *controller,
                                GtkSuggestionEntry *self)
{
  if (!gtk_widget_get_mapped (self->popup))
    return;

  gtk_popover_popdown (GTK_POPOVER (self->popup));
  accept_current_selection (self);
}

static void
gtk_suggestion_entry_init (GtkSuggestionEntry *self)
{
  GtkWidget *sw;
  GtkEventController *controller;

  self->entry = gtk_text_new ();
  gtk_widget_set_parent (self->entry, GTK_WIDGET (self));
  gtk_widget_set_hexpand (self->entry, TRUE);
  gtk_editable_init_delegate (GTK_EDITABLE (self));
  self->changed_id = g_signal_connect (self->entry, "notify::text", G_CALLBACK (text_changed), self);

  self->factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (self->factory, "setup", G_CALLBACK (setup_item), self);
  g_signal_connect (self->factory, "bind", G_CALLBACK (bind_item), self);

  self->popup = gtk_popover_new ();
  gtk_popover_set_position (GTK_POPOVER (self->popup), GTK_POS_BOTTOM);
  gtk_popover_set_autohide (GTK_POPOVER (self->popup), FALSE);
  gtk_popover_set_has_arrow (GTK_POPOVER (self->popup), FALSE);
  gtk_widget_add_css_class (self->popup, "menu");
  gtk_widget_set_parent (self->popup, GTK_WIDGET (self));
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

  gtk_list_view_set_factory (GTK_LIST_VIEW (self->list), self->factory);

  gtk_widget_add_css_class (GTK_WIDGET (self), I_("suggestion"));

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

static void
update_filter (GtkSuggestionEntry *self)
{
  if (self->filter_model)
    {
      GtkExpression *expression;
      GtkFilter *filter;

      if (self->expression)
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
          gtk_expression_unref (expression);
        }
      else
        filter = NULL;

      gtk_filter_list_model_set_filter (GTK_FILTER_LIST_MODEL (self->filter_model), filter);

      g_object_unref (filter);
    }
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

  if (model == NULL)
    {
#if 0
      if (self->selection)
        g_signal_handlers_disconnect_by_func (self->selection, selection_changed, self);
#endif

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

#if 0
      g_signal_connect (self->selection, "notify::selected", G_CALLBACK (selection_changed), self);
#endif
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
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
 * @factory: (allow-none) (transfer none): the factory to use or %NULL for none
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
