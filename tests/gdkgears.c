#include <stdlib.h>
#include <gtk/gtk.h>

#include "gtkgears.h"

/************************************************************************
 *                 DEMO CODE                                            *
 ************************************************************************/

static void
toggle_alpha (GtkWidget *checkbutton,
              GtkWidget *gears)
{
  gtk_gl_area_set_has_alpha (GTK_GL_AREA (gears),
                             gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(checkbutton)));
}

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
  gtk_container_add (GTK_CONTAINER (box), label);
  gtk_widget_show (label);

  adj = gtk_adjustment_new (gtk_gears_get_axis (gears, axis), 0.0, 360.0, 1.0, 12.0, 0.0);
  g_object_set_data (G_OBJECT (adj), "axis", GINT_TO_POINTER (axis));
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (on_axis_value_change),
                    gears);
  slider = gtk_scale_new (GTK_ORIENTATION_VERTICAL, adj);
  gtk_scale_set_draw_value (GTK_SCALE (slider), FALSE);
  gtk_container_add (GTK_CONTAINER (box), slider);
  gtk_widget_set_vexpand (slider, TRUE);
  gtk_widget_show (slider);

  gtk_widget_show (box);

  return box;
}

static void
moar_gears (GtkButton *button, gpointer data)
{
  GtkContainer *container = GTK_CONTAINER (data);
  GtkWidget *gears;

  gears = gtk_gears_new ();
  gtk_widget_set_size_request (gears, 100, 100);
  gtk_container_add (container, gears);
  gtk_widget_show (gears);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *box, *hbox, *button, *spinner, *check,
    *fps_label, *gears, *extra_hbox, *bbox, *overlay,
    *revealer, *frame, *label, *scrolled;
  int i;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "GdkGears");
  gtk_window_set_default_size (GTK_WINDOW (window), 640, 640);
  gtk_container_set_border_width (GTK_CONTAINER (window), 12);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  overlay = gtk_overlay_new ();
  gtk_container_add (GTK_CONTAINER (window), overlay);
  gtk_widget_show (overlay);

  revealer = gtk_revealer_new ();
  gtk_widget_set_halign (revealer, GTK_ALIGN_END);
  gtk_widget_set_valign (revealer, GTK_ALIGN_START);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay),
			   revealer);
  gtk_widget_show (revealer);

  frame = gtk_frame_new (NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (frame),
			       "app-notification");
  gtk_container_add (GTK_CONTAINER (revealer), frame);
  gtk_widget_show (frame);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE);
  gtk_box_set_spacing (GTK_BOX (hbox), 6);
  gtk_container_add (GTK_CONTAINER (frame), hbox);
  gtk_widget_show (hbox);

  label = gtk_label_new ("This is a transparent overlay widget!!!!\nAmazing, eh?");
  gtk_container_add (GTK_CONTAINER (hbox), label);
  gtk_widget_show (label);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, FALSE);
  gtk_box_set_spacing (GTK_BOX (box), 6);
  gtk_container_add (GTK_CONTAINER (overlay), box);
  gtk_widget_show (box);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE);
  gtk_box_set_spacing (GTK_BOX (box), 6);
  gtk_container_add (GTK_CONTAINER (box), hbox);
  gtk_widget_show (hbox);

  gears = gtk_gears_new ();
  gtk_widget_set_hexpand (gears, TRUE);
  gtk_widget_set_vexpand (gears, TRUE);
  gtk_container_add (GTK_CONTAINER (hbox), gears);
  gtk_widget_show (gears);

  for (i = 0; i < GTK_GEARS_N_AXIS; i++)
    gtk_container_add (GTK_CONTAINER (hbox), create_axis_slider (GTK_GEARS (gears), i));

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE);
  gtk_box_set_spacing (GTK_BOX (hbox), 6);
  gtk_container_add (GTK_CONTAINER (box), hbox);
  gtk_widget_show (hbox);

  fps_label = gtk_label_new ("");
  gtk_container_add (GTK_CONTAINER (hbox), fps_label);
  gtk_widget_show (fps_label);
  gtk_gears_set_fps_label (GTK_GEARS (gears), GTK_LABEL (fps_label));

  spinner = gtk_spinner_new ();
  gtk_box_pack_end (GTK_BOX (hbox), spinner, FALSE, FALSE, 0);
  gtk_widget_show (spinner);
  gtk_spinner_start (GTK_SPINNER (spinner));

  check = gtk_check_button_new_with_label ("Animate spinner");
  gtk_box_pack_end (GTK_BOX (hbox), check, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), TRUE);
  gtk_widget_show (check);
  g_signal_connect (check, "toggled",
                    G_CALLBACK (toggle_spin), spinner);

  check = gtk_check_button_new_with_label ("Alpha");
  gtk_box_pack_end (GTK_BOX (hbox), check, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), FALSE);
  gtk_widget_show (check);
  g_signal_connect (check, "toggled",
                    G_CALLBACK (toggle_alpha), gears);

  check = gtk_check_button_new_with_label ("Overlay");
  gtk_box_pack_end (GTK_BOX (hbox), check, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), FALSE);
  gtk_widget_show (check);
  g_signal_connect (check, "toggled",
                    G_CALLBACK (toggle_overlay), revealer);

  scrolled = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_NEVER);
  gtk_container_add (GTK_CONTAINER (box), scrolled);
  gtk_widget_show (scrolled);

  extra_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE);
  gtk_box_set_spacing (GTK_BOX (extra_hbox), 6);
  gtk_container_add (GTK_CONTAINER (scrolled), extra_hbox);
  gtk_widget_show (extra_hbox);

  bbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_box_set_spacing (GTK_BOX (bbox), 6);
  gtk_container_add (GTK_CONTAINER (box), bbox);
  gtk_widget_show (bbox);

  button = gtk_button_new_with_label ("Moar gears!");
  gtk_widget_set_hexpand (button, TRUE);
  gtk_container_add (GTK_CONTAINER (bbox), button);
  g_signal_connect (button, "clicked", G_CALLBACK (moar_gears), extra_hbox);
  gtk_widget_show (button);

  button = gtk_button_new_with_label ("Quit");
  gtk_widget_set_hexpand (button, TRUE);
  gtk_container_add (GTK_CONTAINER (bbox), button);
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (gtk_widget_destroy), window);
  gtk_widget_show (button);

  gtk_widget_show (window);

  gtk_main ();

  return EXIT_SUCCESS;
}
