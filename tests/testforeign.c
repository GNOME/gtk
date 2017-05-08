#include <gtk/gtk.h>
#include <gdk/wayland/gdkwayland.h>

static GtkWidget *window;
static GtkWidget *label;
static GtkWidget *entry;
static GtkWidget *unexport_button;
static char *export_handle;
int export_count;

static void
update_ui (void)
{
  gboolean can_unexport;
  char *label_text;

  gtk_entry_set_text (GTK_ENTRY (entry), export_handle ? export_handle : "");

  label_text = g_strdup_printf ("Export count: %d", export_count);
  gtk_label_set_text (GTK_LABEL (label), label_text);
  g_free (label_text);

  can_unexport = !!export_handle;
  gtk_widget_set_sensitive (unexport_button, can_unexport);
}

static void
exported_callback (GdkWindow  *window,
                   const char *handle,
                   gpointer    user_data)
{
  if (!export_handle)
    export_handle = g_strdup (handle);

  g_assert (g_str_equal (handle, export_handle));

  export_count++;

  update_ui ();
}

static void
export_callback (GtkWidget *widget,
                 gpointer   data)
{
  if (!gdk_wayland_window_export_handle (gtk_widget_get_window (window),
                                         exported_callback,
                                         g_strdup ("user_data"), g_free))
    g_error ("Failed to export window");

  update_ui ();
}

static void
unexport_callback (GtkWidget *widget,
                   gpointer   data)
{
  gdk_wayland_window_unexport_handle (gtk_widget_get_window (window));

  export_count--;
  if (export_count == 0)
    g_clear_pointer (&export_handle, g_free);

  update_ui ();
}

int
main (int   argc,
      char *argv[])
{
  GdkDisplay *display;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *export_button;

  gtk_init (&argc, &argv);


  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  display = gtk_widget_get_display (window);
  if (!GDK_IS_WAYLAND_DISPLAY (display))
    g_error ("This test is only works on Wayland");

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);

  label = gtk_label_new (NULL);
  entry = gtk_entry_new ();
  gtk_editable_set_editable (GTK_EDITABLE (entry), FALSE);

  export_button = gtk_button_new_with_label ("Export");
  unexport_button = gtk_button_new_with_label("Unexport");
  g_signal_connect (export_button, "clicked",
                    G_CALLBACK (export_callback), NULL);
  g_signal_connect (unexport_button, "clicked",
                    G_CALLBACK (unexport_callback), NULL);

  gtk_container_add (GTK_CONTAINER (hbox), export_button);
  gtk_container_add (GTK_CONTAINER (hbox), unexport_button);

  gtk_container_add (GTK_CONTAINER (vbox), entry);
  gtk_container_add (GTK_CONTAINER (vbox), label);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);

  gtk_container_add (GTK_CONTAINER (window), vbox);

  update_ui ();

  g_signal_connect (window, "delete-event", G_CALLBACK (gtk_main_quit), NULL);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
