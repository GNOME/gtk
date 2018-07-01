/* GUADEC 2018/GTK4 lightning talks
 *
 * These are the presentation slides for the GUADEC 2018
 * presentation "GTK4 lightnoing talks."
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "paintable.h"

GtkWidget *slides;

static GtkWidget *
slide_get_nth_child_widget (GtkStack *stack,
                            guint     n)
{
  GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (stack));
  guint i;

  for (i = 0; i < n && child != NULL; i++)
    {
      child = gtk_widget_get_next_sibling (child);
    }

  return child;
}

static void
switch_to_slide (GtkWidget *spinbutton,
                 GtkWidget *stack)
{
  int slide;
  GtkWidget *child;

  slide = gtk_spin_button_get_value (GTK_SPIN_BUTTON (spinbutton)) - 1;
  child = slide_get_nth_child_widget (GTK_STACK (stack), slide);
  gtk_stack_set_visible_child (GTK_STACK (stack), child);
}

GtkWidget *
do_gtk4_lightning_talks (GtkWidget *do_widget)
{
  static GtkWidget *window;

  if (!window)
  {
    GtkWidget *slides;
    GtkBuilder *builder;

    /* g_type_ensure() hacks */
    g_object_unref (gtk_nuclear_icon_new (0.0));
    g_object_unref (gtk_nuclear_animation_new ());
    g_object_unref (gtk_nuclear_media_stream_new ());

    builder = gtk_builder_new_from_resource ("/guadec2018/gtk4_lightning_talks.ui");
    gtk_builder_add_callback_symbols (builder,
                                      "switch_to_slide", G_CALLBACK (switch_to_slide),
                                      NULL);
    gtk_builder_connect_signals (builder, NULL);
    window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
    gtk_window_set_display (GTK_WINDOW (window),
                            gtk_widget_get_display (do_widget));
    g_signal_connect (window, "destroy",
                      G_CALLBACK (gtk_widget_destroyed), &window);

    slides = GTK_WIDGET (gtk_builder_get_object (builder, "slides"));
    slides = slides;

    g_object_unref (builder);
  }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
