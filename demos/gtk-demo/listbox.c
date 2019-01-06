/* List Box
 *
 * GtkListBox allows lists with complicated layouts, using
 * regular widgets supporting sorting and filtering.
 *
 */

#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>

#include "message.h"

static GdkPixbuf *avatar_pixbuf_other;
static GtkWidget *window = NULL;

#define GTK_TYPE_MESSAGE_ROW		  (gtk_message_row_get_type ())
#define GTK_MESSAGE_ROW(message_row)		  (G_TYPE_CHECK_INSTANCE_CAST ((message_row), GTK_TYPE_MESSAGE_ROW, GtkMessageRow))
#define GTK_MESSAGE_ROW_CLASS(klass)		  (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_MESSAGE_ROW, GtkMessageRowClass))
#define GTK_IS_MESSAGE_ROW(message_row)		  (G_TYPE_CHECK_INSTANCE_TYPE ((message_row), GTK_TYPE_MESSAGE_ROW))
#define GTK_IS_MESSAGE_ROW_CLASS(klass)	  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_MESSAGE_ROW))
#define GTK_MESSAGE_ROW_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_MESSAGE_ROW, GtkMessageRowClass))

typedef struct _GtkMessageRow   GtkMessageRow;
typedef struct _GtkMessageRowClass  GtkMessageRowClass;
typedef struct _GtkMessageRowPrivate  GtkMessageRowPrivate;

struct _GtkMessageRow
{
  GtkListBoxRow parent;

  GtkMessageRowPrivate *priv;
};

struct _GtkMessageRowClass
{
  GtkListBoxRowClass parent_class;
};

struct _GtkMessageRowPrivate
{
  GtkMessage *message;
  GtkRevealer *details_revealer;
  GtkImage *avatar_image;
  GtkWidget *extra_buttons_box;
  GtkLabel *content_label;
  GtkLabel *source_name;
  GtkLabel *source_nick;
  GtkLabel *short_time_label;
  GtkLabel *detailed_time_label;
  GtkBox *resent_box;
  GtkLinkButton *resent_by_button;
  GtkLabel *n_favorites_label;
  GtkLabel *n_reshares_label;
  GtkButton *expand_button;
};

GType      gtk_message_row_get_type  (void) G_GNUC_CONST;

G_DEFINE_TYPE_WITH_PRIVATE (GtkMessageRow, gtk_message_row, GTK_TYPE_LIST_BOX_ROW);

static void
gtk_message_row_update (GtkMessageRow *row)
{
  GtkMessageRowPrivate *priv = row->priv;
  GDateTime *t;
  char *s;

  gtk_label_set_text (priv->source_name, priv->message->sender_name);
  gtk_label_set_text (priv->source_nick, priv->message->sender_nick);
  gtk_label_set_text (priv->content_label, priv->message->message);
  t = g_date_time_new_from_unix_utc (priv->message->time);
  s = g_date_time_format (t, "%e %b %y");
  gtk_label_set_text (priv->short_time_label, s);
  g_free (s);
  s = g_date_time_format (t, "%X - %e %b %Y");
  gtk_label_set_text (priv->detailed_time_label, s);
  g_free (s);
  g_date_time_unref (t);

  gtk_widget_set_visible (GTK_WIDGET(priv->n_favorites_label),
                          priv->message->n_favorites != 0);
  s = g_strdup_printf ("<b>%d</b>\nFavorites", priv->message->n_favorites);
  gtk_label_set_markup (priv->n_favorites_label, s);
  g_free (s);

  gtk_widget_set_visible (GTK_WIDGET(priv->n_reshares_label),
                          priv->message->n_reshares != 0);
  s = g_strdup_printf ("<b>%d</b>\nReshares", priv->message->n_reshares);
  gtk_label_set_markup (priv->n_reshares_label, s);
  g_free (s);

  gtk_widget_set_visible (GTK_WIDGET (priv->resent_box), priv->message->resent_by != NULL);
  if (priv->message->resent_by)
    gtk_button_set_label (GTK_BUTTON (priv->resent_by_button), priv->message->resent_by);

  if (strcmp (priv->message->sender_nick, "@GTKtoolkit") == 0)
    {
      gtk_image_set_from_icon_name (priv->avatar_image, "gtk3-demo");
      gtk_image_set_icon_size (priv->avatar_image, GTK_ICON_SIZE_LARGE);
    }
  else
    gtk_image_set_from_pixbuf (priv->avatar_image, avatar_pixbuf_other);

}

static void
gtk_message_row_expand (GtkMessageRow *row)
{
  GtkMessageRowPrivate *priv = row->priv;
  gboolean expand;

  expand = !gtk_revealer_get_reveal_child (priv->details_revealer);

  gtk_revealer_set_reveal_child (priv->details_revealer, expand);
  if (expand)
    gtk_button_set_label (priv->expand_button, "Hide");
  else
    gtk_button_set_label (priv->expand_button, "Expand");
}

static void
expand_clicked (GtkMessageRow *row,
                GtkButton *button)
{
  gtk_message_row_expand (row);
}

static void
reshare_clicked (GtkMessageRow *row,
                 GtkButton *button)
{
  GtkMessageRowPrivate *priv = row->priv;

  priv->message->n_reshares++;
  gtk_message_row_update (row);

}

static void
favorite_clicked (GtkMessageRow *row,
                  GtkButton *button)
{
  GtkMessageRowPrivate *priv = row->priv;

  priv->message->n_favorites++;
  gtk_message_row_update (row);
}

static void
gtk_message_row_state_flags_changed (GtkWidget    *widget,
                                     GtkStateFlags previous_state_flags)
{
  GtkMessageRowPrivate *priv = GTK_MESSAGE_ROW (widget)->priv;
  GtkStateFlags flags;

  flags = gtk_widget_get_state_flags (widget);

  gtk_widget_set_visible (priv->extra_buttons_box,
                          flags & (GTK_STATE_FLAG_PRELIGHT | GTK_STATE_FLAG_SELECTED));

  GTK_WIDGET_CLASS (gtk_message_row_parent_class)->state_flags_changed (widget, previous_state_flags);
}

static void
gtk_message_row_finalize (GObject *obj)
{
  GtkMessageRowPrivate *priv = GTK_MESSAGE_ROW (obj)->priv;
  g_object_unref (priv->message);
  G_OBJECT_CLASS (gtk_message_row_parent_class)->finalize(obj);
}

static void
gtk_message_row_class_init (GtkMessageRowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_message_row_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/listbox/listbox.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkMessageRow, content_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkMessageRow, source_name);
  gtk_widget_class_bind_template_child_private (widget_class, GtkMessageRow, source_nick);
  gtk_widget_class_bind_template_child_private (widget_class, GtkMessageRow, short_time_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkMessageRow, detailed_time_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkMessageRow, extra_buttons_box);
  gtk_widget_class_bind_template_child_private (widget_class, GtkMessageRow, details_revealer);
  gtk_widget_class_bind_template_child_private (widget_class, GtkMessageRow, avatar_image);
  gtk_widget_class_bind_template_child_private (widget_class, GtkMessageRow, resent_box);
  gtk_widget_class_bind_template_child_private (widget_class, GtkMessageRow, resent_by_button);
  gtk_widget_class_bind_template_child_private (widget_class, GtkMessageRow, n_reshares_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkMessageRow, n_favorites_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkMessageRow, expand_button);
  gtk_widget_class_bind_template_callback (widget_class, expand_clicked);
  gtk_widget_class_bind_template_callback (widget_class, reshare_clicked);
  gtk_widget_class_bind_template_callback (widget_class, favorite_clicked);

  widget_class->state_flags_changed = gtk_message_row_state_flags_changed;
}

static void
gtk_message_row_init (GtkMessageRow *row)
{
  row->priv = gtk_message_row_get_instance_private (row);

  gtk_widget_init_template (GTK_WIDGET (row));
}

static GtkMessageRow *
gtk_message_row_new (GtkMessage *message)
{
  GtkMessageRow *row;

  row = g_object_new (gtk_message_row_get_type (), NULL);
  row->priv->message = message;
  gtk_message_row_update (row);

  return row;
}

static int
gtk_message_row_sort (GtkMessageRow *a, GtkMessageRow *b, gpointer data)
{
  return b->priv->message->time - a->priv->message->time;
}

static void
row_activated (GtkListBox *listbox, GtkListBoxRow *row)
{
  gtk_message_row_expand (GTK_MESSAGE_ROW (row));
}

static void
update_count (GtkListBox *listbox, GtkLabel *label)
{
  GList *children = gtk_container_get_children (GTK_CONTAINER (listbox));
  guint n_items = g_list_length (children);
  g_list_free (children);

  char *text = g_strdup_printf ("%u rows", n_items);
  gtk_label_set_label (label, text);
  g_free (text);
}

static GtkWidget *header_label;

static void
add_more (GtkListBox *listbox)
{
  GBytes *data;
  char **lines;
  int i;

  data = g_resources_lookup_data ("/listbox/messages.txt", 0, NULL);
  lines = g_strsplit (g_bytes_get_data (data, NULL), "\n", 0);

  for (i = 0; lines[i] != NULL && *lines[i]; i++)
    {
      GtkMessage *message = gtk_message_new (lines[i]);
      GtkMessageRow *row = gtk_message_row_new (message);
      gtk_container_add (GTK_CONTAINER (listbox), GTK_WIDGET (row));
    }

  g_strfreev (lines);
  g_bytes_unref (data);

  update_count (listbox, GTK_LABEL (header_label));
}

GtkWidget *
do_listbox (GtkWidget *do_widget)
{
  GtkWidget *scrolled, *listbox, *vbox, *label;
  GtkWidget *header, *more;

  if (!window)
    {
      avatar_pixbuf_other = gdk_pixbuf_new_from_resource_at_scale ("/listbox/apple-red.png", 32, 32, FALSE, NULL);

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_default_size (GTK_WINDOW (window), 400, 600);

      /* NULL window variable when window is closed */
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed),
                        &window);

      listbox = gtk_list_box_new ();

      header = gtk_header_bar_new ();
      gtk_header_bar_set_show_title_buttons (GTK_HEADER_BAR (header), TRUE);
      gtk_header_bar_set_title (GTK_HEADER_BAR (header), "List View");
      header_label = gtk_label_new ("");
      gtk_header_bar_pack_start (GTK_HEADER_BAR (header), header_label);
      more = gtk_button_new_from_icon_name ("list-add");

      g_signal_connect_swapped (more, "clicked", G_CALLBACK (add_more), listbox);

      gtk_header_bar_pack_start (GTK_HEADER_BAR (header), more);
      gtk_window_set_titlebar (GTK_WINDOW (window), header);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
      gtk_container_add (GTK_CONTAINER (window), vbox);
      label = gtk_label_new ("Messages from Gtk+ and friends");
      gtk_box_pack_start (GTK_BOX (vbox), label);
      scrolled = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
      gtk_widget_set_vexpand (scrolled, TRUE);
      gtk_box_pack_start (GTK_BOX (vbox), scrolled);
      gtk_container_add (GTK_CONTAINER (scrolled), listbox);

      gtk_list_box_set_sort_func (GTK_LIST_BOX (listbox), (GtkListBoxSortFunc)gtk_message_row_sort, listbox, NULL);
      gtk_list_box_set_activate_on_single_click (GTK_LIST_BOX (listbox), FALSE);
      g_signal_connect (listbox, "row-activated", G_CALLBACK (row_activated), NULL);

      add_more (listbox);
      update_count (listbox, header_label);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
