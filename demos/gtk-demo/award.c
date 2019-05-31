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

  char *explanation;
  char *name;
  char *title;
  GDateTime *granted; /* or NULL if not granted */
};

enum {
  PROP_0,
  PROP_EXPLANATION,
  PROP_NAME,
  PROP_TITLE,
  PROP_GRANTED,

  N_PROPS,
};

static GParamSpec *properties[N_PROPS] = { NULL, };

G_DEFINE_TYPE (GtkAward, gtk_award, G_TYPE_OBJECT)

static void
gtk_award_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)

{
  GtkAward *self = GTK_AWARD (object);

  switch (prop_id)
    {
    case PROP_EXPLANATION:
      self->explanation = g_value_dup_string (value);
      break;

    case PROP_NAME:
      self->name = g_value_dup_string (value);
      break;

    case PROP_TITLE:
      self->title = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_award_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  GtkAward *self = GTK_AWARD (object);

  switch (prop_id)
    {
    case PROP_EXPLANATION:
      g_value_set_string (value, self->explanation);
      break;

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

  gobject_class->set_property = gtk_award_set_property;
  gobject_class->get_property = gtk_award_get_property;
  gobject_class->dispose = gtk_award_dispose;

  properties[PROP_EXPLANATION] =
    g_param_spec_string ("explanation",
                         "Explanation",
                         "How to get the title",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "internal name of the award",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "user-visible title",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

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
      GtkBuilder *builder;

      g_type_ensure (GTK_TYPE_AWARD);
      builder = gtk_builder_new_from_resource ("/awards.ui");
      gtk_builder_connect_signals (builder, NULL);
      list = G_LIST_MODEL (gtk_builder_get_object (builder, "list"));
      g_object_ref (list);
      g_object_unref (builder);
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

GtkAward *
award_find (const char *name)
{
  GListModel *list;
  GtkAward *self;
  guint i;

  list = gtk_award_get_list ();
  g_object_unref (list);

  for (i = 0; i < g_list_model_get_n_items (list); i++)
    {
      self = g_list_model_get_item (list, i);
      g_object_unref (self);

      if (g_ascii_strcasecmp (name, self->name) == 0)
        return self;
    }

  return NULL;
}

void
award (const char *name)
{
  GtkAward *self;
  GNotification *notification;

  self = award_find (name);
  if (self == NULL)
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

