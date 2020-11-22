#include <gtk/gtk.h>
#include "curve-editor.h"


static GskPath *
make_circle_path (void)
{
  float w = 200;
  float h = 200;
  float cx = w / 2;
  float cy = h / 2;
  float pad = 20;
  float r = (w - 2 * pad) / 2;
  float k = 0.55228;
  float kr = k  * r;
  GskPathBuilder *builder;

  builder = gsk_path_builder_new ();

  gsk_path_builder_move_to (builder,  cx, pad);
  gsk_path_builder_curve_to (builder, cx + kr, pad,
                                      w - pad, cy - kr,
                                      w - pad, cy);
  gsk_path_builder_curve_to (builder, w - pad, cy + kr,
                                      cx + kr, h - pad,
                                      cx, h - pad);
  gsk_path_builder_curve_to (builder, cx - kr, h - pad,
                                      pad, cy + kr,
                                      pad, cy);
  gsk_path_builder_curve_to (builder, pad, cy - kr,
                                      cx - kr, pad,
                                      cx, pad);

  return gsk_path_builder_free_to_path (builder);
}

static void
edit_changed (GtkToggleButton *button,
              GParamSpec      *pspec,
              CurveEditor     *editor)
{
  curve_editor_set_edit (editor, gtk_toggle_button_get_active (button));
}

static void
reset (GtkButton   *button,
       CurveEditor *editor)
{
  GskPath *path;

  path = make_circle_path ();
  curve_editor_set_path (editor, path);
  gsk_path_unref (path);
}

static void
stroke_changed (GtkRange    *range,
                CurveEditor *editor)
{
  GskStroke *stroke;

  stroke = gsk_stroke_new (gtk_range_get_value (range));
  curve_editor_set_stroke (editor, stroke);
  gsk_stroke_free (stroke);
}

int
main (int argc, char *argv[])
{
  GtkWindow *window;
  GtkWidget *demo;
  GtkWidget *edit_toggle;
  GtkWidget *reset_button;
  GtkWidget *titlebar;
  GtkWidget *scale;

  gtk_init ();

  window = GTK_WINDOW (gtk_window_new ());
  gtk_window_set_default_size (GTK_WINDOW (window), 250, 250);

  edit_toggle = gtk_toggle_button_new ();
  gtk_button_set_icon_name (GTK_BUTTON (edit_toggle), "document-edit-symbolic");

  reset_button = gtk_button_new_from_icon_name ("edit-undo-symbolic");

  scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 1, 10, 1);
  gtk_widget_set_size_request (scale, 60, -1);

  titlebar = gtk_header_bar_new ();
  gtk_header_bar_pack_start (GTK_HEADER_BAR (titlebar), edit_toggle);
  gtk_header_bar_pack_start (GTK_HEADER_BAR (titlebar), reset_button);
  gtk_header_bar_pack_end (GTK_HEADER_BAR (titlebar), scale);

  gtk_window_set_titlebar (GTK_WINDOW (window), titlebar);

  demo = curve_editor_new ();

  g_signal_connect (edit_toggle, "notify::active", G_CALLBACK (edit_changed), demo);
  g_signal_connect (reset_button, "clicked", G_CALLBACK (reset), demo);
  g_signal_connect (scale, "value-changed", G_CALLBACK (stroke_changed), demo);

  reset (NULL, CURVE_EDITOR (demo));

  gtk_window_set_child (window, demo);

  gtk_window_present (window);

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
