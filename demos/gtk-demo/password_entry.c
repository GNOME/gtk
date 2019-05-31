/* Entry/Password Entry
 *
 * GtkPasswordEntry provides common functionality of
 * entries that are used to enter passwords and other
 * secrets.
 *
 * It will display a warning if CapsLock is on, and it
 * can optionally provide a way to see the text.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "award.h"

static GtkWidget *entry;
static GtkWidget *entry2;
static GtkWidget *button;

static void
update_button (GObject    *object,
               GParamSpec *pspec,
               gpointer    data)
{
  const char *text = gtk_editable_get_text (GTK_EDITABLE (entry));
  const char *text2 = gtk_editable_get_text (GTK_EDITABLE (entry2));

  gtk_widget_set_sensitive (button,
                            text[0] != '\0' && g_str_equal (text, text2));

  if (g_str_equal (text, text2) &&
      g_ascii_strcasecmp (text, "12345") == 0)
    award ("password-best");
}

static void
button_pressed (GtkButton *button,
                GtkWidget *window)
{
  award ("password-correct");
  gtk_widget_destroy (window);
}

GtkWidget *
do_password_entry (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *box;
  GtkWidget *header;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      header = gtk_header_bar_new ();
      gtk_header_bar_set_title (GTK_HEADER_BAR (header), "Choose a Password");
      gtk_header_bar_set_show_title_buttons (GTK_HEADER_BAR (header), FALSE);
      gtk_window_set_titlebar (GTK_WINDOW (window), header);
      gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
      gtk_window_set_deletable (GTK_WINDOW (window), FALSE);
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
      g_object_set (box, "margin", 18, NULL);
      gtk_container_add (GTK_CONTAINER (window), box);

      entry = gtk_password_entry_new ();
      gtk_password_entry_set_show_peek_icon (GTK_PASSWORD_ENTRY (entry), TRUE);
      g_object_set (entry,
                    "placeholder-text", "Password",
                    "activates-default", TRUE,
                    NULL);
      g_signal_connect (entry, "notify::text", G_CALLBACK (update_button), NULL);
      gtk_container_add (GTK_CONTAINER (box), entry);

      entry2 = gtk_password_entry_new ();
      gtk_password_entry_set_show_peek_icon (GTK_PASSWORD_ENTRY (entry2), TRUE);
      g_object_set (entry2,
                    "placeholder-text", "Confirm",
                    "activates-default", TRUE,
                    NULL);
      g_signal_connect (entry2, "notify::text", G_CALLBACK (update_button), NULL);
      gtk_container_add (GTK_CONTAINER (box), entry2);

      button = gtk_button_new_with_mnemonic ("_Done");
      gtk_style_context_add_class (gtk_widget_get_style_context (button), "suggested-action");
      g_signal_connect (button, "clicked", G_CALLBACK (button_pressed), window);
      gtk_widget_set_sensitive (button, FALSE);
      gtk_header_bar_pack_end (GTK_HEADER_BAR (header), button);

      gtk_window_set_default_widget (GTK_WINDOW (window), button);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
