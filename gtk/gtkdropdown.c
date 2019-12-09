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

#include "gtkdropdown.h"

#include "gtkintl.h"
#include "gtklistview.h"
#include "gtklistitemfactory.h"
#include "gtklistitemwidgetprivate.h"
#include "gtkpopover.h"
#include "gtkprivate.h"
#include "gtksingleselection.h"
#include "gtkfilterlistmodel.h"
#include "gtkstringfilter.h"
#include "gtknoselection.h"
#include "gtkselectionmodel.h"
#include "gtkstylecontext.h"
#include "gtkiconprivate.h"
#include "gtkwidgetprivate.h"
#include "gtknative.h"
#include "gtktogglebutton.h"
#include "gtkexpression.h"
#include "gtkstack.h"
#include "gtksearchentry.h"


struct _GtkDropDown
{
  GtkWidget parent_instance;

  GtkListItemFactory *factory;
  GListModel *model;
  GListModel *selection;
  GListModel *filter_model;
  GListModel *popup_selection;

  GtkWidget *popup;
  GtkWidget *button;

  GtkWidget *popup_list;
  GtkWidget *button_stack;
  GtkWidget *button_item;
  GtkWidget *button_placeholder;
  GtkWidget *search_entry;

  gboolean enable_search;
  GtkExpression *search_expression;
};

struct _GtkDropDownClass
{
  GtkDropDownClass parent_class;
};

enum
{
  PROP_0,
  PROP_FACTORY,
  PROP_MODEL,
  PROP_SELECTED,
  PROP_ENABLE_SEARCH,
  PROP_SEARCH_EXPRESSION,

  N_PROPS
};

G_DEFINE_TYPE (GtkDropDown, gtk_drop_down, GTK_TYPE_WIDGET)

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
button_toggled (GtkWidget *widget,
                gpointer   data)
{
  GtkDropDown *self = data;

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
    gtk_popover_popup (GTK_POPOVER (self->popup));
  else
    gtk_popover_popdown (GTK_POPOVER (self->popup));
}

static void
popover_closed (GtkPopover *popover,
                gpointer    data)
{
  GtkDropDown *self = data;

  gtk_editable_set_text (GTK_EDITABLE (self->search_entry), "");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->button), FALSE);
}

static void
row_activated (GtkListView *listview,
               guint        position,
               gpointer     data)
{
  GtkDropDown *self = data;
  GtkFilter *filter;

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->button), FALSE);
  gtk_popover_popdown (GTK_POPOVER (self->popup));

  filter = gtk_filter_list_model_get_filter (GTK_FILTER_LIST_MODEL (self->filter_model));
  gtk_string_filter_set_search (GTK_STRING_FILTER (filter), "");
  gtk_drop_down_set_selected (self, gtk_single_selection_get_selected (GTK_SINGLE_SELECTION (self->popup_selection)));
}
 
static void
selection_changed (GtkSelectionModel *selection,
                   guint              position,
                   guint              n_items,
                   gpointer           data)
{
  GtkDropDown *self = data;
  guint selected;
  gpointer item;
  GtkFilter *filter;

  selected = gtk_single_selection_get_selected (GTK_SINGLE_SELECTION (self->selection));
  item = gtk_single_selection_get_selected_item (GTK_SINGLE_SELECTION (self->selection));

  if (selected == GTK_INVALID_LIST_POSITION)
    {
      gtk_stack_set_visible_child_name (GTK_STACK (self->button_stack), "empty");
    }
  else
    {
      gtk_stack_set_visible_child_name (GTK_STACK (self->button_stack), "item");
      gtk_list_item_widget_update (GTK_LIST_ITEM_WIDGET (self->button_item), selected, item, FALSE);
    }

  filter = gtk_filter_list_model_get_filter (GTK_FILTER_LIST_MODEL (self->filter_model));
  gtk_string_filter_set_search (GTK_STRING_FILTER (filter), "");
  gtk_single_selection_set_selected (GTK_SINGLE_SELECTION (self->popup_selection), selected);
}

static void
search_changed (GtkSearchEntry *entry, gpointer data)
{
  GtkDropDown *self = data;
  const char *text;
  GtkFilter *filter;

  text  = gtk_editable_get_text (GTK_EDITABLE (entry));

  filter = gtk_filter_list_model_get_filter (GTK_FILTER_LIST_MODEL (self->filter_model));
  gtk_string_filter_set_search (GTK_STRING_FILTER (filter), text);
}

static void
gtk_drop_down_dispose (GObject *object)
{
  GtkDropDown *self = GTK_DROP_DOWN (object);

  g_clear_pointer (&self->popup, gtk_widget_unparent);
  g_clear_pointer (&self->button, gtk_widget_unparent);

  g_clear_object (&self->model);
  if (self->selection)
    g_signal_handlers_disconnect_by_func (self->selection, selection_changed, self);
  g_clear_object (&self->filter_model);
  g_clear_pointer (&self->search_expression, gtk_expression_unref);
  g_clear_object (&self->selection);
  g_clear_object (&self->popup_selection);

  G_OBJECT_CLASS (gtk_drop_down_parent_class)->dispose (object);
}

static void
gtk_drop_down_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkDropDown *self = GTK_DROP_DOWN (object);

  switch (property_id)
    {
    case PROP_FACTORY:
      g_value_set_object (value, self->factory);
      break;

    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;

    case PROP_SELECTED:
      g_value_set_uint (value, gtk_drop_down_get_selected (self));
      break;

    case PROP_ENABLE_SEARCH:
      g_value_set_boolean (value, self->enable_search);
      break;

    case PROP_SEARCH_EXPRESSION:
      g_value_set_boxed (value, self->search_expression);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_drop_down_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkDropDown *self = GTK_DROP_DOWN (object);

  switch (property_id)
    {
    case PROP_FACTORY:
      gtk_drop_down_set_factory (self, g_value_get_object (value));
      break;

    case PROP_MODEL:
      gtk_drop_down_set_model (self, g_value_get_object (value));
      break;

    case PROP_SELECTED:
      gtk_drop_down_set_selected (self, g_value_get_uint (value));
      break;

    case PROP_ENABLE_SEARCH:
      gtk_drop_down_set_enable_search (self, g_value_get_boolean (value));
      break;

    case PROP_SEARCH_EXPRESSION:
      gtk_drop_down_set_search_expression (self, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_drop_down_measure (GtkWidget      *widget,
                       GtkOrientation  orientation,
                       int             size,
                       int            *minimum,
                       int            *natural,
                       int            *minimum_baseline,
                       int            *natural_baseline)
{
  GtkDropDown *self = GTK_DROP_DOWN (widget);

  gtk_widget_measure (self->button,
                      orientation,
                      size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);
}

static void
gtk_drop_down_size_allocate (GtkWidget *widget,
                             int        width,
                             int        height,
                             int        baseline)
{
  GtkDropDown *self = GTK_DROP_DOWN (widget);

  gtk_widget_size_allocate (self->button, &(GtkAllocation) { 0, 0, width, height }, baseline);

  gtk_native_check_resize (GTK_NATIVE (self->popup));
}


static void
gtk_drop_down_class_init (GtkDropDownClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gtk_drop_down_dispose;
  gobject_class->get_property = gtk_drop_down_get_property;
  gobject_class->set_property = gtk_drop_down_set_property;

  widget_class->measure = gtk_drop_down_measure;
  widget_class->size_allocate = gtk_drop_down_size_allocate;

  /**
   * GtkDropDown:factory:
   *
   * Factory for populating list items
   */
  properties[PROP_FACTORY] =
    g_param_spec_object ("factory",
                         P_("Factory"),
                         P_("Factory for populating list items"),
                         GTK_TYPE_LIST_ITEM_FACTORY,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkDropDown:model:
   *
   * Model for the items displayed
   */
  properties[PROP_MODEL] =
    g_param_spec_object ("model",
                         P_("Model"),
                         P_("Model for the items displayed"),
                         G_TYPE_LIST_MODEL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_SELECTED] =
    g_param_spec_uint ("selected",
                         P_("Selected"),
                         P_("Position of the selected item"),
                         0, G_MAXUINT, GTK_INVALID_LIST_POSITION,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_ENABLE_SEARCH] =
    g_param_spec_boolean  ("enable-search",
                         P_("Enable search"),
                         P_("Whether to show a search entry in the popup"),
                         FALSE,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_SEARCH_EXPRESSION] =
    g_param_spec_boxed ("search-expression",
                        P_("Search expression"),
                        P_("Search expression"),
                        GTK_TYPE_EXPRESSION,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/ui/gtkdropdown.ui");
  gtk_widget_class_bind_template_child (widget_class, GtkDropDown, button);
  gtk_widget_class_bind_template_child (widget_class, GtkDropDown, button_stack);
  gtk_widget_class_bind_template_child (widget_class, GtkDropDown, button_item);
  gtk_widget_class_bind_template_child (widget_class, GtkDropDown, popup);
  gtk_widget_class_bind_template_child (widget_class, GtkDropDown, popup_list);
  gtk_widget_class_bind_template_child (widget_class, GtkDropDown, search_entry);

  gtk_widget_class_bind_template_callback (widget_class, row_activated);
  gtk_widget_class_bind_template_callback (widget_class, button_toggled);
  gtk_widget_class_bind_template_callback (widget_class, popover_closed);
  gtk_widget_class_bind_template_callback (widget_class, search_changed);

  gtk_widget_class_set_css_name (widget_class, I_("combobox"));
}

static void
gtk_drop_down_init (GtkDropDown *self)
{
  g_type_ensure (GTK_TYPE_ICON);
  g_type_ensure (GTK_TYPE_LIST_ITEM_WIDGET);

  gtk_widget_init_template (GTK_WIDGET (self));
}

/**
 * gtk_drop_down_new:
 *
 * Creates a new empty #GtkDropDown.
 *
 * You most likely want to call gtk_drop_down_set_factory() to
 * set up a way to map its items to widgets and gtk_drop_down_set_model()
 * to set a model to provide items next.
 *
 * Returns: a new #GtkDropDown
 **/
GtkWidget *
gtk_drop_down_new (void)
{
  return g_object_new (GTK_TYPE_DROP_DOWN, NULL);
}

/**
 * gtk_drop_down_new_with_factory:
 * @factory: (transfer full): The factory to populate items with
 *
 * Creates a new #GtkDropDown that uses the given @factory for
 * mapping items to widgets.
 *
 * You most likely want to call gtk_drop_down_set_model() to set
 * a model next.
 *
 * Returns: a new #GtkDropDown using the given @factory
 **/
GtkWidget *
gtk_drop_down_new_with_factory (GtkListItemFactory *factory)
{
  GtkWidget *result;

  g_return_val_if_fail (GTK_IS_LIST_ITEM_FACTORY (factory), NULL);

  result = g_object_new (GTK_TYPE_DROP_DOWN,
                         "factory", factory,
                         NULL);

  g_object_unref (factory);

  return result;
}

/**
 * gtk_drop_down_get_model:
 * @self: a #GtkDropDown
 *
 * Gets the model that's currently used to read the items displayed.
 *
 * Returns: (nullable) (transfer none): The model in use
 **/
GListModel *
gtk_drop_down_get_model (GtkDropDown *self)
{
  g_return_val_if_fail (GTK_IS_DROP_DOWN (self), NULL);

  return self->model;
}

/**
 * gtk_drop_down_set_model:
 * @self: a #GtkDropDown
 * @model: (allow-none) (transfer none): the model to use or %NULL for none
 *
 * Sets the #GListModel to use.
 */
void
gtk_drop_down_set_model (GtkDropDown *self,
                         GListModel  *model)
{
  g_return_if_fail (GTK_IS_DROP_DOWN (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));

  if (!g_set_object (&self->model, model))
    return;

  if (model == NULL)
    {
      gtk_list_view_set_model (GTK_LIST_VIEW (self->popup_list), NULL);

      if (self->selection)
        g_signal_handlers_disconnect_by_func (self->selection, selection_changed, self);

      g_clear_object (&self->selection);
      g_clear_object (&self->filter_model);
      g_clear_object (&self->popup_selection);
    }
  else
    {
      GListModel *filter_model;
      GListModel *selection;
      GtkFilter *filter;

      filter = gtk_string_filter_new ();
      gtk_string_filter_set_match_mode (GTK_STRING_FILTER (filter), GTK_STRING_FILTER_MATCH_MODE_PREFIX);
      gtk_string_filter_set_expression (GTK_STRING_FILTER (filter), self->search_expression);
      filter_model = G_LIST_MODEL (gtk_filter_list_model_new (model, filter));
      g_set_object (&self->filter_model, filter_model);
      g_object_unref (filter_model);
      g_object_unref (filter);

      selection = G_LIST_MODEL (gtk_single_selection_new (filter_model));
      g_set_object (&self->popup_selection, selection);
      gtk_list_view_set_model (GTK_LIST_VIEW (self->popup_list), selection);
      g_object_unref (selection);

      if (GTK_IS_SINGLE_SELECTION (model))
        selection = g_object_ref (model);
      else
        selection = G_LIST_MODEL (gtk_single_selection_new (model));
      g_set_object (&self->selection, selection);
      g_object_unref (selection);

      g_signal_connect (self->selection, "selection-changed", G_CALLBACK (selection_changed), self);
    }
  
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

/**
 * gtk_drop_down_get_factory:
 * @self: a #GtkDropDown
 *
 * Gets the factory that's currently used to populate list items.
 *
 * Returns: (nullable) (transfer none): The factory in use
 **/
GtkListItemFactory *
gtk_drop_down_get_factory (GtkDropDown *self)
{
  g_return_val_if_fail (GTK_IS_DROP_DOWN (self), NULL);

  return self->factory;
}

/**
 * gtk_drop_down_set_factory:
 * @self: a #GtkDropDown
 * @factory: (allow-none) (transfer none): the factory to use or %NULL for none
 *
 * Sets the #GtkListItemFactory to use for populating list items.
 **/
void
gtk_drop_down_set_factory (GtkDropDown        *self,
                           GtkListItemFactory *factory)
{
  g_return_if_fail (GTK_IS_DROP_DOWN (self));
  g_return_if_fail (factory == NULL || GTK_LIST_ITEM_FACTORY (factory));

  if (!g_set_object (&self->factory, factory))
    return;

  gtk_list_view_set_factory (GTK_LIST_VIEW (self->popup_list), factory);
  gtk_list_item_widget_set_factory (GTK_LIST_ITEM_WIDGET (self->button_item), factory);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FACTORY]);
}

void
gtk_drop_down_set_selected (GtkDropDown *self,
                            guint        position)
{
  g_return_if_fail (GTK_IS_DROP_DOWN (self));

  if (self->selection == NULL)
    return;

  if (gtk_single_selection_get_selected (GTK_SINGLE_SELECTION (self->selection)) == position)
    return;

  gtk_single_selection_set_selected (GTK_SINGLE_SELECTION (self->selection), position);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTED]);
}

guint
gtk_drop_down_get_selected (GtkDropDown *self)
{
  g_return_val_if_fail (GTK_IS_DROP_DOWN (self), GTK_INVALID_LIST_POSITION);

  if (self->selection == NULL)
    return GTK_INVALID_LIST_POSITION;

  return gtk_single_selection_get_selected (GTK_SINGLE_SELECTION (self->selection));
}

void
gtk_drop_down_set_enable_search (GtkDropDown *self,
                                 gboolean     enable_search)
{
  g_return_if_fail (GTK_IS_DROP_DOWN (self));

  if (self->enable_search == enable_search)
    return;

  self->enable_search = enable_search;

  gtk_editable_set_text (GTK_EDITABLE (self->search_entry), "");
  gtk_widget_set_visible (self->search_entry, enable_search);
  
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ENABLE_SEARCH]);
}

gboolean
gtk_drop_down_get_enable_search (GtkDropDown *self)
{
  g_return_val_if_fail (GTK_IS_DROP_DOWN (self), FALSE);

  return self->enable_search;
}

void
gtk_drop_down_set_search_expression (GtkDropDown   *self,
                                     GtkExpression *expression)
{
  GtkFilter *filter;

  g_return_if_fail (GTK_IS_DROP_DOWN (self));
  g_return_if_fail (expression == NULL || gtk_expression_get_value_type (expression) == G_TYPE_STRING);

  if (self->search_expression == expression)
    return;

  if (self->search_expression)
    gtk_expression_unref (self->search_expression);
  self->search_expression = expression;
  if (self->search_expression)
    gtk_expression_ref (self->search_expression);

  filter = gtk_filter_list_model_get_filter (GTK_FILTER_LIST_MODEL (self->filter_model));
  gtk_string_filter_set_expression (GTK_STRING_FILTER (filter), expression);
  
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SEARCH_EXPRESSION]);
}

GtkExpression *
gtk_drop_down_get_search_expression (GtkDropDown *self)
{
  g_return_val_if_fail (GTK_IS_DROP_DOWN (self), NULL);

  return self->search_expression;
}
