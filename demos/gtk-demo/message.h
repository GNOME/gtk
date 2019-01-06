#pragma once

#define GTK_TYPE_MESSAGE		  (gtk_message_get_type ())
#define GTK_MESSAGE(message)		  (G_TYPE_CHECK_INSTANCE_CAST ((message), GTK_TYPE_MESSAGE, GtkMessage))
#define GTK_MESSAGE_CLASS(klass)		  (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_MESSAGE, GtkMessageClass))
#define GTK_IS_MESSAGE(message)		  (G_TYPE_CHECK_INSTANCE_TYPE ((message), GTK_TYPE_MESSAGE))
#define GTK_IS_MESSAGE_CLASS(klass)	  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_MESSAGE))
#define GTK_MESSAGE_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_MESSAGE, GtkMessageClass))

typedef struct _GtkMessage   GtkMessage;
typedef struct _GtkMessageClass  GtkMessageClass;

struct _GtkMessage
{
  GObject parent;

  guint id;
  char *sender_name;
  char *sender_nick;
  char *message;
  gint64 time;
  guint reply_to;
  char *resent_by;
  int n_favorites;
  int n_reshares;
};

struct _GtkMessageClass
{
  GObjectClass parent_class;
};

GType        gtk_message_get_type  (void) G_GNUC_CONST;
GtkMessage * gtk_message_new       (const char *str);
