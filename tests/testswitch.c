#include <stdlib.h>
#include <gtk/gtk.h>

static gboolean
boolean_to_text (GBinding *binding,
                 const GValue *source,
                 GValue *target,
                 gpointer dummy G_GNUC_UNUSED)
{
  if (g_value_get_boolean (source))
    g_value_set_string (target, "Enabled");
  else
    g_value_set_string (target, "Disabled");

  return TRUE;
}

static GtkWidget *
make_switch (gboolean is_on,
             gboolean is_sensitive)
{
  GtkWidget *hbox;
  GtkWidget *sw, *label;

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

  sw = gtk_switch_new ();
  gtk_switch_set_active (GTK_SWITCH (sw), is_on);
  gtk_box_pack_start (GTK_BOX (hbox), sw, FALSE, FALSE, 0);
  gtk_widget_set_sensitive (sw, is_sensitive);
  gtk_widget_show (sw);

  label = gtk_label_new (is_on ? "Enabled" : "Disabled");
  gtk_box_pack_end (GTK_BOX (hbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  g_object_bind_property_full (sw, "active",
                               label, "label",
                               G_BINDING_DEFAULT,
                               boolean_to_text,
                               NULL,
                               NULL, NULL);

  return hbox;
}

int
main (int   argc,
      char *argv[])
{
  GtkWidget *window;
  GtkWidget *vbox, *hbox;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "GtkSwitch");
  gtk_window_set_default_size (GTK_WINDOW (window), 400, -1);
  gtk_container_set_border_width (GTK_CONTAINER (window), 6);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  gtk_widget_show (window);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_widget_show (vbox);

  hbox = make_switch (FALSE, TRUE);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);
  gtk_widget_show (hbox);

  hbox = make_switch (TRUE, TRUE);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);
  gtk_widget_show (hbox);

  hbox = make_switch (FALSE, FALSE);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);
  gtk_widget_show (hbox);

  hbox = make_switch (TRUE, FALSE);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);
  gtk_widget_show (hbox);

  gtk_main ();

  return EXIT_SUCCESS;
}
