/* Lists/Settings
 *
 * This demo shows a settings viewer for GSettings.
 *
 * It demonstrates how to implement support for trees with GtkListView.
 *
 * It also shows how to set up sorting for columns in a GtkColumnView.
 */

#include <gtk/gtk.h>

/* Create an object that wraps GSettingsSchemaKey because that's a boxed type */
typedef struct _SettingsKey SettingsKey;
struct _SettingsKey
{
  GObject parent_instance;

  GSettings *settings;
  GSettingsSchemaKey *key;
};

enum {
  PROP_0,
  PROP_NAME,
  PROP_SUMMARY,
  PROP_DESCRIPTION,
  PROP_VALUE,

  N_PROPS
};

#define SETTINGS_TYPE_KEY (settings_key_get_type ())
G_DECLARE_FINAL_TYPE (SettingsKey, settings_key, SETTINGS, KEY, GObject);

G_DEFINE_TYPE (SettingsKey, settings_key, G_TYPE_OBJECT);
static GParamSpec *properties[N_PROPS] = { NULL, };

static void
settings_key_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  SettingsKey *self = SETTINGS_KEY (object);

  switch (property_id)
    {
    case PROP_DESCRIPTION:
      g_value_set_string (value, g_settings_schema_key_get_description (self->key));
      break;

    case PROP_NAME:
      g_value_set_string (value, g_settings_schema_key_get_name (self->key));
      break;

    case PROP_SUMMARY:
      g_value_set_string (value, g_settings_schema_key_get_summary (self->key));
      break;

    case PROP_VALUE:
      {
        GVariant *variant = g_settings_get_value (self->settings, g_settings_schema_key_get_name (self->key));
        g_value_take_string (value, g_variant_print (variant, FALSE));
        g_variant_unref (variant);
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
settings_key_class_init (SettingsKeyClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = settings_key_get_property;

  properties[PROP_DESCRIPTION] =
    g_param_spec_string ("description", NULL, NULL, NULL, G_PARAM_READABLE);
  properties[PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL, NULL, G_PARAM_READABLE);
  properties[PROP_SUMMARY] =
    g_param_spec_string ("summary", NULL, NULL, NULL, G_PARAM_READABLE);
  properties[PROP_VALUE] =
    g_param_spec_string ("value", NULL, NULL, NULL, G_PARAM_READABLE);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
settings_key_init (SettingsKey *self)
{
}

static SettingsKey *
settings_key_new (GSettings          *settings,
                  GSettingsSchemaKey *key)
{
  SettingsKey *result = g_object_new (SETTINGS_TYPE_KEY, NULL);

  result->settings = g_object_ref (settings);
  result->key = g_settings_schema_key_ref (key);

  return result;
}

static int
strvcmp (gconstpointer p1,
         gconstpointer p2)
{
  const char * const *s1 = p1;
  const char * const *s2 = p2;

  return strcmp (*s1, *s2);
}

static GtkFilter *current_filter;

static gboolean
transform_settings_to_keys (GBinding     *binding,
                            const GValue *from_value,
                            GValue       *to_value,
                            gpointer      data)
{
  GtkTreeListRow *treelistrow;
  GSettings *settings;
  GSettingsSchema *schema;
  GListStore *store;
  GtkSortListModel *sort_model;
  GtkFilterListModel *filter_model;
  GtkFilter *filter;
  GtkExpression *expression;
  char **keys;
  guint i;

  treelistrow = g_value_get_object (from_value);
  if (treelistrow == NULL)
    return TRUE;
  settings = gtk_tree_list_row_get_item (treelistrow);
  g_object_get (settings, "settings-schema", &schema, NULL);

  store = g_list_store_new (SETTINGS_TYPE_KEY);

  keys = g_settings_schema_list_keys (schema);

  for (i = 0; keys[i] != NULL; i++)
    {
      GSettingsSchemaKey *almost_there = g_settings_schema_get_key (schema, keys[i]);
      SettingsKey *finally = settings_key_new (settings, almost_there);
      g_list_store_append (store, finally);
      g_object_unref (finally);
      g_settings_schema_key_unref (almost_there);
    }

  g_strfreev (keys);
  g_settings_schema_unref (schema);
  g_object_unref (settings);

  sort_model = gtk_sort_list_model_new (G_LIST_MODEL (store),
                                        gtk_column_view_get_sorter (GTK_COLUMN_VIEW (data)));
  expression = gtk_property_expression_new (SETTINGS_TYPE_KEY, NULL, "name");
  filter = gtk_string_filter_new ();
  gtk_string_filter_set_expression (GTK_STRING_FILTER (filter), expression);
  filter_model = gtk_filter_list_model_new (G_LIST_MODEL (sort_model), filter);
  gtk_expression_unref (expression);
  g_object_unref (sort_model);

  g_set_object (&current_filter, filter);

  g_object_unref (filter);

  g_value_take_object (to_value, filter_model);

  return TRUE;
}

static GListModel *
create_settings_model (gpointer item,
                       gpointer unused)
{
  GSettings *settings = item;
  char **schemas;
  GListStore *result;
  guint i;

  if (settings == NULL)
    {
      g_settings_schema_source_list_schemas (g_settings_schema_source_get_default (),
                                             TRUE,
                                             &schemas,
                                             NULL);
    }
  else
    {
      schemas = g_settings_list_children (settings);
    }

  if (schemas == NULL || schemas[0] == NULL)
    {
      g_free (schemas);
      return NULL;
    }

  qsort (schemas, g_strv_length (schemas), sizeof (char *), strvcmp);

  result = g_list_store_new (G_TYPE_SETTINGS);
  for (i = 0;  schemas[i] != NULL; i++)
    {
      GSettings *child;

      if (settings == NULL)
        child = g_settings_new (schemas[i]);
      else
        child = g_settings_get_child (settings, schemas[i]);

      g_list_store_append (result, child);
      g_object_unref (child);
    }

  return G_LIST_MODEL (result);
}

static void
search_enabled (GtkSearchEntry *entry)
{
  gtk_editable_set_text (GTK_EDITABLE (entry), "");
}

static void
search_changed (GtkSearchEntry *entry,
                gpointer data)
{
  const char *text = gtk_editable_get_text (GTK_EDITABLE (entry));

  if (current_filter)
    gtk_string_filter_set_search (GTK_STRING_FILTER (current_filter), text);
}

static void
stop_search (GtkSearchEntry *entry,
             gpointer data)
{
  gtk_editable_set_text (GTK_EDITABLE (entry), "");

  if (current_filter)
    gtk_string_filter_set_search (GTK_STRING_FILTER (current_filter), "");
}

static GtkWidget *window = NULL;

GtkWidget *
do_listview_settings (GtkWidget *do_widget)
{
  if (window == NULL)
    {
      GtkWidget *listview, *columnview;
      GListModel *model;
      GtkTreeListModel *treemodel;
      GtkSingleSelection *selection;
      GtkBuilderScope *scope;
      GtkBuilder *builder;
      GtkColumnViewColumn *name_column;
      GtkSorter *sorter;

      g_type_ensure (SETTINGS_TYPE_KEY);

      scope = gtk_builder_cscope_new ();
      gtk_builder_cscope_add_callback_symbol (GTK_BUILDER_CSCOPE (scope), "search_enabled", (GCallback)search_enabled);
      gtk_builder_cscope_add_callback_symbol (GTK_BUILDER_CSCOPE (scope), "search_changed", (GCallback)search_changed);
      gtk_builder_cscope_add_callback_symbol (GTK_BUILDER_CSCOPE (scope), "stop_search", (GCallback)stop_search);

      builder = gtk_builder_new ();
      gtk_builder_set_scope (builder, scope);
      gtk_builder_add_from_resource (builder, "/listview_settings/listview_settings.ui", NULL);

      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *) &window);

      listview = GTK_WIDGET (gtk_builder_get_object (builder, "listview"));
      columnview = GTK_WIDGET (gtk_builder_get_object (builder, "columnview"));
      model = create_settings_model (NULL, NULL);
      treemodel = gtk_tree_list_model_new (FALSE,
                                           model,
                                           TRUE,
                                           create_settings_model,
                                           NULL,
                                           NULL);
      selection = gtk_single_selection_new (G_LIST_MODEL (treemodel));
      g_object_bind_property_full (selection, "selected-item",
                                   columnview, "model",
                                   G_BINDING_SYNC_CREATE,
                                   transform_settings_to_keys,
                                   NULL,
                                   columnview, NULL);
      gtk_list_view_set_model (GTK_LIST_VIEW (listview), G_LIST_MODEL (selection));
      g_object_unref (selection);
      g_object_unref (treemodel);
      g_object_unref (model);

      name_column = GTK_COLUMN_VIEW_COLUMN (gtk_builder_get_object (builder, "name_column"));
      sorter = gtk_string_sorter_new (gtk_property_expression_new (SETTINGS_TYPE_KEY, NULL, "name"));
      gtk_column_view_column_set_sorter (name_column, sorter);
      g_object_unref (sorter);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
