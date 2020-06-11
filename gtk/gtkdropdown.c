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

#include "gtkbuiltiniconprivate.h"
#include "gtkintl.h"
#include "gtklistview.h"
#include "gtklistitemfactory.h"
#include "gtksignallistitemfactory.h"
#include "gtklistitemwidgetprivate.h"
#include "gtkpopover.h"
#include "gtkprivate.h"
#include "gtksingleselection.h"
#include "gtkfilterlistmodel.h"
#include "gtkstringfilter.h"
#include "gtkmultifilter.h"
#include "gtkstylecontext.h"
#include "gtkwidgetprivate.h"
#include "gtknative.h"
#include "gtktogglebutton.h"
#include "gtkexpression.h"
#include "gtkstack.h"
#include "gtksearchentry.h"
#include "gtklabel.h"
#include "gtklistitem.h"
#include "gtkbuildable.h"
#include "gtkbuilderprivate.h"

/**
 * SECTION:gtkdropdown
 * @Title: GtkDropDown
 * @Short_description: Choose an item from a list
 * @See_also: #GtkComboBox
 *
 * GtkDropDown is a widget that allows the user to choose an item
 * from a list of options. The GtkDropDown displays the selected
 * choice.
 *
 * The options are given to GtkDropDown in the form of #GListModel,
 * and how the individual options are represented is determined by
 * a #GtkListItemFactory. The default factory displays simple strings,
 * and expects to obtain these from the model by evaluating an expression
 * that has to be provided via gtk_drop_down_set_expression().
 *
 * The convenience method gtk_drop_down_set_from_strings() can be used
 * to set up a model that is populated from an array of strings and
 * an expression for obtaining those strings.
 *
 * GtkDropDown can optionally allow search in the popup, which is
 * useful if the list of options is long. To enable the search entry,
 * use gtk_drop_down_set_enable_search().
 *
 * # GtkDropDown as GtkBuildable
 *
 * The GtkDropDown implementation of the GtkBuildable interface supports
 * adding items directly using the <items> element and specifying <item>
 * elements for each item. Using <items> is equivalent to calling
 * gtk_drop_down_set_from_strings(). Each <item> element supports
 * the regular translation attributes “translatable”, “context”
 * and “comments”.
 *
 * Here is a UI definition fragment specifying GtkDropDown items:
 * |[
 * <object class="GtkDropDown">
 *   <items>
 *     <item translatable="yes">Factory</item>
 *     <item translatable="yes">Home</item>
 *     <item translatable="yes">Subway</item>
 *   </items>
 * </object>
 * ]|
 *
 * * # CSS nodes
 *
 * GtkDropDown has a single CSS node with name dropdown,
 * with the button and popover nodes as children.
 */

struct _GtkDropDown
{
  GtkWidget parent_instance;

  GtkListItemFactory *factory;
  GtkListItemFactory *list_factory;
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
  GtkExpression *expression;
};

struct _GtkDropDownClass
{
  GtkWidgetClass parent_class;
};

enum
{
  PROP_0,
  PROP_FACTORY,
  PROP_LIST_FACTORY,
  PROP_MODEL,
  PROP_SELECTED,
  PROP_ENABLE_SEARCH,
  PROP_EXPRESSION,

  N_PROPS
};

static void gtk_drop_down_buildable_interface_init (GtkBuildableIface *iface);

static GtkBuildableIface *buildable_parent_iface = NULL;

G_DEFINE_TYPE_WITH_CODE (GtkDropDown, gtk_drop_down, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_drop_down_buildable_interface_init))

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

  /* reset the filter so positions are 1-1 */
  filter = gtk_filter_list_model_get_filter (GTK_FILTER_LIST_MODEL (self->filter_model));
  if (GTK_IS_STRING_FILTER (filter))
    gtk_string_filter_set_search (GTK_STRING_FILTER (filter), "");
  gtk_drop_down_set_selected (self, gtk_single_selection_get_selected (GTK_SINGLE_SELECTION (self->popup_selection)));
}

static void
selection_changed (GtkSingleSelection *selection,
                   GParamSpec         *pspec,
                   gpointer            data)
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

  /* reset the filter so positions are 1-1 */
  filter = gtk_filter_list_model_get_filter (GTK_FILTER_LIST_MODEL (self->filter_model));
  if (GTK_IS_STRING_FILTER (filter))
    gtk_string_filter_set_search (GTK_STRING_FILTER (filter), "");
  gtk_single_selection_set_selected (GTK_SINGLE_SELECTION (self->popup_selection), selected);
}

static void
update_filter (GtkDropDown *self)
{
  if (self->filter_model)
    {
      GtkFilter *filter;

      if (self->expression)
        {
          filter = gtk_string_filter_new ();
          gtk_string_filter_set_match_mode (GTK_STRING_FILTER (filter), GTK_STRING_FILTER_MATCH_MODE_PREFIX);
          gtk_string_filter_set_expression (GTK_STRING_FILTER (filter), self->expression);
        }
      else
        filter = gtk_every_filter_new ();
      gtk_filter_list_model_set_filter (GTK_FILTER_LIST_MODEL (self->filter_model), filter);
      g_object_unref (filter);
    }
}

static void
search_changed (GtkSearchEntry *entry, gpointer data)
{
  GtkDropDown *self = data;
  const char *text;
  GtkFilter *filter;

  text  = gtk_editable_get_text (GTK_EDITABLE (entry));

  filter = gtk_filter_list_model_get_filter (GTK_FILTER_LIST_MODEL (self->filter_model));
  if (GTK_IS_STRING_FILTER (filter))
    gtk_string_filter_set_search (GTK_STRING_FILTER (filter), text);
}

static void
search_stop (GtkSearchEntry *entry, gpointer data)
{
  GtkDropDown *self = data;
  GtkFilter *filter;

  filter = gtk_filter_list_model_get_filter (GTK_FILTER_LIST_MODEL (self->filter_model));
  if (GTK_IS_STRING_FILTER (filter))
    {
      if (gtk_string_filter_get_search (GTK_STRING_FILTER (filter)))
        gtk_string_filter_set_search (GTK_STRING_FILTER (filter), NULL);
      else
        gtk_popover_popdown (GTK_POPOVER (self->popup));
    }
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
  g_clear_pointer (&self->expression, gtk_expression_unref);
  g_clear_object (&self->selection);
  g_clear_object (&self->popup_selection);
  g_clear_object (&self->factory);
  g_clear_object (&self->list_factory);

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

    case PROP_LIST_FACTORY:
      g_value_set_object (value, self->list_factory);
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

    case PROP_EXPRESSION:
      gtk_value_set_expression (value, self->expression);
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

    case PROP_LIST_FACTORY:
      gtk_drop_down_set_list_factory (self, g_value_get_object (value));
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

    case PROP_EXPRESSION:
      gtk_drop_down_set_expression (self, gtk_value_get_expression (value));
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

  gtk_widget_set_size_request (self->popup, width, -1);

  gtk_native_check_resize (GTK_NATIVE (self->popup));
}

static gboolean
gtk_drop_down_focus (GtkWidget        *widget,
                     GtkDirectionType  direction)
{
  GtkDropDown *self = GTK_DROP_DOWN (widget);

  if (self->popup && gtk_widget_get_visible (self->popup))
    return gtk_widget_child_focus (self->popup, direction);
  else
    return gtk_widget_child_focus (self->button, direction);
}

static gboolean
gtk_drop_down_grab_focus (GtkWidget *widget)
{
  GtkDropDown *self = GTK_DROP_DOWN (widget);

  return gtk_widget_grab_focus (self->button);
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
  widget_class->focus = gtk_drop_down_focus;
  widget_class->grab_focus = gtk_drop_down_grab_focus;

  /**
   * GtkDropDown:factory:
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
   * GtkDropDown:list-factory:
   *
   * The factory for populating list items in the popup.
   *
   * If this is not set, #GtkDropDown:factory is used.
   */
  properties[PROP_LIST_FACTORY] =
    g_param_spec_object ("list-factory",
                         P_("List Factory"),
                         P_("Factory for populating list items"),
                         GTK_TYPE_LIST_ITEM_FACTORY,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkDropDown:model:
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
   * GtkDropDown:selected:
   *
   * The position of the selected item in #GtkDropDown:model,
   * or #GTK_INVALID_LIST_POSITION if no item is selected.
   */
  properties[PROP_SELECTED] =
    g_param_spec_uint ("selected",
                         P_("Selected"),
                         P_("Position of the selected item"),
                         0, G_MAXUINT, GTK_INVALID_LIST_POSITION,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkDropDown:enable-search:
   *
   * Whether to show a search entry in the popup.
   *
   * Note that search requires #GtkDropDown:expression to be set.
   */
  properties[PROP_ENABLE_SEARCH] =
    g_param_spec_boolean  ("enable-search",
                         P_("Enable search"),
                         P_("Whether to show a search entry in the popup"),
                         FALSE,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkDropDown:expression: (type GtkExpression)
   *
   * An expression to evaluate to obtain strings to match against the search
   * term (see #GtkDropDown:enable-search). If #GtkDropDown:factory is not set,
   * the expression is also used to bind strings to labels produced by a
   * default factory.
   */
  properties[PROP_EXPRESSION] =
    gtk_param_spec_expression ("expression",
                               P_("Expression"),
                               P_("Expression to determine strings to search for"),
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
  gtk_widget_class_bind_template_callback (widget_class, search_stop);

  gtk_widget_class_set_css_name (widget_class, I_("dropdown"));
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
  GtkDropDown *self = data;
  gpointer item;
  GtkWidget *label;
  GValue value = G_VALUE_INIT;

  if (self->expression == NULL)
    {
      g_critical ("Either GtkDropDown::factory or GtkDropDown::expression must be set");
      return;
    }

  item = gtk_list_item_get_item (list_item);
  label = gtk_list_item_get_child (list_item);

  if (gtk_expression_evaluate (self->expression, item, &value))
    {
      gtk_label_set_label (GTK_LABEL (label), g_value_get_string (&value));
      g_value_unset (&value);
    }
}

static void
set_default_factory (GtkDropDown *self)
{
  GtkListItemFactory *factory;

  factory = gtk_signal_list_item_factory_new ();

  g_signal_connect (factory, "setup", G_CALLBACK (setup_item), self);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_item), self);

  gtk_drop_down_set_factory (self, factory);

  g_object_unref (factory);
}

static void
gtk_drop_down_init (GtkDropDown *self)
{
  g_type_ensure (GTK_TYPE_BUILTIN_ICON);
  g_type_ensure (GTK_TYPE_LIST_ITEM_WIDGET);

  gtk_widget_init_template (GTK_WIDGET (self));

  set_default_factory (self);
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
 * gtk_drop_down_get_model:
 * @self: a #GtkDropDown
 *
 * Gets the model that provides the displayed items.
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

      filter_model = G_LIST_MODEL (gtk_filter_list_model_new (model, NULL));
      g_set_object (&self->filter_model, filter_model);
      g_object_unref (filter_model);

      update_filter (self);

      selection = G_LIST_MODEL (gtk_single_selection_new (filter_model));
      g_set_object (&self->popup_selection, selection);
      gtk_list_view_set_model (GTK_LIST_VIEW (self->popup_list), selection);
      g_object_unref (selection);

      selection = G_LIST_MODEL (gtk_single_selection_new (model));
      g_set_object (&self->selection, selection);
      g_object_unref (selection);

      g_signal_connect (self->selection, "notify::selected", G_CALLBACK (selection_changed), self);
      selection_changed (GTK_SINGLE_SELECTION (self->selection), NULL, self);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

/**
 * gtk_drop_down_get_factory:
 * @self: a #GtkDropDown
 *
 * Gets the factory that's currently used to populate list items.
 *
 * The factory returned by this function is always used for the
 * item in the button. It is also used for items in the popup
 * if #GtkDropDown:list-factory is not set.
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

  gtk_list_item_widget_set_factory (GTK_LIST_ITEM_WIDGET (self->button_item), factory);
  if (self->list_factory == NULL)
    gtk_list_view_set_factory (GTK_LIST_VIEW (self->popup_list), factory);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FACTORY]);
}

/**
 * gtk_drop_down_get_list_factory:
 * @self: a #GtkDropDown
 *
 * Gets the factory that's currently used to populate list items in the popup.
 *
 * Returns: (nullable) (transfer none): The factory in use
 **/
GtkListItemFactory *
gtk_drop_down_get_list_factory (GtkDropDown *self)
{
  g_return_val_if_fail (GTK_IS_DROP_DOWN (self), NULL);

  return self->list_factory;
}

/**
 * gtk_drop_down_set_list_factory:
 * @self: a #GtkDropDown
 * @factory: (allow-none) (transfer none): the factory to use or %NULL for none
 *
 * Sets the #GtkListItemFactory to use for populating list items in the popup.
 **/
void
gtk_drop_down_set_list_factory (GtkDropDown        *self,
                                GtkListItemFactory *factory)
{
  g_return_if_fail (GTK_IS_DROP_DOWN (self));
  g_return_if_fail (factory == NULL || GTK_LIST_ITEM_FACTORY (factory));

  if (!g_set_object (&self->list_factory, factory))
    return;
 
  if (self->list_factory != NULL)
    gtk_list_view_set_factory (GTK_LIST_VIEW (self->popup_list), self->list_factory);
  else
    gtk_list_view_set_factory (GTK_LIST_VIEW (self->popup_list), self->factory);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LIST_FACTORY]);
}

/**
 * gtk_drop_down_set_selected:
 * @self: a #GtkDropDown
 * @position: the position of the item to select, or #GTK_INVALID_LIST_POSITION
 *
 * Selects the item at the given position.
 **/
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

/**
 * gtk_drop_down_get_selected:
 * @self: a #GtkDropDown
 *
 * Gets the position of the selected item.
 *
 * Returns: the position of the selected item, or #GTK_INVALID_LIST_POSITION
 *     if not item is selected
 **/
guint
gtk_drop_down_get_selected (GtkDropDown *self)
{
  g_return_val_if_fail (GTK_IS_DROP_DOWN (self), GTK_INVALID_LIST_POSITION);

  if (self->selection == NULL)
    return GTK_INVALID_LIST_POSITION;

  return gtk_single_selection_get_selected (GTK_SINGLE_SELECTION (self->selection));
}

/**
 * gtk_drop_down_set_enable_search:
 * @self: a #GtkDropDown
 * @enable_search: whether to enable search
 *
 * Sets whether a search entry will be shown in the popup that
 * allows to search for items in the list.
 *
 * Note that #GtkDropDown:expression must be set for search to work.
 **/
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

/**
 * gtk_drop_down_get_enable_search:
 * @self: a #GtkDropDown
 *
 * Returns whether search is enabled.
 *
 * Returns: %TRUE if the popup includes a search entry
 **/
gboolean
gtk_drop_down_get_enable_search (GtkDropDown *self)
{
  g_return_val_if_fail (GTK_IS_DROP_DOWN (self), FALSE);

  return self->enable_search;
}

/**
 * gtk_drop_down_set_expression:
 * @self: a #GtkDropDown
 * @expression: (nullable): a #GtkExpression, or %NULL
 *
 * Sets the expression that gets evaluated to obtain strings from items
 * when searching in the popup. The expression must have a value type of
 * #GTK_TYPE_STRING.
 */
void
gtk_drop_down_set_expression (GtkDropDown   *self,
                              GtkExpression *expression)
{
  g_return_if_fail (GTK_IS_DROP_DOWN (self));
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
 * gtk_drop_down_get_expression:
 * @self: a #GtkDropDown
 *
 * Gets the expression set with gtk_drop_down_set_expression().
 *
 * Returns: (nullable) (transfer none): a #GtkExpression or %NULL
 */
GtkExpression *
gtk_drop_down_get_expression (GtkDropDown *self)
{
  g_return_val_if_fail (GTK_IS_DROP_DOWN (self), NULL);

  return self->expression;
}


#define GTK_TYPE_DROP_DOWN_STRING_HOLDER (gtk_drop_down_string_holder_get_type ())
G_DECLARE_FINAL_TYPE (GtkDropDownStringHolder, gtk_drop_down_string_holder, GTK, DROP_DOWN_STRING_HOLDER, GObject)

struct _GtkDropDownStringHolder {
  GObject parent_instance;
  char *string;
};

enum {
  PROP_STRING = 1,
  PROP_NUM_PROPERTIES
};

G_DEFINE_TYPE (GtkDropDownStringHolder, gtk_drop_down_string_holder, G_TYPE_OBJECT);

static void
gtk_drop_down_string_holder_init (GtkDropDownStringHolder *holder)
{
}

static void
gtk_drop_down_string_holder_finalize (GObject *object)
{
  GtkDropDownStringHolder *holder = GTK_DROP_DOWN_STRING_HOLDER (object);

  g_free (holder->string);

  G_OBJECT_CLASS (gtk_drop_down_string_holder_parent_class)->finalize (object);
}

static void
gtk_drop_down_string_holder_set_property (GObject      *object,
                                          guint         property_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  GtkDropDownStringHolder *holder = GTK_DROP_DOWN_STRING_HOLDER (object);

  switch (property_id)
    {
    case PROP_STRING:
      g_free (holder->string);
      holder->string = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_drop_down_string_holder_get_property (GObject      *object,
                                          guint         property_id,
                                          GValue       *value,
                                          GParamSpec   *pspec)
{
  GtkDropDownStringHolder *holder = GTK_DROP_DOWN_STRING_HOLDER (object);

  switch (property_id)
    {
    case PROP_STRING:
      g_value_set_string (value, holder->string);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_drop_down_string_holder_class_init (GtkDropDownStringHolderClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GParamSpec *pspec;

  object_class->finalize = gtk_drop_down_string_holder_finalize;
  object_class->set_property = gtk_drop_down_string_holder_set_property;
  object_class->get_property = gtk_drop_down_string_holder_get_property;

  pspec = g_param_spec_string ("string", "String", "String",
                               NULL,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_property (object_class, PROP_STRING, pspec);

}

static GtkDropDownStringHolder *
gtk_drop_down_string_holder_new (const char *string)
{
  return g_object_new (GTK_TYPE_DROP_DOWN_STRING_HOLDER, "string", string, NULL);
}

static GListModel *
gtk_drop_down_strings_model_new (const char *const *text)
{
  GListStore *store;
  int i;

  store = g_list_store_new (GTK_TYPE_DROP_DOWN_STRING_HOLDER);
  for (i = 0; text[i]; i++)
    {
      GtkDropDownStringHolder *holder = gtk_drop_down_string_holder_new (text[i]);
      g_list_store_append (store, holder);
      g_object_unref (holder);
    }
  return G_LIST_MODEL (store);
}

/**
 * gtk_drop_down_set_from_strings:
 * @self: a #GtkDropDown
 * @texts: a %NULL-terminated string array
 *
 * Populates @self with the strings in @text,
 * by creating a suitable model and factory.
 */
void
gtk_drop_down_set_from_strings (GtkDropDown       *self,
                                const char *const *texts)
{
  GtkExpression *expression;
  GListModel *model;

  g_return_if_fail (GTK_IS_DROP_DOWN (self));
  g_return_if_fail (texts != NULL);

  set_default_factory (self);

  expression = gtk_property_expression_new (GTK_TYPE_DROP_DOWN_STRING_HOLDER, NULL, "string");
  gtk_drop_down_set_expression (self, expression);
  gtk_expression_unref (expression);

  model = gtk_drop_down_strings_model_new (texts);
  gtk_drop_down_set_model (self, model);
  g_object_unref (model);
}

typedef struct {
  GtkBuilder    *builder;
  GObject       *object;
  const gchar   *domain;

  gchar         *context;
  guint          translatable : 1;
  guint          is_text : 1;

  GString       *string;
  GPtrArray     *strings;
} ItemParserData;

static void
item_start_element (GtkBuildableParseContext  *context,
                    const gchar               *element_name,
                    const gchar              **names,
                    const gchar              **values,
                    gpointer                   user_data,
                    GError                   **error)
{
  ItemParserData *data = (ItemParserData*)user_data;

  if (strcmp (element_name, "items") == 0)
    {
      if (!_gtk_builder_check_parent (data->builder, context, "object", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_INVALID, NULL, NULL,
                                        G_MARKUP_COLLECT_INVALID))
        _gtk_builder_prefix_error (data->builder, context, error);
    }
  else if (strcmp (element_name, "item") == 0)
    {
      gboolean translatable = FALSE;
      const gchar *msg_context = NULL;

      if (!_gtk_builder_check_parent (data->builder, context, "items", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_BOOLEAN|G_MARKUP_COLLECT_OPTIONAL, "translatable", &translatable,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "comments", NULL,
                                        G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "context", &msg_context,
                                        G_MARKUP_COLLECT_INVALID))
        {
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      data->is_text = TRUE;
      data->translatable = translatable;
      data->context = g_strdup (msg_context);
    }
  else
    {
      _gtk_builder_error_unhandled_tag (data->builder, context,
                                        "GtkDropDown", element_name,
                                        error);
    }
}

static void
item_text (GtkBuildableParseContext  *context,
           const gchar               *text,
           gsize                      text_len,
           gpointer                   user_data,
           GError                   **error)
{
  ItemParserData *data = (ItemParserData*)user_data;

  if (data->is_text)
    g_string_append_len (data->string, text, text_len);
}

static void
item_end_element (GtkBuildableParseContext  *context,
                  const gchar               *element_name,
                  gpointer                   user_data,
                  GError                   **error)
{
  ItemParserData *data = (ItemParserData*)user_data;

  /* Append the translated strings */
  if (data->string->len)
    {
      if (data->translatable)
        {
          const gchar *translated;

          translated = _gtk_builder_parser_translate (data->domain,
                                                      data->context,
                                                      data->string->str);
          g_string_assign (data->string, translated);
        }

      g_ptr_array_add (data->strings, g_strdup (data->string->str));
    }

  data->translatable = FALSE;
  g_string_set_size (data->string, 0);
  g_clear_pointer (&data->context, g_free);
  data->is_text = FALSE;
}

static const GtkBuildableParser item_parser =
{
  item_start_element,
  item_end_element,
  item_text
};

static gboolean
gtk_drop_down_buildable_custom_tag_start (GtkBuildable       *buildable,
                                          GtkBuilder         *builder,
                                          GObject            *child,
                                          const gchar        *tagname,
                                          GtkBuildableParser *parser,
                                          gpointer           *parser_data)
{
  if (buildable_parent_iface->custom_tag_start (buildable, builder, child,
                                                tagname, parser, parser_data))
    return TRUE;

  if (strcmp (tagname, "items") == 0)
    {
      ItemParserData *data;

      data = g_slice_new0 (ItemParserData);
      data->builder = g_object_ref (builder);
      data->object = g_object_ref (G_OBJECT (buildable));
      data->domain = gtk_builder_get_translation_domain (builder);
      data->string = g_string_new ("");
      data->strings = g_ptr_array_new_with_free_func (g_free);

      *parser = item_parser;
      *parser_data = data;

      return TRUE;
    }

  return FALSE;
}

static void
gtk_drop_down_buildable_custom_finished (GtkBuildable *buildable,
                                         GtkBuilder   *builder,
                                         GObject      *child,
                                         const gchar  *tagname,
                                         gpointer      user_data)
{
  ItemParserData *data;

  buildable_parent_iface->custom_finished (buildable, builder, child,
                                           tagname, user_data);

  if (strcmp (tagname, "items") == 0)
    {
      data = (ItemParserData*)user_data;

      g_ptr_array_add (data->strings, NULL);

      gtk_drop_down_set_from_strings (GTK_DROP_DOWN (data->object), (const char **)data->strings->pdata);

      g_object_unref (data->object);
      g_object_unref (data->builder);
      g_string_free (data->string, TRUE);
      g_ptr_array_unref (data->strings);
      g_slice_free (ItemParserData, data);
    }
}

static void
gtk_drop_down_buildable_interface_init (GtkBuildableIface *iface)
{
  buildable_parent_iface = g_type_interface_peek_parent (iface);

  iface->custom_tag_start = gtk_drop_down_buildable_custom_tag_start;
  iface->custom_finished = gtk_drop_down_buildable_custom_finished;
}
