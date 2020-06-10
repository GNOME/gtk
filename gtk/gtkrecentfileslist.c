/*
 * Copyright © 2020 Red Hat, Inc.
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
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gtkrecentfileslist.h"

#include "gtksettings.h"
#include "gtkintl.h"
#include "gtkprivate.h"

/**
 * SECTION:gtkrecentfileslist
 * @title: GtkRecentFilesList
 * @short_description: A list model for recent files
 * @see_also: #GListModel, #GBookmarkFile
 *
 * #GtkRecentFilesList is a list model that wraps GBookmarkFile.
 * It presents a #GListModel and fills it asynchronously with the #GFileInfos
 * returned from that function.
 */

enum {
  PROP_0,
  PROP_ATTRIBUTES,
  PROP_APPLICATION_ID,
  PROP_IO_PRIORITY,
  PROP_LOADING,
  NUM_PROPERTIES
};

struct _GtkRecentFilesList
{
  GObject parent_instance;

  char *application_id;
  char *attributes;
  char *filename;
  int max_age;
  gboolean enabled;
  int io_priority;

  GCancellable *cancellable;
  GFileMonitor *monitor;

  GSequence *items;
};

struct _GtkRecentFilesListClass
{
  GObjectClass parent_class;
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static GType
gtk_recent_files_list_get_item_type (GListModel *list)
{
  return G_TYPE_FILE_INFO;
}

static guint
gtk_recent_files_list_get_n_items (GListModel *list)
{
  GtkRecentFilesList *self = GTK_RECENT_FILES_LIST (list);

  return g_sequence_get_length (self->items);
}

static gpointer
gtk_recent_files_list_get_item (GListModel *list,
                                guint       position)
{
  GtkRecentFilesList *self = GTK_RECENT_FILES_LIST (list);
  GSequenceIter *iter;

  iter = g_sequence_get_iter_at_pos (self->items, position);

  if (g_sequence_iter_is_end (iter))
    return NULL;
  else
    return g_object_ref (g_sequence_get (iter));
}

static void
gtk_recent_files_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_recent_files_list_get_item_type;
  iface->get_n_items = gtk_recent_files_list_get_n_items;
  iface->get_item = gtk_recent_files_list_get_item;
}

static void     gtk_recent_files_list_start_loading (GtkRecentFilesList *self);
static gboolean gtk_recent_files_list_stop_loading  (GtkRecentFilesList *self);
static void     bookmark_file_changed (GFileMonitor       *monitor,
                                       GFile              *file,
                                       GFile              *other_file,
                                       GFileMonitorEvent   event,
                                       gpointer            data);

G_DEFINE_TYPE_WITH_CODE (GtkRecentFilesList, gtk_recent_files_list, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_recent_files_list_model_init))

static void
gtk_recent_files_list_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GtkRecentFilesList *self = GTK_RECENT_FILES_LIST (object);

  switch (prop_id)
    {
    case PROP_ATTRIBUTES:
      gtk_recent_files_list_set_attributes (self, g_value_get_string (value));
      break;

    case PROP_APPLICATION_ID:
      gtk_recent_files_list_set_application_id (self, g_value_get_string (value));
      break;

    case PROP_IO_PRIORITY:
      gtk_recent_files_list_set_io_priority (self, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_recent_files_list_get_property (GObject     *object,
                                    guint        prop_id,
                                    GValue      *value,
                                    GParamSpec  *pspec)
{
  GtkRecentFilesList *self = GTK_RECENT_FILES_LIST (object);

  switch (prop_id)
    {
    case PROP_ATTRIBUTES:
      g_value_set_string (value, self->attributes);
      break;

    case PROP_APPLICATION_ID:
      g_value_set_string (value, self->application_id);
      break;

    case PROP_IO_PRIORITY:
      g_value_set_int (value, self->io_priority);
      break;

    case PROP_LOADING:
      g_value_set_boolean (value, gtk_recent_files_list_is_loading (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_recent_files_list_dispose (GObject *object)
{
  GtkRecentFilesList *self = GTK_RECENT_FILES_LIST (object);

  gtk_recent_files_list_stop_loading (self);

  g_clear_pointer (&self->attributes, g_free);
  g_clear_pointer (&self->application_id, g_free);
  g_clear_pointer (&self->filename, g_free);
  g_clear_pointer (&self->items, g_sequence_free);

  g_signal_handlers_disconnect_by_func (self->monitor, G_CALLBACK (bookmark_file_changed), self);
  g_clear_object (&self->monitor);

  G_OBJECT_CLASS (gtk_recent_files_list_parent_class)->dispose (object);
}

static void
gtk_recent_files_list_class_init (GtkRecentFilesListClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->set_property = gtk_recent_files_list_set_property;
  gobject_class->get_property = gtk_recent_files_list_get_property;
  gobject_class->dispose = gtk_recent_files_list_dispose;

  /**
   * GtkRecentFilesList:attributes:
   *
   * The attributes to query
   */
  properties[PROP_ATTRIBUTES] =
      g_param_spec_string ("attributes",
                           P_("Attributes"),
                           P_("Attributes to query"),
                           NULL,
                           GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkRecentFilesList:io-priority:
   *
   * Priority used when loading
   */
  properties[PROP_IO_PRIORITY] =
      g_param_spec_int ("io-priority",
                        P_("IO priority"),
                        P_("Priority used when loading"),
                        -G_MAXINT, G_MAXINT, G_PRIORITY_DEFAULT,
                        GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkRecentFilesList:loading:
   *
   * %TRUE if files are being loaded
   */
  properties[PROP_LOADING] =
      g_param_spec_boolean ("loading",
                            P_("loading"),
                            P_("TRUE if files are being loaded"),
                            FALSE,
                            GTK_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_APPLICATION_ID] =
      g_param_spec_string ("application-id",
                           P_("Application ID"),
                           P_("Applicaiton ID"),
                           NULL,
                           GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, properties);
}

static void
gtk_recent_files_list_init (GtkRecentFilesList *self)
{
  GtkSettings *settings;
  GFile *file;

  self->items = g_sequence_new (g_object_unref);
  self->io_priority = G_PRIORITY_DEFAULT;

  settings = gtk_settings_get_default ();
  if (settings)
    {
      g_object_get (G_OBJECT (settings),
                    "gtk-recent-files-max-age", &self->max_age,
                    "gtk-recent-files-enabled", &self->enabled,
                    NULL);
     }
   else
     {
       self->max_age = 30;
       self->enabled = TRUE;
     }

  self->filename = g_build_filename (g_get_user_data_dir (), "recently-used.xbel", NULL);
  file = g_file_new_for_path (self->filename);
  self->monitor = g_file_monitor_file (file, G_FILE_MONITOR_NONE, NULL, NULL);
  g_signal_connect (self->monitor, "changed",
                    G_CALLBACK (bookmark_file_changed), self);
  g_object_unref (file);
}

static gboolean
gtk_recent_files_list_stop_loading (GtkRecentFilesList *self)
{
  if (self->cancellable == NULL)
    return FALSE;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  return TRUE;
}

static void
got_file_info (GObject      *source,
               GAsyncResult *res,
               gpointer      user_data)
{
  GtkRecentFilesList *self = user_data;
  GFile *file = G_FILE (source);
  GFileInfo *info;

  info = g_file_query_info_finish (file, res, NULL);
  if (info)
    {
      g_file_info_set_attribute_object (info, "standard::file", G_OBJECT (file));
      g_sequence_append (self->items, info);
      g_list_model_items_changed (G_LIST_MODEL (self), g_sequence_get_length (self->items) - 1, 0, 1);
    }
}

static void
gtk_recent_files_list_clear_items (GtkRecentFilesList *self)
{
  guint n_items;

  n_items = g_sequence_get_length (self->items);
  if (n_items > 0)
    {
      g_sequence_remove_range (g_sequence_get_begin_iter (self->items),
                               g_sequence_get_end_iter (self->items));

      g_list_model_items_changed (G_LIST_MODEL (self), 0, n_items, 0);
    }
}

static void
gtk_recent_files_list_start_loading (GtkRecentFilesList *self)
{
  gboolean was_loading;
  GBookmarkFile *bookmarks;

  was_loading = gtk_recent_files_list_stop_loading (self);
  gtk_recent_files_list_clear_items (self);

  if (!self->enabled)
    {
      if (was_loading)
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOADING]);
      return;
    }

  self->cancellable = g_cancellable_new ();

  bookmarks = g_bookmark_file_new ();

  if (g_bookmark_file_load_from_file (bookmarks, self->filename, NULL))
    {
      char **uris;
      gsize len;
      int i;
      time_t now;

      now = time (NULL);

      uris = g_bookmark_file_get_uris (bookmarks, &len);
      for (i = 0; i < len; i++)
        {
          const gchar *uri = uris[i];
          time_t modified;
          gint item_age;
          gboolean is_private;
          GFile *file;

          /* skip old items */
          modified = g_bookmark_file_get_modified (bookmarks, uri, NULL);
          item_age = (gint) ((now - modified) / (60 * 60 * 24));
          if (item_age > self->max_age)
            continue;

          /* skip private items */
          is_private = g_bookmark_file_get_is_private (bookmarks, uri, NULL);
          if (is_private)
            {
              char **apps;
              gsize app_len;

              if (!self->application_id)
                continue;

              apps = g_bookmark_file_get_applications (bookmarks, uri, &app_len, NULL);
              if (!g_strv_contains ((const char * const *)apps, self->application_id))
                {
                  g_strfreev (apps);
                  continue;
                }

              g_strfreev (apps);
            }

          /* add this item */
          file = g_file_new_for_uri (uri);
          g_file_query_info_async (file,
                                   self->attributes,
                                   0,
                                   self->io_priority,
                                   self->cancellable,
                                   got_file_info,
                                   self);
          g_object_unref (file);
        }
    }

  g_bookmark_file_free (bookmarks);

  if (was_loading)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOADING]);
}

static void
bookmark_file_changed (GFileMonitor      *monitor,
                       GFile             *file,
                       GFile             *other_file,
                       GFileMonitorEvent  event_type,
                       gpointer           data)
{
  GtkRecentFilesList *self = data;

  switch (event_type)
    {
    case G_FILE_MONITOR_EVENT_CHANGED:
    case G_FILE_MONITOR_EVENT_CREATED:
    case G_FILE_MONITOR_EVENT_DELETED:
      gtk_recent_files_list_start_loading (self);
      break;

    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
    case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
    case G_FILE_MONITOR_EVENT_PRE_UNMOUNT:
    case G_FILE_MONITOR_EVENT_UNMOUNTED:
    case G_FILE_MONITOR_EVENT_MOVED:
    case G_FILE_MONITOR_EVENT_RENAMED:
    case G_FILE_MONITOR_EVENT_MOVED_IN:
    case G_FILE_MONITOR_EVENT_MOVED_OUT:

    default:
      break;
    }

}

/**
 * gtk_recent_files_list_new:
 * @application_id: (allow-none): The application ID
 * @attributes: (allow-none): The attributes to query with
 *
 * Creates a new #GtkRecentFilesList for the given application
 * with the given @attributes.
 *
 * Returns: a new #GtkRecentFilesList
 **/
GtkRecentFilesList *
gtk_recent_files_list_new (const char *application_id,
                           const char *attributes)
{
  return g_object_new (GTK_TYPE_RECENT_FILES_LIST,
                       "application-id", application_id,
                       "attributes", attributes,
                       NULL);
}

/**
 * gtk_recent_files_list_set_attributes:
 * @self: a #GtkRecentFilesList
 * @attributes: (allow-none): the attributes to enumerate
 *
 * Sets the @attributes to be enumerated and starts the enumeration.
 *
 * If @attributes is %NULL, no attributes will be queried, but a list
 * of #GFileInfos will still be created.
 */
void
gtk_recent_files_list_set_attributes (GtkRecentFilesList *self,
                                      const char         *attributes)
{
  g_return_if_fail (GTK_IS_RECENT_FILES_LIST (self));

  if (g_strcmp0 (self->attributes, attributes) == 0)
    return;

  g_object_freeze_notify (G_OBJECT (self));

  g_free (self->attributes);
  self->attributes = g_strdup (attributes);

  gtk_recent_files_list_start_loading (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ATTRIBUTES]);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * gtk_recent_files_list_get_attributes:
 * @self: a #GtkRecentFilesList
 *
 * Gets the attributes queried on the children.
 *
 * Returns: (nullable) (transfer none): The queried attributes
 */
const char *
gtk_recent_files_list_get_attributes (GtkRecentFilesList *self)
{
  g_return_val_if_fail (GTK_IS_RECENT_FILES_LIST (self), NULL);

  return self->attributes;
}

/**
 * gtk_recent_files_list_set_application_id:
 * @self: a #GtkRecentFilesList
 * @application_id: (allow-none): the application ID
 *
 * Sets the application ID to filter private items by.
 *
 * If @application_id is %NULL, all private items will be skipped.
 */
void
gtk_recent_files_list_set_application_id (GtkRecentFilesList *self,
                                          const char         *application_id)
{
  g_return_if_fail (GTK_IS_RECENT_FILES_LIST (self));

  if (g_strcmp0 (self->application_id, application_id) == 0)
    return;

  g_object_freeze_notify (G_OBJECT (self));

  g_free (self->application_id);
  self->application_id = g_strdup (application_id);

  gtk_recent_files_list_start_loading (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_APPLICATION_ID]);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * gtk_recent_files_list_get_application_id:
 * @self: a #GtkRecentFilesList
 *
 * Gets the application ID for this recent files list.
 *
 * Returns: (nullable) (transfer none): The application ID
 */
const char *
gtk_recent_files_list_get_application_id (GtkRecentFilesList *self)
{
  g_return_val_if_fail (GTK_IS_RECENT_FILES_LIST (self), NULL);

  return self->application_id;
}

/**
 * gtk_recent_files_list_set_io_priority:
 * @self: a #GtkRecentFilesList
 * @io_priority: IO priority to use
 *
 * Sets the IO priority to use while loading files.
 *
 * The default IO priority is %G_PRIORITY_DEFAULT.
 */
void
gtk_recent_files_list_set_io_priority (GtkRecentFilesList *self,
                                       int                 io_priority)
{
  g_return_if_fail (GTK_IS_RECENT_FILES_LIST (self));

  if (self->io_priority == io_priority)
    return;

  self->io_priority = io_priority;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_IO_PRIORITY]);
}

/**
 * gtk_recent_files_list_get_io_priority:
 * @self: a #GtkRecentFilesList
 *
 * Gets the IO priority set via gtk_recent_files_list_set_io_priority().
 *
 * Returns: The IO priority.
 */
int
gtk_recent_files_list_get_io_priority (GtkRecentFilesList *self)
{
  g_return_val_if_fail (GTK_IS_RECENT_FILES_LIST (self), G_PRIORITY_DEFAULT);

  return self->io_priority;
}

/**
 * gtk_recent_files_list_is_loading:
 * @self: a #GtkRecentFilesList
 *
 * Returns %TRUE if the files are currently being loaded.
 *
 * Files will be added to @self from time to time while loading is
 * going on. The order in which are added is undefined and may change
 * inbetween runs.
 *
 * Returns: %TRUE if @self is loading
 */
gboolean
gtk_recent_files_list_is_loading (GtkRecentFilesList *self)
{
  g_return_val_if_fail (GTK_IS_RECENT_FILES_LIST (self), FALSE);

  return self->cancellable != NULL;
}
