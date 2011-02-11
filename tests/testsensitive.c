#include <gtk/gtk.h>

static void
set_insensitive (GtkButton *b, GtkWidget *w)
{
  gtk_widget_set_sensitive (w, FALSE);
}

static void
state_changed (GtkWidget *widget)
{
  GtkStateFlags flags;
  const gchar *sep;

  g_print ("state changed: \n");
  switch (gtk_widget_get_state (widget))
    {
    case GTK_STATE_ACTIVE:
      g_print ("active, ");
      break;
    case GTK_STATE_PRELIGHT:
      g_print ("prelight, ");
      break;
    case GTK_STATE_SELECTED:
      g_print ("selected, ");
      break;
    case GTK_STATE_INSENSITIVE:
      g_print ("insensitive, ");
      break;
    case GTK_STATE_INCONSISTENT:
      g_print ("inconsistent, ");
      break;
    case GTK_STATE_FOCUSED:
      g_print ("focused, ");
      break;
    case GTK_STATE_NORMAL:
      g_print ("normal, ");
      break;
    }

  flags = gtk_widget_get_state_flags (widget);
  sep = "";
  if (flags & GTK_STATE_FLAG_ACTIVE)
    {
      g_print ("%sactive", sep);
      sep = "|";
    }
  if (flags & GTK_STATE_FLAG_PRELIGHT)
    {
      g_print ("%sprelight", sep);
      sep = "|";
    }
  if (flags & GTK_STATE_FLAG_SELECTED)
    {
      g_print ("%sselected", sep);
      sep = "|";
    }
  if (flags & GTK_STATE_FLAG_INSENSITIVE)
    {
      g_print ("%sinsensitive", sep);
      sep = "|";
    }
  if (flags & GTK_STATE_FLAG_INCONSISTENT)
    {
      g_print ("%sinconsistent", sep);
      sep = "|";
    }
  if (flags & GTK_STATE_FLAG_FOCUSED)
    {
      g_print ("%sfocused", sep);
      sep = "|";
    }
  if (sep[0] == 0)
    g_print ("normal");
  g_print ("\n");
}

int main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *bu;
  GtkWidget *w, *c;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_add (GTK_CONTAINER (window), box);

  w = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 15);
  gtk_box_pack_start (GTK_BOX (box), w, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (w), gtk_entry_new (), TRUE, TRUE, 0);
  bu = gtk_button_new_with_label ("Bu");
  gtk_box_pack_start (GTK_BOX (w), bu, TRUE, TRUE, 0);
  c = gtk_switch_new ();
  gtk_switch_set_active (GTK_SWITCH (c), TRUE);
  gtk_widget_set_halign (c, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (c, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), c, TRUE, TRUE, 0);
  g_signal_connect (bu, "clicked", G_CALLBACK (set_insensitive), w);
  g_signal_connect (bu, "state-changed", G_CALLBACK (state_changed), NULL);

  g_object_bind_property (c, "active", w, "sensitive", G_BINDING_BIDIRECTIONAL);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
