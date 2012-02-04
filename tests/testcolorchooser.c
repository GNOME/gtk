#include <gtk/gtk.h>

static void
color_changed (GObject *o, GParamSpec *pspect, gpointer data)
{
  GdkRGBA color;

  gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (o), &color);
  g_print ("color changed: %g %g %g %g\n",
           color.red, color.green, color.blue, color.alpha);
}

static void
dialog_response (GtkDialog *dialog, gint response)
{
  GdkRGBA color;

  switch (response)
    {
    case GTK_RESPONSE_OK:
      gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (dialog), &color);
      g_print ("color accepted: %g %g %g %g\n",
           color.red, color.green, color.blue, color.alpha);
      break;
    default:
      g_print ("canceled\n");
      break;
    }

  gtk_main_quit ();
}

int
main (int argc, char *argv[])
{
  GtkWidget *dialog;
  gint i;

  gtk_init (NULL, NULL);

  dialog = gtk_color_chooser_dialog_new ("Select a color", NULL);

  for (i = 1; i < argc; i++)
    {
      if (g_strcmp0 (argv[i], "--no-alpha") == 0)
        {
          g_print ("turning alpha off\n");
          gtk_color_chooser_set_use_alpha (GTK_COLOR_CHOOSER (dialog), FALSE);
        }
      else if (g_strcmp0 (argv[i], "--edit") == 0)
        {
          g_print ("starting in edit mode\n");
          g_object_set (dialog, "show-editor", TRUE, NULL);
        }
    }

  g_signal_connect (dialog, "notify::color", G_CALLBACK (color_changed), NULL);
  g_signal_connect (dialog, "response", G_CALLBACK (dialog_response), NULL);

  gtk_widget_show_all (dialog);

  gtk_main ();

  return 0;
}

