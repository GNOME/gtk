#include <gtk/gtk.h>
#include <stdio.h>

static void
clear_pressed (GtkEntry *entry, gint icon, gpointer data)
{
   if (icon == GTK_ENTRY_ICON_SECONDARY)
     gtk_editable_set_text (GTK_EDITABLE (entry), "");
}

static void
drag_begin_cb (GtkWidget      *widget,
               GdkDrag        *drag,
               gpointer        user_data)
{
  gint pos;

  pos = gtk_entry_get_current_icon_drag_source (GTK_ENTRY (widget));
  if (pos != -1)
    gtk_drag_set_icon_name (drag, "dialog-information", 2, 2);
}

static void
drag_data_get_cb (GtkWidget        *widget,
                  GdkDrag          *drag,
                  GtkSelectionData *data,
                  gpointer          user_data)
{
  gint pos;

  pos = gtk_entry_get_current_icon_drag_source (GTK_ENTRY (widget));

  if (pos == GTK_ENTRY_ICON_PRIMARY)
    {
      gint start, end;

      if (gtk_editable_get_selection_bounds (GTK_EDITABLE (widget), &start, &end))
        {
          gchar *str;

          str = gtk_editable_get_chars (GTK_EDITABLE (widget), start, end);
          gtk_selection_data_set_text (data, str, -1);
          g_free (str);
        }
      else
        gtk_selection_data_set_text (data, "XXX", -1);
    }
}

static void
set_blank (GtkWidget *button,
           GtkEntry  *entry)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    gtk_entry_set_icon_from_icon_name (entry, GTK_ENTRY_ICON_SECONDARY, NULL);
}

static void
set_icon_name (GtkWidget *button,
               GtkEntry  *entry)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    gtk_entry_set_icon_from_icon_name (entry, GTK_ENTRY_ICON_SECONDARY, "media-floppy");
}

static void
set_gicon (GtkWidget *button,
           GtkEntry  *entry)
{
  GIcon *icon;

 if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    {
      icon = g_themed_icon_new ("gtk-yes");
      gtk_entry_set_icon_from_gicon (entry, GTK_ENTRY_ICON_SECONDARY, icon);
      g_object_unref (icon);
    }
}

static void
set_texture (GtkWidget *button,
             GtkEntry  *entry)
{
  GdkTexture *texture;

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    {
      texture = gdk_texture_new_from_resource ("/org/gtk/libgtk/inspector/logo.png");
      gtk_entry_set_icon_from_paintable (entry, GTK_ENTRY_ICON_SECONDARY, GDK_PAINTABLE (texture));
      g_object_unref (texture);
    }
}

static const char cssdata[] =
".entry-frame:not(:focus) { "
"  border: 2px solid alpha(gray,0.3);"
"}"
".entry-frame:focus { "
"  border: 2px solid red;"
"}"
".entry-frame entry { "
"  border: none; "
"  box-shadow: none; "
"}";

static void
icon_pressed_cb (GtkGesture *gesture,
                 int         n_press,
                 double      x,
                 double      y,
                 gpointer    data)
{
  g_print ("You clicked me!\n");
}

int
main (int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *grid;
  GtkWidget *label;
  GtkWidget *entry;
  GtkWidget *box;
  GtkWidget *image;
  GtkWidget *button1;
  GtkWidget *button2;
  GtkWidget *button3;
  GtkWidget *button4;
  GIcon *icon;
  GdkContentFormats *tlist;

  gtk_init ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Gtk Entry Icons Test");

  g_signal_connect (G_OBJECT (window), "destroy",
		    G_CALLBACK (gtk_main_quit), NULL);

  grid = gtk_grid_new ();
  gtk_container_add (GTK_CONTAINER (window), grid);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
  g_object_set (grid, "margin", 10, NULL);

  /*
   * Open File - Sets the icon using a GIcon
   */
  label = gtk_label_new ("Open File:");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_CENTER);

  entry = gtk_entry_new ();
  gtk_widget_set_hexpand (entry, TRUE);
  gtk_grid_attach (GTK_GRID (grid), entry, 1, 0, 1, 1);

  icon = g_themed_icon_new ("folder-symbolic");
  g_themed_icon_append_name (G_THEMED_ICON (icon), "folder-symbolic");

  gtk_entry_set_icon_from_gicon (GTK_ENTRY (entry),
				 GTK_ENTRY_ICON_PRIMARY,
				 icon);
  gtk_entry_set_icon_sensitive (GTK_ENTRY (entry),
			        GTK_ENTRY_ICON_PRIMARY,
				FALSE);

  gtk_entry_set_icon_tooltip_text (GTK_ENTRY (entry),
				   GTK_ENTRY_ICON_PRIMARY,
				   "Open a file");

  /*
   * Save File - sets the icon using an icon name.
   */
  label = gtk_label_new ("Save File:");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_CENTER);

  entry = gtk_entry_new ();
  gtk_widget_set_hexpand (entry, TRUE);
  gtk_grid_attach (GTK_GRID (grid), entry, 1, 1, 1, 1);
  gtk_editable_set_text (GTK_EDITABLE (entry), "‏Right-to-left");
  gtk_widget_set_direction (entry, GTK_TEXT_DIR_RTL);
  
  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry),
                                     GTK_ENTRY_ICON_PRIMARY,
                                     "document-save-symbolic");
  gtk_entry_set_icon_tooltip_text (GTK_ENTRY (entry),
				   GTK_ENTRY_ICON_PRIMARY,
				   "Save a file");
  tlist = gdk_content_formats_new (NULL, 0);
  tlist = gtk_content_formats_add_text_targets (tlist);
  gtk_entry_set_icon_drag_source (GTK_ENTRY (entry),
                                  GTK_ENTRY_ICON_PRIMARY,
                                  tlist, GDK_ACTION_COPY); 
  g_signal_connect_after (entry, "drag-begin", 
                          G_CALLBACK (drag_begin_cb), NULL);
  g_signal_connect (entry, "drag-data-get", 
                    G_CALLBACK (drag_data_get_cb), NULL);
  gdk_content_formats_unref (tlist);

  /*
   * Search - Uses a helper function
   */
  label = gtk_label_new ("Search:");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 2, 1, 1);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_CENTER);

  entry = gtk_entry_new ();
  gtk_widget_set_hexpand (entry, TRUE);
  gtk_grid_attach (GTK_GRID (grid), entry, 1, 2, 1, 1);

  gtk_entry_set_placeholder_text (GTK_ENTRY (entry),
                                  "Type some text, then click an icon");

  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry),
                                     GTK_ENTRY_ICON_PRIMARY,
                                     "edit-find-symbolic");

  gtk_entry_set_icon_tooltip_text (GTK_ENTRY (entry),
                                   GTK_ENTRY_ICON_PRIMARY,
                                   "Clicking the other icon is more interesting!");

  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry),
                                     GTK_ENTRY_ICON_SECONDARY,
                                     "edit-clear-symbolic");

  gtk_entry_set_icon_tooltip_text (GTK_ENTRY (entry),
                                   GTK_ENTRY_ICON_SECONDARY,
                                   "Clear");

  g_signal_connect (entry, "icon-press", G_CALLBACK (clear_pressed), NULL);

  /*
   * Password - Sets the icon using an icon name
   */
  label = gtk_label_new ("Password:");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 3, 1, 1);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_CENTER);

  entry = gtk_password_entry_new ();
  gtk_password_entry_set_show_peek_icon (GTK_PASSWORD_ENTRY (entry), TRUE);
  gtk_widget_set_hexpand (entry, TRUE);
  gtk_grid_attach (GTK_GRID (grid), entry, 1, 3, 1, 1);

  /* Name - Does not set any icons. */
  label = gtk_label_new ("Name:");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 4, 1, 1);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_CENTER);

  entry = gtk_entry_new ();
  gtk_widget_set_hexpand (entry, TRUE);
  gtk_entry_set_placeholder_text (GTK_ENTRY (entry),
                                  "Use the RadioButtons to choose an icon");
  gtk_entry_set_icon_tooltip_text (GTK_ENTRY (entry),
                                   GTK_ENTRY_ICON_SECONDARY,
                                   "Use the RadioButtons to change this icon");
  gtk_grid_attach (GTK_GRID (grid), entry, 1, 4, 1, 1);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_set_vexpand (GTK_WIDGET (box), TRUE);
  gtk_grid_attach (GTK_GRID (grid), box, 0, 5, 3, 1);

  button1 = gtk_radio_button_new_with_label (NULL, "Blank");
  gtk_widget_set_valign (button1, GTK_ALIGN_START);
  g_signal_connect (button1, "toggled", G_CALLBACK (set_blank), entry);
  gtk_container_add (GTK_CONTAINER (box), button1);
  button2 = gtk_radio_button_new_with_label (NULL, "Icon Name");
  gtk_widget_set_valign (button2, GTK_ALIGN_START);
  gtk_radio_button_join_group (GTK_RADIO_BUTTON (button2), GTK_RADIO_BUTTON (button1));
  g_signal_connect (button2, "toggled", G_CALLBACK (set_icon_name), entry);
  gtk_container_add (GTK_CONTAINER (box), button2);
  button3 = gtk_radio_button_new_with_label (NULL, "GIcon");
  gtk_widget_set_valign (button3, GTK_ALIGN_START);
  gtk_radio_button_join_group (GTK_RADIO_BUTTON (button3), GTK_RADIO_BUTTON (button1));
  g_signal_connect (button3, "toggled", G_CALLBACK (set_gicon), entry);
  gtk_container_add (GTK_CONTAINER (box), button3);
  button4 = gtk_radio_button_new_with_label (NULL, "Texture");
  gtk_widget_set_valign (button4, GTK_ALIGN_START);
  gtk_radio_button_join_group (GTK_RADIO_BUTTON (button4), GTK_RADIO_BUTTON (button1));
  g_signal_connect (button4, "toggled", G_CALLBACK (set_texture), entry);
  gtk_container_add (GTK_CONTAINER (box), button4);

  label = gtk_label_new ("Emoji:");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 6, 1, 1);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_CENTER);

  entry = gtk_entry_new ();
  g_object_set (entry, "show-emoji-icon", TRUE, NULL);
  gtk_widget_set_hexpand (entry, TRUE);
  gtk_grid_attach (GTK_GRID (grid), entry, 1, 6, 1, 1);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_style_context_add_class (gtk_widget_get_style_context (box), "view");
  gtk_style_context_add_class (gtk_widget_get_style_context (box), "entry-frame");
  gtk_widget_set_cursor_from_name (box, "text");
  entry = gtk_entry_new ();
  gtk_widget_set_hexpand (entry, TRUE);
  gtk_container_add (GTK_CONTAINER (box), entry);
  image = gtk_image_new_from_icon_name ("edit-find-symbolic");
  gtk_widget_set_cursor_from_name (image, "default");
  g_object_set (image, "margin", 6, NULL);
  gtk_widget_set_tooltip_text (image, "Click me");

  GtkGesture *gesture;
  gesture = gtk_gesture_click_new ();
  g_signal_connect (gesture, "pressed", G_CALLBACK (icon_pressed_cb), NULL);
  gtk_widget_add_controller (image, GTK_EVENT_CONTROLLER (gesture));
  gtk_container_add (GTK_CONTAINER (box), image);
  image = gtk_image_new_from_icon_name ("document-save-symbolic");
  g_object_set (image, "margin", 6, NULL);
  gtk_container_add (GTK_CONTAINER (box), image);
  gtk_grid_attach (GTK_GRID (grid), box, 1, 7, 1, 1);

  GtkCssProvider *provider;
  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, cssdata, -1);
  gtk_style_context_add_provider_for_display (gdk_display_get_default (), GTK_STYLE_PROVIDER (provider), 800);
  gtk_widget_show (window);
  gtk_main();

  return 0;
}
