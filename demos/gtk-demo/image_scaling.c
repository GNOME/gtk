/* Image Scaling
 * #Keywords: zoom, scale, filter, action, menu
 *
 * The custom widget we create here is similar to a GtkPicture,
 * but allows setting a zoom level and filtering mode for the
 * displayed paintable.
 *
 * It also demonstrates how to add a context menu to a custom
 * widget and connect it with widget actions.
 *
 * The context menu has items to change the zoom level.
 */

#include <gtk/gtk.h>
#include "demo3widget.h"

static GtkWidget *window = NULL;
static GCancellable *cancellable = NULL;

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

  if (!window)
    {
      g_object_unref (texture);
      return;
    }

  unset_wait_cursor (GTK_WIDGET (data));

  g_object_set (G_OBJECT (data), "texture", texture, NULL);
}

static void
open_file_async (GFile     *file,
                 GtkWidget *demo)
{
  GTask *task;

  set_wait_cursor (demo);

  task = g_task_new (demo, cancellable, texture_loaded, demo);
  g_task_set_task_data (task, g_object_ref (file), g_object_unref);
  g_task_run_in_thread (task, load_texture);
  g_object_unref (task);
}

static void
open_portland_rose (GtkWidget *button,
                    GtkWidget *demo)
{
  GFile *file;

  file = g_file_new_for_uri ("resource:///transparent/portland-rose.jpg");
  open_file_async (file, demo);
  g_object_unref (file);
}

static void
open_large_image (GtkWidget *button,
                  GtkWidget *demo)
{
  GFile *file;

  file = g_file_new_for_uri ("resource:///org/gtk/Demo4/large-image.png");
  open_file_async (file, demo);
  g_object_unref (file);
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
open_file (GtkWidget *picker,
           GtkWidget *demo)
{
  GtkWindow *parent = GTK_WINDOW (gtk_widget_get_root (picker));
  GtkFileDialog *dialog;
  GtkFileFilter *filter;
  GListStore *filters;

  dialog = gtk_file_dialog_new ();

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "Images");
  gtk_file_filter_add_pixbuf_formats (filter);
  filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
  g_list_store_append (filters, filter);
  g_object_unref (filter);

  gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));
  g_object_unref (filters);

  gtk_file_dialog_open (dialog, parent, NULL, file_opened, demo);

  g_object_unref (dialog);
}

static void
rotate (GtkWidget *button,
        GtkWidget *demo)
{
  float angle;

  g_object_get (demo, "angle", &angle, NULL);

  angle = fmodf (angle + 90.f, 360.f);

  g_object_set (demo, "angle", angle, NULL);
}

static gboolean
transform_to (GBinding     *binding,
              const GValue *src,
              GValue       *dest,
              gpointer      user_data)
{
  double from;
  float to;

  from = g_value_get_double (src);
  to = (float) pow (2., from);
  g_value_set_float (dest, to);

  return TRUE;
}

static gboolean
transform_from (GBinding     *binding,
                const GValue *src,
                GValue       *dest,
                gpointer      user_data)
{
  float to;
  double from;

  to = g_value_get_float (src);
  from = log2 (to);
  g_value_set_double (dest, from);

  return TRUE;
}

static void
free_cancellable (gpointer data)
{
  g_cancellable_cancel (cancellable);
  g_clear_object (&cancellable);
}

static gboolean
cancel_load (GtkWidget *widget,
             GVariant  *args,
             gpointer   data)
{
  unset_wait_cursor (widget);
  g_cancellable_cancel (G_CANCELLABLE (data));
  return TRUE;
}

GtkWidget *
do_image_scaling (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkWidget *box;
      GtkWidget *box2;
      GtkWidget *sw;
      GtkWidget *widget;
      GtkWidget *scale;
      GtkWidget *dropdown;
      GtkWidget *button;
      GtkEventController *controller;

      window = gtk_window_new ();
      gtk_window_set_title (GTK_WINDOW (window), "Image Scaling");
      gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      cancellable = g_cancellable_new ();
      g_object_set_data_full (G_OBJECT (window), "cancellable",
                              cancellable, free_cancellable);

      controller = gtk_shortcut_controller_new ();
      gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller),
                                            gtk_shortcut_new (
                                                gtk_keyval_trigger_new (GDK_KEY_Escape, 0),
                                                gtk_callback_action_new (cancel_load, cancellable, NULL)
                                                ));
      gtk_shortcut_controller_set_scope (GTK_SHORTCUT_CONTROLLER (controller),
                                         GTK_SHORTCUT_SCOPE_GLOBAL);
      gtk_widget_add_controller (window, controller);

      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_window_set_child (GTK_WINDOW (window), box);

      sw = gtk_scrolled_window_new ();
      gtk_widget_set_vexpand (sw, TRUE);
      gtk_box_append (GTK_BOX (box), sw);

      widget = demo3_widget_new ("/transparent/portland-rose.jpg");
      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), widget);

      box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_append (GTK_BOX (box), box2);

      button = gtk_button_new_from_icon_name ("document-open-symbolic");
      gtk_widget_set_tooltip_text (button, "Open File");
      g_signal_connect (button, "clicked", G_CALLBACK (open_file), widget);
      gtk_box_append (GTK_BOX (box2), button);

      button = gtk_button_new ();
      gtk_button_set_child (GTK_BUTTON (button),
                            gtk_image_new_from_resource ("/org/gtk/Demo4/portland-rose-thumbnail.png"));
      gtk_widget_add_css_class (button, "image-button");
      gtk_widget_set_tooltip_text (button, "Portland Rose");
      g_signal_connect (button, "clicked", G_CALLBACK (open_portland_rose), widget);
      gtk_box_append (GTK_BOX (box2), button);

      button = gtk_button_new ();
      gtk_button_set_child (GTK_BUTTON (button),
                            gtk_image_new_from_resource ("/org/gtk/Demo4/large-image-thumbnail.png"));
      gtk_widget_add_css_class (button, "image-button");
      gtk_widget_set_tooltip_text (button, "Large image");
      g_signal_connect (button, "clicked", G_CALLBACK (open_large_image), widget);
      gtk_box_append (GTK_BOX (box2), button);

      button = gtk_button_new_from_icon_name ("object-rotate-right-symbolic");
      gtk_widget_set_tooltip_text (button, "Rotate");
      g_signal_connect (button, "clicked", G_CALLBACK (rotate), widget);
      gtk_box_append (GTK_BOX (box2), button);

      scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, -10., 10., 0.1);
      gtk_scale_add_mark (GTK_SCALE (scale), 0., GTK_POS_TOP, NULL);
      gtk_widget_set_tooltip_text (scale, "Zoom");
      gtk_accessible_update_property (GTK_ACCESSIBLE (scale),
                                      GTK_ACCESSIBLE_PROPERTY_LABEL, "Zoom",
                                      -1);
      gtk_range_set_value (GTK_RANGE (scale), 0.);
      gtk_widget_set_hexpand (scale, TRUE);
      gtk_box_append (GTK_BOX (box2), scale);

      dropdown = gtk_drop_down_new (G_LIST_MODEL (gtk_string_list_new ((const char *[]){ "Linear", "Nearest", "Trilinear", NULL })), NULL);
      gtk_widget_set_tooltip_text (dropdown, "Filter");
      gtk_accessible_update_property (GTK_ACCESSIBLE (dropdown),
                                      GTK_ACCESSIBLE_PROPERTY_LABEL, "Filter",
                                      -1);
      gtk_box_append (GTK_BOX (box2), dropdown);

      g_object_bind_property (dropdown, "selected", widget, "filter", G_BINDING_DEFAULT);

      g_object_bind_property_full (gtk_range_get_adjustment (GTK_RANGE (scale)), "value",
                                   widget, "scale",
                                   G_BINDING_BIDIRECTIONAL,
                                   transform_to,
                                   transform_from,
                                   NULL, NULL);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    {
      gtk_window_destroy (GTK_WINDOW (window));
    }

  return window;
}
