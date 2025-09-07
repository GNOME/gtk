/* Image Filtering
 * #Keywords: brightness, contrast, saturation, sepia, blur, color, posterize, graph
 *
 * Show some image filters.
 *
 * This includes both TV-style brightness and contrast controls, as well
 * as more complex effects such as sepia. All of the filters are applied
 * on the GPU.
 *
 * Also demonstrate how to use GskPath for drawing simple graphs.
 */

#include <gtk/gtk.h>

#include "filter_paintable.h"
#include "component_filter.h"

static GtkWidget *window = NULL;

static void
set_wait_cursor (GtkWidget *widget)
{
  gtk_widget_set_cursor_from_name (GTK_WIDGET (gtk_widget_get_root (widget)), "wait");
}

static void
unset_wait_cursor (GtkWidget *widget)
{
  gtk_widget_set_cursor (GTK_WIDGET (gtk_widget_get_root (widget)), NULL);
}

static void
texture_loaded (GObject      *source,
                GAsyncResult *result,
                gpointer      data)
{
  GdkTexture *texture;
  GError *error = NULL;

  texture = g_task_propagate_pointer (G_TASK (result), &error);

  if (!texture)
    {
      g_print ("%s\n", error->message);
      g_error_free (error);
      return;
    }

  g_object_set (gtk_picture_get_paintable (GTK_PICTURE (source)),
                "texture", texture,
                NULL);

  unset_wait_cursor (GTK_WIDGET (source));
}

static void
load_texture (GTask        *task,
              gpointer      source_object,
              gpointer      task_data,
              GCancellable *cable)
{
  GFile *file = task_data;
  GdkTexture *texture;
  GError *error = NULL;

  texture = gdk_texture_new_from_file (file, &error);

  if (texture)
    g_task_return_pointer (task, texture, g_object_unref);
  else
    g_task_return_error (task, error);
}

static void
open_file_async (GFile    *file,
                 gpointer  data)
{
  GTask *task;

  set_wait_cursor (GTK_WIDGET (data));

  task = g_task_new (G_OBJECT (data), NULL, texture_loaded, NULL);
  g_task_set_task_data (task, g_object_ref (file), g_object_unref);
  g_task_run_in_thread (task, load_texture);
  g_object_unref (task);
}

static void
file_opened (GObject      *source,
             GAsyncResult *result,
             void         *data)
{
  GFile *file;
  GError *error = NULL;

  file = gtk_file_dialog_open_finish (GTK_FILE_DIALOG (source), result, &error);

  if (!file)
    {
      g_print ("%s\n", error->message);
      g_error_free (error);
      return;
    }

  open_file_async (file, data);

  g_object_unref (file);
}

static void
open_file (GtkWidget *picture)
{
  GtkWindow *parent = GTK_WINDOW (gtk_widget_get_root (picture));
  GtkFileDialog *dialog;
  GtkFileFilter *filter;
  GListStore *filters;

  dialog = gtk_file_dialog_new ();

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "Images");
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_file_filter_add_pixbuf_formats (filter);
G_GNUC_END_IGNORE_DEPRECATIONS
  filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
  g_list_store_append (filters, filter);
  g_object_unref (filter);

  gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));
  g_object_unref (filters);

  gtk_file_dialog_open (dialog, parent, NULL, file_opened, picture);

  g_object_unref (dialog);
}

GtkWidget *
do_image_filtering (GtkWidget *do_widget)
{
  static GtkCssProvider *css_provider = NULL;

  if (!css_provider)
    {
      css_provider = gtk_css_provider_new ();
      gtk_css_provider_load_from_resource (css_provider, "/image_filtering/image_filtering.css");

      gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                                  GTK_STYLE_PROVIDER (css_provider),
                                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

  if (!window)
    {
      GtkBuilder *builder;
      GtkBuilderScope *scope;

      g_type_ensure (gtk_filter_paintable_get_type ());
      g_type_ensure (component_filter_get_type ());

      scope = gtk_builder_cscope_new ();
      gtk_builder_cscope_add_callback (scope, open_file);

      builder = gtk_builder_new ();
      gtk_builder_set_scope (builder, scope);

      GError *error = NULL;
      if (!gtk_builder_add_from_resource (builder, "/image_filtering/image_filtering.ui", &error))
        g_print ("%s", error->message);

      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      g_object_unref (builder);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
