/* Entry/Tagged Entry
 *
 * This example shows how to build a complex composite
 * entry using GtkText, outside of GTK.
 *
 * This tagged entry can display tags and other widgets
 * inside the entry area.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include "demotaggedentry.h"

static GtkWidget *spinner = NULL;

static void
closed_cb (DemoTaggedEntryTag *tag, DemoTaggedEntry *entry)
{
  demo_tagged_entry_remove_tag (entry, GTK_WIDGET (tag));
}

static void
add_tag (GtkButton *button, DemoTaggedEntry *entry)
{
  DemoTaggedEntryTag *tag;

  tag = demo_tagged_entry_tag_new ("Blue");
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (tag)), "blue");
  demo_tagged_entry_tag_set_has_close_button (tag, TRUE);
  g_signal_connect (tag, "button-clicked", G_CALLBACK (closed_cb), entry);

  if (spinner == NULL)
    demo_tagged_entry_add_tag (entry, GTK_WIDGET (tag));
  else
    demo_tagged_entry_insert_tag_after (entry, GTK_WIDGET (tag), gtk_widget_get_prev_sibling (spinner));
}

static void
toggle_spinner (GtkCheckButton *button, DemoTaggedEntry *entry)
{
  if (spinner)
    {
      demo_tagged_entry_remove_tag (entry, spinner);
      spinner = NULL; 
    }
  else
    {
      spinner = gtk_spinner_new ();
      gtk_spinner_start (GTK_SPINNER (spinner));
      demo_tagged_entry_add_tag (entry, spinner);
    }
}

GtkWidget *
do_tagged_entry (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *box;
  GtkWidget *box2;
  GtkWidget *header;
  GtkWidget *entry;
  GtkWidget *button;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      header = gtk_header_bar_new ();
      gtk_header_bar_set_title (GTK_HEADER_BAR (header), "A tagged entry");
      gtk_header_bar_set_show_title_buttons (GTK_HEADER_BAR (header), FALSE);
      gtk_window_set_titlebar (GTK_WINDOW (window), header);
      gtk_window_set_resizable (GTK_WINDOW (window), TRUE);
      gtk_window_set_deletable (GTK_WINDOW (window), FALSE);
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
      g_object_set (box, "margin", 18, NULL);
      gtk_container_add (GTK_CONTAINER (window), box);

      entry = demo_tagged_entry_new ();
      gtk_container_add (GTK_CONTAINER (box), entry);

      box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
      gtk_widget_set_halign (box2, GTK_ALIGN_END);
      gtk_container_add (GTK_CONTAINER (box), box2);

      button = gtk_button_new_with_mnemonic ("Add _Tag");
      g_signal_connect (button, "clicked", G_CALLBACK (add_tag), entry);
      gtk_container_add (GTK_CONTAINER (box2), button);

      button = gtk_check_button_new_with_mnemonic ("_Spinner");
      g_signal_connect (button, "toggled", G_CALLBACK (toggle_spinner), entry);
      gtk_container_add (GTK_CONTAINER (box2), button);
      
      button = gtk_button_new_with_mnemonic ("_Done");
      gtk_style_context_add_class (gtk_widget_get_style_context (button), "suggested-action");
      g_signal_connect_swapped (button, "clicked", G_CALLBACK (gtk_widget_destroy), window);
      gtk_header_bar_pack_end (GTK_HEADER_BAR (header), button);

      gtk_window_set_default_widget (GTK_WINDOW (window), button);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
