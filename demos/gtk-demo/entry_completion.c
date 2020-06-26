/* Entry/Completion
 *
 * GtkEntryCompletion provides a mechanism for adding support for
 * completion in GtkEntry.
 *
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

/* Creates a list model containing the completions */
static GListModel *
create_completion_model (void)
{
  const char *strings[] = {
    "GNOME",
    "gnominious",
    "Gnomonic projection",
    "Gnosophy",
    "total",
    "totally",
    "toto",
    "tottery",
    "totterer",
    "Totten trust",
    "Tottenham hotspurs",
    "totipotent",
    "totipotency",
    "totemism",
    "totem pole",
    "Totara",
    "totalizer",
    "totalizator",
    "totalitarianism",
    "total parenteral nutrition",
    "total eclipse",
    "Totipresence",
    "Totipalmi",
    "zombie",
    "aæx",
    "aæy",
    "aæz",
    NULL
  };

  return G_LIST_MODEL (gtk_string_list_new (strings));
}


GtkWidget *
do_entry_completion (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *vbox;
  GtkWidget *label;
  GtkWidget *entry;
  GtkEntryCompletion *completion;
  GListModel *completion_model;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Completion");
      gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
      gtk_widget_set_margin_start (vbox, 5);
      gtk_widget_set_margin_end (vbox, 5);
      gtk_widget_set_margin_top (vbox, 5);
      gtk_widget_set_margin_bottom (vbox, 5);
      gtk_window_set_child (GTK_WINDOW (window), vbox);

      label = gtk_label_new (NULL);
      gtk_label_set_markup (GTK_LABEL (label), "Try writing <b>total</b> or <b>gnome</b> for example.");
      gtk_box_append (GTK_BOX (vbox), label);

      /* Create our entry */
      entry = gtk_entry_new ();
      gtk_box_append (GTK_BOX (vbox), entry);

      /* Create the completion object */
      completion = gtk_entry_completion_new ();

      /* Assign the completion to the entry */
      gtk_entry_set_completion (GTK_ENTRY (entry), completion);
      g_object_unref (completion);

      /* Create a tree model and use it as the completion model */
      completion_model = create_completion_model ();
      gtk_entry_completion_set_model (completion, completion_model);
      g_object_unref (completion_model);

      gtk_entry_completion_set_inline_completion (completion, TRUE);
      gtk_entry_completion_set_inline_selection (completion, TRUE);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
