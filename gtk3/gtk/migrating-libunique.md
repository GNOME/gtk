Title: Migrating from libunique to GtkApplication

# Migrating from libunique to GtkApplication

[libunique](https://gitlab.gnome.org/Archive/unique) offers 'unique
application' support as well as ways to communicate with a running
application instance. This is implemented in various ways, either using
D-Bus, or socket-based communication.

Starting with GLib 2.26, D-Bus support has been integrated into GIO in the
form of GDBus, and [`class@Gio.Application`] has been added to provide the
same level of application support as libunique.

Here is a simple application using libunique:

```c
int
main (int argc, char *argv[])
{
  UniqueApp *app;
  GtkWidget *window;

  gtk_init (&argc, &argv);

  app = unique_app_new ("org.gtk.TestApplication", NULL);

  if (unique_app_is_running (app))
    {
      UniqueResponse response;

      response = unique_app_send_message (app, UNIQUE_ACTIVATE, NULL);
      g_object_unref (app);

      return response == UNIQUE_RESPONSE_OK ? 0 : 1;
    }

  window = create_my_window ();

  unique_app_watch_window (app, GTK_WINDOW (window));

  gtk_widget_show (window);

  gtk_main ();

  g_object_unref (app);

  return 0;
}
```

The same application using [`class@Gtk.Application`]:

```c
static void
activate (GtkApplication *app)
{
  GList *list;
  GtkWidget *window;

  list = gtk_application_get_windows (app);

  if (list)
    {
      gtk_window_present (GTK_WINDOW (list->data));
    }
  else
    {
      window = create_my_window ();
      gtk_window_set_application (GTK_WINDOW (window), app);
      gtk_widget_show (window);
    }
}

int
main (int argc, char *argv[])
{
  GtkApplication *app;
  gint status;

  app = gtk_application_new ("org.gtk.TestApplication", 0);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);

  status = g_application_run (G_APPLICATION (app), argc, argv);

  g_object_unref (app);

  return status;
}
```

### Uniqueness

Instead of creating a `UniqueApp` with `unique_app_new()`, create a
`GApplication` with [`ctor@Gio.Application.new`] or a `GtkApplication` with
[`ctor@Gtk.Application.new`]. The name that was used with `unique_app_new()`
is very likely usable as the `application_id` for `g_application_new()` without
any changes, and `GtkApplication` passes the `DESKTOP_STARTUP_ID` environment
variable automatically.

While libunique expects you to check for an already running instance
yourself and activate it manually, `GApplication` handles all this on its own
in `g_application_run()`. If you still need to find out if there is a running
instance of your application, use `g_application_get_is_remote()` instead of
`unique_app_is_running()`.
