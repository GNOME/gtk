/* Path/Curve Editor
 *
 * This demo shows an elaborate curve editor that you would expect to find
 * in a vector graphics editor. It is built on top of GTK's path APIs.
 */

#include <gtk/gtk.h>
#include "curve-editor.h"


static GskPath *
make_circle_path (void)
{
  float w = 310;
  float h = 310;
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
  gsk_path_builder_close (builder);

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
line_width_changed (GtkSpinButton *spin,
                    CurveEditor   *editor)
{
  GskStroke *stroke;

  stroke = gsk_stroke_copy (curve_editor_get_stroke (editor));
  gsk_stroke_set_line_width (stroke, gtk_spin_button_get_value (spin));
  curve_editor_set_stroke (editor, stroke);
  gsk_stroke_free (stroke);
}

static void
cap_changed (GtkDropDown *combo,
             GParamSpec  *pspec,
             CurveEditor *editor)
{
  GskStroke *stroke;

  stroke = gsk_stroke_copy (curve_editor_get_stroke (editor));
  gsk_stroke_set_line_cap (stroke, (GskLineCap)gtk_drop_down_get_selected (combo));
  curve_editor_set_stroke (editor, stroke);
  gsk_stroke_free (stroke);
}

static void
join_changed (GtkDropDown *combo,
              GParamSpec  *pspec,
              CurveEditor *editor)
{
  GskStroke *stroke;

  stroke = gsk_stroke_copy (curve_editor_get_stroke (editor));
  gsk_stroke_set_line_join (stroke, (GskLineJoin)gtk_drop_down_get_selected (combo));
  curve_editor_set_stroke (editor, stroke);
  gsk_stroke_free (stroke);
}

static void
color_changed (GtkColorChooser *chooser,
               GParamSpec      *pspec,
               CurveEditor     *editor)
{
  GdkRGBA color;

  gtk_color_chooser_get_rgba (chooser, &color);
  curve_editor_set_color (editor, &color);
}

static void
stroke_toggled (GtkCheckButton *button,
                CurveEditor    *editor)
{
  curve_editor_set_show_outline (editor, gtk_check_button_get_active (button));
  gtk_widget_queue_draw (GTK_WIDGET (editor));
}

static void
limit_changed (GtkSpinButton *spin,
               CurveEditor   *editor)
{
  GskStroke *stroke;

  stroke = gsk_stroke_copy (curve_editor_get_stroke (editor));
  gsk_stroke_set_miter_limit (stroke, gtk_spin_button_get_value (spin));
  curve_editor_set_stroke (editor, stroke);
  gsk_stroke_free (stroke);
}

static void
dashes_changed (GtkEntry    *entry,
                GParamSpec  *spec,
                CurveEditor *editor)
{
  const char *text;
  char **split;
  GArray *dash;
  GskStroke *stroke;

  text = gtk_editable_get_text (GTK_EDITABLE (entry));
  split = g_strsplit (text, " ", 0);

  dash = g_array_new (FALSE, FALSE, sizeof (float));
  for (int i = 0; split[i] != NULL; i++)
    {
      double d;
      char *endp = 0;

      d = g_strtod (split[i], &endp);
      if (*endp == '\0')
        g_array_append_vals (dash, (float[1]) { d }, 1);
    }

  g_strfreev (split);

  stroke = gsk_stroke_copy (curve_editor_get_stroke (editor));
  gsk_stroke_set_dash (stroke, (const float *)dash->data, dash->len);
  curve_editor_set_stroke (editor, stroke);
  gsk_stroke_free (stroke);

  g_array_free (dash, TRUE);
}

GtkWidget *
do_curve (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *demo;
  GtkWidget *edit_toggle;
  GtkWidget *reset_button;
  GtkWidget *titlebar;
  GtkWidget *stroke_toggle;
  GtkWidget *line_width_spin;
  GtkWidget *stroke_button;
  GtkWidget *popover;
  GtkWidget *grid;
  GtkWidget *cap_combo;
  GtkWidget *join_combo;
  GtkWidget *color_button;
  GtkWidget *limit_spin;
  GtkWidget *dash_entry;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_title (GTK_WINDOW (window), "Curve Editor");
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);
      gtk_window_set_default_size (GTK_WINDOW (window), 310, 350);

      edit_toggle = gtk_toggle_button_new ();
      gtk_button_set_icon_name (GTK_BUTTON (edit_toggle), "document-edit-symbolic");

      reset_button = gtk_button_new_from_icon_name ("edit-undo-symbolic");

      stroke_button = gtk_menu_button_new ();
      gtk_menu_button_set_icon_name (GTK_MENU_BUTTON (stroke_button), "open-menu-symbolic");
      popover = gtk_popover_new ();
      gtk_menu_button_set_popover (GTK_MENU_BUTTON (stroke_button), popover);

      grid = gtk_grid_new ();
      gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
      gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
      gtk_popover_set_child (GTK_POPOVER (popover), grid);

      gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Color:"), 0, 0, 1, 1);
      color_button = gtk_color_button_new_with_rgba (&(GdkRGBA){ 0., 0., 0., 1.});
      gtk_grid_attach (GTK_GRID (grid), color_button, 1, 0, 1, 1);

      gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Line width:"), 0, 1, 1, 1);
      line_width_spin = gtk_spin_button_new_with_range (1, 20, 1);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (line_width_spin), 1);
      gtk_grid_attach (GTK_GRID (grid), line_width_spin, 1, 1, 1, 1);

      gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Line cap:"), 0, 2, 1, 1);
      cap_combo = gtk_drop_down_new_from_strings ((const char *[]){"Butt", "Round", "Square", NULL});
      gtk_grid_attach (GTK_GRID (grid), cap_combo, 1, 2, 1, 1);

      gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Line join:"), 0, 3, 1, 1);
      join_combo = gtk_drop_down_new_from_strings ((const char *[]){"Miter", "Miter-clip", "Round", "Bevel", "Arcs", NULL});
      gtk_grid_attach (GTK_GRID (grid), join_combo, 1, 3, 1, 1);

      gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Miter limit:"), 0, 4, 1, 1);
      limit_spin = gtk_spin_button_new_with_range (0, 10, 1);
      gtk_spin_button_set_digits (GTK_SPIN_BUTTON (limit_spin), 1);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (limit_spin), 4);
      gtk_grid_attach (GTK_GRID (grid), limit_spin, 1, 4, 1, 1);

      gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Dashes:"), 0, 5, 1, 1);
      dash_entry = gtk_entry_new ();
      gtk_grid_attach (GTK_GRID (grid), dash_entry, 1, 5, 1, 1);

      stroke_toggle = gtk_check_button_new_with_label ("Show outline");
      gtk_grid_attach (GTK_GRID (grid), stroke_toggle, 1, 6, 1, 1);

      titlebar = gtk_header_bar_new ();
      gtk_header_bar_pack_start (GTK_HEADER_BAR (titlebar), edit_toggle);
      gtk_header_bar_pack_start (GTK_HEADER_BAR (titlebar), reset_button);
      gtk_header_bar_pack_start (GTK_HEADER_BAR (titlebar), stroke_button);

      gtk_window_set_titlebar (GTK_WINDOW (window), titlebar);

      demo = curve_editor_new ();

      g_signal_connect (stroke_toggle, "toggled", G_CALLBACK (stroke_toggled), demo);
      g_signal_connect (edit_toggle, "notify::active", G_CALLBACK (edit_changed), demo);
      g_signal_connect (reset_button, "clicked", G_CALLBACK (reset), demo);
      g_signal_connect (cap_combo, "notify::selected", G_CALLBACK (cap_changed), demo);
      g_signal_connect (join_combo, "notify::selected", G_CALLBACK (join_changed), demo);
      g_signal_connect (color_button, "notify::rgba", G_CALLBACK (color_changed), demo);
      g_signal_connect (line_width_spin, "value-changed", G_CALLBACK (line_width_changed), demo);
      g_signal_connect (limit_spin, "value-changed", G_CALLBACK (limit_changed), demo);
      g_signal_connect (dash_entry, "notify::text", G_CALLBACK (dashes_changed), demo);

      reset (NULL, CURVE_EDITOR (demo));

      gtk_spin_button_set_value (GTK_SPIN_BUTTON (line_width_spin), 6);
      gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (color_button), &(GdkRGBA) { 1, 0, 0, 1 });
      gtk_drop_down_set_selected (GTK_DROP_DOWN (cap_combo), GSK_LINE_CAP_ROUND);
      gtk_editable_set_text (GTK_EDITABLE (dash_entry), "0 8");

      gtk_window_set_child (GTK_WINDOW (window), demo);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
