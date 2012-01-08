#include <gtk/gtk.h>

static gboolean will_quit = TRUE;
static gchar *reason;
static GtkWidget *inhibit_entry;
static GtkWidget *inhibit_logout;
static GtkWidget *inhibit_switch;
static GtkWidget *inhibit_suspend;
static GtkWidget *inhibit_idle;
static GtkWidget *inhibit_label;
static GtkWidget *end_combo;
static GtkWidget *end_confirm;

static void
end_session (GtkButton *button, GtkApplication *app)
{
  const gchar *id;
  GtkApplicationEndStyle style;
  gboolean confirm;

  id = gtk_combo_box_get_active_id (GTK_COMBO_BOX (end_combo));
  if (g_strcmp0 (id, "logout") == 0)
    style = GTK_APPLICATION_LOGOUT;
  else if (g_strcmp0 (id, "reboot") == 0)
    style = GTK_APPLICATION_REBOOT;
  else if (g_strcmp0 (id, "shutdown") == 0)
    style = GTK_APPLICATION_SHUTDOWN;
  else
    style = GTK_APPLICATION_LOGOUT;

  confirm = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (end_confirm));

  g_print ("Calling gtk_application_end_session: %d, %d\n", style, confirm);

  gtk_application_end_session (app, style, confirm);
}

static void
toggle_will_quit (GtkToggleButton *button, gpointer data)
{
  will_quit = gtk_toggle_button_get_active (button);
}

static void
reason_changed (GtkEntry *entry, GParamSpec *pspec, GtkApplication *app)
{
  g_free (reason);
  reason = g_strdup (gtk_entry_get_text (entry));
}

static void
inhibitor_toggled (GtkToggleButton *button, GtkApplication *app)
{
  gboolean active;
  const gchar *reason;
  GtkApplicationInhibitFlags flags;
  GtkWidget *toplevel;
  guint cookie;

  active = gtk_toggle_button_get_active (button);
  reason = gtk_entry_get_text (GTK_ENTRY (inhibit_entry));

  flags = 0;
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (inhibit_logout)))
    flags |= GTK_APPLICATION_INHIBIT_LOGOUT;
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (inhibit_switch)))
    flags |= GTK_APPLICATION_INHIBIT_SWITCH;
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (inhibit_suspend)))
    flags |= GTK_APPLICATION_INHIBIT_SUSPEND;
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (inhibit_idle)))
    flags |= GTK_APPLICATION_INHIBIT_IDLE;

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (button));

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
          gtk_label_set_label (GTK_LABEL (inhibit_label), text);
          g_free (text);
        }
    }
  else
    {
      cookie = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (button), "cookie"));
      g_print ("Calling gtk_application_uninhibit: %#x\n", cookie);
      gtk_application_uninhibit (app, cookie);
      gtk_label_set_label (GTK_LABEL (inhibit_label), "");
    }

  gtk_widget_set_sensitive (inhibit_entry, !active);
  gtk_widget_set_sensitive (inhibit_logout, !active);
  gtk_widget_set_sensitive (inhibit_switch, !active);
  gtk_widget_set_sensitive (inhibit_suspend, !active);
  gtk_widget_set_sensitive (inhibit_idle, !active);
}

static void
activate (GtkApplication *app,
          gpointer        data)
{
  GtkWidget *win;
  GtkWidget *box;
  GtkWidget *separator;
  GtkWidget *grid;
  GtkWidget *button;
  GtkWidget *entry;
  GtkWidget *label;

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  g_object_set (box, "margin", 12, NULL);
  gtk_container_add (GTK_CONTAINER (win), box);

  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 6);

  gtk_container_add (GTK_CONTAINER (box), grid);

  label = gtk_label_new ("Inhibitor");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

  inhibit_label = gtk_label_new ("");
  gtk_grid_attach (GTK_GRID (grid), inhibit_label, 1, 0, 1, 1);

  inhibit_logout = gtk_check_button_new_with_label ("Logout");
  gtk_grid_attach (GTK_GRID (grid), inhibit_logout, 1, 1, 1, 1);

  inhibit_switch = gtk_check_button_new_with_label ("User switching");
  gtk_grid_attach (GTK_GRID (grid), inhibit_switch, 1, 2, 1, 1);

  inhibit_suspend = gtk_check_button_new_with_label ("Suspend");
  gtk_grid_attach (GTK_GRID (grid), inhibit_suspend, 1, 4, 1, 1);

  inhibit_idle = gtk_check_button_new_with_label ("Idle");
  gtk_grid_attach (GTK_GRID (grid), inhibit_idle, 1, 5, 1, 1);

  inhibit_entry = gtk_entry_new ();
  gtk_grid_attach (GTK_GRID (grid), inhibit_entry, 1, 6, 1, 1);

  button = gtk_toggle_button_new_with_label ("Inhibit");
  g_signal_connect (button, "toggled",
                    G_CALLBACK (inhibitor_toggled), app);
  gtk_grid_attach (GTK_GRID (grid), button, 2, 6, 1, 1);

  separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_container_add (GTK_CONTAINER (box), separator);

  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 6);

  gtk_container_add (GTK_CONTAINER (box), grid);

  button = gtk_check_button_new_with_label ("Will quit");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), will_quit);
  g_signal_connect (button, "toggled",
                    G_CALLBACK (toggle_will_quit), NULL);
  gtk_grid_attach (GTK_GRID (grid), button, 0, 0, 1, 1);

  entry = gtk_entry_new ();
  g_signal_connect (entry, "notify::text",
                    G_CALLBACK (reason_changed), app);
  gtk_grid_attach (GTK_GRID (grid), entry, 1, 0, 1, 1);

  separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_container_add (GTK_CONTAINER (box), separator);

  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 6);

  gtk_container_add (GTK_CONTAINER (box), grid);

  end_combo = gtk_combo_box_text_new ();
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (end_combo), "logout", "Logout");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (end_combo), "reboot", "Reboot");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (end_combo), "shutdown", "Shutdown");
  gtk_combo_box_set_active_id (GTK_COMBO_BOX (end_combo), "logout");
  gtk_grid_attach (GTK_GRID (grid), end_combo, 0, 0, 1, 1);

  end_confirm = gtk_check_button_new_with_label ("Confirm");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (end_confirm), TRUE);
  gtk_grid_attach (GTK_GRID (grid), end_confirm, 1, 0, 1, 1);

  button = gtk_button_new_with_label ("Go");
  g_signal_connect (button, "clicked",
                    G_CALLBACK (end_session), app);

  gtk_grid_attach (GTK_GRID (grid), button, 2, 0, 1, 1);

  gtk_widget_show_all (win);

  gtk_application_add_window (app, GTK_WINDOW (win));
}

static void
quit_requested (GtkApplication *app,
                gpointer        data)
{
  g_print ("Received quit-requested, reply: %d, '%s'\n", will_quit, reason);
  gtk_application_quit_response (app, will_quit, reason);
}

static void
quit (GtkApplication *app,
      gpointer        data)
{
  g_print ("Received quit\n");
}

static void
quit_cancelled (GtkApplication *app,
                gpointer        data)
{
  g_print ("received quit-cancelled\n");
}

int
main (int argc, char *argv[])
{
  GtkApplication *app;

  app = gtk_application_new ("org.gtk.Test.session", 0);
  g_object_set (app, "register-session", TRUE, NULL);

  g_signal_connect (app, "activate",
                    G_CALLBACK (activate), NULL);
  g_signal_connect (app, "quit-requested",
                    G_CALLBACK (quit_requested), NULL);
  g_signal_connect (app, "quit",
                    G_CALLBACK (quit), NULL);
  g_signal_connect (app, "quit-cancelled",
                    G_CALLBACK (quit_cancelled), NULL);

  g_application_run (G_APPLICATION (app), argc, argv);

  return 0;
}
