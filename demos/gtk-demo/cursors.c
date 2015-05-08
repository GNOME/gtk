/* Cursors
 *
 * Demonstrates a useful set of available cursors.
 */
#include <gtk/gtk.h>

static void
set_cursor (GtkWidget *button, gpointer data)
{
  GtkWidget *toplevel;
  GdkCursor *cursor = data;
  GdkWindow *window;

  toplevel = gtk_widget_get_toplevel (button);
  window = gtk_widget_get_window (toplevel);
  gdk_window_set_cursor (window, cursor);
}

static GtkWidget *
add_section (GtkWidget   *box,
             const gchar *heading)
{
  GtkWidget *label;
  GtkWidget *section;

  label = gtk_label_new (heading);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_widget_set_margin_top (label, 10);
  gtk_widget_set_margin_bottom (label, 10);
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, TRUE, 0);
  section = gtk_flow_box_new ();
  gtk_widget_set_halign (section, GTK_ALIGN_START);
  gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (section), GTK_SELECTION_NONE);
  gtk_flow_box_set_min_children_per_line (GTK_FLOW_BOX (section), 2);
  gtk_flow_box_set_max_children_per_line (GTK_FLOW_BOX (section), 20);
  gtk_box_pack_start (GTK_BOX (box), section, FALSE, TRUE, 0);

  return section;
}

static void
add_button (GtkWidget   *section,
            const gchar *css_name)
{
  GtkWidget *image, *button;
  GdkDisplay *display;
  GdkCursor *cursor;

  display = gtk_widget_get_display (section);
  cursor = gdk_cursor_new_from_name (display, css_name);
  if (cursor == NULL)
    image = gtk_image_new_from_icon_name ("image-missing", GTK_ICON_SIZE_MENU);
  else
    image = gtk_image_new_from_pixbuf (gdk_cursor_get_image (cursor));
  gtk_widget_set_size_request (image, 24, 24);
  button = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (button), image);
  gtk_style_context_add_class (gtk_widget_get_style_context (button), "image-button");
  g_signal_connect (button, "clicked", G_CALLBACK (set_cursor), cursor);

  gtk_widget_set_tooltip_text (button, css_name);
  gtk_container_add (GTK_CONTAINER (section), button);
}

GtkWidget *
do_cursors (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *sw;
      GtkWidget *box;
      GtkWidget *section;

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
                             gtk_widget_get_screen (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Cursors");
      gtk_window_set_default_size (GTK_WINDOW (window), 500, 500);

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed),
                        &window);

      sw = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_NEVER,
                                      GTK_POLICY_AUTOMATIC);
      gtk_container_add (GTK_CONTAINER (window), sw);
      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      g_object_set (box,
                    "margin-start", 20,
                    "margin-end", 20,
                    "margin-bottom", 10,
                    NULL);
      gtk_container_add (GTK_CONTAINER (sw), box);

      section = add_section (box, "General");
      add_button (section, "default");
      add_button (section, "none");

      section = add_section (box, "Link & Status");
      add_button (section, "context-menu");
      add_button (section, "help");
      add_button (section, "pointer");
      add_button (section, "progress");
      add_button (section, "wait");

      section = add_section (box, "Selection");
      add_button (section, "cell");
      add_button (section, "crosshair");
      add_button (section, "text");
      add_button (section, "vertical-text");

      section = add_section (box, "Drag & Drop");
      add_button (section, "alias");
      add_button (section, "copy");
      add_button (section, "move");
      add_button (section, "no-drop");
      add_button (section, "not-allowed");
      add_button (section, "grab");
      add_button (section, "grabbing");

      section = add_section (box, "Resize & Scrolling");
      add_button (section, "all-scroll");
      add_button (section, "col-resize");
      add_button (section, "row-resize");
      add_button (section, "n-resize");
      add_button (section, "e-resize");
      add_button (section, "s-resize");
      add_button (section, "w-resize");
      add_button (section, "ne-resize");
      add_button (section, "nw-resize");
      add_button (section, "se-resize");
      add_button (section, "sw-resize");
      add_button (section, "ew-resize");
      add_button (section, "ns-resize");
      add_button (section, "nesw-resize");
      add_button (section, "nwse-resize");

      section = add_section (box, "Zoom");
      add_button (section, "zoom-in");
      add_button (section, "zoom-out");
    }

  if (!gtk_widget_get_visible (window))
    {
      gtk_widget_show_all (window);
    }
  else
    {
      gtk_widget_destroy (window);
      window = NULL;
    }


  return window;
}
