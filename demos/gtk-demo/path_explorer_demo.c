/* Path/Path Explorer
 *
 * This demo lets you explore some of the features of GskPath.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "range-editor.h"
#include "path_explorer.h"

static gboolean
text_to_path (GBinding     *binding,
              const GValue *from,
              GValue       *to,
              gpointer      user_data)
{
  const char *text;
  GskPath *path;

  text = g_value_get_string (from);
  path = gsk_path_parse (text);
  g_value_take_boxed (to, path);

  return TRUE;
}

static gboolean
path_to_text (GBinding     *binding,
              const GValue *from,
              GValue       *to,
              gpointer      user_data)
{
  GskPath *path;
  char *text;

  path = g_value_get_boxed (from);
  text = gsk_path_to_string (path);
  g_value_take_string (to, text);

  return TRUE;
}

GtkWidget *
do_path_explorer_demo (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  static GtkCssProvider *css_provider = NULL;

  if (!css_provider)
    {
      css_provider = gtk_css_provider_new ();
      gtk_css_provider_load_from_resource (css_provider, "/path_explorer_demo/path_explorer_demo.css");

      gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                                  GTK_STYLE_PROVIDER (css_provider),
                                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

  if (!window)
    {
      GtkBuilder *builder;
      PathExplorer *demo;
      GtkWidget *entry;
      GError *error = NULL;

      g_type_ensure (path_explorer_get_type ());
      g_type_ensure (range_editor_get_type ());

      builder = gtk_builder_new ();

      gtk_builder_add_from_resource (builder, "/path_explorer_demo/path_explorer_demo.ui", &error);
      if (error)
        g_error ("%s", error->message);

      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      demo = PATH_EXPLORER (gtk_builder_get_object (builder, "demo"));
      entry = GTK_WIDGET (gtk_builder_get_object (builder, "entry"));
      g_object_bind_property_full (demo, "path",
                                   entry, "text",
                                   G_BINDING_BIDIRECTIONAL|G_BINDING_SYNC_CREATE,
                                   path_to_text,
                                   text_to_path,
                                   NULL, NULL);

      g_object_unref (builder);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
