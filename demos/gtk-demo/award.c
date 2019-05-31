/*
 * Copyright © 2018 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "award.h"

struct _GtkAward
{
  GObject parent;

  char *name;
  char *title;
  GDateTime *granted; /* or NULL if not granted */
};

enum {
  PROP_0,
  PROP_NAME,
  PROP_TITLE,
  PROP_GRANTED,

  N_PROPS,
};

static GParamSpec *properties[N_PROPS] = { NULL, };

G_DEFINE_TYPE (GtkAward, gtk_award, G_TYPE_OBJECT)

static void
gtk_award_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  GtkAward *self = GTK_AWARD (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    case PROP_TITLE:
      g_value_set_string (value, self->title);
      break;

    case PROP_GRANTED:
      g_value_set_boxed (value, self->granted);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_award_dispose (GObject *object)
{
  GtkAward *self = GTK_AWARD (object);

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->granted, g_date_time_unref);

  G_OBJECT_CLASS (gtk_award_parent_class)->dispose (object);
}

static void
gtk_award_class_init (GtkAwardClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->get_property = gtk_award_get_property;
  gobject_class->dispose = gtk_award_dispose;

  properties[PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "internal name of the award",
                         NULL,
                         G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "user-visible title",
                         NULL,
                         G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_GRANTED] =
    g_param_spec_boxed ("granted",
                        "Granted",
                        "Timestamp the award was granted or NULL if not granted yet",
                        G_TYPE_DATE_TIME,
                        G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gtk_award_init (GtkAward *self)
{
}

GListModel *
gtk_award_get_list (void)
{
  static GListModel *list = NULL;

  if (list == NULL)
    {
      GBytes *data;
      char **lines;
      guint i;

      list = G_LIST_MODEL (g_list_store_new (GTK_TYPE_AWARD));

      data = g_resources_lookup_data ("/awards.txt", 0, NULL);
      lines = g_strsplit (g_bytes_get_data (data, NULL), "\n", 0);

      for (i = 0; lines[i] != NULL && *lines[i]; i++)
        {
          GtkAward *award = g_object_new (GTK_TYPE_AWARD, NULL);
          const char *s;
          s = strchr (lines[i], ' ');
          g_assert (s);
          award->name = g_strndup (lines[i], s - lines[i]);
          while (*s == ' ') s++;
          award->title = g_strdup (s);
          g_list_store_append (G_LIST_STORE (list), award);
          g_object_unref (award);
        }

      g_strfreev (lines);
      g_bytes_unref (data);
    }

  return g_object_ref (list);
}

const char *
gtk_award_get_name (GtkAward *award)
{
  return award->name;
}

const char *
gtk_award_get_title (GtkAward *award)
{
  return award->title;
}

GDateTime *
gtk_award_get_granted (GtkAward *award)
{
  return award->granted;
}

void
award (const char *name)
{
  GListModel *list;
  GtkAward *self;
  GNotification *notification;
  guint i;

  list = gtk_award_get_list ();
  g_object_unref (list);

  for (i = 0; i < g_list_model_get_n_items (list); i++)
    {
      self = g_list_model_get_item (list, i);
      g_object_unref (self);

      if (g_ascii_strcasecmp (name, self->name) == 0)
        break;
    }

  if (i == g_list_model_get_n_items (list))
    {
      g_warning ("Did not find award \"%s\"", name);
      return;
    }

  if (self->granted)
    return;

  self->granted = g_date_time_new_now_utc ();
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_GRANTED]);

  notification = g_notification_new ("You won an award!");
  g_notification_set_body (notification, self->title);
  g_application_send_notification (g_application_get_default (), NULL, notification);
  g_object_unref (notification);
}

