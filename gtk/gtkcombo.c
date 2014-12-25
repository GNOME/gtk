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
#include "gtkentry.h"
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


#define GTK_TYPE_COMBO_ROW    (gtk_combo_row_get_type ())
#define GTK_COMBO_ROW(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_COMBO_ROW, GtkComboRow))
#define GTK_IS_COMBO_ROW(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_COMBO_ROW))

typedef struct
{
  GtkListBoxRow row;

  gchar *id;
  gchar *text;
  gchar *sort;
  gboolean active;

  GtkWidget *label;
  GtkWidget *check;
} GtkComboRow;

enum {
  ROW_PROP_ACTIVE = 1,
  ROW_PROP_ID,
  ROW_PROP_SORT,
  ROW_PROP_TEXT
};

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

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/ui/gtkcomborow.ui");

  gtk_widget_class_bind_template_child (widget_class, GtkComboRow, label);
  gtk_widget_class_bind_template_child (widget_class, GtkComboRow, check);
}

static GtkWidget *
gtk_combo_row_new (const gchar *id,
                   const gchar *sort,
                   const gchar *text)
{
  return g_object_new (GTK_TYPE_COMBO_ROW,
                       "id", id,
                       "sort", sort,
                       "text", text,
                       NULL);
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
  return row->text ? row->text : row->id;
}

static const gchar *
gtk_combo_row_get_sort (GtkComboRow *row)
{
  return row->sort ? row->sort : gtk_combo_row_get_text (row);
}


/***/


static void     list_row_activated   (GtkListBox     *list,
                                      GtkListBoxRow  *row,
                                      GtkCombo       *combo);
static void     search_changed       (GtkSearchEntry *entry,
                                      GtkCombo       *combo);
static void     custom_row_activated (GtkListBox     *list,
                                      GtkListBoxRow  *row,
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
static void     collapse_list        (GtkCombo       *combo);
static void     expand_list          (GtkCombo       *combo);
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

static void     gtk_combo_buildable_init (GtkBuildableIface *iface);


struct _GtkCombo
{
  GtkBin parent;

  const gchar *active;
  gchar *placeholder;
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
  PROP_PLACEHOLDER,
  PROP_ALLOW_CUSTOM
};

static GtkBuildableIface *buildable_parent_iface = NULL;

G_DEFINE_TYPE_WITH_CODE (GtkCombo, gtk_combo, GTK_TYPE_BIN,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_combo_buildable_init))

static void
gtk_combo_init (GtkCombo *combo)
{
  gtk_widget_init_template (GTK_WIDGET (combo));

  gtk_list_box_set_header_func (GTK_LIST_BOX (combo->list), list_header_func, combo, NULL);
  gtk_list_box_set_filter_func (GTK_LIST_BOX (combo->list), list_filter_func, combo, NULL);
  gtk_list_box_set_sort_func (GTK_LIST_BOX (combo->list), list_sort_func, combo, NULL);

  gtk_list_box_set_header_func (GTK_LIST_BOX (combo->custom), custom_header_func, combo, NULL);

  reset_popover (combo->popover, combo);
}

static void
gtk_combo_finalize (GObject *object)
{
  GtkCombo *combo = GTK_COMBO (object);

  g_free (combo->placeholder);

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
    case PROP_PLACEHOLDER:
      gtk_combo_set_placeholder (combo, g_value_get_string (value));
      break;
    case PROP_ALLOW_CUSTOM:
      gtk_combo_set_allow_custom (combo, g_value_get_boolean (value));
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
    case PROP_PLACEHOLDER:
      g_value_set_string (value, gtk_combo_get_placeholder (combo));
      break;
    case PROP_ALLOW_CUSTOM:
      g_value_set_boolean (value, gtk_combo_get_allow_custom (combo));
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

  g_object_class_install_property (object_class,
                                   PROP_ACTIVE,
                                   g_param_spec_string ("active", P_("Active"), P_("Active"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_PLACEHOLDER,
                                   g_param_spec_string ("placeholder", P_("Placeholder"), P_("Placeholder"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_ALLOW_CUSTOM,
                                   g_param_spec_boolean ("allow-custom", P_("Allow custom"), P_("Allow custom"),
                                                         FALSE,
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
  gtk_widget_class_bind_template_callback (widget_class, custom_row_activated);
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

  GString       *string;

  gchar         *context;
  guint          translatable : 1;
  guint          is_text      : 1;
} ItemParserData;

static void
item_start_element (GMarkupParseContext *context,
                    const gchar         *element_name,
                    const gchar        **names,
                    const gchar        **values,
                    gpointer             user_data,
                    GError             **error)
{
  ItemParserData *data = (ItemParserData*)user_data;
  guint i;

  if (strcmp (element_name, "item") == 0)
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
          else
            g_warning ("Unknown custom combo item attribute: %s", names[i]);
        }
    }
}

static void
item_text (GMarkupParseContext *context,
           const gchar         *text,
           gsize                text_len,
           gpointer             user_data,
           GError             **error)
{
  ItemParserData *data = (ItemParserData*)user_data;

  if (data->is_text)
    g_string_append_len (data->string, text, text_len);
}

static void
item_end_element (GMarkupParseContext *context,
                  const gchar         *element_name,
                  gpointer             user_data,
                  GError             **error)
{
  ItemParserData *data = (ItemParserData*)user_data;

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

      gtk_combo_add_item (GTK_COMBO (data->object), data->id, data->string->str, data->sort);
    }

  data->translatable = FALSE;
  g_string_set_size (data->string, 0);
  g_free (data->context);
  data->context = NULL;
  g_free (data->id);
  data->id = NULL;
  data->is_text = FALSE;
}

static const GMarkupParser item_parser =
{
  item_start_element,
  item_end_element,
  item_text
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

  if (strcmp (tagname, "items") == 0)
    {
      ItemParserData *parser_data;

      parser_data = g_slice_new0 (ItemParserData);
      parser_data->builder = g_object_ref (builder);
      parser_data->object = g_object_ref (buildable);
      parser_data->domain = gtk_builder_get_translation_domain (builder);
      parser_data->string = g_string_new ("");
      *parser = item_parser;
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
  ItemParserData *data;

  buildable_parent_iface->custom_finished (buildable, builder, child,
                                           tagname, user_data);

  if (strcmp (tagname, "items") == 0)
    {
      data = (ItemParserData*)user_data;

      g_object_unref (data->object);
      g_object_unref (data->builder);
      g_string_free (data->string, TRUE);
      g_slice_free (ItemParserData, data);
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
  GtkWidget *ret = NULL;

  if (before && !gtk_list_box_row_get_header (row))
    {
      ret = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_list_box_row_set_header (row, ret);
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

  if (!gtk_revealer_get_child_revealed (GTK_REVEALER (combo->search_revealer)))
    return TRUE;

  text = gtk_entry_get_text (GTK_ENTRY (combo->search_entry));
  if (text[0] == '\0')
    return TRUE;

  if (!GTK_IS_COMBO_ROW (row))
    return TRUE;

  row_text = g_utf8_strdown (gtk_combo_row_get_text (GTK_COMBO_ROW (row)), -1);
  search_text = g_utf8_strdown (text, -1);
  ret = strstr (row_text, search_text) != NULL;
  g_free (row_text);
  g_free (search_text);

  return ret;
}

static gint
list_sort_func (GtkListBoxRow *row1,
                GtkListBoxRow *row2,
                gpointer       data)
{
  GtkCombo *combo = data;

  if (row1 == row2)
    return 0;

  if (GTK_IS_COMBO_ROW (row1) && GTK_IS_COMBO_ROW (row2))
    {
      const gchar *sort1;
      const gchar *sort2;

      sort1 = gtk_combo_row_get_sort (GTK_COMBO_ROW (row1));
      sort2 = gtk_combo_row_get_sort (GTK_COMBO_ROW (row2));

      return g_strcmp0 (sort1, sort2);
    }

  if ((GtkWidget*)row1 == combo->add_custom)
    return -1;

  if ((GtkWidget*)row1 == combo->show_more)
    return 1;

  if ((GtkWidget*)row2 == combo->add_custom)
    return 1;

  if ((GtkWidget*)row2 == combo->show_more)
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
  expand_list (combo);
  gtk_list_box_invalidate_filter (GTK_LIST_BOX (combo->list));
}

static void
reset_popover (GtkWidget *widget,
               GtkCombo  *combo)
{
  gtk_stack_set_visible_child_name (GTK_STACK (combo->stack), "list");
  gtk_entry_set_text (GTK_ENTRY (combo->search_entry), "");
  collapse_list (combo);
  gtk_entry_set_text (GTK_ENTRY (combo->custom_entry), "");
}

typedef struct
{
  const gchar *id;
  GtkWidget *row;
} ForeachData;

static void
find_row (GtkWidget *row,
          gpointer   data)
{
  ForeachData *d = data;
  const gchar *id;

  if (!GTK_IS_COMBO_ROW (row))
    return;

  id = gtk_combo_row_get_id (GTK_COMBO_ROW (row));

  if (g_strcmp0 (id, d->id) == 0)
    d->row = row;
}

static void
add_to_list (GtkCombo    *combo,
             const gchar *id,
             const gchar *text,
             const gchar *sort)
{
  GtkWidget *row;
  ForeachData data;

  data.id = id;
  data.row = NULL;
  gtk_container_foreach (GTK_CONTAINER (combo->list), find_row, &data);

  if (data.row)
    {
      gtk_combo_row_set_sort (GTK_COMBO_ROW (data.row), sort);
      gtk_combo_row_set_text (GTK_COMBO_ROW (data.row), text);
      gtk_list_box_invalidate_sort (GTK_LIST_BOX (combo->list));
      gtk_list_box_invalidate_filter (GTK_LIST_BOX (combo->list));
    }
  else
    {
      row = gtk_combo_row_new (id, sort, text);
      gtk_list_box_insert (GTK_LIST_BOX (combo->list), row, -1);
    }
}

static gboolean
remove_from_list (GtkCombo    *combo,
                  const gchar *id)
{
  ForeachData data;

  data.id = id;
  data.row = NULL;
  gtk_container_foreach (GTK_CONTAINER (combo->list), find_row, &data);

  if (data.row)
    {
      gtk_container_remove (GTK_CONTAINER (combo->list), data.row);
      return TRUE;
    }

  return FALSE;
}

static void
update_check (GtkWidget *row,
              gpointer   data)
{
  ForeachData *d = data;
  const gchar *id;

  if (!GTK_IS_COMBO_ROW (row))
    return;

  id = gtk_combo_row_get_id (GTK_COMBO_ROW (row));
  if (g_strcmp0 (id, d->id) == 0)
    {
      g_object_set (row, "active", TRUE, NULL);
      d->row = row;
    }
  else
    {
      g_object_set (row, "active", FALSE, NULL);
    }
}

static void
set_active (GtkCombo    *combo,
            const gchar *id)
{
  const gchar *key;
  const gchar *text;
  ForeachData data;

  data.id = id;
  data.row = NULL;
  gtk_container_foreach (GTK_CONTAINER (combo->list), update_check, &data);

  if (!data.row)
    {
      key = NULL;
      text = combo->placeholder;
    }
  else
    {
      key = gtk_combo_row_get_id (GTK_COMBO_ROW (data.row));
      text = gtk_combo_row_get_text (GTK_COMBO_ROW (data.row));
    }

  combo->active = key;
  gtk_label_set_text (GTK_LABEL (combo->active_label), text);

  g_object_notify (G_OBJECT (combo), "active");
}

static void
list_row_activated (GtkListBox    *list,
                    GtkListBoxRow *row,
                    GtkCombo      *combo)
{
  if ((GtkWidget*)row == combo->show_more)
    {
      expand_list (combo);
      return;
    }

  if ((GtkWidget*)row == combo->add_custom)
    {
      gtk_stack_set_visible_child_name (GTK_STACK (combo->stack), "custom");
      gtk_widget_grab_focus (combo->custom_entry);
      return;
    }

  if (GTK_IS_COMBO_ROW (row))
    set_active (combo, gtk_combo_row_get_id (GTK_COMBO_ROW (row)));

  gtk_widget_hide (combo->popover);
}

static void
custom_row_activated (GtkListBox    *list,
                      GtkListBoxRow *row,
                      GtkCombo      *combo)
{
  if ((GtkWidget*)row == combo->back_to_list)
    {
      gtk_stack_set_visible_child_name (GTK_STACK (combo->stack), "list");
    }
}

static void
custom_entry_done (GtkWidget *widget,
                   GtkCombo  *combo)
{
  const gchar *text;

  text = gtk_entry_get_text (GTK_ENTRY (combo->custom_entry));
  if (text[0] != '\0')
    {
      gtk_combo_add_item (combo, text, text, text);
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
update_scrollbar (GtkCombo *combo,
                  gboolean  allow)
{
  GtkPolicyType policy;

  g_object_get (combo->scrolled_window, "vscrollbar-policy", &policy, NULL);

  if ((allow && policy == GTK_POLICY_AUTOMATIC) ||
      (!allow && policy == GTK_POLICY_NEVER))
    return;

  if (allow)
    {
      gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (combo->scrolled_window),
                                                  gtk_widget_get_allocated_height (combo->list));
      g_object_set (combo->scrolled_window, "vscrollbar-policy", GTK_POLICY_AUTOMATIC, NULL);
    }
  else
    {
      gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (combo->scrolled_window),
                                                  -1);
      g_object_set (combo->scrolled_window, "vscrollbar-policy", GTK_POLICY_NEVER, NULL);
    }
}

static void
show_few (GtkWidget *widget, gpointer data)
{
  gint *count = data;

  if (!GTK_IS_COMBO_ROW (widget))
    return;

  if (*count < 6)
    gtk_widget_show (widget);
  else
    gtk_widget_hide (widget);

  (*count)++;
}

static void
collapse_list (GtkCombo *combo)
{
  gint count;

  count = 0;
  gtk_container_foreach (GTK_CONTAINER (combo->list), show_few, &count);
  if (count < 7)
    {
      gtk_revealer_set_reveal_child (GTK_REVEALER (combo->search_revealer), FALSE);
      gtk_widget_hide (combo->show_more);
    }
  else
    {
      gtk_revealer_set_reveal_child (GTK_REVEALER (combo->search_revealer), TRUE);
      gtk_widget_show (combo->show_more);
    }

  update_scrollbar (combo, FALSE);
}

static void
show_all (GtkWidget *widget, gpointer data)
{
  if (!GTK_IS_COMBO_ROW (widget))
    return;

  gtk_widget_show (widget);
}

static void
expand_list (GtkCombo *combo)
{
  if (gtk_widget_get_visible (combo->show_more))
    {
      gtk_container_foreach (GTK_CONTAINER (combo->list), show_all, NULL);
      gtk_widget_hide (combo->show_more);
      update_scrollbar (combo, TRUE);
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

  if (g_strcmp0 (gtk_stack_get_visible_child_name (GTK_STACK (combo->stack)), "custom") == 0 ||
      gtk_revealer_get_reveal_child (GTK_REVEALER (combo->search_revealer)) ||
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
  gtk_widget_show (combo->popover);
  return popover_key_press (combo->popover, event, combo);
}


/***/

GtkWidget *
gtk_combo_new (void)
{
  return g_object_new (GTK_TYPE_COMBO, NULL);
}

const gchar *
gtk_combo_get_active (GtkCombo *combo)
{
  g_return_val_if_fail (GTK_IS_COMBO (combo), NULL);

  return combo->active;
}

void
gtk_combo_set_active (GtkCombo    *combo,
                      const gchar *id)
{
  g_return_if_fail (GTK_IS_COMBO (combo));

  set_active (combo, id);
}

void
gtk_combo_add_item (GtkCombo    *combo,
                    const gchar *id,
                    const gchar *text,
                    const gchar *sort)
{
  g_return_if_fail (GTK_IS_COMBO (combo));

  add_to_list (combo, id, text, sort);
  collapse_list (combo);
}

void
gtk_combo_remove_item (GtkCombo    *combo,
                       const gchar *id)
{
  g_return_if_fail (GTK_IS_COMBO (combo));

  if (!remove_from_list (combo, id))
    return;

  if (g_strcmp0 (id, combo->active) == 0)
    set_active (combo, NULL);

  collapse_list (combo);
}

void
gtk_combo_set_placeholder (GtkCombo    *combo,
                           const gchar *text)
{
  g_return_if_fail (GTK_IS_COMBO (combo));

  g_free (combo->placeholder);
  combo->placeholder = g_strdup (text);

  if (combo->active == NULL)
    gtk_label_set_text (GTK_LABEL (combo->active_label), combo->placeholder);

  g_object_notify (G_OBJECT (combo), "placeholder");
}

const gchar *
gtk_combo_get_placeholder (GtkCombo *combo)
{
  g_return_val_if_fail (GTK_IS_COMBO (combo), NULL);

  return combo->placeholder;
}

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

gboolean
gtk_combo_get_allow_custom (GtkCombo *combo)
{
  g_return_val_if_fail (GTK_IS_COMBO (combo), FALSE);

  return combo->allow_custom;
}
