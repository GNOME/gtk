#include "x11/gdkx.h"
#include <gtk/gtk.h>

static void
remove_buttons (GtkWidget *widget, GtkWidget *other_button)
{
  gtk_widget_destroy (other_button);
  gtk_widget_destroy (widget);
}

static gboolean
blink_cb (gpointer data)
{
  GtkWidget *widget = data;

  gtk_widget_show (widget);
  g_object_set_data (G_OBJECT (widget), "blink", GPOINTER_TO_UINT (0));

  return FALSE;
}

static void
blink (GtkWidget *widget,
       GtkWidget *window)
{
  guint blink_timeout = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (window), "blink"));
  
  if (!blink_timeout)
    {
      blink_timeout = g_timeout_add (1000, blink_cb, window);
      gtk_widget_hide (window);

      g_object_set_data (G_OBJECT (window), "blink", GUINT_TO_POINTER (blink_timeout));
    }
}

static void
local_destroy (GtkWidget *window)
{
  guint blink_timeout = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (window), "blink"));
  if (blink_timeout)
    g_source_remove (blink_timeout);
}

static void
remote_destroy (GtkWidget *window)
{
  local_destroy (window);
  gtk_main_quit ();
}

static void
add_buttons (GtkWidget *widget, GtkWidget *box)
{
  GtkWidget *add_button;
  GtkWidget *remove_button;

  add_button = gtk_button_new_with_mnemonic ("_Add");
  gtk_box_pack_start (GTK_BOX (box), add_button, TRUE, TRUE, 0);
  gtk_widget_show (add_button);

  gtk_signal_connect (GTK_OBJECT (add_button), "clicked",
		      GTK_SIGNAL_FUNC (add_buttons),
		      box);

  remove_button = gtk_button_new_with_mnemonic ("_Remove");
  gtk_box_pack_start (GTK_BOX (box), remove_button, TRUE, TRUE, 0);
  gtk_widget_show (remove_button);

  gtk_signal_connect (GTK_OBJECT (remove_button), "clicked",
		      GTK_SIGNAL_FUNC (remove_buttons),
		      add_button);
}

guint32
create_child_plug (guint32  xid,
		   gboolean local)
{
  GtkWidget *window;
  GtkWidget *hbox;
  GtkWidget *entry;
  GtkWidget *button;
  GtkWidget *label;

  window = gtk_plug_new (xid);

  gtk_signal_connect (GTK_OBJECT (window), "destroy",
		      local ? GTK_SIGNAL_FUNC (local_destroy) : GTK_SIGNAL_FUNC (remote_destroy),
		      NULL);
  gtk_container_set_border_width (GTK_CONTAINER (window), 0);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), hbox);

  label = gtk_label_new (local ? "Local:" : "Remote:");
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
  
  entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);

  button = gtk_button_new_with_mnemonic ("_Close");
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);

  gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
			     GTK_SIGNAL_FUNC (gtk_widget_destroy),
			     GTK_OBJECT (window));

  button = gtk_button_new_with_mnemonic ("_Blink");
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);

  gtk_signal_connect (GTK_OBJECT (button), "clicked",
		      GTK_SIGNAL_FUNC (blink),
		      GTK_OBJECT (window));

  add_buttons (NULL, hbox);
  
  gtk_widget_show_all (window);

  if (GTK_WIDGET_REALIZED (window))
    return GDK_WINDOW_XID (window->window);
  else
    return 0;
}
