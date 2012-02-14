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

#include <gtk/gtk.h>
#include <string.h>

static GtkWidget *window = NULL;

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

static GdkPixbuf *
get_image_pixbuf (GtkImage *image)
{
  gchar *stock_id;
  GtkIconSize size;

  switch (gtk_image_get_storage_type (image))
    {
    case GTK_IMAGE_PIXBUF:
      return g_object_ref (gtk_image_get_pixbuf (image));
    case GTK_IMAGE_STOCK:
      gtk_image_get_stock (image, &stock_id, &size);
      return gtk_widget_render_icon_pixbuf (GTK_WIDGET (image),
                                            stock_id, size);
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
  GdkPixbuf *pixbuf;

  pixbuf = get_image_pixbuf (GTK_IMAGE (data));
  gtk_drag_set_icon_pixbuf (context, pixbuf, -2, -2);
  g_object_unref (pixbuf);
}

void
drag_data_get  (GtkWidget        *widget,
                GdkDragContext   *context,
                GtkSelectionData *selection_data,
                guint             info,
                guint             time,
                gpointer          data)
{
  GdkPixbuf *pixbuf;

  pixbuf = get_image_pixbuf (GTK_IMAGE (data));
  gtk_selection_data_set_pixbuf (selection_data, pixbuf);
  g_object_unref (pixbuf);
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
  GdkPixbuf *pixbuf;

  if (gtk_selection_data_get_length (selection_data) > 0)
    {
      pixbuf = gtk_selection_data_get_pixbuf (selection_data);
      gtk_image_set_from_pixbuf (GTK_IMAGE (data), pixbuf);
      g_object_unref (pixbuf);
    }
}

static void
copy_image (GtkMenuItem *item,
            gpointer     data)
{
  GtkClipboard *clipboard;
  GdkPixbuf *pixbuf;

  clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
  pixbuf = get_image_pixbuf (GTK_IMAGE (data));

  gtk_clipboard_set_image (clipboard, pixbuf);
  g_object_unref (pixbuf);
}

static void
paste_image (GtkMenuItem *item,
             gpointer     data)
{
  GtkClipboard *clipboard;
  GdkPixbuf *pixbuf;

  clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
  pixbuf = gtk_clipboard_wait_for_image (clipboard);

  if (pixbuf)
    {
      gtk_image_set_from_pixbuf (GTK_IMAGE (data), pixbuf);
      g_object_unref (pixbuf);
    }
}

static gboolean
button_press (GtkWidget      *widget,
              GdkEventButton *button,
              gpointer        data)
{
  GtkWidget *menu;
  GtkWidget *item;

  if (button->button != GDK_BUTTON_SECONDARY)
    return FALSE;

  menu = gtk_menu_new ();

  item = gtk_image_menu_item_new_from_stock (GTK_STOCK_COPY, NULL);
  g_signal_connect (item, "activate", G_CALLBACK (copy_image), data);
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  item = gtk_image_menu_item_new_from_stock (GTK_STOCK_PASTE, NULL);
  g_signal_connect (item, "activate", G_CALLBACK (paste_image), data);
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, 3, button->time);
  return TRUE;
}

GtkWidget *
do_clipboard (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkWidget *vbox, *hbox;
      GtkWidget *label;
      GtkWidget *entry, *button;
      GtkWidget *ebox, *image;
      GtkClipboard *clipboard;

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
                             gtk_widget_get_screen (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Clipboard demo");

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);

      gtk_container_add (GTK_CONTAINER (window), vbox);

      label = gtk_label_new ("\"Copy\" will copy the text\nin the entry to the clipboard");

      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 8);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

      /* Create the first entry */
      entry = gtk_entry_new ();
      gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);

      /* Create the button */
      button = gtk_button_new_from_stock (GTK_STOCK_COPY);
      gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
      g_signal_connect (button, "clicked",
                        G_CALLBACK (copy_button_clicked), entry);

      label = gtk_label_new ("\"Paste\" will paste the text from the clipboard to the entry");
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 8);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

      /* Create the second entry */
      entry = gtk_entry_new ();
      gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);

      /* Create the button */
      button = gtk_button_new_from_stock (GTK_STOCK_PASTE);
      gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
      g_signal_connect (button, "clicked",
                        G_CALLBACK (paste_button_clicked), entry);

      label = gtk_label_new ("Images can be transferred via the clipboard, too");
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 8);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

      /* Create the first image */
      image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_WARNING,
                                        GTK_ICON_SIZE_BUTTON);
      ebox = gtk_event_box_new ();
      gtk_container_add (GTK_CONTAINER (ebox), image);
      gtk_container_add (GTK_CONTAINER (hbox), ebox);

      /* make ebox a drag source */
      gtk_drag_source_set (ebox, GDK_BUTTON1_MASK, NULL, 0, GDK_ACTION_COPY);
      gtk_drag_source_add_image_targets (ebox);
      g_signal_connect (ebox, "drag-begin",
                        G_CALLBACK (drag_begin), image);
      g_signal_connect (ebox, "drag-data-get",
                        G_CALLBACK (drag_data_get), image);

      /* accept drops on ebox */
      gtk_drag_dest_set (ebox, GTK_DEST_DEFAULT_ALL,
                         NULL, 0, GDK_ACTION_COPY);
      gtk_drag_dest_add_image_targets (ebox);
      g_signal_connect (ebox, "drag-data-received",
                        G_CALLBACK (drag_data_received), image);

      /* context menu on ebox */
      g_signal_connect (ebox, "button-press-event",
                        G_CALLBACK (button_press), image);

      /* Create the second image */
      image = gtk_image_new_from_stock (GTK_STOCK_STOP,
                                        GTK_ICON_SIZE_BUTTON);
      ebox = gtk_event_box_new ();
      gtk_container_add (GTK_CONTAINER (ebox), image);
      gtk_container_add (GTK_CONTAINER (hbox), ebox);

      /* make ebox a drag source */
      gtk_drag_source_set (ebox, GDK_BUTTON1_MASK, NULL, 0, GDK_ACTION_COPY);
      gtk_drag_source_add_image_targets (ebox);
      g_signal_connect (ebox, "drag-begin",
                        G_CALLBACK (drag_begin), image);
      g_signal_connect (ebox, "drag-data-get",
                        G_CALLBACK (drag_data_get), image);

      /* accept drops on ebox */
      gtk_drag_dest_set (ebox, GTK_DEST_DEFAULT_ALL,
                         NULL, 0, GDK_ACTION_COPY);
      gtk_drag_dest_add_image_targets (ebox);
      g_signal_connect (ebox, "drag-data-received",
                        G_CALLBACK (drag_data_received), image);

      /* context menu on ebox */
      g_signal_connect (ebox, "button-press-event",
                        G_CALLBACK (button_press), image);

      /* tell the clipboard manager to make the data persistent */
      clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
      gtk_clipboard_set_can_store (clipboard, NULL, 0);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    {
      gtk_widget_destroy (window);
      window = NULL;
    }

  return window;
}
