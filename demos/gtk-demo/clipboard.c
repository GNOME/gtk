/* Clipboard
 *
 * GdkClipboard is used for clipboard handling. This demo shows how to
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

static GtkWidget *window = NULL;

void
copy_button_clicked (GtkWidget *button,
                     gpointer   user_data)
{
  GtkWidget *entry;
  GdkClipboard *clipboard;

  entry = GTK_WIDGET (user_data);

  /* Get the clipboard object */
  clipboard = gtk_widget_get_clipboard (entry);

  /* Set clipboard text */
  gdk_clipboard_set_text (clipboard, gtk_entry_get_text (GTK_ENTRY (entry)));
}

void
paste_received (GObject      *source_object,
                GAsyncResult *result,
                gpointer      user_data)
{
  GdkClipboard *clipboard;
  GtkWidget *entry;
  char *text;
  GError *error = NULL;

  clipboard = GDK_CLIPBOARD (source_object);
  entry = GTK_WIDGET (user_data);

  /* Get the resulting text of the read operation */
  text = gdk_clipboard_read_text_finish (clipboard, result, &error);

  if (text)
    {
      /* Set the entry text */
      gtk_entry_set_text (GTK_ENTRY (entry), text);
      g_free (text);
    }
  else
    {
      GtkWidget *dialog;

      /* Show an error about why pasting failed.
       * Usually you probably want to ignore such failures,
       * but for demonstration purposes, we show the error.
       */
      dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                       GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       "Could not paste text: %s",
                                       error->message);
      g_signal_connect (dialog, "response",
                        G_CALLBACK (gtk_widget_destroy), NULL);
      gtk_widget_show (dialog);

      g_error_free (error);
    }
}

void
paste_button_clicked (GtkWidget *button,
                      gpointer   user_data)
{
  GtkWidget *entry;
  GdkClipboard *clipboard;

  entry = GTK_WIDGET (user_data);

  /* Get the clipboard object */
  clipboard = gtk_widget_get_clipboard (entry);

  /* Request the contents of the clipboard, contents_received will be
     called when we do get the contents.
   */
  gdk_clipboard_read_text_async (clipboard, NULL, paste_received, entry);
}

static GdkTexture *
get_image_texture (GtkImage *image)
{
  const gchar *icon_name;
  GtkIconTheme *icon_theme;
  GtkIconInfo *icon_info;

  switch (gtk_image_get_storage_type (image))
    {
    case GTK_IMAGE_TEXTURE:
      return g_object_ref (gtk_image_get_texture (image));
    case GTK_IMAGE_ICON_NAME:
      icon_name = gtk_image_get_icon_name (image);
      icon_theme = gtk_icon_theme_get_for_display (gtk_widget_get_display (GTK_WIDGET (image)));
      icon_info = gtk_icon_theme_lookup_icon (icon_theme, icon_name, 48, GTK_ICON_LOOKUP_GENERIC_FALLBACK);
      if (icon_info == NULL)
        return NULL;
      return gtk_icon_info_load_texture (icon_info);
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
  GdkTexture *texture;

  texture = get_image_texture (GTK_IMAGE (widget));
  if (texture)
    {
      gtk_drag_set_icon_paintable (context, GDK_PAINTABLE (texture), -2, -2);
      g_object_unref (texture);
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
  GdkTexture *texture;

  texture = get_image_texture (GTK_IMAGE (widget));
  if (texture)
    gtk_selection_data_set_texture (selection_data, texture);
}

static void
drag_data_received (GtkWidget        *widget,
                    GdkDragContext   *context,
                    GtkSelectionData *selection_data,
                    guint32           time,
                    gpointer          data)
{
  if (gtk_selection_data_get_length (selection_data) > 0)
    {
      GdkTexture *texture;

      texture = gtk_selection_data_get_texture (selection_data);
      gtk_image_set_from_texture (GTK_IMAGE (data), texture);
      g_object_unref (texture);
    }
}

static void
copy_image (GtkMenuItem *item,
            gpointer     data)
{
  GdkClipboard *clipboard;
  GdkTexture *texture;

  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (data));
  texture = get_image_texture (GTK_IMAGE (data));

  if (texture)
    {
      gdk_clipboard_set_texture (clipboard, texture);
      g_object_unref (texture);
    }
}

static void
paste_image_received (GObject      *source,
                      GAsyncResult *result,
                      gpointer      data)
{
  GdkTexture *texture;

  texture = gdk_clipboard_read_texture_finish (GDK_CLIPBOARD (source), result, NULL);
  if (texture == NULL)
    return;
    
  gtk_image_set_from_texture (GTK_IMAGE (data), texture);
  g_object_unref (texture);
}

static void
paste_image (GtkMenuItem *item,
             gpointer     data)
{
  GdkClipboard *clipboard;

  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (data));
  gdk_clipboard_read_texture_async (clipboard,
                                    NULL,
                                    paste_image_received,
                                    data);
}

static void
pressed_cb (GtkGesture *gesture,
            int         n_press,
            double      x,
            double      y,
            GtkWidget  *image)
{
  GtkWidget *menu;
  GtkWidget *item;

  menu = gtk_menu_new ();

  item = gtk_menu_item_new_with_mnemonic (_("_Copy"));
  g_signal_connect (item, "activate", G_CALLBACK (copy_image), image);
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  item = gtk_menu_item_new_with_mnemonic (_("_Paste"));
  g_signal_connect (item, "activate", G_CALLBACK (paste_image), image);
  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  gtk_menu_popup_at_pointer (GTK_MENU (menu), NULL);
}

GtkWidget *
do_clipboard (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkWidget *vbox, *hbox;
      GtkWidget *label;
      GtkWidget *entry, *button;
      GtkWidget *image;
      GtkGesture *gesture;

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
      gesture = gtk_gesture_multi_press_new (image);
      gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_SECONDARY);
      g_object_set_data_full (G_OBJECT (image), "gesture", gesture, g_object_unref);
      g_signal_connect (gesture, "pressed", G_CALLBACK (pressed_cb), image);

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
      gesture = gtk_gesture_multi_press_new (image);
      gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_SECONDARY);
      g_object_set_data_full (G_OBJECT (image), "gesture", gesture, g_object_unref);
      g_signal_connect (gesture, "pressed", G_CALLBACK (pressed_cb), image);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
