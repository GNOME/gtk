/* GTK - The GIMP Toolkit
 * Copyright (C) 2014 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <string.h>

#include "gtkoptionlist.h"

#include "gtkbin.h"
#include "gtkbox.h"
#include "gtkbuildable.h"
#include "gtkbuilderprivate.h"
#include "gtkbutton.h"
#include "gtkcontainer.h"
#include "gtkentry.h"
#include "gtkenums.h"
#include "gtkframe.h"
#include "gtklabel.h"
#include "gtklistbox.h"
#include "gtkimage.h"
#include "gtkscrolledwindow.h"
#include "gtksearchbar.h"
#include "gtksearchentry.h"
#include "gtkseparator.h"
#include "gtkstack.h"
#include "gtktypebuiltins.h"
#include "gtkintl.h"
#include "gtkprivate.h"


/**
 * SECTION:gtkoptionlist
 * @Short_description: A simple text-only choice
 * @Title: GtkOptionList
 * @See_also: #GtkOptionListBoxText, #GtkOptionButton
 *
 * A GtkOptionList is a simple variant of a choice widget that hides
 * the model-view complexity of #GtkOptionListBox.
 *
 * To create a GtkOptionList, use gtk_option_list_new().
 *
 * You can add items to a GtkOptionList using gtk_option_list_add_item()
 * and remove them with gtk_option_list_remove_item(). Each item has an ID
 * that is returned as part of of the #GtkOptionList:selected property when
 * the item is currently selected. Each item also has an optional text that
 * is used to display the item, and an optional sort key that is used to
 * sort the items.
 *
 * If you want to allow the user to enter custom values, use
 * gtk_option_list_set_allow_custom().
 *
 * Items can optionally be grouped, by using gtk_option_list_item_set_group_key().
 * Groups can have display text and sort keys that are different from the
 * group ID, by using gtk_option_list_box_add_group().
 *
 * # GtkOptionList as GtkBuildable
 *
 * The GtkOptionList implementation of the GtkBuildable interface supports
 * adding items directly using the <items> element and specifying <item>
 * elements for each item. Each <item> element can specify the “id”
 * and "sort" corresponding to the appended text and also supports
 * the regular translation attributes “translatable”, “context” and
 * “comments”. Groups can be specified similarly with <groups> and
 * <group> elements, and associated with items with the “group” attribute.
 *
 * Here is a UI definition fragment specifying some GtkOptionList items:
 * |[
 * <object class="GtkOptionList">
 *   <items>
 *     <item translatable="yes" id="factory" sort="aaa">Factory</item>
 *     <item translatable="yes" id="home" sort="ccc">Home</item>
 *     <item translatable="yes" id="subway" sort="bbb" group="group1">Subway</item>
 *   </items>
 *   <groups>
 *     <group translatable="yes" id="group1">Subterranean</group>
 *   </groups>
 * </object>
 * ]|
 */


#define GTK_TYPE_OPTION_LIST_ROW    (gtk_option_list_row_get_type ())
#define GTK_OPTION_LIST_ROW(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_OPTION_LIST_ROW, GtkOptionListRow))
#define GTK_IS_OPTION_LIST_ROW(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_OPTION_LIST_ROW))

typedef struct
{
  GtkListBoxRow row;

  gchar *id;
  gchar *text;
  gchar *sort;
  gchar *group;
  gboolean inverted;
  gboolean active;

  GtkWidget *label;
  GtkWidget *check;
  GtkWidget *box;
  GtkWidget *arrow;
} GtkOptionListRow;

enum {
  ROW_PROP_ACTIVE = 1,
  ROW_PROP_ID,
  ROW_PROP_SORT,
  ROW_PROP_TEXT,
  ROW_PROP_GROUP,
  ROW_PROP_INVERTED
};

static void gtk_option_list_row_set_group       (GtkOptionListRow *row,
                                                 const gchar      *group);
static void gtk_option_list_row_set_inverted    (GtkOptionListRow *row,
                                                 gboolean          inverted);

typedef GtkListBoxRowClass GtkOptionListRowClass;

G_DEFINE_TYPE (GtkOptionListRow, gtk_option_list_row, GTK_TYPE_LIST_BOX_ROW)

static void
gtk_option_list_row_init (GtkOptionListRow *row)
{
  gtk_widget_init_template (GTK_WIDGET (row));
}

static void
gtk_option_list_row_finalize (GObject *object)
{
  GtkOptionListRow *row = (GtkOptionListRow *)object;

  g_free (row->id);
  g_free (row->text);
  g_free (row->sort);
  g_free (row->group);

  G_OBJECT_CLASS (gtk_option_list_row_parent_class)->finalize (object);
}

static void
gtk_option_list_row_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GtkOptionListRow *row = (GtkOptionListRow *)object;

  switch (prop_id)
    {
    case ROW_PROP_ACTIVE:
      row->active = g_value_get_boolean (value);
      break;
    case ROW_PROP_ID:
      g_free (row->id);
      row->id = g_value_dup_string (value);
      break;
    case ROW_PROP_TEXT:
      g_free (row->text);
      row->text = g_value_dup_string (value);
      break;
    case ROW_PROP_SORT:
      g_free (row->sort);
      row->sort = g_value_dup_string (value);
      break;
    case ROW_PROP_GROUP:
      gtk_option_list_row_set_group (row, g_value_get_string (value));
      break;
    case ROW_PROP_INVERTED:
      gtk_option_list_row_set_inverted (row, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_option_list_row_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GtkOptionListRow *row = (GtkOptionListRow *)object;

  switch (prop_id)
    {
    case ROW_PROP_ACTIVE:
      g_value_set_boolean (value, row->active);
      break;
    case ROW_PROP_ID:
      g_value_set_string (value, row->id);
      break;
    case ROW_PROP_TEXT:
      g_value_set_string (value, row->text);
      break;
    case ROW_PROP_SORT:
      g_value_set_string (value, row->sort);
      break;
    case ROW_PROP_GROUP:
      g_value_set_string (value, row->group);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_option_list_row_class_init (GtkOptionListRowClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->finalize = gtk_option_list_row_finalize;
  object_class->set_property = gtk_option_list_row_set_property;
  object_class->get_property = gtk_option_list_row_get_property;

  g_object_class_install_property (object_class,
                                   ROW_PROP_ACTIVE,
                                   g_param_spec_boolean ("active", P_("Active"), P_("Active"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   ROW_PROP_ID,
                                   g_param_spec_string ("id", P_("ID"), P_("ID"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
                                   ROW_PROP_TEXT,
                                   g_param_spec_string ("text", P_("Text"), P_("Text"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   ROW_PROP_SORT,
                                   g_param_spec_string ("sort", P_("Sort"), P_("Sort"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   ROW_PROP_GROUP,
                                   g_param_spec_string ("group", P_("Group"), P_("Group"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   ROW_PROP_INVERTED,
                                   g_param_spec_boolean ("inverted", P_("Inverted"), P_("Inverted"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/ui/gtkoptionlistrow.ui");

  gtk_widget_class_bind_template_child (widget_class, GtkOptionListRow, box);
  gtk_widget_class_bind_template_child (widget_class, GtkOptionListRow, label);
  gtk_widget_class_bind_template_child (widget_class, GtkOptionListRow, check);
}

static void
update_group (GtkOptionListRow *row)
{
  g_object_ref (row->label);
  g_object_ref (row->check);

  gtk_container_remove (GTK_CONTAINER (row->box), row->label);
  gtk_container_remove (GTK_CONTAINER (row->box), row->check);

  if (row->group)
    {
      if (!row->arrow)
        {
          row->arrow = gtk_image_new ();
          gtk_widget_show (row->arrow);
          gtk_box_pack_end (GTK_BOX (row->box), row->arrow, FALSE, FALSE, 0);
        }

      if (row->inverted)
        {
          gtk_box_set_center_widget (GTK_BOX (row->box), row->label);
          gtk_container_child_set (GTK_CONTAINER (row->box), row->arrow, "pack-type", GTK_PACK_START, NULL);
          gtk_image_set_from_icon_name (GTK_IMAGE (row->arrow), "pan-start-symbolic", GTK_ICON_SIZE_MENU);
        }
      else
        {
          gtk_box_pack_start (GTK_BOX (row->box), row->label, FALSE, FALSE, 0);
          gtk_container_child_set (GTK_CONTAINER (row->box), row->arrow, "pack-type", GTK_PACK_END, NULL);
          gtk_image_set_from_icon_name (GTK_IMAGE (row->arrow), "pan-end-symbolic", GTK_ICON_SIZE_MENU);
        }
    }
  else
    {
      if (row->arrow)
        {
          gtk_container_remove (GTK_CONTAINER (row->box), row->arrow);
          row->arrow = NULL;
        }

      gtk_box_pack_start (GTK_BOX (row->box), row->label, FALSE, FALSE, 0);
    }

  gtk_box_pack_start (GTK_BOX (row->box), row->check, FALSE, FALSE, 0);

  g_object_unref (row->label);
  g_object_unref (row->check);
}

static void
gtk_option_list_row_set_group (GtkOptionListRow *row,
                               const gchar      *group)
{
  g_free (row->group);
  row->group = g_strdup (group);
  update_group (row);
}

static void
gtk_option_list_row_set_inverted (GtkOptionListRow *row,
                                  gboolean          inverted)
{
  row->inverted = inverted;
  update_group (row);
}

static GtkWidget *
gtk_option_list_row_new_item (const gchar *id,
                              const gchar *text)
{
  return g_object_new (GTK_TYPE_OPTION_LIST_ROW, "id", id, "text", text, NULL);
}

static GtkWidget *
gtk_option_list_row_new_group (const gchar *group,
                               const gchar *text,
                               const gchar *sort)
{
  return g_object_new (GTK_TYPE_OPTION_LIST_ROW, "group", group, "text", text, "sort", sort, NULL);
}

static void
gtk_option_list_row_set_active (GtkOptionListRow *row,
                                gboolean          active)
{
  row->active = active;
  g_object_notify (G_OBJECT (row), "active");
}

static void
gtk_option_list_row_set_text (GtkOptionListRow *row,
                              const gchar      *text)
{
  g_free (row->text);
  row->text = g_strdup (text);
  g_object_notify (G_OBJECT (row), "text");
}

static void
gtk_option_list_row_set_sort (GtkOptionListRow *row,
                              const gchar      *sort)
{
  g_free (row->sort);
  row->sort = g_strdup (sort);
  g_object_notify (G_OBJECT (row), "sort");
}

static const gchar *
gtk_option_list_row_get_id (GtkOptionListRow *row)
{
  return row->id;
}

static const gchar *
gtk_option_list_row_get_text (GtkOptionListRow *row)
{
  return row->text;
}

static const gchar *
gtk_option_list_row_get_sort (GtkOptionListRow *row)
{
  return row->sort ? row->sort : gtk_option_list_row_get_text (row);
}

static const gchar *
gtk_option_list_row_get_group (GtkOptionListRow *row)
{
  return row->group;
}


/***/


static void      list_row_activated   (GtkListBox     *list,
                                       GtkListBoxRow  *row,
                                       GtkOptionList  *ol);
static void      search_changed       (GtkSearchEntry *entry,
                                       GtkOptionList  *ol);
static void      search_focus_grabbed (GtkWidget      *entry,
                                       GtkOptionList  *ol);
static void      custom_entry_done    (GtkWidget      *widget,
                                       GtkOptionList  *ol);
static void      custom_entry_changed (GObject        *entry,
                                       GParamSpec     *pspec,
                                       GtkOptionList  *ol);
static void      reset                (GtkOptionList  *ol);
static void      collapse             (GtkOptionList  *ol,
                                       GtkWidget      *list);
static void      expand               (GtkOptionList  *ol,
                                       GtkWidget      *list);
static void      list_header_func     (GtkListBoxRow  *row,
                                       GtkListBoxRow  *before,
                                       gpointer        data);
static gboolean  list_filter_func     (GtkListBoxRow  *row,
                                       gpointer        data);
static gint      list_sort_func       (GtkListBoxRow  *row1,
                                       GtkListBoxRow  *row2,
                                       gpointer        data);
static void      custom_header_func   (GtkListBoxRow  *row,
                                       GtkListBoxRow  *before,
                                       gpointer        data);
static GtkWidget *group_get_list      (GtkOptionList  *ol,
                                       const gchar    *group);
static GtkWidget *group_get_header    (GtkOptionList  *ol,
                                       const gchar    *group);
static GtkWidget *group_get_item      (GtkOptionList  *ol,
                                       const gchar    *group);
static void       ensure_group        (GtkOptionList  *ol,
                                       const gchar    *group);
static void       set_selected        (GtkOptionList  *ol,
                                       const gchar   **ids);


struct _GtkOptionList
{
  GtkBin parent;

  GPtrArray *selected;
  gchar *custom_text;
  gboolean allow_custom;
  GtkSelectionMode selection_mode;

  GtkWidget *scrolled_window;
  GtkWidget *list;
  GtkWidget *search_revealer;
  GtkWidget *search_entry;
  GtkWidget *stack;
  GtkWidget *show_more;
  GtkWidget *add_custom;
  GtkWidget *back_to_list;
  GtkWidget *custom_entry;
  GtkWidget *custom_button;
  GtkWidget *custom;
};

struct _GtkOptionListClass
{
  GtkBinClass parent_class;
};

enum {
  PROP_SELECTED = 1,
  PROP_ALLOW_CUSTOM,
  PROP_CUSTOM_TEXT,
  PROP_SELECTION_MODE
};

static GtkBuildableIface *buildable_parent_iface = NULL;

static void gtk_option_list_buildable_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkOptionList, gtk_option_list, GTK_TYPE_BIN,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_option_list_buildable_init))

static void
gtk_option_list_init (GtkOptionList *list)
{
  g_type_ensure (GTK_TYPE_OPTION_LIST_ROW);

  list->selected = g_ptr_array_new ();
  g_ptr_array_add (list->selected, NULL);

  list->selection_mode = GTK_SELECTION_BROWSE;

  gtk_widget_init_template (GTK_WIDGET (list));

  gtk_list_box_set_header_func (GTK_LIST_BOX (list->list), list_header_func, list, NULL);
  gtk_list_box_set_filter_func (GTK_LIST_BOX (list->list), list_filter_func, list, NULL);
  gtk_list_box_set_sort_func (GTK_LIST_BOX (list->list), list_sort_func, list, NULL);

  gtk_list_box_set_header_func (GTK_LIST_BOX (list->custom), custom_header_func, list, NULL);

  g_object_set_data (G_OBJECT (list->list), "show-more-item", list->show_more);

  reset (list);
}

static void
gtk_option_list_finalize (GObject *object)
{
  GtkOptionList *ol = GTK_OPTION_LIST (object);

  g_free (ol->custom_text);
  g_ptr_array_free (ol->selected, TRUE);

  G_OBJECT_CLASS (gtk_option_list_parent_class)->finalize (object);
}

static void
gtk_option_list_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkOptionList *ol = (GtkOptionList *)object;

  switch (prop_id)
    {
    case PROP_SELECTED:
      set_selected (ol, (const gchar **)g_value_get_boxed (value));
      break;
    case PROP_ALLOW_CUSTOM:
      gtk_option_list_set_allow_custom (ol, g_value_get_boolean (value));
      break;
    case PROP_CUSTOM_TEXT:
      gtk_option_list_set_custom_text (ol, g_value_get_string (value));
      break;
    case PROP_SELECTION_MODE:
      gtk_option_list_set_selection_mode (ol, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_option_list_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkOptionList *ol = (GtkOptionList *)object;

  switch (prop_id)
    {
    case PROP_SELECTED:
      g_value_set_boxed (value, (gconstpointer)ol->selected->pdata);
      break;
    case PROP_ALLOW_CUSTOM:
      g_value_set_boolean (value, gtk_option_list_get_allow_custom (ol));
      break;
    case PROP_CUSTOM_TEXT:
      g_value_set_string (value, gtk_option_list_get_custom_text (ol));
      break;
    case PROP_SELECTION_MODE:
      g_value_set_enum (value, gtk_option_list_get_selection_mode (ol));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_option_list_class_init (GtkOptionListClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->finalize = gtk_option_list_finalize;
  object_class->set_property = gtk_option_list_set_property;
  object_class->get_property = gtk_option_list_get_property;

  /**
   * GtkOptionList:selected:
   *
   * The IDs of the currently selected items, as a
   * %NULL-terminated array of strings.
   */
  g_object_class_install_property (object_class,
                                   PROP_SELECTED,
                                   g_param_spec_boxed ("selected",
                                                       P_("Selected items"),
                                                       P_("The IDs of the selected items"),
                                                       G_TYPE_STRV,
                                                       GTK_PARAM_READWRITE));

  /**
   * GtkOptionList:allow-custom:
   *
   * Whether the combo should allow the user to enter
   * custom values.
   */
  g_object_class_install_property (object_class,
                                   PROP_ALLOW_CUSTOM,
                                   g_param_spec_boolean ("allow-custom",
                                                         P_("Allow custom"),
                                                         P_("Allow custom items"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkOptionList:custom-text:
   *
   * The text that is displayed for the custom entry.
   */
  g_object_class_install_property (object_class,
                                   PROP_CUSTOM_TEXT,
                                   g_param_spec_string ("custom-text",
                                                        P_("Custom text"),
                                                        P_("Text to show for the custom entry"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_SELECTION_MODE,
                                   g_param_spec_enum ("selection-mode",
                                                      P_("Selection mode"),
                                                      P_("The selection mode"),
                                                      GTK_TYPE_SELECTION_MODE,
                                                      GTK_SELECTION_BROWSE,
                                                      GTK_PARAM_READWRITE));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/ui/gtkoptionlist.ui");

  gtk_widget_class_bind_template_child (widget_class, GtkOptionList, scrolled_window);
  gtk_widget_class_bind_template_child (widget_class, GtkOptionList, list);
  gtk_widget_class_bind_template_child (widget_class, GtkOptionList, search_revealer);
  gtk_widget_class_bind_template_child (widget_class, GtkOptionList, search_entry);
  gtk_widget_class_bind_template_child (widget_class, GtkOptionList, show_more);
  gtk_widget_class_bind_template_child (widget_class, GtkOptionList, add_custom);
  gtk_widget_class_bind_template_child (widget_class, GtkOptionList, stack);
  gtk_widget_class_bind_template_child (widget_class, GtkOptionList, back_to_list);
  gtk_widget_class_bind_template_child (widget_class, GtkOptionList, custom_entry);
  gtk_widget_class_bind_template_child (widget_class, GtkOptionList, custom_button);
  gtk_widget_class_bind_template_child (widget_class, GtkOptionList, custom);
  gtk_widget_class_bind_template_callback (widget_class, list_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, search_changed);
  gtk_widget_class_bind_template_callback (widget_class, search_focus_grabbed);
  gtk_widget_class_bind_template_callback (widget_class, custom_entry_done);
  gtk_widget_class_bind_template_callback (widget_class, custom_entry_changed);
  gtk_widget_class_bind_template_callback (widget_class, reset);
}


/***/


typedef struct {
  GtkBuilder    *builder;
  GObject       *object;
  const gchar   *domain;
  gchar         *id;
  gchar         *sort;
  gchar         *group;
  GString       *string;
  gchar         *context;
  guint          translatable : 1;
  guint          is_text      : 1;
  guint          is_group     : 1;
} OptionListParserData;

static void
option_list_start_element (GMarkupParseContext *context,
                           const gchar         *element_name,
                           const gchar        **names,
                           const gchar        **values,
                           gpointer             user_data,
                           GError             **error)
{
  OptionListParserData *data = (OptionListParserData*)user_data;
  guint i;

  if ((strcmp (element_name, "item") == 0 && !data->is_group) ||
      (strcmp (element_name, "group") == 0 && data->is_group))
    {
      data->is_text = TRUE;
      for (i = 0; names[i]; i++)
        {
          if (strcmp (names[i], "translatable") == 0)
            {
              gboolean bval;

              if (!_gtk_builder_boolean_from_string (values[i], &bval, error))
                return;

              data->translatable = bval;
            }
          else if (strcmp (names[i], "comments") == 0)
            {
              /* do nothing, comments are for translators */
            }
          else if (strcmp (names[i], "context") == 0)
            data->context = g_strdup (values[i]);
          else if (strcmp (names[i], "id") == 0)
            data->id = g_strdup (values[i]);
          else if (strcmp (names[i], "sort") == 0)
            data->sort = g_strdup (values[i]);
          else if (strcmp (names[i], "group") == 0 && !data->is_group)
            data->group = g_strdup (values[i]);
          else
            g_warning ("Unknown custom GtkOptionList item attribute: %s", names[i]);
        }
    }
}

static void
option_list_text (GMarkupParseContext *context,
                  const gchar         *text,
                  gsize                text_len,
                  gpointer             user_data,
                  GError             **error)
{
  OptionListParserData *data = (OptionListParserData*)user_data;

  if (data->is_text)
    g_string_append_len (data->string, text, text_len);
}

static void
option_list_end_element (GMarkupParseContext *context,
                         const gchar         *element_name,
                         gpointer             user_data,
                         GError             **error)
{
  OptionListParserData *data = (OptionListParserData*)user_data;

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

      if (data->is_group)
        gtk_option_list_add_group (GTK_OPTION_LIST (data->object), data->id, data->string->str, data->sort);
      else
        {
          gtk_option_list_add_item (GTK_OPTION_LIST (data->object), data->id, data->string->str);
          if (data->sort)
            gtk_option_list_item_set_sort_key (GTK_OPTION_LIST (data->object), data->id, data->sort);
          if (data->group)
            gtk_option_list_item_set_group_key (GTK_OPTION_LIST (data->object), data->id, data->group);
        }
    }

  data->translatable = FALSE;
  g_string_set_size (data->string, 0);
  g_free (data->context);
  data->context = NULL;
  g_free (data->id);
  data->id = NULL;
  g_free (data->sort);
  data->sort = NULL;
  g_free (data->group);
  data->group = NULL;
  data->is_text = FALSE;
}

static const GMarkupParser option_list_parser =
{
  option_list_start_element,
  option_list_end_element,
  option_list_text
};

static gboolean
gtk_option_list_buildable_custom_tag_start (GtkBuildable  *buildable,
                                            GtkBuilder    *builder,
                                            GObject       *child,
                                            const gchar   *tagname,
                                            GMarkupParser *parser,
                                            gpointer      *data)
{
  if (buildable_parent_iface->custom_tag_start (buildable, builder, child,
                                                tagname, parser, data))
    return TRUE;

  if (strcmp (tagname, "items") == 0 ||
      strcmp (tagname, "groups") == 0)
    {
      OptionListParserData *parser_data;

      parser_data = g_slice_new0 (OptionListParserData);
      parser_data->builder = g_object_ref (builder);
      parser_data->object = g_object_ref (buildable);
      parser_data->domain = gtk_builder_get_translation_domain (builder);
      parser_data->string = g_string_new ("");
      parser_data->is_group = tagname[0] == 'g';
      *parser = option_list_parser;
      *data = parser_data;

      return TRUE;
    }

  return FALSE;
}

static void
gtk_option_list_buildable_custom_finished (GtkBuildable *buildable,
                                           GtkBuilder   *builder,
                                           GObject      *child,
                                           const gchar  *tagname,
                                           gpointer      user_data)
{
  OptionListParserData *data;

  buildable_parent_iface->custom_finished (buildable, builder, child,
                                           tagname, user_data);

  if (strcmp (tagname, "items") == 0 ||
      strcmp (tagname, "groups") == 0)
    {
      data = (OptionListParserData*)user_data;

      g_object_unref (data->object);
      g_object_unref (data->builder);
      g_string_free (data->string, TRUE);
      g_slice_free (OptionListParserData, data);
    }
}

static void
gtk_option_list_buildable_init (GtkBuildableIface *iface)
{
  buildable_parent_iface = g_type_interface_peek_parent (iface);

  iface->custom_tag_start = gtk_option_list_buildable_custom_tag_start;
  iface->custom_finished = gtk_option_list_buildable_custom_finished;
}


/***/


static void
list_header_func (GtkListBoxRow *row,
                  GtkListBoxRow *before,
                  gpointer       data)
{
  if (before != NULL)
    {
      if (gtk_list_box_row_get_header (row) == NULL)
        gtk_list_box_row_set_header (row, gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
    }
  else
    {
      gtk_list_box_row_set_header (row, NULL);
    }
}

static gboolean
list_filter_func (GtkListBoxRow *row,
                  gpointer       data)
{
  GtkOptionList *ol = data;
  const gchar *text;
  gchar *row_text;
  gchar *search_text;
  gboolean ret;

  text = gtk_entry_get_text (GTK_ENTRY (ol->search_entry));
  if (text[0] == '\0')
    return TRUE;

  if (!GTK_IS_OPTION_LIST_ROW (row))
    return TRUE;

  if (gtk_option_list_row_get_group (GTK_OPTION_LIST_ROW (row)))
    return TRUE;

  row_text = g_utf8_strdown (gtk_option_list_row_get_text (GTK_OPTION_LIST_ROW (row)), -1);
  search_text = g_utf8_strdown (text, -1);
  ret = strstr (row_text, search_text) != NULL;
  g_free (row_text);
  g_free (search_text);

  return ret;
}

static GtkWidget *
list_get_show_more_item (GtkWidget *list)
{
  return (GtkWidget*)g_object_get_data (G_OBJECT (list), "show-more-item");
}

static gint
list_sort_func (GtkListBoxRow *row1,
                GtkListBoxRow *row2,
                gpointer       data)
{
  GtkWidget *show_more;
  const gchar *sort1;
  const gchar *sort2;

  if (row1 == row2)
    return 0;

  sort1 = NULL;
  sort2 = NULL;

  if (GTK_IS_OPTION_LIST_ROW (row1))
    {
      if (g_strcmp0 (gtk_option_list_row_get_group (GTK_OPTION_LIST_ROW (row1)), "list") == 0 ||
          g_strcmp0 (gtk_option_list_row_get_group (GTK_OPTION_LIST_ROW (row1)), "custom") == 0)
        return -1;

      sort1 = gtk_option_list_row_get_sort (GTK_OPTION_LIST_ROW (row1));
    }

  if (GTK_IS_OPTION_LIST_ROW (row2))
    {
      if (g_strcmp0 (gtk_option_list_row_get_group (GTK_OPTION_LIST_ROW (row2)), "list") == 0 ||
          g_strcmp0 (gtk_option_list_row_get_group (GTK_OPTION_LIST_ROW (row2)), "custom") == 0)
        return 1;

      sort2 = gtk_option_list_row_get_sort (GTK_OPTION_LIST_ROW (row2));
    }

  if (sort1 && sort2)
    return g_strcmp0 (sort1, sort2);

  show_more = list_get_show_more_item (gtk_widget_get_parent (GTK_WIDGET (row1)));

  if ((GtkWidget*)row1 == show_more)
    return 1;

  if ((GtkWidget*)row2 == show_more)
    return -1;

  return 0;
}

static void
custom_header_func (GtkListBoxRow *row,
                    GtkListBoxRow *before,
                    gpointer       data)
{
  GtkOptionList *ol = data;
  GtkWidget *ret = NULL;

  if ((GtkWidget*)before == ol->back_to_list &&
      !gtk_list_box_row_get_header (row))
    {
      ret = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_list_box_row_set_header (row, ret);
    }
}

static void
search_changed (GtkSearchEntry *entry,
                GtkOptionList  *ol)
{
  expand (ol, ol->list);
  gtk_list_box_invalidate_filter (GTK_LIST_BOX (ol->list));
}

static void
search_focus_grabbed (GtkWidget     *entry,
                      GtkOptionList *ol)
{
  gtk_editable_select_region (GTK_EDITABLE (ol->search_entry), -1, -1);
}

static void
reset (GtkOptionList *ol)
{
  gtk_stack_set_visible_child_name (GTK_STACK (ol->stack), "list");
  gtk_entry_set_text (GTK_ENTRY (ol->search_entry), "");
  gtk_widget_hide (ol->search_revealer);
  collapse (ol, ol->list);
  gtk_entry_set_text (GTK_ENTRY (ol->custom_entry), "");
}

typedef struct
{
  GtkOptionList *ol;
  const gchar *id;
  GtkWidget *row;
  GtkWidget *group_row;
} ForeachData;

static void
find_in_group_cb (GtkWidget *row,
                  gpointer   data)
{
  ForeachData *d = data;
  const gchar *id;

  if (!GTK_IS_OPTION_LIST_ROW (row))
    return;

  if (gtk_option_list_row_get_group (GTK_OPTION_LIST_ROW (row)))
    return;

  id = gtk_option_list_row_get_id (GTK_OPTION_LIST_ROW (row));

  if (g_strcmp0 (id, d->id) == 0)
    d->row = row;
}

static void
find_item_cb (GtkWidget *row,
              gpointer   data)
{
  ForeachData *d = data;
  const gchar *id;
  const gchar *group;

  if (d->row)
    return;

  if (!GTK_IS_OPTION_LIST_ROW (row))
    return;

  group = gtk_option_list_row_get_group (GTK_OPTION_LIST_ROW (row));
  if (group &&
      g_strcmp0 (group, "list") != 0 &&
      g_strcmp0 (group, "custom") != 0)
    {
      GtkWidget *list;

      list = group_get_list (d->ol, group);
      gtk_container_foreach (GTK_CONTAINER (list), find_in_group_cb, d);

      if (d->row)
        d->group_row = row;
    }

  if (d->row)
    return;

  id = gtk_option_list_row_get_id (GTK_OPTION_LIST_ROW (row));
  if (g_strcmp0 (id, d->id) == 0)
    d->row = row;
}

static gboolean
find_item (GtkOptionList  *ol,
           const gchar    *id,
           GtkWidget     **item,
           GtkWidget     **group)
{
  ForeachData data;

  data.ol = ol;
  data.id = id;
  data.row = NULL;
  data.group_row = NULL;
  gtk_container_foreach (GTK_CONTAINER (ol->list), find_item_cb, &data);

  if (item)
    *item = data.row;
  if (group)
    *group = data.group_row;

  return data.row != NULL;
}

static void
count_items (GtkWidget *widget,
             gpointer   data)
{
  gint *count = data;

  if (GTK_IS_OPTION_LIST_ROW (widget) &&
      gtk_option_list_row_get_id (GTK_OPTION_LIST_ROW (widget)))
    (*count)++;
}

static gboolean
remove_from_list (GtkOptionList *ol,
                  const gchar   *id)
{
  GtkWidget *item;
  GtkWidget *group;
  GtkWidget *list;
  GtkWidget *tab;

  if (!find_item (ol, id, &item, &group))
    return FALSE;

  list = gtk_widget_get_parent (item);
  gtk_container_remove (GTK_CONTAINER (list), item);

  if (group)
    {
      gint count = 0;
      gtk_container_foreach (GTK_CONTAINER (list), count_items, &count);
      if (count == 0)
        {
          tab = gtk_widget_get_ancestor (list, GTK_TYPE_FRAME);
          gtk_container_remove (GTK_CONTAINER (ol->stack), tab);
          gtk_container_remove (GTK_CONTAINER (ol->list), group);
        }
      else
        collapse (ol, list);
     }
  else
    collapse (ol, list);

  return TRUE;
}

static void
list_row_activated (GtkListBox    *list,
                    GtkListBoxRow *row,
                    GtkOptionList *ol)
{
  const gchar *group;
  const gchar *id;

  if ((GtkWidget*)row == list_get_show_more_item (GTK_WIDGET (list)))
    {
      expand (ol, GTK_WIDGET (list));
      return;
    }

  if ((GtkWidget*)row == ol->add_custom)
    {
      gtk_stack_set_visible_child_name (GTK_STACK (ol->stack), "custom");
      gtk_widget_grab_focus (ol->custom_entry);
      return;
    }

  if (!GTK_IS_OPTION_LIST_ROW (row))
    return;

  group = gtk_option_list_row_get_group (GTK_OPTION_LIST_ROW (row));
  if (group)
    {
      if (g_strcmp0 (group, "list") != 0)
        collapse (ol, group_get_list (ol, group));
      gtk_stack_set_visible_child_name (GTK_STACK (ol->stack), group);
      return;
    }

  id = gtk_option_list_row_get_id (GTK_OPTION_LIST_ROW (row));
  if (GTK_OPTION_LIST_ROW (row)->active)
    {
      if (ol->selection_mode != GTK_SELECTION_BROWSE)
        gtk_option_list_unselect_item (ol, id);
    }
  else
    {
      gtk_option_list_select_item (ol, id);
    }
}

static void
custom_entry_done (GtkWidget     *widget,
                   GtkOptionList *ol)
{
  const gchar *text;

  text = gtk_entry_get_text (GTK_ENTRY (ol->custom_entry));
  gtk_option_list_add_item (ol, text, text);
  gtk_option_list_select_item (ol, text);
  reset (ol);
}

static void
custom_entry_changed (GObject       *entry,
                      GParamSpec    *pspec,
                      GtkOptionList *ol)
{
  const gchar *text;

  text = gtk_entry_get_text (GTK_ENTRY (ol->custom_entry));
  gtk_widget_set_sensitive (ol->custom_button, text[0] != '\0');
}

static void
update_scrolling (GtkWidget *list,
                  gboolean   allow)
{
  GtkPolicyType policy;
  GtkWidget *sw;

  sw = gtk_widget_get_ancestor (list, GTK_TYPE_SCROLLED_WINDOW);
  g_object_get (sw, "vscrollbar-policy", &policy, NULL);

  if ((allow && policy == GTK_POLICY_AUTOMATIC) ||
      (!allow && policy == GTK_POLICY_NEVER))
    return;

  if (allow)
    {
      gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (sw),
                                                  gtk_widget_get_allocated_height (list));
      g_object_set (sw, "vscrollbar-policy", GTK_POLICY_AUTOMATIC, NULL);
    }
  else
    {
      gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (sw), -1);
      g_object_set (sw, "vscrollbar-policy", GTK_POLICY_NEVER, NULL);
    }
}

static void
show_few (GtkWidget *widget,
          gpointer   data)
{
  gint *count = data;

  if (!GTK_IS_OPTION_LIST_ROW (widget) ||
      gtk_option_list_row_get_group (GTK_OPTION_LIST_ROW (widget)) != NULL)
    return;

  if (*count < 6)
    gtk_widget_show (widget);
  else
    gtk_widget_hide (widget);

  (*count)++;
}

static void
collapse (GtkOptionList *ol,
          GtkWidget     *list)
{
  gint count;
  GtkWidget *show_more;

  show_more = list_get_show_more_item (list);
  count = 0;
  gtk_container_foreach (GTK_CONTAINER (list), show_few, &count);
  if (count < 7)
    {
      if (list == ol->list)
        gtk_revealer_set_reveal_child (GTK_REVEALER (ol->search_revealer), FALSE);
      gtk_widget_hide (show_more);
    }
  else
    {
      if (list == ol->list)
        {
          gtk_widget_show (ol->search_revealer);
          gtk_revealer_set_reveal_child (GTK_REVEALER (ol->search_revealer), TRUE);
        }
      gtk_widget_show (show_more);
    }

  update_scrolling (list, FALSE);
}

static void
show_all (GtkWidget *widget,
          gpointer   data)
{
  if (!GTK_IS_OPTION_LIST_ROW (widget) ||
      gtk_option_list_row_get_group (GTK_OPTION_LIST_ROW (widget)) != NULL)
    return;

  gtk_widget_show (widget);
}

static void
expand (GtkOptionList *ol,
        GtkWidget     *list)
{
  GtkWidget *show_more;

  show_more = list_get_show_more_item (list);
  if (gtk_widget_get_visible (show_more))
    {
      gtk_container_foreach (GTK_CONTAINER (list), show_all, NULL);
      gtk_widget_hide (show_more);
      update_scrolling (list, TRUE);
    }
}

static gboolean
is_keynav_event (GdkEvent *event,
                 guint     keyval)
{
  GdkModifierType state = 0;

  gdk_event_get_state (event, &state);

  if (keyval == GDK_KEY_Tab       || keyval == GDK_KEY_KP_Tab ||
      keyval == GDK_KEY_Up        || keyval == GDK_KEY_KP_Up ||
      keyval == GDK_KEY_Down      || keyval == GDK_KEY_KP_Down ||
      keyval == GDK_KEY_Left      || keyval == GDK_KEY_KP_Left ||
      keyval == GDK_KEY_Right     || keyval == GDK_KEY_KP_Right ||
      keyval == GDK_KEY_Home      || keyval == GDK_KEY_KP_Home ||
      keyval == GDK_KEY_End       || keyval == GDK_KEY_KP_End ||
      keyval == GDK_KEY_Page_Up   || keyval == GDK_KEY_KP_Page_Up ||
      keyval == GDK_KEY_Page_Down || keyval == GDK_KEY_KP_Page_Down ||
      ((state & (GDK_CONTROL_MASK | GDK_MOD1_MASK)) != 0))
        return TRUE;

 /* Other navigation events should get automatically
  * ignored as they will not change the content of the entry
  */

  return FALSE;
}

static void
preedit_changed_cb (GtkEntry  *entry,
                    GtkWidget *popup,
                    gboolean  *preedit_changed)
{
  *preedit_changed = TRUE;
}

static gboolean
handle_key_event (GtkOptionList *ol,
                  GdkEvent      *event)
{
  guint keyval;
  gboolean handled;
  gboolean preedit_changed;
  guint preedit_change_id;
  gboolean res;
  gchar *old_text, *new_text;

  if (g_strcmp0 (gtk_stack_get_visible_child_name (GTK_STACK (ol->stack)), "list") != 0 ||
      !gdk_event_get_keyval (event, &keyval) ||
      is_keynav_event (event, keyval) ||
      keyval == GDK_KEY_space ||
      keyval == GDK_KEY_Menu)
    return GDK_EVENT_PROPAGATE;

  if (!gtk_widget_get_realized (ol->search_entry))
    gtk_widget_realize (ol->search_entry);

  handled = GDK_EVENT_PROPAGATE;
  preedit_changed = FALSE;
  preedit_change_id = g_signal_connect (ol->search_entry, "preedit-changed",
                                        G_CALLBACK (preedit_changed_cb), &preedit_changed);

  old_text = g_strdup (gtk_entry_get_text (GTK_ENTRY (ol->search_entry)));
  res = gtk_widget_event (ol->search_entry, event);
  new_text = g_strdup (gtk_entry_get_text (GTK_ENTRY (ol->search_entry)));

  g_signal_handler_disconnect (ol->search_entry, preedit_change_id);

  if ((res && g_strcmp0 (new_text, old_text) != 0) || preedit_changed)
    {
      handled = GDK_EVENT_STOP;
      gtk_widget_show (ol->search_revealer);
      gtk_revealer_set_reveal_child (GTK_REVEALER (ol->search_revealer), TRUE);
      gtk_entry_grab_focus_without_selecting (GTK_ENTRY (ol->search_entry));
    }

  g_free (old_text);
  g_free (new_text);

  return handled;
}

static GtkWidget *
group_get_list (GtkOptionList *ol,
                const gchar   *group)
{
  GtkWidget *tab;

  tab = gtk_stack_get_child_by_name (GTK_STACK (ol->stack), group);
  g_return_val_if_fail (tab != NULL, NULL);
  return (GtkWidget*)g_object_get_data (G_OBJECT (tab), "list");
}

static GtkWidget *
group_get_header (GtkOptionList *ol,
                  const gchar   *group)
{
  GtkWidget *tab;

  tab = gtk_stack_get_child_by_name (GTK_STACK (ol->stack), group);
  g_return_val_if_fail (tab != NULL, NULL);
  return (GtkWidget*)g_object_get_data (G_OBJECT (tab), "header");
}

static GtkWidget *
group_get_item (GtkOptionList *ol,
                const gchar   *group)
{
  GtkWidget *tab;

  tab = gtk_stack_get_child_by_name (GTK_STACK (ol->stack), group);
  g_return_val_if_fail (tab != NULL, NULL);
  return (GtkWidget*)g_object_get_data (G_OBJECT (tab), "item");
}

static void
ensure_group (GtkOptionList *ol,
              const gchar   *group)
{
  GtkWidget *tab;

  tab = gtk_stack_get_child_by_name (GTK_STACK (ol->stack), group);
  if (tab == NULL)
    {
      GtkBuilder *builder;
      GtkWidget *list;
      GtkWidget *item;
      GtkWidget *header;

      builder = gtk_builder_new_from_resource ("/org/gtk/libgtk/ui/gtkoptionlisttab.ui");
      tab = (GtkWidget*)gtk_builder_get_object (builder, "tab");
      list = (GtkWidget*)gtk_builder_get_object (builder, "list");
      header = (GtkWidget*)gtk_builder_get_object (builder, "header");
      item = (GtkWidget*)gtk_builder_get_object (builder, "show_more");
      gtk_stack_add_named (GTK_STACK (ol->stack), tab, group);
      g_object_unref (builder);

      g_object_set_data (G_OBJECT (tab), "list", list);
      g_object_set_data (G_OBJECT (tab), "header", header);
      g_object_set_data (G_OBJECT (list), "show-more-item", item);

      g_signal_connect (list, "row-activated", G_CALLBACK (list_row_activated), ol);
      gtk_list_box_set_sort_func (GTK_LIST_BOX (list), list_sort_func, ol, NULL);
      gtk_list_box_set_header_func (GTK_LIST_BOX (list), list_header_func, ol, NULL);

      gtk_option_list_row_set_text (GTK_OPTION_LIST_ROW (header), group);

      item = gtk_option_list_row_new_group (group, group, NULL);
      gtk_list_box_insert (GTK_LIST_BOX (ol->list), item, -1);

      g_object_set_data (G_OBJECT (tab), "item", item);
    }
}


/***/


/**
 * gtk_option_list_new:
 *
 * Creates a new #GtkOptionList.
 *
 * Returns: A new #GtkOptionList
 *
 * Since: 3.16
 */
GtkWidget *
gtk_option_list_new (void)
{
  return g_object_new (GTK_TYPE_OPTION_LIST, NULL);
}

/**
 * gtk_option_list_get_selected_items:
 * @list: a #GtkOptionList
 *
 * Gets the IDs of the currently selected items.
 *
 * Returns: (transfer none): the IDs of the selected items, as
 *   a %NULL-terminated array of strings.
 *
 * Since: 3.16
 */
const gchar **
gtk_option_list_get_selected_items (GtkOptionList *list)
{
  g_return_val_if_fail (GTK_IS_OPTION_LIST (list), NULL);

  return (const gchar **)list->selected->pdata;
}

typedef struct
{
  GtkOptionList *ol;
  gint count;
} UpdateActiveData;

static void
update_active_cb (GtkWidget *row,
                  gpointer   data)
{
  UpdateActiveData *d = data;
  const gchar *group;
  const gchar *id;

  if (!GTK_IS_OPTION_LIST_ROW (row))
    return;

  group = gtk_option_list_row_get_group (GTK_OPTION_LIST_ROW (row));
  if (group &&
      g_strcmp0 (group, "list") != 0 &&
      g_strcmp0 (group, "custom") != 0)
    {
      GtkWidget *list;
      UpdateActiveData d2;

      d2.ol = d->ol;
      d2.count = 0;

      list = group_get_list (d->ol, group);
      gtk_container_foreach (GTK_CONTAINER (list), update_active_cb, &d2);

      gtk_option_list_row_set_active (GTK_OPTION_LIST_ROW (row), d2.count > 0);
      d->count += d2.count;
    }

  id = gtk_option_list_row_get_id (GTK_OPTION_LIST_ROW (row));
  if (id != NULL)
    {
      gint i;

      for (i = 0; i < d->ol->selected->len; i++)
        {
          if (g_ptr_array_index (d->ol->selected, i) == id)
            break;
        }
      gtk_option_list_row_set_active (GTK_OPTION_LIST_ROW (row), i < d->ol->selected->len);
      d->count += i < d->ol->selected->len ? 1 : 0;
    }
}

static void
update_active_rows (GtkOptionList *ol)
{
  UpdateActiveData data;

  data.ol = ol;
  data.count = 0;
  gtk_container_foreach (GTK_CONTAINER (ol->list), update_active_cb, &data);
  g_assert (data.count + 1 == ol->selected->len);
}

static void
set_selected (GtkOptionList  *ol,
              const gchar   **ids)
{
  gint i;
  GtkWidget *item;
  const gchar *id;

  g_ptr_array_set_size (ol->selected, 0);
  for (i = 0; ids[i]; i++)
    {
      find_item (ol, ids[i], &item, NULL);
      if (item == NULL)
        {
          g_warning ("GtkOptionList: no item with ID '%s' found", ids[i]);
          continue;
        }

      id = gtk_option_list_row_get_id (GTK_OPTION_LIST_ROW (item));

      if (i > 0 && ol->selection_mode != GTK_SELECTION_MULTIPLE)
        {
          g_warning ("GtkOptionList: ignoring extra item '%s' in single selection mode", id);
          continue;
        }

      g_ptr_array_add (ol->selected, (gpointer)id);
    }

  g_ptr_array_add (ol->selected, NULL);

  update_active_rows (ol);
  g_object_notify (G_OBJECT (ol), "selected");
}

/**
 * gtk_option_list_select_item:
 * @list: a #GtkOptionList
 * @id: the ID to select
 *
 * Selects the item with the given ID. Depending
 * on the seleciton mode, this may cause other
 * items to be deselected.
 *
 * Since: 3.16
 */
void
gtk_option_list_select_item (GtkOptionList *list,
                             const gchar   *id)
{
  GtkWidget *item;
  gint i;

  g_return_if_fail (GTK_IS_OPTION_LIST (list));
  g_return_if_fail (id != NULL);

  find_item (list, id, &item, NULL);
  if (item == NULL)
    {
      g_warning ("GtkOptionList: no item with ID '%s' found", id);
      return;
    }

  id = gtk_option_list_row_get_id (GTK_OPTION_LIST_ROW (item));

  for (i = 0; i < list->selected->len; i++)
    {
      if (g_ptr_array_index (list->selected, i) == id)
        return;
    }

  if (list->selection_mode != GTK_SELECTION_MULTIPLE)
    g_ptr_array_set_size (list->selected, 1);

  g_ptr_array_index (list->selected, list->selected->len - 1) = (gpointer)id;
  g_ptr_array_add (list->selected, NULL);

  update_active_rows (list);
  g_object_notify (G_OBJECT (list), "selected");
}

/**
 * gtk_option_list_unselect_item:
 * @list: a #GtkOptionList
 * @id: the ID to unselect
 *
 * Deselects the item with the given ID. Depending
 * on the seleciton mode, this may or may not be
 * possible.
 *
 * Since: 3.16
 */
void
gtk_option_list_unselect_item (GtkOptionList *list,
                               const gchar   *id)
{
  GtkWidget *item;
  gint i;

  g_return_if_fail (GTK_IS_OPTION_LIST (list));
  g_return_if_fail (id != NULL);

  find_item (list, id, &item, NULL);
  if (item == NULL)
    {
      g_warning ("GtkOptionList: no item with ID '%s' found", id);
      return;
    }

  id = gtk_option_list_row_get_id (GTK_OPTION_LIST_ROW (item));

  for (i = 0; i < list->selected->len; i++)
    {
      if (g_ptr_array_index (list->selected, i) == id)
        break;
    }
  if (i == list->selected->len)
    return;

  g_ptr_array_remove_index (list->selected, i);

  update_active_rows (list);
  g_object_notify (G_OBJECT (list), "selected");
}

/**
 * gtk_option_list_add_item:
 * @list: a #GtkOptionList
 * @id: the ID for the item to add
 * @text: the text to display for the item
 *
 * Adds an item to the list.
 *
 * If an item with this ID already exists, its display text
 * will be updated with the new values.
 *
 * Since: 3.16
 */
void
gtk_option_list_add_item (GtkOptionList *list,
                          const gchar   *id,
                          const gchar   *text)
{
  GtkWidget *box;
  GtkWidget *item;

  g_return_if_fail (GTK_IS_OPTION_LIST (list));
  g_return_if_fail (id != NULL);
  g_return_if_fail (text != NULL);

  if (find_item (list, id, &item, NULL))
    {
      box = gtk_widget_get_parent (item);
      gtk_option_list_row_set_text (GTK_OPTION_LIST_ROW (item), text);
      gtk_list_box_invalidate_filter (GTK_LIST_BOX (box));
    }
  else
    {
      box = list->list;
      item = gtk_option_list_row_new_item (id, text);
      gtk_list_box_insert (GTK_LIST_BOX (box), item, -1);
    }

  collapse (list, box);
}

/**
 * gtk_option_list_item_get_text:
 * @list: a #GtkOptionList
 * @id: the ID to find the text for
 *
 * Gets the display text that belongs to the item
 * with the given ID.
 *
 * Returns: the display text for @id, or %NULL
 *
 * Since: 3.16
 */
const gchar *
gtk_option_list_item_get_text (GtkOptionList *list,
                               const gchar   *id)
{
  GtkWidget *item;

  g_return_val_if_fail (GTK_IS_OPTION_LIST (list), NULL);
  g_return_val_if_fail (id != NULL, NULL);

  if (!find_item (list, id, &item, NULL))
    return NULL;

  return GTK_OPTION_LIST_ROW (item)->text;
}

/**
 * gtk_option_list_item_set_sort_key:
 * @list: a #GtkOptionList
 * @id: the ID of an item in @list
 * @sort: the sort key to use for the item
 *
 * Associates a sort key with the item identified by @id.
 * If no sort key is set, items are sorted according to
 * their display text.
 *
 * Since: 3.16
 */
void
gtk_option_list_item_set_sort_key (GtkOptionList *list,
                                   const gchar   *id,
                                   const gchar   *sort)
{
  GtkWidget *item;

  g_return_if_fail (GTK_IS_OPTION_LIST (list));
  g_return_if_fail (id != NULL);

  if (find_item (list, id, &item, NULL))
    {
      gtk_option_list_row_set_sort (GTK_OPTION_LIST_ROW (item), sort);
      gtk_list_box_invalidate_sort (GTK_LIST_BOX (gtk_widget_get_parent (item)));
    }
  else
    {
      g_warning ("GtkOptionList: no item with ID '%s' found", id);
    }
}

/**
 * gtk_option_list_item_set_group_key:
 * @list: a #GtkOptionList
 * @id: the ID of an item in @list
 * @group: the ID of the group to place the item in
 *
 * Places the item identified by @id in a group.
 * By default, items are not grouped.
 *
 * Since: 3.16
 */
void
gtk_option_list_item_set_group_key (GtkOptionList *list,
                                    const gchar   *id,
                                    const gchar   *group)
{
  GtkWidget *item;

  g_return_if_fail (GTK_IS_OPTION_LIST (list));
  g_return_if_fail (id != NULL);

  if (!find_item (list, id, &item, NULL))
    {
      g_warning ("GtkOptionList: no item with ID '%s' found", id);
      return;
    }

  if (g_strcmp0 (GTK_OPTION_LIST_ROW (item)->group, group) == 0)
    return;

  g_object_ref (item);

  remove_from_list (list, id);
  ensure_group (list, group);
  gtk_list_box_insert (GTK_LIST_BOX (group_get_list (list, group)), item, -1);

  g_object_unref (item);
}
/**
 * gtk_option_list_remove_item:
 * @list: a #GtkOptionList
 * @id: the ID of the item to remove
 *
 * Removes an item from the list.
 *
 * If the item with the this ID is currently selected,
 * no item will be selected after this call.
 *
 * If the removed item was the last one in its group,
 * the group will be removed as well.
 *
 * Since: 3.16
 */
void
gtk_option_list_remove_item (GtkOptionList *list,
                             const gchar   *id)
{
  g_return_if_fail (GTK_IS_OPTION_LIST (list));
  g_return_if_fail (id != NULL);

  gtk_option_list_unselect_item (list, id);
  remove_from_list (list, id);
}

/**
 * gtk_option_list_set_allow_custom:
 * @list: a #GtkOptionList
 * @allow: whether to allow custom items
 *
 * Sets whether the list should allow the user
 * to enter custom values.
 *
 * Since: 3.16
 */
void
gtk_option_list_set_allow_custom (GtkOptionList *list,
                                  gboolean       allow)
{
  g_return_if_fail (GTK_IS_OPTION_LIST (list));

  if (list->allow_custom == allow)
    return;

  list->allow_custom = allow;

  g_object_notify (G_OBJECT (list), "allow-custom");
}

/**
 * gtk_option_list_get_allow_custom:
 * #list: a #GtkOptionList
 *
 * Gets whether the list should allow the user
 * to enter custom values.
 *
 * Returns: %TRUE if custom values are allowed
 *
 * Since: 3.16
 */
gboolean
gtk_option_list_get_allow_custom (GtkOptionList *list)
{
  g_return_val_if_fail (GTK_IS_OPTION_LIST (list), FALSE);

  return list->allow_custom;
}

/**
 * gtk_option_list_set_custom_text:
 * @list: a #GtkOptionList
 * @text: the text to show for the custom entry
 *
 * Sets the text that is displayed for the custom entry.
 *
 * Since: 3.16
 */
void
gtk_option_list_set_custom_text (GtkOptionList *list,
                                 const gchar   *text)
{
  g_return_if_fail (GTK_IS_OPTION_LIST (list));

  g_free (list->custom_text);
  list->custom_text = g_strdup (text);

  g_object_notify (G_OBJECT (list), "custom-text");
}

/**
 * gtk_option_list_get_custom_text:
 * @list: a #GtkOptionList
 *
 * Gets the text that is displayed for the custom entry.
 *
 * Returns: (transfer none): the custom text
 *
 * Since: 3.16
 */
const gchar *
gtk_option_list_get_custom_text (GtkOptionList *list)
{
  g_return_val_if_fail (GTK_IS_OPTION_LIST (list), NULL);

  return list->custom_text;
}

/**
 * gtk_option_list_add_group:
 * @list: a #GtkOptionList
 * @group: a group ID
 * @text: An display text for the group
 * @sort: (allow-none): An optional sort key for the group
 *
 * Associates a display text and sort key with a group of items.
 *
 * Since: 3.16
 */
void
gtk_option_list_add_group (GtkOptionList *list,
                           const gchar   *group,
                           const gchar   *text,
                           const gchar   *sort)
{
  GtkWidget *header, *item;

  g_return_if_fail (GTK_IS_OPTION_LIST (list));
  g_return_if_fail (group != NULL);
  g_return_if_fail (text != NULL);

  ensure_group (list, group);

  header = group_get_header (list, group);
  item = group_get_item (list, group);

  gtk_option_list_row_set_text (GTK_OPTION_LIST_ROW (header), text);
  gtk_option_list_row_set_text (GTK_OPTION_LIST_ROW (item), text);
  gtk_option_list_row_set_sort (GTK_OPTION_LIST_ROW (item), sort);

  gtk_list_box_invalidate_filter (GTK_LIST_BOX (list->list));
  gtk_list_box_invalidate_sort (GTK_LIST_BOX (list->list));
}

/**
 * gtk_option_list_set_selection_mode:
 * @list: a #GtkOptionList
 * @mode: the new selection mode
 *
 * Sets the selection mode for the list. It determines
 * whether multiple items can be selected at the same
 * time and whether the user can unselect all items.
 *
 * Note that #GTK_SELECTION_NONE does not make sense
 * for option lists.
 *
 * Since: 3.16
 */
void
gtk_option_list_set_selection_mode (GtkOptionList    *list,
                                    GtkSelectionMode  mode)
{
  g_return_if_fail (GTK_IS_OPTION_LIST (list));
  g_return_if_fail (mode != GTK_SELECTION_NONE);

  if (list->selection_mode == mode)
    return;

  list->selection_mode = mode;

  g_object_notify (G_OBJECT (list), "selection-mode");
}

/**
 * gtk_option_list_get_selection_mode:
 * @list: a #GtkOptionList
 *
 * Gets the selection mode for the list.
 *
 * Returns: the selection mode
 *
 * Since: 3.16
 */
GtkSelectionMode
gtk_option_list_get_selection_mode (GtkOptionList *list)
{
  g_return_val_if_fail (GTK_IS_OPTION_LIST (list), GTK_SELECTION_BROWSE);

  return list->selection_mode;
}

/**
 * gtk_optin_list_handle_key_event:
 * @list: a #GtkOptionList
 * @event: a key event
 *
 * This function should be called when the button or other
 * widget that the option list is associated with received
 * a key event.
 *
 * If this function returns #GDK_EVENT_STOP, the expected
 * behaviour is to make the option list visible (e.g. by
 * showing the popover that contains it).
 *
 * Returns: %GDK_EVENT_STOP if the key press event resulted
 *   in text being entered in the search entry,
 *   %GDK_EVENT_PROPAGATE otherwise.
 *
 * Since: 3.16
 */
gboolean
gtk_option_list_handle_key_event (GtkOptionList *list,
                                  GdkEvent      *event)
{
  g_return_val_if_fail (GTK_IS_OPTION_LIST (list), GDK_EVENT_PROPAGATE);

  return handle_key_event (list, event);
}
