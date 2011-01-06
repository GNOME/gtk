#include <gtk/gtk.h>

static void
new_window (GApplication *app,
            GFile        *file)
{
  GtkWidget *window, *scrolled, *view;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_application (GTK_WINDOW (window), GTK_APPLICATION (app));
  gtk_window_set_title (GTK_WINDOW (window), "Bloatpad");
  scrolled = gtk_scrolled_window_new (NULL, NULL);
  view = gtk_text_view_new ();
  gtk_container_add (GTK_CONTAINER (scrolled), view);
  gtk_container_add (GTK_CONTAINER (window), scrolled);

  if (file != NULL)
    {
      gchar *contents;
      gsize length;

      if (g_file_load_contents (file, NULL, &contents, &length, NULL, NULL))
        {
          GtkTextBuffer *buffer;

          buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
          gtk_text_buffer_set_text (buffer, contents, length);
          g_free (contents);
        }
    }

  gtk_widget_show_all (GTK_WIDGET (window));
}

static void
bloat_pad_activate (GApplication *application)
{
  new_window (application, NULL);
}

static void
bloat_pad_open (GApplication  *application,
                GFile        **files,
                gint           n_files,
                const gchar   *hint)
{
  gint i;

  for (i = 0; i < n_files; i++)
    new_window (application, files[i]);
}

typedef GtkApplication BloatPad;
typedef GtkApplicationClass BloatPadClass;

G_DEFINE_TYPE (BloatPad, bloat_pad, GTK_TYPE_APPLICATION)

static void
bloat_pad_finalize (GObject *object)
{
  G_OBJECT_CLASS (bloat_pad_parent_class)->finalize (object);
}

static void
bloat_pad_init (BloatPad *app)
{
}

static void
bloat_pad_class_init (BloatPadClass *class)
{
  G_OBJECT_CLASS (class)->finalize= bloat_pad_finalize;

  G_APPLICATION_CLASS (class)->activate = bloat_pad_activate;
  G_APPLICATION_CLASS (class)->open = bloat_pad_open;
}

BloatPad *
bloat_pad_new (void)
{
  g_type_init ();

  return g_object_new (bloat_pad_get_type (),
                       "application-id", "org.gtk.Test.bloatpad",
                       "flags", G_APPLICATION_HANDLES_OPEN,
                       NULL);
}

int
main (int argc, char **argv)
{
  BloatPad *bloat_pad;
  int status;

  bloat_pad = bloat_pad_new ();
  status = g_application_run (G_APPLICATION (bloat_pad), argc, argv);
  g_object_unref (bloat_pad);

  return status;
}
