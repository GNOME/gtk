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
      else if (g_strcmp0 (argv[i], "--palette") == 0)
        {
          const gchar *c[9] = { "red", "maroon", "yellow", "green", "blue", "magenta", "DarkOliveGreen4", "khaki2", "thistle1" };
          GdkRGBA color;
          GdkRGBA colors[9*9];
          gint i,j;
          gdouble f[5] = { 0.2, 0.35, 0.5, 0.65, 0.8 };

          g_print ("setting custom palette\n");
          for (i = 0; i < 9; i++)
            {
              gdk_rgba_parse (&color, c[i]);
              for (j = 0; j < 5; j++)
                {
                  colors[i*9 + j].red   = f[j]*color.red;
                  colors[i*9 + j].green = f[j]*color.green;
                  colors[i*9 + j].blue  = f[j]*color.blue;
                  colors[i*9 + j].alpha = 1;
                }
              for (j = 5; j < 9; j++)
                {
                  colors[i*9 + j].red   = f[9-j]*color.red + (1-f[9-j]);
                  colors[i*9 + j].green = f[9-j]*color.green + (1-f[9-j]);
                  colors[i*9 + j].blue  = f[9-j]*color.blue + (1-f[9-j]);
                  colors[i*9 + j].alpha = 1;
                }
            }
          gtk_color_chooser_add_palette (GTK_COLOR_CHOOSER (dialog),
                                         GTK_ORIENTATION_VERTICAL,
                                         9, 9*9,
                                         colors);
        }
      else if (g_strcmp0 (argv[i], "--no-palette") == 0)
        {
          gtk_color_chooser_add_palette (GTK_COLOR_CHOOSER (dialog), 
                                         GTK_ORIENTATION_VERTICAL, 0, 0, NULL);
        }
    }

  g_signal_connect (dialog, "notify::color", G_CALLBACK (color_changed), NULL);
  g_signal_connect (dialog, "response", G_CALLBACK (dialog_response), NULL);

  gtk_widget_show_all (dialog);

  gtk_main ();

  return 0;
}

