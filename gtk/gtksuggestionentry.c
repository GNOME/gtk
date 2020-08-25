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
#include "gtktext.h"
#include "gtkscrolledwindow.h"
#include "gtkeventcontrollerkey.h"
#include "gtkeventcontrollerfocus.h"
#include "gtkactionable.h"
#include "gtkimage.h"
#include "gtkgestureclick.h"
#include "gtkmaplistmodel.h"
#include "gtksortlistmodel.h"
#include "gtknumericsorter.h"
#include "gtkcustomfilter.h"


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
 * entry.suggestion
 * ├── text
 * ├── [arrow]
 * ╰── popover
 * ]|
 *
 * GtkSuggestionEntry has a single CSS node with name entry that carries
 * a .suggestion style class, and the text, arrow and popover nodes are children
 * of that.
 */

struct _GtkMatchObject
{
  GObject parent_instance;

  GObject *item;
  char *string;
  guint match_start;
  guint match_end;
  guint score;
};

typedef struct
{
  GObjectClass parent_class;
} GtkMatchObjectClass;

enum
{
  PROP_ITEM = 1,
  PROP_STRING,
  PROP_MATCH_START,
  PROP_MATCH_END,
  PROP_SCORE,
  N_MATCH_PROPERTIES
};

static GParamSpec *match_properties[N_MATCH_PROPERTIES];

G_DEFINE_TYPE (GtkMatchObject, gtk_match_object, G_TYPE_OBJECT)

static void
gtk_match_object_init (GtkMatchObject *object)
{
}

static void
gtk_match_object_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GtkMatchObject *self = GTK_MATCH_OBJECT (object);

  switch (property_id)
    {
    case PROP_ITEM:
      g_value_set_object (value, self->item);
      break;

    case PROP_STRING:
      g_value_set_string (value, self->string);
      break;

    case PROP_MATCH_START:
      g_value_set_uint (value, self->match_start);
      break;

    case PROP_MATCH_END:
      g_value_set_uint (value, self->match_end);
      break;

    case PROP_SCORE:
      g_value_set_uint (value, self->score);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_match_object_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkMatchObject *self = GTK_MATCH_OBJECT (object);

  switch (property_id)
    {
    case PROP_ITEM:
      self->item = g_value_get_object (value);
      break;

    case PROP_STRING:
      self->string = g_value_dup_string (value);
      break;

    case PROP_MATCH_START:
      if (self->match_start != g_value_get_uint (value))
        {
          self->match_start = g_value_get_uint (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case PROP_MATCH_END:
      if (self->match_end != g_value_get_uint (value))
        {
          self->match_end = g_value_get_uint (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case PROP_SCORE:
      if (self->score != g_value_get_uint (value))
        {
          self->score = g_value_get_uint (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_match_object_dispose (GObject *object)
{
  GtkMatchObject *self = GTK_MATCH_OBJECT (object);

  g_clear_object (&self->item);
  g_clear_pointer (&self->string, g_free);

  G_OBJECT_CLASS (gtk_match_object_parent_class)->dispose (object);
}

static void
gtk_match_object_class_init (GtkMatchObjectClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = gtk_match_object_dispose;
  object_class->get_property = gtk_match_object_get_property;
  object_class->set_property = gtk_match_object_set_property;

  match_properties[PROP_ITEM]
      = g_param_spec_object ("item", "Item", "Item",
                             G_TYPE_OBJECT,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);
  match_properties[PROP_STRING]
      = g_param_spec_string ("string", "String", "String",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);
  match_properties[PROP_MATCH_START]
      = g_param_spec_uint ("match-start", "Match Start", "Match Start",
                           0, G_MAXUINT, 0,
                           G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS);
  match_properties[PROP_MATCH_END]
      = g_param_spec_uint ("match-end", "Match End", "Match End",
                           0, G_MAXUINT, 0,
                           G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS);
  match_properties[PROP_SCORE]
      = g_param_spec_uint ("score", "Score", "Score",
                           0, G_MAXUINT, 0,
                           G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_MATCH_PROPERTIES, match_properties);
}

static GtkMatchObject *
gtk_match_object_new (gpointer    item,
                      const char *string)
{
  return g_object_new (GTK_TYPE_MATCH_OBJECT,
                       "item", item,
                       "string", string,
                       NULL);
}

gpointer
gtk_match_object_get_item (GtkMatchObject *object)
{
  return object->item;
}

const char *
gtk_match_object_get_string (GtkMatchObject *object)
{
  return object->string;
}

guint
gtk_match_object_get_match_start (GtkMatchObject *object)
{
  return object->match_start;
}

guint
gtk_match_object_get_match_end (GtkMatchObject *object)
{
  return object->match_end;
}

guint
gtk_match_object_get_score (GtkMatchObject *object)
{
  return object->score;
}

void
gtk_match_object_set_match (GtkMatchObject *object,
                            guint           start,
                            guint           end,
                            guint           score)
{
  g_object_freeze_notify (G_OBJECT (object));

  g_object_set (object,
                "match-start", start,
                "match-end", end,
                "score", score,
                NULL);

  g_object_thaw_notify (G_OBJECT (object));
}

/* ---- */


struct _GtkSuggestionEntry
{
  GtkWidget parent_instance;

  GListModel *model;
  GtkListItemFactory *factory;
  GtkExpression *expression;

  GtkFilter *filter;
  GtkMapListModel *map_model;
  GtkSingleSelection *selection;

  GtkWidget *entry;
  GtkWidget *arrow;
  GtkWidget *popup;
  GtkWidget *list;

  char *search;

  GtkSuggestionEntryMatchFunc match_func;
  gpointer match_data;
  GDestroyNotify destroy;

  gulong changed_id;

  guint use_filter       : 1;
  guint show_arrow       : 1;
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
  PROP_SHOW_ARROW,

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
  g_clear_pointer (&self->entry, gtk_widget_unparent);
  g_clear_pointer (&self->arrow, gtk_widget_unparent);
  g_clear_pointer (&self->popup, gtk_widget_unparent);

  g_clear_pointer (&self->expression, gtk_expression_unref);
  g_clear_object (&self->factory);

  g_clear_object (&self->model);
  g_clear_object (&self->map_model);
  g_clear_object (&self->selection);

  g_clear_pointer (&self->search, g_free);

  if (self->destroy)
    self->destroy (self->match_data);

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

    case PROP_SHOW_ARROW:
      g_value_set_boolean (value, gtk_suggestion_entry_get_show_arrow (self));
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

    case PROP_SHOW_ARROW:
      gtk_suggestion_entry_set_show_arrow (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_suggestion_entry_measure (GtkWidget      *widget,
                              GtkOrientation  orientation,
                              int             for_size,
                              int            *minimum,
                              int            *natural,
                              int            *minimum_baseline,
                              int            *natural_baseline)
{
  GtkSuggestionEntry *self = GTK_SUGGESTION_ENTRY (widget);
  int arrow_min = 0, arrow_nat = 0;

  gtk_widget_measure (self->entry, orientation, for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);

  if (self->arrow && gtk_widget_get_visible (self->arrow))
    gtk_widget_measure (self->arrow, orientation, for_size,
                        &arrow_min, &arrow_nat,
                        NULL, NULL);
}

static void
gtk_suggestion_entry_size_allocate (GtkWidget *widget,
                                    int        width,
                                    int        height,
                                    int        baseline)
{
  GtkSuggestionEntry *self = GTK_SUGGESTION_ENTRY (widget);
  int arrow_min = 0, arrow_nat = 0;

  if (self->arrow && gtk_widget_get_visible (self->arrow))
    gtk_widget_measure (self->arrow, GTK_ORIENTATION_HORIZONTAL, -1,
                        &arrow_min, &arrow_nat,
                        NULL, NULL);

  gtk_widget_size_allocate (self->entry,
                            &(GtkAllocation) { 0, 0, width - arrow_nat, height },
                            baseline);

  if (self->arrow && gtk_widget_get_visible (self->arrow))
    gtk_widget_size_allocate (self->arrow,
                              &(GtkAllocation) { width - arrow_nat, 0, arrow_nat, height },
                              baseline);

  gtk_widget_set_size_request (self->popup, gtk_widget_get_allocated_width (GTK_WIDGET (self)), -1);
  gtk_widget_queue_resize (self->popup);

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

  properties[PROP_SHOW_ARROW] =
      g_param_spec_boolean ("show-arrow",
                            P_("Show arrow"),
                            P_("Whether to show a clickable arrow for presenting the popup"),
                            FALSE,
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

  gtk_widget_class_add_binding_action (widget_class,
                                       GDK_KEY_Down, GDK_ALT_MASK,
                                       "popup.show", NULL);

  gtk_widget_class_set_css_name (widget_class, "entry");
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
  gpointer item;
  GtkWidget *label;
  GValue value = G_VALUE_INIT;

  item = gtk_list_item_get_item (list_item);
  label = gtk_list_item_get_child (list_item);

  gtk_label_set_label (GTK_LABEL (label), gtk_match_object_get_string (GTK_MATCH_OBJECT (item)));
  g_value_unset (&value);
}

static void
gtk_suggestion_entry_set_popup_visible (GtkSuggestionEntry *self,
                                        gboolean            visible)
{
  if (gtk_widget_get_visible (self->popup) == visible)
    return;

  if (g_list_model_get_n_items (G_LIST_MODEL (self->selection)) == 0)
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

static void update_map (GtkSuggestionEntry *self);

static gboolean
text_changed_idle (gpointer data)
{
  GtkSuggestionEntry *self = data;
  const char *text;
  guint matches;

  if (!self->map_model)
    return G_SOURCE_REMOVE;

  text = gtk_editable_get_text (GTK_EDITABLE (self->entry));

  g_free (self->search);
  self->search = g_strdup (text);

  update_map (self);

  matches = g_list_model_get_n_items (G_LIST_MODEL (self->selection));

  gtk_suggestion_entry_set_popup_visible (self, matches > 0);

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

  item = gtk_single_selection_get_selected_item (self->selection);
  if (!item)
    return;

  g_signal_handler_block (self->entry, self->changed_id);

  gtk_editable_set_text (GTK_EDITABLE (self->entry),
                         gtk_match_object_get_string (GTK_MATCH_OBJECT (item)));

  gtk_editable_set_position (GTK_EDITABLE (self->entry), -1);

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

  if (state & (GDK_SHIFT_MASK | GDK_ALT_MASK | GDK_CONTROL_MASK))
    return FALSE;

  if (keyval == GDK_KEY_Return ||
      keyval == GDK_KEY_KP_Enter ||
      keyval == GDK_KEY_ISO_Enter)
    {
      gtk_suggestion_entry_set_popup_visible (self, FALSE);
      accept_current_selection (self);
      g_free (self->search);
      self->search = g_strdup (gtk_editable_get_text (GTK_EDITABLE (self->entry)));
      update_map (self);

      return TRUE;
    }
  else if (keyval == GDK_KEY_Escape)
    {
      if (gtk_widget_get_mapped (self->popup))
        {
          gtk_suggestion_entry_set_popup_visible (self, FALSE);

          g_signal_handler_block (self->entry, self->changed_id);

          gtk_editable_set_text (GTK_EDITABLE (self->entry), self->search ? self->search : "");

          gtk_editable_set_position (GTK_EDITABLE (self->entry), -1);

          g_signal_handler_unblock (self->entry, self->changed_id);
          return TRUE;
       }
    }
  else if (keyval == GDK_KEY_Right ||
           keyval == GDK_KEY_KP_Right)
    {
      gtk_editable_set_position (GTK_EDITABLE (self->entry), -1);
      return TRUE;
    }
  else if (keyval == GDK_KEY_Left ||
           keyval == GDK_KEY_KP_Left)
    {
      return FALSE;
    }
  else if (keyval == GDK_KEY_Tab ||
           keyval == GDK_KEY_KP_Tab ||
           keyval == GDK_KEY_ISO_Left_Tab)
    {
      gtk_suggestion_entry_set_popup_visible (self, FALSE);
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

  gtk_suggestion_entry_set_popup_visible (self, FALSE);
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

static void default_match_func (GtkMatchObject *object,
                                const char     *search,
                                gpointer        data);

static void
gtk_suggestion_entry_init (GtkSuggestionEntry *self)
{
  GtkWidget *sw;
  GtkEventController *controller;

  self->use_filter = TRUE;
  self->show_arrow = FALSE;

  self->match_func = default_match_func;
  self->match_data = NULL;
  self->destroy = NULL;

  gtk_widget_add_css_class (GTK_WIDGET (self), "suggestion");

  self->entry = gtk_text_new ();
  gtk_widget_set_parent (self->entry, GTK_WIDGET (self));
  gtk_widget_set_hexpand (self->entry, TRUE);
  gtk_editable_init_delegate (GTK_EDITABLE (self));
  self->changed_id = g_signal_connect (self->entry, "notify::text", G_CALLBACK (text_changed), self);

  self->popup = gtk_popover_new ();
  gtk_popover_set_position (GTK_POPOVER (self->popup), GTK_POS_BOTTOM);
  gtk_popover_set_autohide (GTK_POPOVER (self->popup), FALSE);
  gtk_popover_set_has_arrow (GTK_POPOVER (self->popup), FALSE);
  gtk_widget_set_halign (self->popup, GTK_ALIGN_START);
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

static void
selection_changed (GtkSingleSelection *selection,
                   GParamSpec         *pspec,
                   GtkSuggestionEntry *self)
{
  accept_current_selection (self);
}

static gboolean
filter_func (gpointer item, gpointer user_data)
{
  GtkSuggestionEntry *self = GTK_SUGGESTION_ENTRY (user_data);
  guint min_score;

  if (self->use_filter)
    min_score = 1;
  else
    min_score = 0;

  return gtk_match_object_get_score (GTK_MATCH_OBJECT (item)) >= min_score;
}

static void
default_match_func (GtkMatchObject *object,
                    const char     *search,
                    gpointer        data)
{
  char *tmp1, *tmp2, *tmp3, *tmp4;

  tmp1 = g_utf8_normalize (gtk_match_object_get_string (object), -1, G_NORMALIZE_ALL);
  tmp2 = g_utf8_casefold (tmp1, -1);

  tmp3 = g_utf8_normalize (search, -1, G_NORMALIZE_ALL);
  tmp4 = g_utf8_casefold (tmp3, -1);

  if (g_str_has_prefix (tmp2, tmp4))
    gtk_match_object_set_match (object, 0, g_utf8_strlen (search, -1), 1);
  else
    gtk_match_object_set_match (object, 0, 0, 0);

  g_free (tmp1);
  g_free (tmp2);
  g_free (tmp3);
  g_free (tmp4);
}

static gpointer
map_func (gpointer item, gpointer user_data)
{
  GtkSuggestionEntry *self = GTK_SUGGESTION_ENTRY (user_data);
  GValue value = G_VALUE_INIT;
  gpointer obj;

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
      g_critical ("Either GtkSuggestionEntry:expression must be set "
                  "or GtkSuggestionEntry:model must be a GtkStringList");
      g_value_set_string (&value, "No value");
    }

  obj = gtk_match_object_new (item, g_value_get_string (&value));

  g_value_unset (&value);

  if (self->search && self->search[0])
    self->match_func (obj, self->search, self->match_data);
  else
    gtk_match_object_set_match (obj, 0, 0, 1);

  return obj;
}

static void
update_map (GtkSuggestionEntry *self)
{
  gtk_map_list_model_set_map_func (self->map_model, map_func, self, NULL);
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
      g_clear_object (&self->map_model);
      g_clear_object (&self->filter);
    }
  else
    {
      GtkMapListModel *map_model;
      GtkFilterListModel *filter_model;
      GtkFilter *filter;
      GtkSortListModel *sort_model;
      GtkSingleSelection *selection;
      GtkSorter *sorter;

      map_model = gtk_map_list_model_new (model, NULL, NULL, NULL);
      g_set_object (&self->map_model, map_model);
      g_object_unref (map_model);

      update_map (self);

      filter = gtk_custom_filter_new (filter_func, self, NULL);
      filter_model = gtk_filter_list_model_new (G_LIST_MODEL (self->map_model), filter);
      g_set_object (&self->filter, filter);
      g_object_unref (filter);

      sorter = gtk_numeric_sorter_new (gtk_property_expression_new (GTK_TYPE_MATCH_OBJECT, NULL, "score"));
      gtk_numeric_sorter_set_sort_order (GTK_NUMERIC_SORTER (sorter), GTK_SORT_DESCENDING);
      sort_model = gtk_sort_list_model_new (G_LIST_MODEL (filter_model), sorter);
      g_object_unref (sorter);

      update_map (self);

      selection = gtk_single_selection_new (G_LIST_MODEL (sort_model));
      gtk_single_selection_set_autoselect (selection, FALSE);
      gtk_single_selection_set_can_unselect (selection, TRUE);
      gtk_single_selection_set_selected (selection, GTK_INVALID_LIST_POSITION);
      g_set_object (&self->selection, selection);
      gtk_list_view_set_model (GTK_LIST_VIEW (self->list), G_LIST_MODEL (selection));
      g_object_unref (selection);
      g_object_unref (sort_model);
      g_object_unref (filter_model);
    }

  if (self->selection)
    {
      g_signal_connect (self->selection, "notify::selected",
                        G_CALLBACK (selection_changed), self);
      selection_changed (self->selection, NULL, self);
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

  update_map (self);

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

  gtk_filter_changed (self->filter, GTK_FILTER_CHANGE_DIFFERENT);

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

static void
gtk_suggestion_entry_arrow_clicked (GtkSuggestionEntry *self)
{
  gboolean visible;

  visible = gtk_widget_get_visible (self->popup);
  gtk_suggestion_entry_set_popup_visible (self, !visible);
}

/**
 * gtk_suggestion_entry_set_show_arrow:
 * @self: a #GtkSuggestionEntry
 * @show_arrow: %TRUE to show a clickable icon
 *
 * Sets whether the GtkSuggestionEntry should show a
 * clickable icon for opening the popup with suggestions.
 */
void
gtk_suggestion_entry_set_show_arrow (GtkSuggestionEntry *self,
                                     gboolean            show_arrow)
{
  g_return_if_fail (GTK_IS_SUGGESTION_ENTRY (self));

  if (self->show_arrow == show_arrow)
    return;

  self->show_arrow = show_arrow;

  if (show_arrow)
    {
      GtkGesture *press;

      self->arrow = gtk_image_new_from_icon_name ("pan-down-symbolic");
      gtk_widget_set_tooltip_text (self->arrow, _("Show suggestions"));
      gtk_widget_set_parent (self->arrow, GTK_WIDGET (self));

      press = gtk_gesture_click_new ();
      g_signal_connect_swapped (press, "released",
                                G_CALLBACK (gtk_suggestion_entry_arrow_clicked), self);
      gtk_widget_add_controller (self->arrow, GTK_EVENT_CONTROLLER (press));

    }
  else
    {
      g_clear_pointer (&self->arrow, gtk_widget_unparent);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_ARROW]);
}

/**
 * gtk_suggestion_entry_get_show_arrow:
 * @self: a #GtkSuggestionEntry
 *
 * Gets the value set by gtk_suggestion_entry_set_show_arrow().
 *
 * Returns: %TRUE if @self is showing a arrow for suggestions
 */
gboolean
gtk_suggestion_entry_get_show_arrow (GtkSuggestionEntry *self)
{
  g_return_val_if_fail (GTK_IS_SUGGESTION_ENTRY (self), FALSE);

  return self->show_arrow;
}

void
gtk_suggestion_entry_set_match_func (GtkSuggestionEntry          *self,
                                     GtkSuggestionEntryMatchFunc  match_func,
                                     gpointer                     user_data,
                                     GDestroyNotify               destroy)
{
  if (self->destroy)
    self->destroy (self->match_data);
  self->match_func = match_func;
  self->match_data = user_data;
  self->destroy = destroy;
}

