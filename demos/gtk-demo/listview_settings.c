/* Lists/Settings
 * #Keywords: GtkListItemFactory, GListModel
 *
 * This demo shows a settings viewer for GSettings.
 *
 * It demonstrates how to implement support for trees with GtkListView.
 * It also shows how to set up sorting and filtering for columns in a
 * GtkColumnView.
 *
 * It also demonstrates different styles of list. The tree on the left
 * uses the ­.navigation-sidebar style class, the list on the right uses
 * the ­.data-table style class.
 */

#include <gtk/gtk.h>

#include <stdlib.h>

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
  PROP_SETTINGS,
  PROP_SUMMARY,
  PROP_DESCRIPTION,
  PROP_VALUE,
  PROP_TYPE,
  PROP_DEFAULT_VALUE,

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

    case PROP_TYPE:
      {
        const GVariantType *type = g_settings_schema_key_get_value_type (self->key);
        g_value_set_string (value, g_variant_type_peek_string (type));
      }
      break;

    case PROP_DEFAULT_VALUE:
      {
        GVariant *variant = g_settings_schema_key_get_default_value (self->key);
        g_value_take_string (value, g_variant_print (variant, FALSE));
        g_variant_unref (variant);
      }
      break;

    case PROP_SETTINGS:
      g_value_set_object (value, self->settings);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
settings_key_finalize (GObject *object)
{
  SettingsKey *self = SETTINGS_KEY (object);

  g_object_unref (self->settings);
  g_settings_schema_key_unref (self->key);

  G_OBJECT_CLASS (settings_key_parent_class)->finalize (object);
}

static void
settings_key_class_init (SettingsKeyClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = settings_key_finalize;
  gobject_class->get_property = settings_key_get_property;

  properties[PROP_DESCRIPTION] =
    g_param_spec_string ("description", NULL, NULL, NULL, G_PARAM_READABLE);
  properties[PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL, NULL, G_PARAM_READABLE);
  properties[PROP_SETTINGS] =
    g_param_spec_object ("settings", NULL, NULL, G_TYPE_SETTINGS, G_PARAM_READABLE);
  properties[PROP_SUMMARY] =
    g_param_spec_string ("summary", NULL, NULL, NULL, G_PARAM_READABLE);
  properties[PROP_VALUE] =
    g_param_spec_string ("value", NULL, NULL, NULL, G_PARAM_READABLE);
  properties[PROP_TYPE] =
    g_param_spec_string ("type", NULL, NULL, NULL, G_PARAM_READABLE);
  properties[PROP_DEFAULT_VALUE] =
    g_param_spec_string ("default-value", NULL, NULL, NULL, G_PARAM_READABLE);

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

static char *
settings_key_get_search_string (SettingsKey *self)
{
  char *schema, *result;

  g_object_get (self->settings, "schema-id", &schema, NULL);

  result = g_strconcat (g_settings_schema_key_get_name (self->key), " ",
                        g_settings_schema_key_get_summary (self->key), " ",
                        schema,
                        NULL);

  g_free (schema);

  return result;
}

static void
item_value_changed (GtkEditableLabel  *label,
                    GParamSpec        *pspec,
                    GtkColumnViewCell *cell)
{
  SettingsKey *self;
  const char *text;
  const GVariantType *type;
  GVariant *variant;
  GError *error = NULL;
  const char *name;
  char *value;

  text = gtk_editable_get_text (GTK_EDITABLE (label));

  self = gtk_column_view_cell_get_item (cell);

  type = g_settings_schema_key_get_value_type (self->key);
  name = g_settings_schema_key_get_name (self->key);

  variant = g_variant_parse (type, text, NULL, NULL, &error);
  if (!variant)
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
      goto revert;
    }

  if (!g_settings_schema_key_range_check (self->key, variant))
    {
      g_warning ("Not a valid value for %s", name);
      goto revert;
    }

  g_settings_set_value (self->settings, name, variant);
  g_variant_unref (variant);
  return;

revert:
  gtk_widget_error_bell (GTK_WIDGET (label));

  g_object_get (self, "value", &value, NULL);
  gtk_editable_set_text (GTK_EDITABLE (label), value);
  g_free (value);
}

static int
strvcmp (gconstpointer p1,
         gconstpointer p2)
{
  const char * const *s1 = p1;
  const char * const *s2 = p2;

  return strcmp (*s1, *s2);
}

static gpointer
map_settings_to_keys (gpointer item,
                      gpointer unused)
{
  GSettings *settings = item;
  GSettingsSchema *schema;
  GListStore *store;
  char **keys;
  guint i;

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

  return store;
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

  g_strfreev (schemas);

  return G_LIST_MODEL (result);
}

static void
search_enabled (GtkSearchEntry *entry)
{
  gtk_editable_set_text (GTK_EDITABLE (entry), "");
}

static void
stop_search (GtkSearchEntry *entry,
             gpointer data)
{
  gtk_editable_set_text (GTK_EDITABLE (entry), "");
}

static GtkWidget *window = NULL;

GtkWidget *
do_listview_settings (GtkWidget *do_widget)
{
  if (window == NULL)
    {
      GtkListView *listview;
      GListModel *model;
      GtkTreeListModel *treemodel;
      GtkNoSelection *selection;
      GtkBuilderScope *scope;
      GtkBuilder *builder;
      GError *error = NULL;
      GtkFilter *filter;

      g_type_ensure (SETTINGS_TYPE_KEY);

      scope = gtk_builder_cscope_new ();
      gtk_builder_cscope_add_callback (scope, search_enabled);
      gtk_builder_cscope_add_callback (scope, stop_search);
      gtk_builder_cscope_add_callback (scope, settings_key_get_search_string);
      gtk_builder_cscope_add_callback (scope, item_value_changed);

      builder = gtk_builder_new ();
      gtk_builder_set_scope (builder, scope);
      g_object_unref (scope);

      gtk_builder_add_from_resource (builder, "/listview_settings/listview_settings.ui", &error);
      g_assert_no_error (error);

      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *) &window);

      listview = GTK_LIST_VIEW (gtk_builder_get_object (builder, "listview"));
      filter = GTK_FILTER (gtk_builder_get_object (builder, "filter"));

      model = create_settings_model (NULL, NULL);
      treemodel = gtk_tree_list_model_new (model,
                                           TRUE,
                                           TRUE,
                                           create_settings_model,
                                           NULL,
                                           NULL);
      model = G_LIST_MODEL (gtk_map_list_model_new (G_LIST_MODEL (treemodel), map_settings_to_keys, NULL, NULL));
      model = G_LIST_MODEL (gtk_flatten_list_model_new (model));
      model = G_LIST_MODEL (gtk_filter_list_model_new (model, g_object_ref (filter)));
      selection = gtk_no_selection_new (model);

      gtk_list_view_set_model (GTK_LIST_VIEW (listview), GTK_SELECTION_MODEL (selection));
      g_object_unref (selection);

      g_object_unref (builder);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
