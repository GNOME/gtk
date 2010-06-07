#include <gtk/gtk.h>

static const char *builder_data =
"<interface>"
"<object class=\"GtkAboutDialog\" id=\"about_dialog\">"
"  <property name=\"program-name\">Example Application</property>"
"  <property name=\"website\">http://www.gtk.org</property>"
"</object>"
"<object class=\"GtkActionGroup\" id=\"actions\">"
"  <child>"
"      <object class=\"GtkAction\" id=\"About\">"
"          <property name=\"name\">About</property>"
"          <property name=\"stock_id\">gtk-about</property>"
"      </object>"
"  </child>"
"</object>"
"</interface>";

static GtkWidget *about_dialog;

static void
about_activate (GtkAction *action,
                gpointer   user_data)
{
  gtk_dialog_run (GTK_DIALOG (about_dialog));
  gtk_widget_hide (GTK_WIDGET (about_dialog));
}

int
main (int argc, char **argv)
{
  GtkApplication *app;
  GtkWindow *window;
  GtkBuilder *builder;
  GtkAction *action;
  GtkActionGroup *actions;

  app = gtk_application_new (&argc, &argv, "org.gtk.Example");
  builder = gtk_builder_new ();
  if (!gtk_builder_add_from_string (builder, builder_data, -1, NULL))
    g_error ("failed to parse UI");
  actions = GTK_ACTION_GROUP (gtk_builder_get_object (builder, "actions"));
  gtk_application_set_action_group (app, actions);

  action = gtk_action_group_get_action (actions, "About");
  g_signal_connect (action, "activate", G_CALLBACK (about_activate), app);

  about_dialog = GTK_WIDGET (gtk_builder_get_object (builder, "about_dialog"));

  gtk_builder_connect_signals (builder, app);
  g_object_unref (builder);

  window = gtk_application_get_window (app);
  gtk_container_add (GTK_CONTAINER (window), gtk_label_new ("Hello world"));
  gtk_widget_show_all (GTK_WIDGET (window));

  gtk_application_run (app);

  return 0;
}
