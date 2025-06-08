/* Snapping
 * #Keywords: fractional, scale
 *
 * This demo lets you experiment with the effect of snapping
 * render nodes.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

#include "snappaintable.h"

static void
file_chooser_response (GObject      *source,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
  SnapPaintable *snap = SNAP_PAINTABLE (user_data);
  GFile *file;

  file = gtk_file_dialog_open_finish (dialog, result, NULL);
  if (file)
    {
      snap_paintable_set_file (snap, file);
      g_object_unref (file);
    }
}

static void
open_file_cb (GtkWidget *button,
              gpointer   user_data)
{
  GtkFileDialog *dialog;

  dialog = gtk_file_dialog_new ();

  gtk_file_dialog_open (dialog,
                        GTK_WINDOW (gtk_widget_get_ancestor (button, GTK_TYPE_WINDOW)),
                        NULL,
                        file_chooser_response, user_data);

  g_object_unref (dialog);
}

static void
open_logo_cb (GtkWidget *button,
              gpointer   user_data)
{
  SnapPaintable *snap = SNAP_PAINTABLE (user_data);
  GFile *file;

  file = g_file_new_for_uri ("resource:///snap/start-here.png");
  snap_paintable_set_file (snap, file);
  snap_paintable_set_zoom (snap, 10);
  g_object_unref (file);
}

static void
open_rose_cb (GtkWidget *button,
              gpointer   user_data)
{
  SnapPaintable *snap = SNAP_PAINTABLE (user_data);
  GFile *file;

  file = g_file_new_for_uri ("resource:///snap/portland-rose.jpg");
  snap_paintable_set_file (snap, file);
  snap_paintable_set_zoom (snap, -4);
  g_object_unref (file);
}

static void
selected_snap_changed_cb (GtkDropDown *dropdown,
                          GParamSpec  *pspec,
                          gpointer     user_data)
{
  SnapPaintable *snap = SNAP_PAINTABLE (user_data);
  GskRectSnap snap_value[] = {
    GSK_RECT_SNAP_NONE,
    GSK_RECT_SNAP_GROW,
    GSK_RECT_SNAP_SHRINK,
    GSK_RECT_SNAP_ROUND
  };
  guint idx;

  idx = gtk_drop_down_get_selected (dropdown);
  snap_paintable_set_snap (snap, snap_value[idx]);
}

static void
selected_tiles_changed_cb (GtkDropDown *dropdown,
                           GParamSpec  *pspec,
                           gpointer     user_data)
{
  SnapPaintable *snap = SNAP_PAINTABLE (user_data);
  guint idx;

  idx = gtk_drop_down_get_selected (dropdown);
  snap_paintable_set_tiles (snap, idx != 0);
}

static void
zoom_in_cb (GtkButton *button,
            gpointer   user_data)
{
  SnapPaintable *snap = SNAP_PAINTABLE (user_data);
  int zoom_level;

  zoom_level = snap_paintable_get_zoom (snap);
  snap_paintable_set_zoom (snap, zoom_level + 1);
}

static void
zoom_out_cb (GtkButton *button,
             gpointer   user_data)
{
  SnapPaintable *snap = SNAP_PAINTABLE (user_data);
  int zoom_level;

  zoom_level = snap_paintable_get_zoom (snap);
  snap_paintable_set_zoom (snap, zoom_level - 1);
}

GtkWidget *
do_snap (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkBuilder *builder;
      GError *error = NULL;
      GtkBuilderScope *scope;

      scope = gtk_builder_cscope_new ();
      gtk_builder_cscope_add_callback (scope, open_file_cb);
      gtk_builder_cscope_add_callback (scope, open_logo_cb);
      gtk_builder_cscope_add_callback (scope, open_rose_cb);
      gtk_builder_cscope_add_callback (scope, selected_snap_changed_cb);
      gtk_builder_cscope_add_callback (scope, selected_tiles_changed_cb);
      gtk_builder_cscope_add_callback (scope, zoom_in_cb);
      gtk_builder_cscope_add_callback (scope, zoom_out_cb);

      g_type_ensure (SNAP_TYPE_PAINTABLE);

      builder = gtk_builder_new ();
      gtk_builder_set_scope (builder, scope);
      if (!gtk_builder_add_from_resource (builder, "/snap/snap.ui", &error))
        g_error ("%s", error->message);

      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);
      gtk_window_set_display (GTK_WINDOW (window), gtk_widget_get_display (do_widget));

      open_logo_cb (NULL, gtk_builder_get_object (builder, "snap_paintable"));

      g_object_unref (builder);
      g_object_unref (scope);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
