/* Video Player
 *
 * This is a simple video player using just GTK widgets.
 */

#include <gtk/gtk.h>

static GtkWidget *window = NULL;

static void
open_dialog_response_cb (GtkWidget *dialog,
                         int        response,
                         GtkWidget *video)
{
  gtk_widget_hide (dialog);

  if (response == GTK_RESPONSE_ACCEPT)
    {
      GFile *file;

      file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));
      gtk_video_set_file (GTK_VIDEO (video), file);
      g_object_unref (file);
    }

  gtk_widget_destroy (dialog);
}

static void
open_clicked_cb (GtkWidget *button,
                 GtkWidget *video)
{
  GtkWidget *dialog;

  dialog = gtk_file_chooser_dialog_new ("Select a video",
                                        GTK_WINDOW (gtk_widget_get_root (button)),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        "_Cancel", GTK_RESPONSE_CANCEL,
                                        "_Open", GTK_RESPONSE_ACCEPT,
                                        NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  g_signal_connect (dialog, "response", G_CALLBACK (open_dialog_response_cb), video);
  gtk_widget_show (dialog);
}

static void
fullscreen_clicked_cb (GtkWidget *button,
                       gpointer   unused)
{
  GtkWidget *window = GTK_WIDGET (gtk_widget_get_root (button));

  gtk_window_fullscreen (GTK_WINDOW (window));
}

GtkWidget *
do_video_player (GtkWidget *do_widget)
{
  GtkWidget *title;
  GtkWidget *video;
  GtkWidget *open_button;
  GtkWidget *fullscreen_button;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Video Player");
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      video = gtk_video_new ();
      gtk_container_add (GTK_CONTAINER (window), video);

      title = gtk_header_bar_new ();
      gtk_header_bar_set_show_title_buttons (GTK_HEADER_BAR (title), TRUE);
      gtk_header_bar_set_title (GTK_HEADER_BAR (title), "Video Player");
      gtk_window_set_titlebar (GTK_WINDOW (window), title);

      open_button = gtk_button_new_with_mnemonic ("_Open");
      g_signal_connect (open_button, "clicked", G_CALLBACK (open_clicked_cb), video);
      gtk_header_bar_pack_start (GTK_HEADER_BAR (title), open_button);

      fullscreen_button = gtk_button_new_from_icon_name ("view-fullscreen-symbolic");
      g_signal_connect (fullscreen_button, "clicked", G_CALLBACK (fullscreen_clicked_cb), NULL);
      gtk_header_bar_pack_end (GTK_HEADER_BAR (title), fullscreen_button);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
