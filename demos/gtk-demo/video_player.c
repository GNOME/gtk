/* Video Player
 * #Keywords: GtkVideo, GtkMediaStream, GtkMediaFile, GdkPaintable
 * #Keywords: GtkMediaControls
 *
 * This is a simple video player using just GTK widgets.
 */

#include <gtk/gtk.h>

static GtkWidget *window = NULL;

static void
open_dialog_response_cb (GtkNativeDialog *dialog,
                         int              response,
                         GtkWidget       *video)
{
  gtk_native_dialog_hide (dialog);

  if (response == GTK_RESPONSE_ACCEPT)
    {
      GFile *file;

      file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));
      gtk_video_set_file (GTK_VIDEO (video), file);
      g_object_unref (file);
    }

  gtk_native_dialog_destroy (dialog);
}

static void
open_clicked_cb (GtkWidget *button,
                 GtkWidget *video)
{
  GtkFileChooserNative *dialog;
  GtkFileFilter *filter;

  dialog = gtk_file_chooser_native_new ("Select a video",
                                        GTK_WINDOW (gtk_widget_get_root (button)),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        "_Open",
                                        "_Cancel");

  filter = gtk_file_filter_new ();
  gtk_file_filter_add_pattern (filter, "*");
  gtk_file_filter_set_name (filter, "All Files");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);
  g_object_unref (filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_add_mime_type (filter, "image/*");
  gtk_file_filter_set_name (filter, "Images");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);
  g_object_unref (filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_add_mime_type (filter, "video/*");
  gtk_file_filter_set_name (filter, "Video");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);
  
  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);
  g_object_unref (filter);

  gtk_native_dialog_set_modal (GTK_NATIVE_DIALOG (dialog), TRUE);
  g_signal_connect (dialog, "response", G_CALLBACK (open_dialog_response_cb), video);
  gtk_native_dialog_show (GTK_NATIVE_DIALOG (dialog));
}

static void
logo_clicked_cb (GtkWidget *button,
                 gpointer   video)
{
  GFile *file;

  file = g_file_new_for_uri ("resource:///images/gtk-logo.webm");
  gtk_video_set_file (GTK_VIDEO (video), file);
  g_object_unref (file);
}

static void
bbb_clicked_cb (GtkWidget *button,
                gpointer   video)
{
  GFile *file;

  file = g_file_new_for_uri ("https://download.blender.org/peach/trailer/trailer_400p.ogg");
  gtk_video_set_file (GTK_VIDEO (video), file);
  g_object_unref (file);
}

static void
fullscreen_clicked_cb (GtkWidget *button,
                       gpointer   unused)
{
  GtkWidget *widget_window = GTK_WIDGET (gtk_widget_get_root (button));

  gtk_window_fullscreen (GTK_WINDOW (widget_window));
}

static gboolean
toggle_fullscreen (GtkWidget *widget,
                   GVariant  *args,
                   gpointer   data)
{
  GdkSurface *surface;
  GdkToplevelState state;

  surface = gtk_native_get_surface (GTK_NATIVE (widget));
  state = gdk_toplevel_get_state (GDK_TOPLEVEL (surface));

  if (state & GDK_TOPLEVEL_STATE_FULLSCREEN)
    gtk_window_unfullscreen (GTK_WINDOW (widget));
  else
    gtk_window_fullscreen (GTK_WINDOW (widget));

  return TRUE;
}

GtkWidget *
do_video_player (GtkWidget *do_widget)
{
  GtkWidget *title;
  GtkWidget *video;
  GtkWidget *button;
  GtkWidget *image;
  GtkWidget *fullscreen_button;
  GtkEventController *controller;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Video Player");
      gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      video = gtk_video_new ();
      gtk_video_set_autoplay (GTK_VIDEO (video), TRUE);
      gtk_window_set_child (GTK_WINDOW (window), video);

      title = gtk_header_bar_new ();
      gtk_window_set_titlebar (GTK_WINDOW (window), title);

      button = gtk_button_new_with_mnemonic ("_Open");
      g_signal_connect (button, "clicked", G_CALLBACK (open_clicked_cb), video);
      gtk_header_bar_pack_start (GTK_HEADER_BAR (title), button);

      button = gtk_button_new ();
      image = gtk_image_new_from_resource ("/cursors/images/gtk_logo_cursor.png");
      gtk_image_set_pixel_size (GTK_IMAGE (image), 24);
      gtk_button_set_child (GTK_BUTTON (button), image);
      g_signal_connect (button, "clicked", G_CALLBACK (logo_clicked_cb), video);
      gtk_header_bar_pack_start (GTK_HEADER_BAR (title), button);

      button = gtk_button_new ();
      image = gtk_image_new_from_resource ("/video-player/bbb.png");
      gtk_image_set_pixel_size (GTK_IMAGE (image), 24);
      gtk_button_set_child (GTK_BUTTON (button), image);
      g_signal_connect (button, "clicked", G_CALLBACK (bbb_clicked_cb), video);
      gtk_header_bar_pack_start (GTK_HEADER_BAR (title), button);

      fullscreen_button = gtk_button_new_from_icon_name ("view-fullscreen-symbolic");
      g_signal_connect (fullscreen_button, "clicked", G_CALLBACK (fullscreen_clicked_cb), NULL);
      gtk_header_bar_pack_end (GTK_HEADER_BAR (title), fullscreen_button);

      controller = gtk_shortcut_controller_new ();
      gtk_shortcut_controller_set_scope (GTK_SHORTCUT_CONTROLLER (controller),
                                         GTK_SHORTCUT_SCOPE_GLOBAL);
      gtk_widget_add_controller (window, controller);
      gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller),
           gtk_shortcut_new (gtk_keyval_trigger_new (GDK_KEY_F11, 0),
                             gtk_callback_action_new (toggle_fullscreen, NULL, NULL)));
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
