#include <stdlib.h>
#include <gtk/gtk.h>

#include "gtkgears.h"

/************************************************************************
 *                 DEMO CODE                                            *
 ************************************************************************/

static void
toggle_overlay (GtkWidget *checkbutton,
		GtkWidget *revealer)
{
  gtk_revealer_set_reveal_child (GTK_REVEALER (revealer),
				 gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(checkbutton)));
}

static void
toggle_spin (GtkWidget *checkbutton,
             GtkWidget *spinner)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(checkbutton)))
    gtk_spinner_start (GTK_SPINNER (spinner));
  else
    gtk_spinner_stop (GTK_SPINNER (spinner));
}

static void
on_axis_value_change (GtkAdjustment *adjustment,
                      gpointer       data)
{
  GtkGears *gears = GTK_GEARS (data);
  int axis = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (adjustment), "axis"));

  gtk_gears_set_axis (gears, axis, gtk_adjustment_get_value (adjustment));
}


static GtkWidget *
create_axis_slider (GtkGears *gears,
                    int axis)
{
  GtkWidget *box, *label, *slider;
  GtkAdjustment *adj;
  const char *text;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, FALSE);

  switch (axis)
    {
    case GTK_GEARS_X_AXIS:
      text = "X";
      break;

    case GTK_GEARS_Y_AXIS:
      text = "Y";
      break;

    case GTK_GEARS_Z_AXIS:
      text = "Z";
      break;

    default:
      g_assert_not_reached ();
    }

  label = gtk_label_new (text);
  gtk_box_append (GTK_BOX (box), label);

  adj = gtk_adjustment_new (gtk_gears_get_axis (gears, axis), 0.0, 360.0, 1.0, 12.0, 0.0);
  g_object_set_data (G_OBJECT (adj), "axis", GINT_TO_POINTER (axis));
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (on_axis_value_change),
                    gears);
  slider = gtk_scale_new (GTK_ORIENTATION_VERTICAL, adj);
  gtk_scale_set_draw_value (GTK_SCALE (slider), FALSE);
  gtk_box_append (GTK_BOX (box), slider);
  gtk_widget_set_vexpand (slider, TRUE);

  return box;
}

static void
moar_gears (GtkButton *button, gpointer data)
{
  GtkWidget *gears;

  gears = gtk_gears_new ();
  gtk_widget_set_size_request (gears, 100, 100);
  gtk_box_append (GTK_BOX (data), gears);
}

static void
less_gears (GtkButton *button, gpointer data)
{
  GtkWidget *gears;

  gears = gtk_widget_get_last_child (GTK_WIDGET (data));
  if (gears)
    gtk_box_remove (GTK_BOX (data), gears);
}

static void
quit_cb (GtkWidget *widget,
         gpointer   data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *box, *hbox, *button, *spinner, *check,
    *fps_label, *gears, *extra_hbox, *bbox, *overlay,
    *revealer, *frame, *label, *scrolled, *popover;
  int i;
  gboolean done = FALSE;

  gtk_init ();

  window = gtk_window_new ();
  gtk_window_set_titlebar (GTK_WINDOW (window), gtk_header_bar_new ());
  gtk_window_set_title (GTK_WINDOW (window), "GdkGears");
  gtk_window_set_default_size (GTK_WINDOW (window), 640, 640);
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);

  overlay = gtk_overlay_new ();
  gtk_widget_set_margin_start (overlay, 12);
  gtk_widget_set_margin_end (overlay, 12);
  gtk_widget_set_margin_top (overlay, 12);
  gtk_widget_set_margin_bottom (overlay, 12);

  gtk_window_set_child (GTK_WINDOW (window), overlay);

  revealer = gtk_revealer_new ();
  gtk_widget_set_halign (revealer, GTK_ALIGN_END);
  gtk_widget_set_valign (revealer, GTK_ALIGN_START);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), revealer);

  frame = gtk_frame_new (NULL);
  gtk_widget_add_css_class (frame, "app-notification");
  gtk_revealer_set_child (GTK_REVEALER (revealer), frame);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE);
  gtk_box_set_spacing (GTK_BOX (hbox), 6);
  gtk_frame_set_child (GTK_FRAME (frame), hbox);

  label = gtk_label_new ("This is a transparent overlay widget!!!!\nAmazing, eh?");
  gtk_box_append (GTK_BOX (hbox), label);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, FALSE);
  gtk_box_set_spacing (GTK_BOX (box), 6);
  gtk_overlay_set_child (GTK_OVERLAY (overlay), box);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE);
  gtk_box_set_spacing (GTK_BOX (box), 6);
  gtk_box_append (GTK_BOX (box), hbox);

  gears = gtk_gears_new ();
  gtk_widget_set_hexpand (gears, TRUE);
  gtk_widget_set_vexpand (gears, TRUE);
  gtk_box_append (GTK_BOX (hbox), gears);

  for (i = 0; i < GTK_GEARS_N_AXIS; i++)
    gtk_box_append (GTK_BOX (hbox), create_axis_slider (GTK_GEARS (gears), i));

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE);
  gtk_box_set_spacing (GTK_BOX (hbox), 6);
  gtk_box_append (GTK_BOX (box), hbox);

  fps_label = gtk_label_new ("");
  gtk_widget_set_hexpand (fps_label, TRUE);
  gtk_widget_set_halign (fps_label, GTK_ALIGN_START);
  gtk_box_append (GTK_BOX (hbox), fps_label);
  gtk_gears_set_fps_label (GTK_GEARS (gears), GTK_LABEL (fps_label));


  button = gtk_menu_button_new ();
  gtk_menu_button_set_direction (GTK_MENU_BUTTON (button), GTK_ARROW_UP);
  popover = gtk_popover_new ();
  label = gtk_label_new ("Popovers work too!");
  gtk_popover_set_child (GTK_POPOVER (popover), label);

  gtk_menu_button_set_popover (GTK_MENU_BUTTON (button), popover);
  gtk_box_append (GTK_BOX (hbox), button);

  check = gtk_check_button_new_with_label ("Overlay");
  gtk_box_append (GTK_BOX (hbox), check);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), FALSE);
  g_signal_connect (check, "toggled",
                    G_CALLBACK (toggle_overlay), revealer);



  check = gtk_check_button_new_with_label ("Animate spinner");
  gtk_box_append (GTK_BOX (hbox), check);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), TRUE);


  spinner = gtk_spinner_new ();
  gtk_box_append (GTK_BOX (hbox), spinner);
  gtk_spinner_start (GTK_SPINNER (spinner));
  g_signal_connect (check, "toggled",
                    G_CALLBACK (toggle_spin), spinner);


  scrolled = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_NEVER);
  gtk_box_append (GTK_BOX (box), scrolled);

  extra_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE);
  gtk_box_set_spacing (GTK_BOX (extra_hbox), 6);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled), extra_hbox);

  bbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_set_spacing (GTK_BOX (bbox), 6);
  gtk_box_append (GTK_BOX (box), bbox);

  button = gtk_button_new_with_label ("Moar gears!");
  gtk_box_append (GTK_BOX (bbox), button);
  g_signal_connect (button, "clicked", G_CALLBACK (moar_gears), extra_hbox);

  button = gtk_button_new_with_label ("Less gears!");
  gtk_box_append (GTK_BOX (bbox), button);
  g_signal_connect (button, "clicked", G_CALLBACK (less_gears), extra_hbox);

  button = gtk_button_new_with_label ("Quit");
  gtk_widget_set_hexpand (button, TRUE);
  gtk_box_append (GTK_BOX (bbox), button);
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (gtk_window_destroy), window);
  gtk_widget_show (window);

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return EXIT_SUCCESS;
}
