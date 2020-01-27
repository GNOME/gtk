#include <gtk/gtk.h>

static GtkWidget *win;
static GtkEntry *inhibit_entry;
static GtkCheckButton *inhibit_logout;
static GtkCheckButton *inhibit_switch;
static GtkCheckButton *inhibit_suspend;
static GtkCheckButton *inhibit_idle;
static GtkLabel *inhibit_label;

static void
inhibitor_toggled (GtkToggleButton *button, GtkApplication *app)
{
  gboolean active;
  const gchar *reason;
  GtkApplicationInhibitFlags flags;
  GtkWidget *toplevel;
  guint cookie;

  active = gtk_toggle_button_get_active (button);
  reason = gtk_editable_get_text (GTK_EDITABLE (inhibit_entry));

  flags = 0;
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (inhibit_logout)))
    flags |= GTK_APPLICATION_INHIBIT_LOGOUT;
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (inhibit_switch)))
    flags |= GTK_APPLICATION_INHIBIT_SWITCH;
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (inhibit_suspend)))
    flags |= GTK_APPLICATION_INHIBIT_SUSPEND;
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (inhibit_idle)))
    flags |= GTK_APPLICATION_INHIBIT_IDLE;

  toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (button)));

  if (active)
    {
      gchar *text;

      g_print ("Calling gtk_application_inhibit: %d, '%s'\n", flags, reason);

      cookie = gtk_application_inhibit (app, GTK_WINDOW (toplevel), flags, reason);
      g_object_set_data (G_OBJECT (button), "cookie", GUINT_TO_POINTER (cookie));
      if (cookie == 0)
        {
          g_signal_handlers_block_by_func (button, inhibitor_toggled, app);
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), FALSE);
          g_signal_handlers_unblock_by_func (button, inhibitor_toggled, app);
          active = FALSE;
        }
      else
        {
          text = g_strdup_printf ("%#x", cookie);
          gtk_label_set_label (inhibit_label, text);
          g_free (text);
        }
    }
  else
    {
      cookie = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (button), "cookie"));
      g_print ("Calling gtk_application_uninhibit: %#x\n", cookie);
      gtk_application_uninhibit (app, cookie);
      gtk_label_set_label (inhibit_label, "");
    }

  gtk_widget_set_sensitive (GTK_WIDGET (inhibit_entry), !active);
  gtk_widget_set_sensitive (GTK_WIDGET (inhibit_logout), !active);
  gtk_widget_set_sensitive (GTK_WIDGET (inhibit_switch), !active);
  gtk_widget_set_sensitive (GTK_WIDGET (inhibit_suspend), !active);
  gtk_widget_set_sensitive (GTK_WIDGET (inhibit_idle), !active);
}

static void
activate (GtkApplication *app,
          gpointer        data)
{
  GtkWidget *box;
  GtkSeparator *separator;
  GtkWidget *grid;
  GtkToggleButton *button;
  GtkLabel *label;

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  g_object_set (box, "margin", 12, NULL);
  gtk_container_add (GTK_CONTAINER (win), box);

  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 6);

  gtk_container_add (GTK_CONTAINER (box), grid);

  label = gtk_label_new ("Inhibitor");
  gtk_grid_attach (GTK_GRID (grid), GTK_WIDGET (label), 0, 0, 1, 1);

  inhibit_label = gtk_label_new ("");
  gtk_grid_attach (GTK_GRID (grid), GTK_WIDGET (inhibit_label), 1, 0, 1, 1);

  inhibit_logout = gtk_check_button_new_with_label ("Logout");
  gtk_grid_attach (GTK_GRID (grid), GTK_WIDGET (inhibit_logout), 1, 1, 1, 1);

  inhibit_switch = gtk_check_button_new_with_label ("User switching");
  gtk_grid_attach (GTK_GRID (grid), GTK_WIDGET (inhibit_switch), 1, 2, 1, 1);

  inhibit_suspend = gtk_check_button_new_with_label ("Suspend");
  gtk_grid_attach (GTK_GRID (grid), GTK_WIDGET (inhibit_suspend), 1, 4, 1, 1);

  inhibit_idle = gtk_check_button_new_with_label ("Idle");
  gtk_grid_attach (GTK_GRID (grid), GTK_WIDGET (inhibit_idle), 1, 5, 1, 1);

  inhibit_entry = gtk_entry_new ();
  gtk_grid_attach (GTK_GRID (grid), GTK_WIDGET (inhibit_entry), 1, 6, 1, 1);

  button = gtk_toggle_button_new_with_label ("Inhibit");
  g_signal_connect (button, "toggled",
                    G_CALLBACK (inhibitor_toggled), app);
  gtk_grid_attach (GTK_GRID (grid), GTK_WIDGET (button), 2, 6, 1, 1);

  separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (separator));

  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 6);

  gtk_widget_show (win);

  gtk_application_add_window (app, GTK_WINDOW (win));
}

static void
quit (GtkApplication *app,
      gpointer        data)
{
  g_print ("Received quit\n");
  gtk_widget_destroy (win);
}

int
main (int argc, char *argv[])
{
  GtkApplication *app;

  app = gtk_application_new ("org.gtk.Test.session", 0);
  g_object_set (app, "register-session", TRUE, NULL);

  g_signal_connect (app, "activate",
                    G_CALLBACK (activate), NULL);
  g_signal_connect (app, "quit",
                    G_CALLBACK (quit), NULL);

  g_application_run (G_APPLICATION (app), argc, argv);

  return 0;
}
