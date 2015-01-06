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

#include "gtkcombo.h"

#include "gtkbin.h"
#include "gtkbox.h"
#include "gtkbuildable.h"
#include "gtkbuilderprivate.h"
#include "gtkbutton.h"
#include "gtkcontainer.h"
#include "gtkentry.h"
#include "gtkframe.h"
#include "gtklabel.h"
#include "gtklistbox.h"
#include "gtkimage.h"
#include "gtkscrolledwindow.h"
#include "gtksearchbar.h"
#include "gtksearchentry.h"
#include "gtkseparator.h"
#include "gtkstack.h"
#include "gtkintl.h"
#include "gtkprivate.h"


/**
 * SECTION:gtkcombo
 * @Short_description: A simple text-only combo box using a popover
 * @Title: GtkCombo
 * @See_also: #GtkComboBoxText
 *
 * A GtkCombo is a simple variant of a combo box that hides the
 * model-view complexity of #GtkComboBox and uses a popover.
 *
 * To create a GtkCombo, use gtk_combo_new().
 *
 * You can add items to a GtkCombo using gtk_combo_add_item() and remove
 * them with gtk_combo_remove_item(). Each item has an ID that is returned
 * as the value of the #GtkCombo:active property when the item is currently
 * selected, an optional text that is used to display the item, and an
 * optional sort key that is used to sort the items.
 *
 * If you want to allow the user to enter custom values, use
 * gtk_combo_set_allow_custom().
 *
 * Items can optionally be grouped, by using gtk_combo_item_set_group_key().
 * Groups can have display text and sort keys that are different from the
 * group ID, by using gtk_combo_box_add_group().
 *
 * # GtkCombo as GtkBuildable
 *
 * The GtkCombo implementation of the GtkBuildable interface supports
 * adding items directly using the <items> element and specifying <item>
 * elements for each item. Each <item> element can specify the “id”
 * and "sort" corresponding to the appended text and also supports
 * the regular translation attributes “translatable”, “context” and
 * “comments”. Groups can be specified similarly with <groups> and
 * <group> elements, and associated with items with the “group” attribute.
 *
 * Here is a UI definition fragment specifying some GtkCombo items:
 * |[
 * <object class="GtkCombo">
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


#define GTK_TYPE_COMBO_ROW    (gtk_combo_row_get_type ())
#define GTK_COMBO_ROW(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_COMBO_ROW, GtkComboRow))
#define GTK_IS_COMBO_ROW(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_COMBO_ROW))

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
} GtkComboRow;

enum {
  ROW_PROP_ACTIVE = 1,
  ROW_PROP_ID,
  ROW_PROP_SORT,
  ROW_PROP_TEXT,
  ROW_PROP_GROUP,
  ROW_PROP_INVERTED
};

static void gtk_combo_row_set_group    (GtkComboRow *row,
                                        const gchar *group);
static void gtk_combo_row_set_inverted (GtkComboRow *row,
                                        gboolean     inverted);

typedef GtkListBoxRowClass GtkComboRowClass;

G_DEFINE_TYPE (GtkComboRow, gtk_combo_row, GTK_TYPE_LIST_BOX_ROW)

static void
gtk_combo_row_init (GtkComboRow *row)
{
  gtk_widget_init_template (GTK_WIDGET (row));
}

static void
gtk_combo_row_finalize (GObject *object)
{
  GtkComboRow *row = (GtkComboRow *)object;

  g_free (row->id);
  g_free (row->text);
  g_free (row->sort);
  g_free (row->group);

  G_OBJECT_CLASS (gtk_combo_row_parent_class)->finalize (object);
}

static void
gtk_combo_row_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkComboRow *row = (GtkComboRow *)object;

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
      gtk_combo_row_set_group (row, g_value_get_string (value));
      break;
    case ROW_PROP_INVERTED:
      gtk_combo_row_set_inverted (row, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_combo_row_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkComboRow *row = (GtkComboRow *)object;

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
gtk_combo_row_class_init (GtkComboRowClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->finalize = gtk_combo_row_finalize;
  object_class->set_property = gtk_combo_row_set_property;
  object_class->get_property = gtk_combo_row_get_property;

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

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/ui/gtkcomborow.ui");

  gtk_widget_class_bind_template_child (widget_class, GtkComboRow, box);
  gtk_widget_class_bind_template_child (widget_class, GtkComboRow, label);
  gtk_widget_class_bind_template_child (widget_class, GtkComboRow, check);
}

static void
update_group (GtkComboRow *row)
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
gtk_combo_row_set_group (GtkComboRow *row,
                         const gchar *group)
{
  g_free (row->group);
  row->group = g_strdup (group);
  update_group (row);
}

static void
gtk_combo_row_set_inverted (GtkComboRow *row,
                            gboolean     inverted)
{
  row->inverted = inverted;
  update_group (row);
}

static GtkWidget *
gtk_combo_row_new_item (const gchar *id,
                        const gchar *text)
{
  return g_object_new (GTK_TYPE_COMBO_ROW, "id", id, "text", text, NULL);
}

static GtkWidget *
gtk_combo_row_new_group (const gchar *group,
                         const gchar *text,
                         const gchar *sort)
{
  return g_object_new (GTK_TYPE_COMBO_ROW, "group", group, "text", text, "sort", sort, NULL);
}

static void
gtk_combo_row_set_active (GtkComboRow *row,
                          gboolean     active)
{
  row->active = active;
  g_object_notify (G_OBJECT (row), "active");
}

static void
gtk_combo_row_set_text (GtkComboRow *row,
                        const gchar *text)
{
  g_free (row->text);
  row->text = g_strdup (text);
  g_object_notify (G_OBJECT (row), "text");
}

static void
gtk_combo_row_set_sort (GtkComboRow *row,
                        const gchar *sort)
{
  g_free (row->sort);
  row->sort = g_strdup (sort);
  g_object_notify (G_OBJECT (row), "sort");
}

static const gchar *
gtk_combo_row_get_id (GtkComboRow *row)
{
  return row->id;
}

static const gchar *
gtk_combo_row_get_text (GtkComboRow *row)
{
  return row->text;
}

static const gchar *
gtk_combo_row_get_sort (GtkComboRow *row)
{
  return row->sort ? row->sort : gtk_combo_row_get_text (row);
}

static const gchar *
gtk_combo_row_get_group (GtkComboRow *row)
{
  return row->group;
}

/***/


static void     list_row_activated   (GtkListBox     *list,
                                      GtkListBoxRow  *row,
                                      GtkCombo       *combo);
static void     search_changed       (GtkSearchEntry *entry,
                                      GtkCombo       *combo);
static void     custom_entry_done    (GtkWidget      *widget,
                                      GtkCombo       *combo);
static void     custom_entry_changed (GObject        *entry,
                                      GParamSpec     *pspec,
                                      GtkCombo       *combo);
static gboolean popover_key_press    (GtkWidget      *widget,
                                      GdkEvent       *event,
                                      GtkCombo       *combo);
static gboolean button_key_press     (GtkWidget      *widget,
                                      GdkEvent       *event,
                                      GtkCombo       *combo);
static void     reset_popover        (GtkWidget      *popover,
                                      GtkCombo       *combo);
static void     collapse             (GtkCombo       *combo,
                                      GtkWidget      *list);
static void     expand               (GtkCombo       *combo,
                                      GtkWidget      *list);
static void     list_header_func     (GtkListBoxRow  *row,
                                      GtkListBoxRow  *before,
                                      gpointer        data);
static gboolean list_filter_func     (GtkListBoxRow  *row,
                                      gpointer        data);
static gint     list_sort_func       (GtkListBoxRow  *row1,
                                      GtkListBoxRow  *row2,
                                      gpointer        data);
static void     custom_header_func   (GtkListBoxRow  *row,
                                      GtkListBoxRow  *before,
                                      gpointer        data);
static GtkWidget *group_get_list     (GtkCombo       *combo,
                                      const gchar    *group);
static GtkWidget *group_get_header   (GtkCombo       *combo,
                                      const gchar    *group);
static GtkWidget *group_get_item     (GtkCombo       *combo,
                                      const gchar    *group);
static void       ensure_group       (GtkCombo       *combo,
                                      const gchar    *group);

static void     gtk_combo_buildable_init (GtkBuildableIface *iface);


struct _GtkCombo
{
  GtkBin parent;

  const gchar *active;
  gchar *placeholder;
  gchar *custom_text;
  gboolean allow_custom;

  GtkWidget *active_label;
  GtkWidget *popover;
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

struct _GtkComboClass
{
  GtkBinClass parent_class;
};

enum {
  PROP_ACTIVE = 1,
  PROP_PLACEHOLDER_TEXT,
  PROP_ALLOW_CUSTOM,
  PROP_CUSTOM_TEXT
};

static GtkBuildableIface *buildable_parent_iface = NULL;

G_DEFINE_TYPE_WITH_CODE (GtkCombo, gtk_combo, GTK_TYPE_BIN,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_combo_buildable_init))

static void
gtk_combo_init (GtkCombo *combo)
{
  g_type_ensure (GTK_TYPE_COMBO_ROW);

  gtk_widget_init_template (GTK_WIDGET (combo));

  gtk_list_box_set_header_func (GTK_LIST_BOX (combo->list), list_header_func, combo, NULL);
  gtk_list_box_set_filter_func (GTK_LIST_BOX (combo->list), list_filter_func, combo, NULL);
  gtk_list_box_set_sort_func (GTK_LIST_BOX (combo->list), list_sort_func, combo, NULL);

  gtk_list_box_set_header_func (GTK_LIST_BOX (combo->custom), custom_header_func, combo, NULL);

  g_object_set_data (G_OBJECT (combo->list), "show-more-item", combo->show_more);

  reset_popover (combo->popover, combo);
}

static void
gtk_combo_finalize (GObject *object)
{
  GtkCombo *combo = GTK_COMBO (object);

  g_free (combo->placeholder);
  g_free (combo->custom_text);

  G_OBJECT_CLASS (gtk_combo_parent_class)->finalize (object);
}

static void
gtk_combo_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  GtkCombo *combo = (GtkCombo *)object;

  switch (prop_id)
    {
    case PROP_ACTIVE:
      gtk_combo_set_active (combo, g_value_get_string (value));
      break;
    case PROP_PLACEHOLDER_TEXT:
      gtk_combo_set_placeholder_text (combo, g_value_get_string (value));
      break;
    case PROP_ALLOW_CUSTOM:
      gtk_combo_set_allow_custom (combo, g_value_get_boolean (value));
      break;
    case PROP_CUSTOM_TEXT:
      gtk_combo_set_custom_text (combo, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_combo_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  GtkCombo *combo = (GtkCombo *)object;

  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_string (value, gtk_combo_get_active (combo));
      break;
    case PROP_PLACEHOLDER_TEXT:
      g_value_set_string (value, gtk_combo_get_placeholder_text (combo));
      break;
    case PROP_ALLOW_CUSTOM:
      g_value_set_boolean (value, gtk_combo_get_allow_custom (combo));
      break;
    case PROP_CUSTOM_TEXT:
      g_value_set_string (value, gtk_combo_get_custom_text (combo));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_combo_class_init (GtkComboClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->finalize = gtk_combo_finalize;
  object_class->set_property = gtk_combo_set_property;
  object_class->get_property = gtk_combo_get_property;

  /**
   * GtkCombo:active:
   *
   * The ID of the currently selected item, or %NULL
   * if no item is currently selected.
   */
  g_object_class_install_property (object_class,
                                   PROP_ACTIVE,
                                   g_param_spec_string ("active",
                                                        P_("Active"),
                                                        P_("The ID of the active item"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkCombo:placeholder-text:
   *
   * The text that is displayed if no item is selected.
   */
  g_object_class_install_property (object_class,
                                   PROP_PLACEHOLDER_TEXT,
                                   g_param_spec_string ("placeholder-text",
                                                        P_("Placeholder text"),
                                                        P_("Text to show when no item is selected"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkCombo:allow-custom:
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
   * GtkCombo:custom-text:
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

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/ui/gtkcombo.ui");

  gtk_widget_class_bind_template_child (widget_class, GtkCombo, active_label);
  gtk_widget_class_bind_template_child (widget_class, GtkCombo, popover);
  gtk_widget_class_bind_template_child (widget_class, GtkCombo, scrolled_window);
  gtk_widget_class_bind_template_child (widget_class, GtkCombo, list);
  gtk_widget_class_bind_template_child (widget_class, GtkCombo, search_revealer);
  gtk_widget_class_bind_template_child (widget_class, GtkCombo, search_entry);
  gtk_widget_class_bind_template_child (widget_class, GtkCombo, show_more);
  gtk_widget_class_bind_template_child (widget_class, GtkCombo, add_custom);
  gtk_widget_class_bind_template_child (widget_class, GtkCombo, stack);
  gtk_widget_class_bind_template_child (widget_class, GtkCombo, back_to_list);
  gtk_widget_class_bind_template_child (widget_class, GtkCombo, custom_entry);
  gtk_widget_class_bind_template_child (widget_class, GtkCombo, custom_button);
  gtk_widget_class_bind_template_child (widget_class, GtkCombo, custom);
  gtk_widget_class_bind_template_callback (widget_class, list_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, search_changed);
  gtk_widget_class_bind_template_callback (widget_class, reset_popover);
  gtk_widget_class_bind_template_callback (widget_class, custom_entry_done);
  gtk_widget_class_bind_template_callback (widget_class, custom_entry_changed);
  gtk_widget_class_bind_template_callback (widget_class, popover_key_press);
  gtk_widget_class_bind_template_callback (widget_class, button_key_press);
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
} ComboParserData;

static void
combo_start_element (GMarkupParseContext *context,
                     const gchar         *element_name,
                     const gchar        **names,
                     const gchar        **values,
                     gpointer             user_data,
                     GError             **error)
{
  ComboParserData *data = (ComboParserData*)user_data;
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
            g_warning ("Unknown custom combo item attribute: %s", names[i]);
        }
    }
}

static void
combo_text (GMarkupParseContext *context,
            const gchar         *text,
            gsize                text_len,
            gpointer             user_data,
            GError             **error)
{
  ComboParserData *data = (ComboParserData*)user_data;

  if (data->is_text)
    g_string_append_len (data->string, text, text_len);
}

static void
combo_end_element (GMarkupParseContext *context,
                   const gchar         *element_name,
                   gpointer             user_data,
                   GError             **error)
{
  ComboParserData *data = (ComboParserData*)user_data;

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
        gtk_combo_add_group (GTK_COMBO (data->object), data->id, data->string->str, data->sort);
      else
        {
          gtk_combo_add_item (GTK_COMBO (data->object), data->id, data->string->str);
          if (data->sort)
            gtk_combo_item_set_sort_key (GTK_COMBO (data->object), data->id, data->sort);
          if (data->group)
            gtk_combo_item_set_group_key (GTK_COMBO (data->object), data->id, data->group);
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

static const GMarkupParser combo_parser =
{
  combo_start_element,
  combo_end_element,
  combo_text
};

static gboolean
gtk_combo_buildable_custom_tag_start (GtkBuildable  *buildable,
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
      ComboParserData *parser_data;

      parser_data = g_slice_new0 (ComboParserData);
      parser_data->builder = g_object_ref (builder);
      parser_data->object = g_object_ref (buildable);
      parser_data->domain = gtk_builder_get_translation_domain (builder);
      parser_data->string = g_string_new ("");
      parser_data->is_group = tagname[0] == 'g';
      *parser = combo_parser;
      *data = parser_data;

      return TRUE;
    }

  return FALSE;
}

static void
gtk_combo_buildable_custom_finished (GtkBuildable *buildable,
                                     GtkBuilder   *builder,
                                     GObject      *child,
                                     const gchar  *tagname,
                                     gpointer      user_data)
{
  ComboParserData *data;

  buildable_parent_iface->custom_finished (buildable, builder, child,
                                           tagname, user_data);

  if (strcmp (tagname, "items") == 0 ||
      strcmp (tagname, "groups") == 0)
    {
      data = (ComboParserData*)user_data;

      g_object_unref (data->object);
      g_object_unref (data->builder);
      g_string_free (data->string, TRUE);
      g_slice_free (ComboParserData, data);
    }
}

static void
gtk_combo_buildable_init (GtkBuildableIface *iface)
{
  buildable_parent_iface = g_type_interface_peek_parent (iface);

  iface->custom_tag_start = gtk_combo_buildable_custom_tag_start;
  iface->custom_finished = gtk_combo_buildable_custom_finished;
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
  GtkCombo *combo = data;
  const gchar *text;
  gchar *row_text;
  gchar *search_text;
  gboolean ret;

  text = gtk_entry_get_text (GTK_ENTRY (combo->search_entry));
  if (text[0] == '\0')
    return TRUE;

  if (!GTK_IS_COMBO_ROW (row))
    return TRUE;

  if (gtk_combo_row_get_group (GTK_COMBO_ROW (row)))
    return TRUE;

  row_text = g_utf8_strdown (gtk_combo_row_get_text (GTK_COMBO_ROW (row)), -1);
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

  if (GTK_IS_COMBO_ROW (row1))
    {
      if (g_strcmp0 (gtk_combo_row_get_group (GTK_COMBO_ROW (row1)), "list") == 0 ||
          g_strcmp0 (gtk_combo_row_get_group (GTK_COMBO_ROW (row1)), "custom") == 0)
        return -1;

      sort1 = gtk_combo_row_get_sort (GTK_COMBO_ROW (row1));
    }

  if (GTK_IS_COMBO_ROW (row2))
    {
      if (g_strcmp0 (gtk_combo_row_get_group (GTK_COMBO_ROW (row2)), "list") == 0 ||
          g_strcmp0 (gtk_combo_row_get_group (GTK_COMBO_ROW (row2)), "custom") == 0)
        return 1;

      sort2 = gtk_combo_row_get_sort (GTK_COMBO_ROW (row2));
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
  GtkCombo *combo = data;
  GtkWidget *ret = NULL;

  if ((GtkWidget*)before == combo->back_to_list &&
      !gtk_list_box_row_get_header (row))
    {
      ret = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_list_box_row_set_header (row, ret);
    }
}

static void
search_changed (GtkSearchEntry *entry,
                GtkCombo       *combo)
{
  expand (combo, combo->list);
  gtk_list_box_invalidate_filter (GTK_LIST_BOX (combo->list));
}

static void
reset_popover (GtkWidget *widget,
               GtkCombo  *combo)
{
  gtk_stack_set_visible_child_name (GTK_STACK (combo->stack), "list");
  gtk_entry_set_text (GTK_ENTRY (combo->search_entry), "");
  gtk_widget_hide (combo->search_revealer);
  collapse (combo, combo->list);
  gtk_entry_set_text (GTK_ENTRY (combo->custom_entry), "");
}

typedef struct
{
  GtkCombo *combo;
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

  if (!GTK_IS_COMBO_ROW (row))
    return;

  if (gtk_combo_row_get_group (GTK_COMBO_ROW (row)))
    return;

  id = gtk_combo_row_get_id (GTK_COMBO_ROW (row));

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

  if (!GTK_IS_COMBO_ROW (row))
    return;

  group = gtk_combo_row_get_group (GTK_COMBO_ROW (row));
  if (group &&
      g_strcmp0 (group, "list") != 0 &&
      g_strcmp0 (group, "custom") != 0)
    {
      GtkWidget *list;

      list = group_get_list (d->combo, group);
      gtk_container_foreach (GTK_CONTAINER (list), find_in_group_cb, d);

      if (d->row)
        d->group_row = row;
    }

  if (d->row)
    return;

  id = gtk_combo_row_get_id (GTK_COMBO_ROW (row));
  if (g_strcmp0 (id, d->id) == 0)
    d->row = row;
}

static gboolean
find_item (GtkCombo     *combo,
           const gchar  *id,
           GtkWidget   **item,
           GtkWidget   **group)
{
  ForeachData data;

  data.combo = combo;
  data.id = id;
  data.row = NULL;
  data.group_row = NULL;
  gtk_container_foreach (GTK_CONTAINER (combo->list), find_item_cb, &data);

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

  if (GTK_IS_COMBO_ROW (widget) &&
      gtk_combo_row_get_id (GTK_COMBO_ROW (widget)))
    (*count)++;
}

static gboolean
remove_from_list (GtkCombo    *combo,
                  const gchar *id)
{
  GtkWidget *item;
  GtkWidget *group;
  GtkWidget *list;
  GtkWidget *tab;

  if (!find_item (combo, id, &item, &group))
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
          gtk_container_remove (GTK_CONTAINER (combo->stack), tab);
          gtk_container_remove (GTK_CONTAINER (combo->list), group);
        }
      else
        collapse (combo, list);
     }
  else
    collapse (combo, list);

  return TRUE;
}

static void
set_active (GtkCombo    *combo,
            const gchar *id)
{
  GtkWidget *item;
  GtkWidget *group;

  if (combo->active)
    {
      find_item (combo, combo->active, &item, &group);
      if (item)
        gtk_combo_row_set_active (GTK_COMBO_ROW (item), FALSE);
      if (group)
        gtk_combo_row_set_active (GTK_COMBO_ROW (group), FALSE);
    }

  if (id)
    {
      find_item (combo, id, &item, &group);
      if (item)
        gtk_combo_row_set_active (GTK_COMBO_ROW (item), TRUE);
      if (group)
        gtk_combo_row_set_active (GTK_COMBO_ROW (group), TRUE);
    }
  else
    item = NULL;

  if (item)
    {
      combo->active = gtk_combo_row_get_id (GTK_COMBO_ROW (item));
      gtk_label_set_text (GTK_LABEL (combo->active_label),
                          gtk_combo_row_get_text (GTK_COMBO_ROW (item)));
    }
  else
    {
      combo->active = NULL;
      gtk_label_set_text (GTK_LABEL (combo->active_label), combo->placeholder);
    }


  g_object_notify (G_OBJECT (combo), "active");
}

static void
list_row_activated (GtkListBox    *list,
                    GtkListBoxRow *row,
                    GtkCombo      *combo)
{
  if ((GtkWidget*)row == list_get_show_more_item (GTK_WIDGET (list)))
    {
      expand (combo, GTK_WIDGET (list));
      return;
    }

  if ((GtkWidget*)row == combo->add_custom)
    {
      gtk_stack_set_visible_child_name (GTK_STACK (combo->stack), "custom");
      gtk_widget_grab_focus (combo->custom_entry);
      return;
    }

  if (GTK_IS_COMBO_ROW (row))
    {
      const gchar *group;

      group = gtk_combo_row_get_group (GTK_COMBO_ROW (row));
      if (group)
        {
          if (g_strcmp0 (group, "list") != 0)
            collapse (combo, group_get_list (combo, group));
          gtk_stack_set_visible_child_name (GTK_STACK (combo->stack), group);
          return;
        }

      set_active (combo, gtk_combo_row_get_id (GTK_COMBO_ROW (row)));
    }

  gtk_widget_hide (combo->popover);
}

static void
custom_entry_done (GtkWidget *widget,
                   GtkCombo  *combo)
{
  const gchar *text;

  text = gtk_entry_get_text (GTK_ENTRY (combo->custom_entry));
  if (text[0] != '\0')
    {
      gtk_combo_add_item (combo, text, text);
      gtk_combo_set_active (combo, text);
      gtk_entry_set_text (GTK_ENTRY (combo->custom_entry), "");
      gtk_widget_hide (combo->popover);
    }
}

static void
custom_entry_changed (GObject    *entry,
                      GParamSpec *pspec,
                      GtkCombo   *combo)
{
  const gchar *text;

  text = gtk_entry_get_text (GTK_ENTRY (combo->custom_entry));
  gtk_widget_set_sensitive (combo->custom_button, text[0] != '\0');
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
show_few (GtkWidget *widget, gpointer data)
{
  gint *count = data;

  if (!GTK_IS_COMBO_ROW (widget) ||
      gtk_combo_row_get_group (GTK_COMBO_ROW (widget)) != NULL)
    return;

  if (*count < 6)
    gtk_widget_show (widget);
  else
    gtk_widget_hide (widget);

  (*count)++;
}

static void
collapse (GtkCombo  *combo,
          GtkWidget *list)
{
  gint count;
  GtkWidget *show_more;

  show_more = list_get_show_more_item (list);
  count = 0;
  gtk_container_foreach (GTK_CONTAINER (list), show_few, &count);
  if (count < 7)
    {
      if (list == combo->list)
        gtk_revealer_set_reveal_child (GTK_REVEALER (combo->search_revealer), FALSE);
      gtk_widget_hide (show_more);
    }
  else
    {
      if (list == combo->list)
        {
          gtk_widget_show (combo->search_revealer);
          gtk_revealer_set_reveal_child (GTK_REVEALER (combo->search_revealer), TRUE);
        }
      gtk_widget_show (show_more);
    }

  update_scrolling (list, FALSE);
}

static void
show_all (GtkWidget *widget, gpointer data)
{
  if (!GTK_IS_COMBO_ROW (widget) ||
      gtk_combo_row_get_group (GTK_COMBO_ROW (widget)) != NULL)
    return;

  gtk_widget_show (widget);
}

static void
expand (GtkCombo  *combo,
        GtkWidget *list)
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

  if (keyval == GDK_KEY_Tab ||
      keyval == GDK_KEY_KP_Tab ||
      keyval == GDK_KEY_Up ||
      keyval == GDK_KEY_KP_Up ||
      keyval == GDK_KEY_Down ||
      keyval == GDK_KEY_KP_Down ||
      keyval == GDK_KEY_Left ||
      keyval == GDK_KEY_KP_Left ||
      keyval == GDK_KEY_Right ||
      keyval == GDK_KEY_KP_Right ||
      keyval == GDK_KEY_Home ||
      keyval == GDK_KEY_KP_Home ||
      keyval == GDK_KEY_End ||
      keyval == GDK_KEY_KP_End ||
      keyval == GDK_KEY_Page_Up ||
      keyval == GDK_KEY_KP_Page_Up ||
      keyval == GDK_KEY_Page_Down ||
      keyval == GDK_KEY_KP_Page_Down ||
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
popover_key_press (GtkWidget *widget,
                   GdkEvent  *event,
                   GtkCombo  *combo)
{
  guint keyval;
  gboolean handled;
  gboolean preedit_changed;
  guint preedit_change_id;
  gboolean res;
  gchar *old_text, *new_text;

  if (g_strcmp0 (gtk_stack_get_visible_child_name (GTK_STACK (combo->stack)), "list") != 0 ||
      !gdk_event_get_keyval (event, &keyval) ||
      is_keynav_event (event, keyval) ||
      keyval == GDK_KEY_space ||
      keyval == GDK_KEY_Menu)
    return GDK_EVENT_PROPAGATE;

  if (!gtk_widget_get_realized (combo->search_entry))
    gtk_widget_realize (combo->search_entry);

  handled = GDK_EVENT_PROPAGATE;
  preedit_changed = FALSE;
  preedit_change_id = g_signal_connect (combo->search_entry, "preedit-changed",
                                        G_CALLBACK (preedit_changed_cb), &preedit_changed);

  old_text = g_strdup (gtk_entry_get_text (GTK_ENTRY (combo->search_entry)));
  res = gtk_widget_event (combo->search_entry, event);
  new_text = g_strdup (gtk_entry_get_text (GTK_ENTRY (combo->search_entry)));

  g_signal_handler_disconnect (combo->search_entry, preedit_change_id);

  if ((res && g_strcmp0 (new_text, old_text) != 0) || preedit_changed)
    {
      handled = GDK_EVENT_STOP;
      gtk_widget_show (combo->search_revealer);
      gtk_revealer_set_reveal_child (GTK_REVEALER (combo->search_revealer), TRUE);
      gtk_entry_grab_focus_without_selecting (GTK_ENTRY (combo->search_entry));
    }

  g_free (old_text);
  g_free (new_text);

  return handled;
}

static gboolean
button_key_press (GtkWidget *widget,
                  GdkEvent  *event,
                  GtkCombo  *combo)
{
  gboolean handled;

  handled = popover_key_press (combo->popover, event, combo);

  if (handled == GDK_EVENT_STOP)
    {
      gtk_widget_show (combo->popover);
      gtk_editable_select_region (GTK_EDITABLE (combo->search_entry), -1, -1);
    }

  return handled;
}

static GtkWidget *
group_get_list (GtkCombo    *combo,
                const gchar *group)
{
  GtkWidget *tab;

  tab = gtk_stack_get_child_by_name (GTK_STACK (combo->stack), group);
  g_return_val_if_fail (tab != NULL, NULL);
  return (GtkWidget*)g_object_get_data (G_OBJECT (tab), "list");
}

static GtkWidget *
group_get_header (GtkCombo    *combo,
                  const gchar *group)
{
  GtkWidget *tab;

  tab = gtk_stack_get_child_by_name (GTK_STACK (combo->stack), group);
  g_return_val_if_fail (tab != NULL, NULL);
  return (GtkWidget*)g_object_get_data (G_OBJECT (tab), "header");
}

static GtkWidget *
group_get_item (GtkCombo    *combo,
                const gchar *group)
{
  GtkWidget *tab;

  tab = gtk_stack_get_child_by_name (GTK_STACK (combo->stack), group);
  g_return_val_if_fail (tab != NULL, NULL);
  return (GtkWidget*)g_object_get_data (G_OBJECT (tab), "item");
}

static void
ensure_group (GtkCombo    *combo,
              const gchar *group)
{
  GtkWidget *tab;

  tab = gtk_stack_get_child_by_name (GTK_STACK (combo->stack), group);
  if (tab == NULL)
    {
      GtkBuilder *builder;
      GtkWidget *list;
      GtkWidget *item;
      GtkWidget *header;

      builder = gtk_builder_new_from_resource ("/org/gtk/libgtk/ui/gtkcombotab.ui");
      tab = (GtkWidget*)gtk_builder_get_object (builder, "tab");
      list = (GtkWidget*)gtk_builder_get_object (builder, "list");
      header = (GtkWidget*)gtk_builder_get_object (builder, "header");
      item = (GtkWidget*)gtk_builder_get_object (builder, "show_more");
      gtk_stack_add_named (GTK_STACK (combo->stack), tab, group);
      g_object_unref (builder);

      g_object_set_data (G_OBJECT (tab), "list", list);
      g_object_set_data (G_OBJECT (tab), "header", header);
      g_object_set_data (G_OBJECT (list), "show-more-item", item);

      g_signal_connect (list, "row-activated", G_CALLBACK (list_row_activated), combo);
      gtk_list_box_set_sort_func (GTK_LIST_BOX (list), list_sort_func, combo, NULL);
      gtk_list_box_set_header_func (GTK_LIST_BOX (list), list_header_func, combo, NULL);

      gtk_combo_row_set_text (GTK_COMBO_ROW (header), group);

      item = gtk_combo_row_new_group (group, group, NULL);
      gtk_list_box_insert (GTK_LIST_BOX (combo->list), item, -1);

      g_object_set_data (G_OBJECT (tab), "item", item);
    }
}


/***/


/**
 * gtk_combo_new:
 *
 * Creates a new #GtkCombo.
 *
 * Returns: A new #GtkCombo
 *
 * Since: 3.16
 */
GtkWidget *
gtk_combo_new (void)
{
  return g_object_new (GTK_TYPE_COMBO, NULL);
}

/**
 * gtk_combo_get_active:
 * @combo: a #GtkCombo
 *
 * Gets the ID of the currently selected item.
 *
 * Returns: (transfer none): the active ID, or %NULL if no
 *   item is selected
 *
 * Since: 3.16
 */
const gchar *
gtk_combo_get_active (GtkCombo *combo)
{
  g_return_val_if_fail (GTK_IS_COMBO (combo), NULL);

  return combo->active;
}

/**
 * gtk_combo_set_active:
 * @combo: a #GtkCombo
 * @id: (allow-none): the ID to select, or %NULL
 *
 * Sets the active ID to @id. If @id is not the ID
 * of an item of combo, no item will be selected
 * after this call.
 *
 * Since: 3.16
 */
void
gtk_combo_set_active (GtkCombo    *combo,
                      const gchar *id)
{
  g_return_if_fail (GTK_IS_COMBO (combo));

  set_active (combo, id);
}

/**
 * gtk_combo_add_item:
 * @combo: a #GtkCombo
 * @id: the ID for the item to add
 * @text: the text to display for the item
 *
 * Adds an item to the combo.
 *
 * If an item with this ID already exists, its display text
 * will be updated with the new values.
 *
 * Since: 3.16
 */
void
gtk_combo_add_item (GtkCombo    *combo,
                    const gchar *id,
                    const gchar *text)
{
  GtkWidget *list;
  GtkWidget *item;

  g_return_if_fail (GTK_IS_COMBO (combo));
  g_return_if_fail (id != NULL);
  g_return_if_fail (text != NULL);

  if (find_item (combo, id, &item, NULL))
    {
      list = gtk_widget_get_parent (item);
      gtk_combo_row_set_text (GTK_COMBO_ROW (item), text);
      gtk_list_box_invalidate_filter (GTK_LIST_BOX (list));
    }
  else
    {
      list = combo->list;
      item = gtk_combo_row_new_item (id, text);
      gtk_list_box_insert (GTK_LIST_BOX (list), item, -1);
    }

  collapse (combo, list);
}

const gchar *
gtk_combo_item_get_text (GtkCombo    *combo,
                         const gchar *id)
{
  GtkWidget *item;

  g_return_val_if_fail (GTK_IS_COMBO (combo), NULL);

  if (!find_item (combo, id, &item, NULL))
    return NULL;

  return GTK_COMBO_ROW (item)->text;
}

/**
 * gtk_combo_item_set_sort_key:
 * @combo: a #GtkCombo
 * @id: the ID of an item in @combo
 * @sort: the sort key to use for the item
 *
 * Associates a sort key with the item identified by @id.
 * If no sort key is set, items are sorted according to
 * their display text.
 *
 * Since: 3.16
 */
void
gtk_combo_item_set_sort_key (GtkCombo    *combo,
                             const gchar *id,
                             const gchar *sort)
{
  GtkWidget *item;

  g_return_if_fail (GTK_IS_COMBO (combo));
  g_return_if_fail (id != NULL);

  if (find_item (combo, id, &item, NULL))
    {
      gtk_combo_row_set_sort (GTK_COMBO_ROW (item), sort);
      gtk_list_box_invalidate_sort (GTK_LIST_BOX (gtk_widget_get_parent (item)));
    }
  else
    {
      g_warning ("GtkCombo: no item with ID '%s' found", id);
    }
}

/**
 * gtk_combo_item_set_group_key:
 * @combo: a #GtkCombo
 * @id: the ID of an item in @combo
 * @group: the ID of the group to place the item in
 *
 * Places the item identified by @id in a group.
 * By default, items are not grouped.
 *
 * Since: 3.16
 */
void
gtk_combo_item_set_group_key (GtkCombo    *combo,
                              const gchar *id,
                              const gchar *group)
{
  GtkWidget *item;
  GtkWidget *list;

  g_return_if_fail (GTK_IS_COMBO (combo));
  g_return_if_fail (id != NULL);

  if (!find_item (combo, id, &item, NULL))
    {
      g_warning ("GtkCombo: no item with ID '%s' found", id);
      return;
    }

  if (g_strcmp0 (GTK_COMBO_ROW (item)->group, group) == 0)
    return;

  g_object_ref (item);

  remove_from_list (combo, id);
  ensure_group (combo, group);
  list = group_get_list (combo, group);
  gtk_list_box_insert (GTK_LIST_BOX (list), item, -1);

  g_object_unref (item);
}
/**
 * gtk_combo_remove_item:
 * @combo: a #GtkCombo
 * @id: the ID of the item to remove
 *
 * Removes an item from the combo.
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
gtk_combo_remove_item (GtkCombo    *combo,
                       const gchar *id)
{
  g_return_if_fail (GTK_IS_COMBO (combo));
  g_return_if_fail (id != NULL);

  if (g_strcmp0 (id, combo->active) == 0)
    set_active (combo, NULL);

  remove_from_list (combo, id);
}

/**
 * gtk_combo_set_placeholder_text:
 * @combo: a #GtkCombo
 * @text: the placeholder text to use
 *
 * Sets the placeholder text that is displayed in the combo
 * if no item is currently selected.
 *
 * Since: 3.16
 */
void
gtk_combo_set_placeholder_text (GtkCombo    *combo,
                                const gchar *text)
{
  g_return_if_fail (GTK_IS_COMBO (combo));
  g_return_if_fail (text != NULL);

  g_free (combo->placeholder);
  combo->placeholder = g_strdup (text);

  if (combo->active_label != NULL && combo->active == NULL)
    gtk_label_set_text (GTK_LABEL (combo->active_label), combo->placeholder);

  g_object_notify (G_OBJECT (combo), "placeholder-text");
}

/**
 * gtk_combo_get_placeholder_text:
 * @combo: a #GtkCombo
 *
 * Gets the placholder text that is displayed in the combo
 * if no item is currently selected.
 *
 * Returns: (transfer none): the placeholder text
 *
 * Since: 3.16
 */
const gchar *
gtk_combo_get_placeholder_text (GtkCombo *combo)
{
  g_return_val_if_fail (GTK_IS_COMBO (combo), NULL);

  return combo->placeholder;
}

/**
 * gtk_combo_set_allow_custom:
 * @combo: a #GtkCombo
 * @allow: whether to allow custom items
 *
 * Sets whether the combo should allow the user
 * to enter custom values.
 *
 * Since: 3.16
 */
void
gtk_combo_set_allow_custom (GtkCombo *combo,
                            gboolean  allow)
{
  g_return_if_fail (GTK_IS_COMBO (combo));

  if (combo->allow_custom == allow)
    return;

  combo->allow_custom = allow;

  gtk_widget_set_visible (combo->add_custom, allow);

  g_object_notify (G_OBJECT (combo), "allow-custom");
}

/**
 * gtk_combo_get_allow_custom:
 * #combo: a #GtkCombo
 *
 * Gets whether the combo should allow the user
 * to enter custom values.
 *
 * Returns: %TRUE if custom values are allowed
 *
 * Since: 3.16
 */
gboolean
gtk_combo_get_allow_custom (GtkCombo *combo)
{
  g_return_val_if_fail (GTK_IS_COMBO (combo), FALSE);

  return combo->allow_custom;
}

/**
 * gtk_combo_set_custom_text:
 * @combo: a #GtkCombo
 * @text: the text to show for the custom entry
 *
 * Sets the text that is displayed for the custom entry.
 *
 * Since: 3.16
 */
void
gtk_combo_set_custom_text (GtkCombo    *combo,
                           const gchar *text)
{
  g_return_if_fail (GTK_IS_COMBO (combo));

  g_free (combo->custom_text);
  combo->custom_text = g_strdup (text);

  g_object_notify (G_OBJECT (combo), "custom-text");
}

/**
 * gtk_combo_get_custom_text:
 * @combo: a #GtkCombo
 *
 * Gets the text that is displayed for the custom entry.
 *
 * Returns: (transfer none): the custom text
 *
 * Since: 3.16
 */
const gchar *
gtk_combo_get_custom_text (GtkCombo *combo)
{
  g_return_val_if_fail (GTK_IS_COMBO (combo), NULL);

  return combo->custom_text;
}

/**
 * gtk_combo_add_group:
 * @combo: a #GtkCombo
 * @group: a group ID
 * @text: An display text for the group
 * @sort: (allow-none): An optional sort key for the group
 *
 * Associates a display text and sort key with a group of items.
 *
 * Since: 3.16
 */
void
gtk_combo_add_group (GtkCombo    *combo,
                     const gchar *group,
                     const gchar *text,
                     const gchar *sort)
{
  GtkWidget *header, *item;

  g_return_if_fail (GTK_IS_COMBO (combo));
  g_return_if_fail (group != NULL);
  g_return_if_fail (text != NULL);

  ensure_group (combo, group);

  header = group_get_header (combo, group);
  item = group_get_item (combo, group);

  gtk_combo_row_set_text (GTK_COMBO_ROW (header), text);
  gtk_combo_row_set_text (GTK_COMBO_ROW (item), text);
  gtk_combo_row_set_sort (GTK_COMBO_ROW (item), sort);

  gtk_list_box_invalidate_filter (GTK_LIST_BOX (combo->list));
  gtk_list_box_invalidate_sort (GTK_LIST_BOX (combo->list));
}

