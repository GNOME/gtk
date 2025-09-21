/* Images
 * #Keywords: GdkPaintable, GtkWidgetPaintable
 *
 * GtkImage and GtkPicture are used to display an image; the image can be
 * in a number of formats.
 *
 * GtkImage is the widget used to display icons or images that should be
 * sized and styled like an icon, while GtkPicture is used for images
 * that should be displayed as-is.
 *
 * This demo code shows some of the more obscure cases, in the simple
 * case a call to gtk_picture_new_for_file() or
 * gtk_image_new_from_icon_name() is all you need.
 */

#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <errno.h>
#include "pixbufpaintable.h"

static GtkWidget *window = NULL;

static void
toggle_sensitivity_callback (GtkWidget *togglebutton,
                             gpointer   user_data)
{
  GtkWidget *child;

  for (child = gtk_widget_get_first_child (GTK_WIDGET (user_data));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      /* don't disable our toggle */
      if (child != togglebutton)
        gtk_widget_set_sensitive (child, !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (togglebutton)));
    }
}


GtkWidget *
do_images (GtkWidget *do_widget)
{
  GtkWidget *video;
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *base_vbox;
  GtkWidget *image;
  GtkWidget *picture;
  GtkWidget *label;
  GtkWidget *button;
  GdkPaintable *paintable;
  GtkWidget *state;
  GIcon *gicon;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Images");
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer*)&window);

      base_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
      gtk_widget_set_margin_start (base_vbox, 16);
      gtk_widget_set_margin_end (base_vbox, 16);
      gtk_widget_set_margin_top (base_vbox, 16);
      gtk_widget_set_margin_bottom (base_vbox, 16);
      gtk_window_set_child (GTK_WINDOW (window), base_vbox);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 16);
      gtk_box_append (GTK_BOX (base_vbox), hbox);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
      gtk_box_append (GTK_BOX (hbox), vbox);

      label = gtk_label_new ("Image from a resource");
      gtk_widget_add_css_class (label, "heading");
      gtk_box_append (GTK_BOX (vbox), label);

      frame = gtk_frame_new (NULL);
      gtk_widget_set_halign (frame, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (frame, GTK_ALIGN_CENTER);
      gtk_box_append (GTK_BOX (vbox), frame);

      image = gtk_image_new_from_resource ("/images/org.gtk.Demo4.svg");
      gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_LARGE);

      gtk_frame_set_child (GTK_FRAME (frame), image);


      /* Animation */

      label = gtk_label_new ("Animation from a resource");
      gtk_widget_add_css_class (label, "heading");
      gtk_box_append (GTK_BOX (vbox), label);

      frame = gtk_frame_new (NULL);
      gtk_widget_set_halign (frame, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (frame, GTK_ALIGN_CENTER);
      gtk_box_append (GTK_BOX (vbox), frame);

      paintable = pixbuf_paintable_new_from_resource ("/images/floppybuddy.gif");
      picture = gtk_picture_new_for_paintable (paintable);
      g_object_unref (paintable);

      gtk_frame_set_child (GTK_FRAME (frame), picture);

      /* Symbolic icon */

      label = gtk_label_new ("Symbolic themed icon");
      gtk_widget_add_css_class (label, "heading");
      gtk_box_append (GTK_BOX (vbox), label);

      frame = gtk_frame_new (NULL);
      gtk_widget_set_halign (frame, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (frame, GTK_ALIGN_CENTER);
      gtk_box_append (GTK_BOX (vbox), frame);

      gicon = g_themed_icon_new_with_default_fallbacks ("battery-level-10-charging-symbolic");
      image = gtk_image_new_from_gicon (gicon);
      gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_LARGE);
      g_object_unref (gicon);

      gtk_frame_set_child (GTK_FRAME (frame), image);


      /* Stateful */

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
      gtk_box_append (GTK_BOX (hbox), vbox);

      label = gtk_label_new ("Stateful icon");
      gtk_widget_add_css_class (label, "heading");
      gtk_box_append (GTK_BOX (vbox), label);

      frame = gtk_frame_new (NULL);
      gtk_widget_set_halign (frame, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (frame, GTK_ALIGN_CENTER);
      gtk_box_append (GTK_BOX (vbox), frame);

      paintable = GDK_PAINTABLE (gtk_path_paintable_new_from_resource ("/images/stateful.gpa"));
      gtk_path_paintable_set_state (GTK_PATH_PAINTABLE (paintable), 0);
      image = gtk_image_new_from_paintable (paintable);
      gtk_image_set_pixel_size (GTK_IMAGE (image), 128);

      gtk_frame_set_child (GTK_FRAME (frame), image);

      state = gtk_switch_new ();
      gtk_widget_set_halign (state, GTK_ALIGN_START);
      g_object_bind_property (state, "active",
                              paintable, "state",
                              G_BINDING_DEFAULT);
      gtk_box_append (GTK_BOX (vbox), state);
      g_object_unref (paintable);


      /* Animations */

      label = gtk_label_new ("Path animation");
      gtk_widget_add_css_class (label, "heading");
      gtk_box_append (GTK_BOX (vbox), label);

      frame = gtk_frame_new (NULL);
      gtk_widget_set_halign (frame, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (frame, GTK_ALIGN_CENTER);
      gtk_box_append (GTK_BOX (vbox), frame);

      paintable = GDK_PAINTABLE (gtk_path_paintable_new_from_resource ("/images/animated.gpa"));
      gtk_path_paintable_set_state (GTK_PATH_PAINTABLE (paintable), 0);
      image = gtk_image_new_from_paintable (paintable);
      gtk_image_set_pixel_size (GTK_IMAGE (image), 128);

      gtk_frame_set_child (GTK_FRAME (frame), image);
      g_object_unref (paintable);

      /* Video */

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
      gtk_box_append (GTK_BOX (hbox), vbox);

      label = gtk_label_new ("Displaying video");
      gtk_widget_add_css_class (label, "heading");
      gtk_box_append (GTK_BOX (vbox), label);

      frame = gtk_frame_new (NULL);
      gtk_widget_set_halign (frame, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (frame, GTK_ALIGN_CENTER);
      gtk_box_append (GTK_BOX (vbox), frame);

      video = gtk_video_new_for_resource ("/images/gtk-logo.webm");
      gtk_media_stream_set_loop (gtk_video_get_media_stream (GTK_VIDEO (video)), TRUE);
      gtk_frame_set_child (GTK_FRAME (frame), video);

      /* Widget paintables */
      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
      gtk_box_append (GTK_BOX (hbox), vbox);

      label = gtk_label_new ("GtkWidgetPaintable");
      gtk_widget_add_css_class (label, "heading");
      gtk_box_append (GTK_BOX (vbox), label);

      paintable = gtk_widget_paintable_new (do_widget);
      picture = gtk_picture_new_for_paintable (paintable);
      gtk_widget_set_size_request (picture, 100, 100);
      gtk_widget_set_valign (picture, GTK_ALIGN_START);
      gtk_box_append (GTK_BOX (vbox), picture);

      /* Sensitivity control */
      button = gtk_toggle_button_new_with_mnemonic ("_Insensitive");
      gtk_widget_set_halign (button, GTK_ALIGN_END);
      gtk_widget_set_valign (button, GTK_ALIGN_END);
      gtk_widget_set_vexpand (button, TRUE);
      gtk_box_append (GTK_BOX (base_vbox), button);

      g_signal_connect (button, "toggled",
                        G_CALLBACK (toggle_sensitivity_callback),
                        base_vbox);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
