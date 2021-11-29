/* Lists/Releases
 * #Keywords: GtkListItemFactory, GListModel,JSON
 *
 * This demo downloads GTK's latest release and displays them in a list.
 *
 * It shows how hard it still is to get JSON into lists.
 */

#include <gtk/gtk.h>

#include "gtk/json/gtkjsonparserprivate.h"

#define GTK_TYPE_RELEASE (gtk_release_get_type ())

G_DECLARE_FINAL_TYPE (GtkRelease, gtk_release, GTK, RELEASE, GObject)

struct _GtkRelease
{
  GObject parent_instance;

  char *name;
  GDateTime *timestamp;
};

struct _GtkReleaseClass
{
  GObjectClass parent_class;
};

enum {
  PROP_0,
  PROP_NAME,
  PROP_TIMESTAMP,

  N_PROPS
};

G_DEFINE_TYPE (GtkRelease, gtk_release, G_TYPE_OBJECT)
static GParamSpec *properties[N_PROPS] = { NULL, };

static void
gtk_release_finalize (GObject *object)
{
  GtkRelease *self = GTK_RELEASE (object);

  g_free (self->name);
  g_clear_pointer (&self->timestamp, g_date_time_unref);

  G_OBJECT_CLASS (gtk_release_parent_class)->finalize (object);
}

static void
gtk_release_get_property (GObject    *object,
                          guint       property_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GtkRelease *self = GTK_RELEASE (object);

  switch (property_id)
    {
    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    case PROP_TIMESTAMP:
      {
        if (self->timestamp)
          g_value_take_string (value, g_date_time_format (self->timestamp, "%x"));
        else
          g_value_set_string (value, "---");
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_release_class_init (GtkReleaseClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = gtk_release_finalize;
  gobject_class->get_property = gtk_release_get_property;

  properties[PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL, NULL, G_PARAM_READABLE);
  properties[PROP_TIMESTAMP] =
    g_param_spec_string ("timestamp", NULL, NULL, NULL, G_PARAM_READABLE);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gtk_release_init (GtkRelease *self)
{
}

static GtkRelease *
gtk_release_new (const char *name,
                 GDateTime  *timestamp)
{
  GtkRelease *result;

  result = g_object_new (GTK_TYPE_RELEASE, NULL);

  result->name = g_strdup (name);
  if (timestamp)
    result->timestamp = g_date_time_ref (timestamp);

  return result;
}

static void
loaded_some_releases_cb (GObject      *file,
                         GAsyncResult *res,
                         gpointer      userdata)
{
  GListStore *store = userdata;
  GBytes *bytes;
  GtkJsonParser *parser;
  GError *error = NULL;

  bytes = g_file_load_bytes_finish (G_FILE (file), res, NULL, &error);
  if (bytes == NULL)
    {
      g_printerr ("Error loading: %s\n", error->message);
      g_clear_error (&error);
      return;
    }

  parser = gtk_json_parser_new_for_bytes (bytes);
  g_bytes_unref (bytes);

  gtk_json_parser_start_array (parser);
  do
    {
      enum { NAME, COMMIT };
      static const char *options[] = { "name", "commit", NULL };
      GDateTime *created = NULL;
      char *name = NULL;

      gtk_json_parser_start_object (parser);
      do
        {
          switch (gtk_json_parser_select_member (parser, options))
            {
              case NAME:
                g_clear_pointer (&name, g_free);
                name = gtk_json_parser_get_string (parser);
                break;
              case COMMIT:
                g_clear_pointer (&created, g_date_time_unref);
                gtk_json_parser_start_object (parser);
                if (gtk_json_parser_find_member (parser, "created_at"))
                  {
                    char *created_string = gtk_json_parser_get_string (parser);
                    created = g_date_time_new_from_iso8601 (created_string, NULL);
                    g_free (created_string);
                  }
                gtk_json_parser_end (parser);
                break;
              default:
                break;
            }
        }
      while (gtk_json_parser_next (parser));
      gtk_json_parser_end (parser);

      if (name)
        {
          GtkRelease *release = gtk_release_new (name, created);
          g_list_store_append (store, release);
          g_object_unref (release);
        }
      g_clear_pointer (&name, g_free);
      g_clear_pointer (&created, g_date_time_unref);
    }
  while (gtk_json_parser_next (parser));
  gtk_json_parser_end (parser);

  if (gtk_json_parser_get_error (parser))
    {
      const GError *json_error = gtk_json_parser_get_error (parser);

      g_printerr ("Error parsing: %s\n", json_error->message);
    }
  gtk_json_parser_free (parser);

  gtk_toggle_button_set_active (g_object_get_data (G_OBJECT (store), "togglebutton"), FALSE);
}

static void
load_some_releases (GListStore *store)
{
  GFile *file;
  guint n_items;
  char *url;

  n_items = g_list_model_get_n_items (G_LIST_MODEL (store));
  if (n_items == 0)
    url = g_strdup ("https://gitlab.gnome.org/api/v4/projects/665/repository/tags");
  else
    url = g_strdup_printf ("https://gitlab.gnome.org/api/v4/projects/665/repository/tags?page=%u", (n_items + 39) / 20);
  file = g_file_new_for_uri (url);

  g_file_load_bytes_async (file, NULL, loaded_some_releases_cb, store);
  g_object_unref (file);
  g_free (url);
}

static GtkWidget *window = NULL;

GtkWidget *
do_listview_releases (GtkWidget *do_widget)
{
  if (window == NULL)
    {
      GtkBuilderScope *scope;
      GtkBuilder *builder;
      GListStore *list;
      GtkWidget *more_button;

      g_type_ensure (GTK_TYPE_RELEASE);

      scope = gtk_builder_cscope_new ();
      gtk_builder_cscope_add_callback_symbol (GTK_BUILDER_CSCOPE (scope), "load_some_releases", (GCallback)load_some_releases);

      builder = gtk_builder_new ();
      gtk_builder_set_scope (builder, scope);
      g_object_unref (scope);

      gtk_builder_add_from_resource (builder, "/listview_releases/listview_releases.ui", NULL);

      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *) &window);

      more_button = GTK_WIDGET (gtk_builder_get_object (builder, "more_button"));
      list = G_LIST_STORE (gtk_builder_get_object (builder, "list"));
      g_object_set_data (G_OBJECT (list), "togglebutton", more_button);
      load_some_releases (list);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (more_button), TRUE);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
