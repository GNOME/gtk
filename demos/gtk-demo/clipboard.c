/* Clipboard
 *
 * GtkClipboard is used for clipboard handling. This demo shows how to
 * copy and paste text to and from the clipboard.
 *
 * It also shows how to transfer images via the clipboard or via
 * drag-and-drop, and how to make clipboard contents persist after
 * the application exits. Clipboard persistence requires a clipboard
 * manager to run.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

void
copy_button_clicked (GtkWidget *button,
                     gpointer   user_data)
{
  GtkWidget *entry;
  GtkClipboard *clipboard;

  entry = GTK_WIDGET (user_data);

  /* Get the clipboard object */
  clipboard = gtk_widget_get_clipboard (entry,
                                        GDK_SELECTION_CLIPBOARD);

  /* Set clipboard text */
  gtk_clipboard_set_text (clipboard, gtk_entry_get_text (GTK_ENTRY (entry)), -1);
}

void
paste_received (GtkClipboard *clipboard,
                const gchar  *text,
                gpointer      user_data)
{
  GtkWidget *entry;

  entry = GTK_WIDGET (user_data);

  /* Set the entry text */
  if(text)
    gtk_entry_set_text (GTK_ENTRY (entry), text);
}

void
paste_button_clicked (GtkWidget *button,
                     gpointer   user_data)
{
  GtkWidget *entry;
  GtkClipboard *clipboard;

  entry = GTK_WIDGET (user_data);

  /* Get the clipboard object */
  clipboard = gtk_widget_get_clipboard (entry,
                                        GDK_SELECTION_CLIPBOARD);

  /* Request the contents of the clipboard, contents_received will be
     called when we do get the contents.
   */
  gtk_clipboard_request_text (clipboard,
                              paste_received, entry);
}

static cairo_surface_t *
get_image_surface (GtkImage *image)
{
  const gchar *icon_name;
  GtkIconTheme *icon_theme;
  int width;
  GtkIconSize size;

  switch (gtk_image_get_storage_type (image))
    {
    case GTK_IMAGE_SURFACE:
      return cairo_surface_reference (gtk_image_get_surface (image));
    case GTK_IMAGE_ICON_NAME:
      icon_name = gtk_image_get_icon_name (image);
      size = gtk_image_get_icon_size (image);
      icon_theme = gtk_icon_theme_get_for_display (gtk_widget_get_display (GTK_WIDGET (image)));
      gtk_icon_size_lookup (size, &width, NULL);
      return gtk_icon_theme_load_surface (icon_theme, icon_name, width, 1, NULL, GTK_ICON_LOOKUP_GENERIC_FALLBACK, NULL);
    default:
      g_warning ("Image storage type %d not handled",
                 gtk_image_get_storage_type (image));
      return NULL;
    }
}

static void
drag_begin (GtkWidget      *widget,
            GdkDragContext *context,
            gpointer        data)
{
  cairo_surface_t *surface;

  surface = get_image_surface (GTK_IMAGE (widget));
  if (surface)
    {
      cairo_surface_set_device_offset (surface, -2, -2);
      gtk_drag_set_icon_surface (context, surface);
    }
}

void
drag_data_get (GtkWidget        *widget,
               GdkDragContext   *context,
               GtkSelectionData *selection_data,
               guint             info,
               guint             time,
               gpointer          data)
{
  cairo_surface_t *surface;

  surface = get_image_surface (GTK_IMAGE (data));
  if (surface)
    gtk_selection_data_set_surface (selection_data, surface);
}

static void
drag_data_received (GtkWidget        *widget,
                    GdkDragContext   *context,
                    gint              x,
                    gint              y,
                    GtkSelectionData *selection_data,
                    guint             info,
                    guint32           time,
                    gpointer          data)
{
  if (gtk_selection_data_get_length (selection_data) > 0)
    {
      cairo_surface_t *surface;

      surface = gtk_selection_data_get_surface (selection_data);
      gtk_image_set_from_surface (GTK_IMAGE (data), surface);
      cairo_surface_destroy (surface);
    }
}

static void
copy_image (GtkMenuItem *item,
            gpointer     data)
{
  GtkClipboard *clipboard;
  cairo_surface_t *surface;

  clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
  surface = get_image_surface (GTK_IMAGE (data));

  if (surface)
    {
      gtk_clipboard_set_surface (clipboard, surface);
      cairo_surface_destroy (surface);
    }
}

static void
paste_image (GtkMenuItem *item,
             gpointer     data)
{
  GtkClipboard *clipboard;
  cairo_surface_t *surface;

  clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
  surface = gtk_clipboard_wait_for_surface (clipboard);

  if (surface)
    {
      gtk_image_set_from_surface (GTK_IMAGE (data), surface);
      cairo_surface_destroy (surface);
    }
}

static gboolean
button_press (GtkWidget      *widget,
              GdkEventButton *event,
              gpointer        data)
{
  GtkWidget *menu;
  GtkWidget *item;
  guint button;

  gdk_event_get_button ((GdkEvent *)event, &button);

  if (button != GDK_BUTTON_SECONDARY)
    return FALSE;

  menu = gtk_menu_new ();

  item = gtk_menu_item_new_with_mnemonic (_("_Copy"));
  g_signal_connect (item, "activate", G_CALLBACK (copy_image), data);
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  item = gtk_menu_item_new_with_mnemonic (_("_Paste"));
  g_signal_connect (item, "activate", G_CALLBACK (paste_image), data);
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  gtk_menu_popup_at_pointer (GTK_MENU (menu), (GdkEvent *) event);
  return TRUE;
}

GtkWidget *
do_clipboard (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *vbox, *hbox;
      GtkWidget *label;
      GtkWidget *entry, *button;
      GtkWidget *image;
      GtkClipboard *clipboard;

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Clipboard");

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      g_object_set (vbox, "margin", 8, NULL);

      gtk_container_add (GTK_CONTAINER (window), vbox);

      label = gtk_label_new ("\"Copy\" will copy the text\nin the entry to the clipboard");

      gtk_box_pack_start (GTK_BOX (vbox), label);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
      g_object_set (hbox, "margin", 8, NULL);
      gtk_box_pack_start (GTK_BOX (vbox), hbox);

      /* Create the first entry */
      entry = gtk_entry_new ();
      gtk_box_pack_start (GTK_BOX (hbox), entry);

      /* Create the button */
      button = gtk_button_new_with_mnemonic (_("_Copy"));
      gtk_box_pack_start (GTK_BOX (hbox), button);
      g_signal_connect (button, "clicked",
                        G_CALLBACK (copy_button_clicked), entry);

      label = gtk_label_new ("\"Paste\" will paste the text from the clipboard to the entry");
      gtk_box_pack_start (GTK_BOX (vbox), label);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
      g_object_set (hbox, "margin", 8, NULL);
      gtk_box_pack_start (GTK_BOX (vbox), hbox);

      /* Create the second entry */
      entry = gtk_entry_new ();
      gtk_box_pack_start (GTK_BOX (hbox), entry);

      /* Create the button */
      button = gtk_button_new_with_mnemonic (_("_Paste"));
      gtk_box_pack_start (GTK_BOX (hbox), button);
      g_signal_connect (button, "clicked",
                        G_CALLBACK (paste_button_clicked), entry);

      label = gtk_label_new ("Images can be transferred via the clipboard, too");
      gtk_box_pack_start (GTK_BOX (vbox), label);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
      g_object_set (hbox, "margin", 8, NULL);
      gtk_box_pack_start (GTK_BOX (vbox), hbox);

      /* Create the first image */
      image = gtk_image_new_from_icon_name ("dialog-warning");
      gtk_container_add (GTK_CONTAINER (hbox), image);

      /* make image a drag source */
      gtk_drag_source_set (image, GDK_BUTTON1_MASK, NULL, GDK_ACTION_COPY);
      gtk_drag_source_add_image_targets (image);
      g_signal_connect (image, "drag-begin",
                        G_CALLBACK (drag_begin), image);
      g_signal_connect (image, "drag-data-get",
                        G_CALLBACK (drag_data_get), image);

      /* accept drops on image */
      gtk_drag_dest_set (image, GTK_DEST_DEFAULT_ALL,
                         NULL, GDK_ACTION_COPY);
      gtk_drag_dest_add_image_targets (image);
      g_signal_connect (image, "drag-data-received",
                        G_CALLBACK (drag_data_received), image);

      /* context menu on image */
      g_signal_connect (image, "button-press-event",
                        G_CALLBACK (button_press), image);

      /* Create the second image */
      image = gtk_image_new_from_icon_name ("process-stop");
      gtk_container_add (GTK_CONTAINER (hbox), image);

      /* make image a drag source */
      gtk_drag_source_set (image, GDK_BUTTON1_MASK, NULL, GDK_ACTION_COPY);
      gtk_drag_source_add_image_targets (image);
      g_signal_connect (image, "drag-begin",
                        G_CALLBACK (drag_begin), image);
      g_signal_connect (image, "drag-data-get",
                        G_CALLBACK (drag_data_get), image);

      /* accept drops on image */
      gtk_drag_dest_set (image, GTK_DEST_DEFAULT_ALL,
                         NULL, GDK_ACTION_COPY);
      gtk_drag_dest_add_image_targets (image);
      g_signal_connect (image, "drag-data-received",
                        G_CALLBACK (drag_data_received), image);

      /* context menu on image */
      g_signal_connect (image, "button-press-event",
                        G_CALLBACK (button_press), image);

      /* tell the clipboard manager to make the data persistent */
      clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
      gtk_clipboard_set_can_store (clipboard, NULL);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
