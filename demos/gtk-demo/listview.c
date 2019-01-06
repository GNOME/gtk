/* List View
 *
 * GtkListView allows lists with complicated layouts, using
 * models to hold the data, and creating rows on demand.
 */

#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>

#include "message.h"

static GdkPixbuf *avatar_pixbuf_other;
static GtkWidget *window = NULL;

#define GTK_TYPE_MSG_ROW		  (gtk_msg_row_get_type ())
#define GTK_MSG_ROW(msg_row)		  (G_TYPE_CHECK_INSTANCE_CAST ((msg_row), GTK_TYPE_MSG_ROW, GtkMsgRow))
#define GTK_MSG_ROW_CLASS(klass)	  (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_MSG_ROW, GtkMsgRowClass))
#define GTK_IS_MSG_ROW(msg_row)		  (G_TYPE_CHECK_INSTANCE_TYPE ((msg_row), GTK_TYPE_MSG_ROW))
#define GTK_IS_MSG_ROW_CLASS(klass)	  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_MSG_ROW))
#define GTK_MSG_ROW_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_MSG_ROW, GtkMsgRowClass))

typedef struct _GtkMsgRow   GtkMsgRow;
typedef struct _GtkMsgRowClass  GtkMsgRowClass;

struct _GtkMsgRow
{
  GtkBin parent;

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

struct _GtkMsgRowClass
{
  GtkBinClass parent_class;
};

static GType      gtk_msg_row_get_type  (void) G_GNUC_CONST;

G_DEFINE_TYPE (GtkMsgRow, gtk_msg_row, GTK_TYPE_BIN);

static void
gtk_msg_row_update (GtkMsgRow *row)
{
  GDateTime *t;
  char *s;

  gtk_label_set_text (row->source_name, row->message->sender_name);
  gtk_label_set_text (row->source_nick, row->message->sender_nick);
  gtk_label_set_text (row->content_label, row->message->message);
  t = g_date_time_new_from_unix_utc (row->message->time);
  s = g_date_time_format (t, "%e %b %y");
  gtk_label_set_text (row->short_time_label, s);
  g_free (s);
  s = g_date_time_format (t, "%X - %e %b %Y");
  gtk_label_set_text (row->detailed_time_label, s);
  g_free (s);
  g_date_time_unref (t);

  gtk_widget_set_visible (GTK_WIDGET(row->n_favorites_label),
                          row->message->n_favorites != 0);
  s = g_strdup_printf ("<b>%d</b>\nFavorites", row->message->n_favorites);
  gtk_label_set_markup (row->n_favorites_label, s);
  g_free (s);

  gtk_widget_set_visible (GTK_WIDGET(row->n_reshares_label),
                          row->message->n_reshares != 0);
  s = g_strdup_printf ("<b>%d</b>\nReshares", row->message->n_reshares);
  gtk_label_set_markup (row->n_reshares_label, s);
  g_free (s);

  gtk_widget_set_visible (GTK_WIDGET (row->resent_box), row->message->resent_by != NULL);
  if (row->message->resent_by)
    gtk_button_set_label (GTK_BUTTON (row->resent_by_button), row->message->resent_by);

  if (strcmp (row->message->sender_nick, "@GTKtoolkit") == 0)
    {
      gtk_image_set_from_icon_name (row->avatar_image, "gtk3-demo");
      gtk_image_set_icon_size (row->avatar_image, GTK_ICON_SIZE_LARGE);
    }
  else
    gtk_image_set_from_pixbuf (row->avatar_image, avatar_pixbuf_other);

}

static void
gtk_msg_row_expand (GtkMsgRow *row)
{
  gboolean expand;

  expand = !gtk_revealer_get_reveal_child (row->details_revealer);

  gtk_revealer_set_reveal_child (row->details_revealer, expand);
  if (expand)
    gtk_button_set_label (row->expand_button, "Hide");
  else
    gtk_button_set_label (row->expand_button, "Expand");
}

static void
expand_clicked (GtkMsgRow *row,
                GtkButton *button)
{
  gtk_msg_row_expand (row);
}

static void
reshare_clicked (GtkMsgRow *row,
                 GtkButton *button)
{
  row->message->n_reshares++;
  gtk_msg_row_update (row);

}

static void
favorite_clicked (GtkMsgRow *row,
                  GtkButton *button)
{
  row->message->n_favorites++;
  gtk_msg_row_update (row);
}

static void
gtk_msg_row_state_flags_changed (GtkWidget    *widget,
                                 GtkStateFlags previous_state_flags)
{
  GtkMsgRow *row = GTK_MSG_ROW (widget);
  GtkStateFlags flags;

  flags = gtk_widget_get_state_flags (widget);

  gtk_widget_set_visible (row->extra_buttons_box,
                          flags & (GTK_STATE_FLAG_PRELIGHT | GTK_STATE_FLAG_SELECTED));

  GTK_WIDGET_CLASS (gtk_msg_row_parent_class)->state_flags_changed (widget, previous_state_flags);
}

static void
gtk_msg_row_finalize (GObject *obj)
{
  G_OBJECT_CLASS (gtk_msg_row_parent_class)->finalize(obj);
}

static void
gtk_msg_row_class_init (GtkMsgRowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_msg_row_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/listview/listview.ui");
  gtk_widget_class_bind_template_child (widget_class, GtkMsgRow, content_label);
  gtk_widget_class_bind_template_child (widget_class, GtkMsgRow, source_name);
  gtk_widget_class_bind_template_child (widget_class, GtkMsgRow, source_nick);
  gtk_widget_class_bind_template_child (widget_class, GtkMsgRow, short_time_label);
  gtk_widget_class_bind_template_child (widget_class, GtkMsgRow, detailed_time_label);
  gtk_widget_class_bind_template_child (widget_class, GtkMsgRow, extra_buttons_box);
  gtk_widget_class_bind_template_child (widget_class, GtkMsgRow, details_revealer);
  gtk_widget_class_bind_template_child (widget_class, GtkMsgRow, avatar_image);
  gtk_widget_class_bind_template_child (widget_class, GtkMsgRow, resent_box);
  gtk_widget_class_bind_template_child (widget_class, GtkMsgRow, resent_by_button);
  gtk_widget_class_bind_template_child (widget_class, GtkMsgRow, n_reshares_label);
  gtk_widget_class_bind_template_child (widget_class, GtkMsgRow, n_favorites_label);
  gtk_widget_class_bind_template_child (widget_class, GtkMsgRow, expand_button);
  gtk_widget_class_bind_template_callback (widget_class, expand_clicked);
  gtk_widget_class_bind_template_callback (widget_class, reshare_clicked);
  gtk_widget_class_bind_template_callback (widget_class, favorite_clicked);

  widget_class->state_flags_changed = gtk_msg_row_state_flags_changed;
}

static void
row_activated (GtkGestureMultiPress *gesture,
               int n_press,
               double x,
               double y,
               GtkMsgRow *row)
{
  if (n_press == 2)
    gtk_msg_row_expand (row);
}

static void
gtk_msg_row_init (GtkMsgRow *row)
{
  GtkGesture *double_click;

  gtk_widget_init_template (GTK_WIDGET (row));

  double_click = gtk_gesture_multi_press_new ();
  g_signal_connect (double_click, "pressed", G_CALLBACK (row_activated), row);
  gtk_widget_add_controller (GTK_WIDGET (row), GTK_EVENT_CONTROLLER (double_click));
}

static int
gtk_message_sort (gconstpointer a, gconstpointer b, gpointer data)
{
  return ((const GtkMessage *)b)->time - ((const GtkMessage *)a)->time;
}

static void
bind_msg_row (GObject *list_item, GParamSpec *pspec, gpointer data)
{
  GtkMessage *message = (GtkMessage *)gtk_list_item_get_item (GTK_LIST_ITEM (list_item));
  GtkMsgRow *row = (GtkMsgRow *) gtk_bin_get_child (GTK_BIN (list_item));

  row->message = message;
  if (message)
    gtk_msg_row_update (row);
}

static void
setup_row (GtkListItem *item,
           gpointer data)
{
  g_signal_connect (item, "notify::item", G_CALLBACK (bind_msg_row), data);
  gtk_container_add (GTK_CONTAINER (item), g_object_new (gtk_msg_row_get_type (), NULL));
}

static void
update_count (GListModel *model, guint position, guint removed, guint added, GtkLabel *label)
{
  guint n_items = g_list_model_get_n_items (model);
  char *text = g_strdup_printf ("%u rows", n_items);
  gtk_label_set_label (label, text);
  g_free (text);
}

static void
add_more (GListModel *model)
{
  GBytes *data;
  char **lines;
  int i;

  data = g_resources_lookup_data ("/listview/messages.txt", 0, NULL);
  lines = g_strsplit (g_bytes_get_data (data, NULL), "\n", 0);

  for (i = 0; lines[i] != NULL && *lines[i]; i++)
    {
      GtkMessage *message = gtk_message_new (lines[i]);
      g_list_store_append (G_LIST_STORE (model), message);
    }

  g_strfreev (lines);
  g_bytes_unref (data);
}

GtkWidget *
do_listview (GtkWidget *do_widget)
{
  GtkWidget *scrolled, *listview, *vbox, *label;
  GtkWidget *header, *header_label, *more;
  GListModel *model;

  if (!window)
    {
      avatar_pixbuf_other = gdk_pixbuf_new_from_resource_at_scale ("/listbox/apple-red.png", 32, 32, FALSE, NULL);

      model = G_LIST_MODEL (g_list_store_new (gtk_message_get_type ()));

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_default_size (GTK_WINDOW (window), 400, 600);

      /* NULL window variable when window is closed */
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed),
                        &window);

      header = gtk_header_bar_new ();
      gtk_header_bar_set_show_title_buttons (GTK_HEADER_BAR (header), TRUE);
      gtk_header_bar_set_title (GTK_HEADER_BAR (header), "List View");
      header_label = gtk_label_new ("");
      gtk_header_bar_pack_start (GTK_HEADER_BAR (header), header_label);
      more = gtk_button_new_from_icon_name ("list-add");

      g_signal_connect_swapped (more, "clicked", G_CALLBACK (add_more), model);
      g_signal_connect (model, "items-changed", G_CALLBACK (update_count), header_label);

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
      listview = gtk_list_view_new ();
      gtk_container_add (GTK_CONTAINER (scrolled), listview);

      gtk_list_view_set_functions (GTK_LIST_VIEW (listview), setup_row, NULL, NULL, NULL);
      gtk_list_view_set_model (GTK_LIST_VIEW (listview), G_LIST_MODEL (gtk_sort_list_model_new (model, gtk_message_sort, NULL, NULL)));

      add_more (model);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
