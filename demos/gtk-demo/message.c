
#include <gtk/gtk.h>
#include "message.h"

#include <stdlib.h>
#include <string.h>

G_DEFINE_TYPE (GtkMessage, gtk_message, G_TYPE_OBJECT);

static void
gtk_message_finalize (GObject *obj)
{
  GtkMessage *msg = GTK_MESSAGE (obj);

  g_free (msg->sender_name);
  g_free (msg->sender_nick);
  g_free (msg->message);
  g_free (msg->resent_by);

  G_OBJECT_CLASS (gtk_message_parent_class)->finalize (obj);
}

static void
gtk_message_class_init (GtkMessageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = gtk_message_finalize;
}

static void
gtk_message_init (GtkMessage *msg)
{
}

static void
gtk_message_parse (GtkMessage *msg, const char *str)
{
  char **strv;
  int i;

  strv = g_strsplit (str, "|", 0);

  i = 0;
  msg->id = strtol (strv[i++], NULL, 10);
  msg->sender_name = g_strdup (strv[i++]);
  msg->sender_nick = g_strdup (strv[i++]);
  msg->message = g_strdup (strv[i++]);
  msg->time = strtol (strv[i++], NULL, 10);
  if (strv[i])
    {
      msg->reply_to = strtol (strv[i++], NULL, 10);
      if (strv[i])
        {
          if (*strv[i])
            msg->resent_by = g_strdup (strv[i]);
          i++;
          if (strv[i])
            {
              msg->n_favorites = strtol (strv[i++], NULL, 10);
              if (strv[i])
                {
                  msg->n_reshares = strtol (strv[i++], NULL, 10);
                }

            }
        }
    }

  g_strfreev (strv);
}

GtkMessage *
gtk_message_new (const char *str)
{
  GtkMessage *msg;
  msg = g_object_new (gtk_message_get_type (), NULL);
  gtk_message_parse (msg, str);
  return msg;
}
