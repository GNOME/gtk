/* Overlay/Decorative Overlay
 *
 * Another example of an overlay with some decorative
 * and some interactive controls.
 */

#include <gtk/gtk.h>

static GtkTextTag *tag;

static void
margin_changed (GtkAdjustment *adjustment,
                GtkTextView   *text)
{
  gint value;

  value = (gint)gtk_adjustment_get_value (adjustment);
  gtk_text_view_set_left_margin (GTK_TEXT_VIEW (text), value);
  g_object_set (tag, "pixels-above-lines", value, NULL);
}

GtkWidget *
do_overlay2 (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *overlay;
      GtkWidget *sw;
      GtkWidget *text;
      GtkWidget *image;
      GtkWidget *scale;
      GtkTextBuffer *buffer;
      GtkTextIter start, end;
      GtkAdjustment *adjustment;

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_default_size (GTK_WINDOW (window), 500, 510);
      gtk_window_set_title (GTK_WINDOW (window), "Decorative Overlay");

      overlay = gtk_overlay_new ();
      sw = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_AUTOMATIC,
                                      GTK_POLICY_AUTOMATIC);
      text = gtk_text_view_new ();
      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text));

      gtk_text_buffer_set_text (buffer, "Dear diary...", -1);

      tag = gtk_text_buffer_create_tag (buffer, "top-margin",
                                        "pixels-above-lines", 0,
                                        NULL);
      gtk_text_buffer_get_start_iter (buffer, &start);
      end = start;
      gtk_text_iter_forward_word_end (&end);
      gtk_text_buffer_apply_tag (buffer, tag, &start, &end);

      gtk_container_add (GTK_CONTAINER (window), overlay);
      gtk_container_add (GTK_CONTAINER (overlay), sw);
      gtk_container_add (GTK_CONTAINER (sw), text);

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      image = gtk_image_new_from_resource ("/overlay2/decor1.png");
      gtk_overlay_add_overlay (GTK_OVERLAY (overlay), image);
      gtk_overlay_set_overlay_pass_through (GTK_OVERLAY (overlay), image, TRUE);
      gtk_widget_set_halign (image, GTK_ALIGN_START);
      gtk_widget_set_valign (image, GTK_ALIGN_START);

      image = gtk_image_new_from_resource ("/overlay2/decor2.png");
      gtk_overlay_add_overlay (GTK_OVERLAY (overlay), image);
      gtk_overlay_set_overlay_pass_through (GTK_OVERLAY (overlay), image, TRUE);
      gtk_widget_set_halign (image, GTK_ALIGN_END);
      gtk_widget_set_valign (image, GTK_ALIGN_END);

      adjustment = gtk_adjustment_new (0, 0, 100, 1, 1, 0);
      g_signal_connect (adjustment, "value-changed",
                        G_CALLBACK (margin_changed), text);

      scale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, adjustment);
      gtk_scale_set_draw_value (GTK_SCALE (scale), FALSE);
      gtk_widget_set_size_request (scale, 120, -1);
      gtk_widget_set_margin_start (scale, 20);
      gtk_widget_set_margin_end (scale, 20);
      gtk_widget_set_margin_bottom (scale, 20);
      gtk_overlay_add_overlay (GTK_OVERLAY (overlay), scale);
      gtk_widget_set_halign (scale, GTK_ALIGN_START);
      gtk_widget_set_valign (scale, GTK_ALIGN_END);
      gtk_widget_set_tooltip_text (scale, "Margin");

      gtk_adjustment_set_value (adjustment, 100);

      gtk_widget_show_all (overlay);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
