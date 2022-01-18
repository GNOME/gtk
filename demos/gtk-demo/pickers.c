/* Pickers
 * #Keywords: GtkColorChooser, GtkFontChooser, GtkApplicationChooser
 *
 * These widgets are mainly intended for use in preference dialogs.
 * They allow to select colors, fonts and applications.
 *
 * This demo shows both the default appearance for these dialogs,
 * as well as some of the customizations that are possible.
 */

#include <gtk/gtk.h>

static gboolean
filter_font_cb (const Pango2FontFamily *family,
                const Pango2FontFace   *face,
                gpointer               data)
{
  const char *alias_families[] = {
    "Cursive",
    "Fantasy",
    "Monospace",
    "Sans",
    "Serif",
    "System-ui",
    NULL
  };
  const char *family_name;

  family_name = pango2_font_family_get_name ((Pango2FontFamily *)family);

  for (int i = 0; alias_families[i]; i++)
    {
      if (g_ascii_strcasecmp (alias_families[i], family_name) == 0)
        return TRUE;
    }

  return FALSE;
}

#define COLOR(r,g,b) { r/255., g/255., b/255., 1.0 }

GtkWidget *
do_pickers (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *table, *label, *picker;
  GdkRGBA solarized[] = {
    COLOR (0xff, 0xff, 0xff),
    COLOR (0x07, 0x36, 0x42),
    COLOR (0xdc, 0x32, 0x2f),
    COLOR (0x85, 0x99, 0x00),
    COLOR (0xb5, 0x89, 0x00),
    COLOR (0x26, 0x8b, 0xd2),
    COLOR (0xd3, 0x36, 0x82),
    COLOR (0x2a, 0xa1, 0x98),
    COLOR (0xee, 0xe8, 0xd5),

    COLOR (0x00, 0x00, 0x00),
    COLOR (0x00, 0x2b, 0x36),
    COLOR (0xcb, 0x4b, 0x16),
    COLOR (0x58, 0x6e, 0x75),
    COLOR (0x65, 0x7b, 0x83),
    COLOR (0x83, 0x94, 0x96),
    COLOR (0x6c, 0x71, 0xc4),
    COLOR (0x93, 0xa1, 0xa1),
    COLOR (0xfd, 0xf6, 0xe3),
  };

  if (!window)
  {
    window = gtk_window_new ();
    gtk_window_set_display (GTK_WINDOW (window),
                            gtk_widget_get_display (do_widget));
    gtk_window_set_title (GTK_WINDOW (window), "Pickers");
    g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

    table = gtk_grid_new ();
    gtk_widget_set_margin_start (table, 20);
    gtk_widget_set_margin_end (table, 20);
    gtk_widget_set_margin_top (table, 20);
    gtk_widget_set_margin_bottom (table, 20);
    gtk_grid_set_row_spacing (GTK_GRID (table), 3);
    gtk_grid_set_column_spacing (GTK_GRID (table), 10);
    gtk_window_set_child (GTK_WINDOW (window), table);

    label = gtk_label_new ("Standard");
    gtk_widget_add_css_class (label, "title-4");
    gtk_grid_attach (GTK_GRID (table), label, 1, -1, 1, 1);
    label = gtk_label_new ("Custom");
    gtk_widget_add_css_class (label, "title-4");
    gtk_grid_attach (GTK_GRID (table), label, 2, -1, 1, 1);

    label = gtk_label_new ("Color:");
    gtk_widget_set_halign (label, GTK_ALIGN_START);
    gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand (label, TRUE);
    gtk_grid_attach (GTK_GRID (table), label, 0, 0, 1, 1);

    picker = gtk_color_button_new ();
    gtk_grid_attach (GTK_GRID (table), picker, 1, 0, 1, 1);

    picker = gtk_color_button_new ();
    gtk_color_button_set_title (GTK_COLOR_BUTTON (picker), "Solarized colors");
    gtk_color_chooser_add_palette (GTK_COLOR_CHOOSER (picker),
                                   GTK_ORIENTATION_HORIZONTAL,
                                   9,
                                   18,
                                   solarized);
    gtk_grid_attach (GTK_GRID (table), picker, 2, 0, 1, 1);

    label = gtk_label_new ("Font:");
    gtk_widget_set_halign (label, GTK_ALIGN_START);
    gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand (label, TRUE);
    gtk_grid_attach (GTK_GRID (table), label, 0, 1, 1, 1);

    picker = gtk_font_button_new ();
    gtk_grid_attach (GTK_GRID (table), picker, 1, 1, 1, 1);

    picker = gtk_font_button_new ();
    gtk_font_chooser_set_level (GTK_FONT_CHOOSER (picker),
                                GTK_FONT_CHOOSER_LEVEL_FAMILY |
                                GTK_FONT_CHOOSER_LEVEL_SIZE);
    gtk_font_chooser_set_filter_func (GTK_FONT_CHOOSER (picker), filter_font_cb, NULL, NULL);

    gtk_grid_attach (GTK_GRID (table), picker, 2, 1, 1, 1);

    label = gtk_label_new ("Mail:");
    gtk_widget_set_halign (label, GTK_ALIGN_START);
    gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand (label, TRUE);
    picker = gtk_app_chooser_button_new ("x-scheme-handler/mailto");
    gtk_app_chooser_button_set_show_dialog_item (GTK_APP_CHOOSER_BUTTON (picker), TRUE);
    gtk_grid_attach (GTK_GRID (table), label, 0, 3, 1, 1);
    gtk_grid_attach (GTK_GRID (table), picker, 1, 3, 1, 1);
  }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
