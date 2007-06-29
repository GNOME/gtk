/* GTK - The GIMP Toolkit
 * gtkrecentmanager.c: a manager for the recently used resources
 *
 * Copyright (C) 2006 Emmanuele Bassi
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gstdio.h>

#include "gtkrecentmanager.h"
#include "gtkintl.h"
#include "gtkstock.h"
#include "gtkicontheme.h"
#include "gtktypebuiltins.h"
#include "gtkprivate.h"
#include "gtkmarshalers.h"
#include "gtkalias.h"

#ifdef G_OS_UNIX
#define XDG_PREFIX _gtk_xdg
#include "xdgmime/xdgmime.h"
#endif

/* the file where we store the recently used items */
#define GTK_RECENTLY_USED_FILE	".recently-used.xbel"

/* a poll every two seconds should be enough */
#define POLL_DELTA	2000

/* return all items by default */
#define DEFAULT_LIMIT	-1

/* keep in sync with xdgmime */
#define GTK_RECENT_DEFAULT_MIME	"application/octet-stream"

typedef struct
{
  gchar *name;
  gchar *exec;
  
  guint count;
  
  time_t stamp;
} RecentAppInfo;

struct _GtkRecentInfo
{
  gchar *uri;
  
  gchar *display_name;
  gchar *description;
  
  time_t added;
  time_t modified;
  time_t visited;
  
  gchar *mime_type;
  
  GSList *applications;
  GHashTable *apps_lookup;
  
  GSList *groups;
  
  gboolean is_private;
  
  GdkPixbuf *icon;
  
  gint ref_count;
};

struct _GtkRecentManagerPrivate
{
  gchar *filename;

  guint is_screen_singleton : 1;
  guint is_dirty : 1;
  guint write_in_progress : 1;
  guint read_in_progress : 1;
  
  gint limit;
  gint size;

  GdkScreen *screen;
  
  GBookmarkFile *recent_items;
  
  time_t last_mtime;
  guint poll_timeout;
};

enum
{
  PROP_0,

  PROP_FILENAME,  
  PROP_LIMIT,
  PROP_SIZE
};

static void           gtk_recent_manager_finalize     (GObject               *object);

static void           gtk_recent_manager_set_property (GObject               *object,
						       guint                  prop_id,
						       const GValue          *value,
						       GParamSpec            *pspec);
static void           gtk_recent_manager_get_property (GObject               *object,
						       guint                  prop_id,
						       GValue                *value,
						       GParamSpec            *pspec);
static void           gtk_recent_manager_changed      (GtkRecentManager      *manager);

static void           gtk_recent_manager_real_changed (GtkRecentManager      *manager);
static gboolean       gtk_recent_manager_poll_timeout (gpointer               data);
static void           gtk_recent_manager_set_filename (GtkRecentManager      *manager,
						       const gchar           *filename);

static void           build_recent_items_list         (GtkRecentManager      *manager);
static void           purge_recent_items_list         (GtkRecentManager      *manager,
						       GError               **error);

static RecentAppInfo *recent_app_info_new             (const gchar           *app_name);
static void           recent_app_info_free            (RecentAppInfo         *app_info);

static GtkRecentInfo *gtk_recent_info_new             (const gchar           *uri);
static void           gtk_recent_info_free            (GtkRecentInfo         *recent_info);

static guint signal_changed = 0;

G_DEFINE_TYPE (GtkRecentManager, gtk_recent_manager, G_TYPE_OBJECT)

static void
filename_warning (const gchar *format, 
                  const gchar *filename, 
                  const gchar *message)
{
  gchar *utf8 = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
  g_warning (format, utf8 ? utf8 : "(invalid filename)", message);
  g_free (utf8);
}

/* Test of haystack has the needle prefix, comparing case
 * insensitive. haystack may be UTF-8, but needle must
 * contain only lowercase ascii. */
static gboolean
has_case_prefix (const gchar *haystack, 
                 const gchar *needle)
{
  const gchar *h, *n;

  /* Eat one character at a time. */
  h = haystack;
  n = needle;

  while (*n && *h && *n == g_ascii_tolower (*h))
    {
      n++;
      h++;
    }

  return *n == '\0';
}

GQuark
gtk_recent_manager_error_quark (void)
{
  return g_quark_from_static_string ("gtk-recent-manager-error-quark");
}


static void
gtk_recent_manager_class_init (GtkRecentManagerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  
  gtk_recent_manager_parent_class = g_type_class_peek_parent (klass);
  
  gobject_class->set_property = gtk_recent_manager_set_property;
  gobject_class->get_property = gtk_recent_manager_get_property;
  gobject_class->finalize = gtk_recent_manager_finalize;
  
  /**
   * GtkRecentManager:filename
   *
   * The full path to the file to be used to store and read the recently
   * used resources list
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
		  		   PROP_FILENAME,
				   g_param_spec_string ("filename",
					   		P_("Filename"),
							P_("The full path to the file to be used to store and read the list"),
							NULL,
							(G_PARAM_CONSTRUCT_ONLY | G_PARAM_READABLE | G_PARAM_WRITABLE)));
  /**
   * GtkRecentManager:limit
   *
   * The maximum number of items to be returned by the
   * gtk_recent_manager_get_items() function.
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
  				   PROP_LIMIT,
  				   g_param_spec_int ("limit",
  				   		     P_("Limit"),
  				   		     P_("The maximum number of items to be returned by gtk_recent_manager_get_items()"),
  				   		     -1,
  				   		     G_MAXINT,
  				   		     DEFAULT_LIMIT,
  				   		     G_PARAM_READWRITE));
  /**
   * GtkRecentManager:size
   * 
   * The size of the recently used resources list.
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
		  		   PROP_SIZE,
				   g_param_spec_int ("size",
					   	     P_("Size"),
						     P_("The size of the recently used resources list"),
						     -1,
						     G_MAXINT,
						     0,
						     G_PARAM_READABLE));
  
  /**
   * GtkRecentManager::changed
   * @recent_manager: the recent manager
   *
   * Emitted when the current recently used resources manager changes its
   * contents.
   *
   * Since: 2.10
   */
  signal_changed =
    g_signal_new (I_("changed"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkRecentManagerClass, changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  
  klass->changed = gtk_recent_manager_real_changed;
  
  g_type_class_add_private (klass, sizeof (GtkRecentManagerPrivate));
}

static void
gtk_recent_manager_init (GtkRecentManager *manager)
{
  GtkRecentManagerPrivate *priv;
  
  priv = g_type_instance_get_private ((GTypeInstance *) manager,
  				      GTK_TYPE_RECENT_MANAGER);
  manager->priv = priv;
  
  priv->limit = DEFAULT_LIMIT;
  priv->size = 0;
  
  priv->is_screen_singleton = FALSE;
  priv->is_dirty = FALSE;
  priv->write_in_progress = FALSE;
  priv->read_in_progress = FALSE;

  priv->screen = NULL;

  priv->filename = g_build_filename (g_get_home_dir (),
				     GTK_RECENTLY_USED_FILE,
				     NULL);
  priv->poll_timeout = g_timeout_add (POLL_DELTA,
		  		      gtk_recent_manager_poll_timeout,
				      manager);

  build_recent_items_list (manager);
}

static void
gtk_recent_manager_set_property (GObject               *object,
				 guint                  prop_id,
				 const GValue          *value,
				 GParamSpec            *pspec)
{
  GtkRecentManager *recent_manager = GTK_RECENT_MANAGER (object);
 
  switch (prop_id)
    {
    case PROP_FILENAME:
      gtk_recent_manager_set_filename (recent_manager, g_value_get_string (value));
      break;      
    case PROP_LIMIT:
      gtk_recent_manager_set_limit (recent_manager, g_value_get_int (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_recent_manager_get_property (GObject               *object,
				 guint                  prop_id,
				 GValue                *value,
				 GParamSpec            *pspec)
{
  GtkRecentManager *recent_manager = GTK_RECENT_MANAGER (object);
  
  switch (prop_id)
    {
    case PROP_FILENAME:
      g_value_set_string (value, recent_manager->priv->filename);
      break;
    case PROP_LIMIT:
      g_value_set_int (value, recent_manager->priv->limit);
      break;
    case PROP_SIZE:
      g_value_set_int (value, recent_manager->priv->size);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
} 

static void
gtk_recent_manager_finalize (GObject *object)
{
  GtkRecentManager *manager = GTK_RECENT_MANAGER (object);
  GtkRecentManagerPrivate *priv = manager->priv;

  /* remove the poll timeout */
  if (priv->poll_timeout)
    g_source_remove (priv->poll_timeout);
  
  if (priv->filename)
    g_free (priv->filename);
  
  if (priv->recent_items)
    g_bookmark_file_free (priv->recent_items);
  
  /* chain up parent's finalize method */  
  G_OBJECT_CLASS (gtk_recent_manager_parent_class)->finalize (object);
}

static void
gtk_recent_manager_real_changed (GtkRecentManager *manager)
{
  GtkRecentManagerPrivate *priv = manager->priv;

  g_object_freeze_notify (G_OBJECT (manager));

  if (priv->is_dirty)
    {
      GError *write_error;
      struct stat stat_buf;
      
      /* we are marked as dirty, so we dump the content of our
       * recently used items list
       */
      g_assert (priv->filename != NULL);

      priv->write_in_progress = TRUE;

      /* if no container object has been defined, we create a new
       * empty container, and dump it
       */
      if (!priv->recent_items)
        {
          priv->recent_items = g_bookmark_file_new ();
	  priv->size = 0;
	}

      write_error = NULL;
      g_bookmark_file_to_file (priv->recent_items,
		               priv->filename,
			       &write_error);

      if (write_error)
        {
          filename_warning ("Attempting to store changes into `%s', "
			    "but failed: %s",
			    priv->filename,
			    write_error->message);
	  g_error_free (write_error);
	}

      priv->write_in_progress = FALSE;
	  
      /* we have sync'ed our list with the storage file, so we
       * update the file mtime in order to skip the timed check
       * and spare us from a re-read.
       */
      if (g_stat (priv->filename, &stat_buf) < 0)
	{
          filename_warning ("Unable to stat() the recently used resources file "
			    "at `%s': %s.",
			    priv->filename,
			    g_strerror (errno));

	  g_object_thaw_notify (G_OBJECT (manager));

	  return;
	}
      
      priv->last_mtime = stat_buf.st_mtime;
      
      /* mark us as clean */
      priv->is_dirty = FALSE;
    }
  else
    {
      /* we are not marked as dirty, so we have been called
       * because the recently used resources file has been
       * changed (and not from us).
       */
      build_recent_items_list (manager);
    }

  g_object_thaw_notify (G_OBJECT (manager));
}

/* timed poll()-ing of the recently used resources file.
 * an event-based system would be more efficient.
 */
static gboolean
gtk_recent_manager_poll_timeout (gpointer data)
{
  GtkRecentManager *manager = GTK_RECENT_MANAGER (data);
  GtkRecentManagerPrivate *priv = manager->priv;
  struct stat stat_buf;
  int stat_res;

  /* wait for the next timeout if we have a read/write in progress */
  if (priv->write_in_progress || priv->read_in_progress)
    return TRUE;

  stat_res = g_stat (priv->filename, &stat_buf);
  if (stat_res < 0)
    {
      /* the file does not exist, yet, so we wait */
      if (errno == ENOENT)
        return TRUE;
      
      filename_warning ("Unable to stat() the recently used resources file "
			"at `%s': %s.",
			priv->filename,
			g_strerror (errno));
      
      return TRUE;
    }

  /* the file didn't change from the last poll(), so we bail out */
  if (stat_buf.st_mtime == priv->last_mtime)
    return TRUE;

  /* the file has been changed, hence we emit the "changed" signal */
  gtk_recent_manager_changed (manager);

  return TRUE;
}

static void
gtk_recent_manager_set_filename (GtkRecentManager *manager,
				 const gchar      *filename)
{
  GtkRecentManagerPrivate *priv;
  
  g_assert (GTK_IS_RECENT_MANAGER (manager));
  priv = manager->priv;
  
  if (!filename || filename[0] == '\0')
    return;
  
  g_free (manager->priv->filename);

  if (manager->priv->poll_timeout)
    {
      g_source_remove (manager->priv->poll_timeout);
      manager->priv->poll_timeout = 0;
    }

  priv->filename = g_strdup (filename);
  priv->poll_timeout = g_timeout_add (POLL_DELTA,
		  		      gtk_recent_manager_poll_timeout,
				      manager);

  /* mark us clean, so that we can re-read the list
   * of recently used resources
   */
  priv->is_dirty = FALSE;
  build_recent_items_list (manager);
}

/* reads the recently used resources file and builds the items list.
 * we keep the items list inside the parser object, and build the
 * RecentInfo object only on user's demand to avoid useless replication.
 * this function resets the dirty bit of the manager.
 */
static void
build_recent_items_list (GtkRecentManager *manager)
{
  GtkRecentManagerPrivate *priv;
  struct stat stat_buf;
  int stat_res;
  gboolean res;
  GError *read_error;
  gint size;

  priv = manager->priv;
  g_assert (priv->filename != NULL);
  
  if (!priv->recent_items)
    {
      priv->recent_items = g_bookmark_file_new ();
      priv->size = 0;
    }

  stat_res = g_stat (priv->filename, &stat_buf);
  if (stat_res < 0)
    {
      /* the file doesn't exists, so we bail out and wait for the first
       * write operation
       */

      if (errno == ENOENT)
        return;
      else
        {
          filename_warning ("Attempting to read the recently used resources file "
			    "at `%s', but an error occurred: %s. Aborting.",
			    priv->filename,
			    g_strerror (errno));

          return;
        }
    }

  /* record the last mtime, for later use */
  priv->last_mtime = stat_buf.st_mtime;
  
  priv->read_in_progress = TRUE;

  /* the file exists, and it's valid (we hope); if not, destroy the container
   * object and hope for a better result when the next "changed" signal is
   * fired. */
  read_error = NULL;
  res = g_bookmark_file_load_from_file (priv->recent_items,
		  			priv->filename,
					&read_error);
  if (read_error)
    {
      filename_warning ("Attempting to read the recently used resources file "
			"at `%s', but the parser failed: %s.",
			priv->filename,
			read_error->message);

      g_bookmark_file_free (priv->recent_items);
      priv->recent_items = NULL;

      g_error_free (read_error);
    }

  size = g_bookmark_file_get_size (priv->recent_items);
  if (priv->size != size)
    {
      priv->size = size;
      
      g_object_notify (G_OBJECT (manager), "size");
    }

  priv->read_in_progress = FALSE;
  priv->is_dirty = FALSE;
}


/********************
 * GtkRecentManager *
 ********************/


/**
 * gtk_recent_manager_new:
 * 
 * Creates a new recent manager object.  Recent manager objects are used to
 * handle the list of recently used resources.  A #GtkRecentManager object
 * monitors the recently used resources list, and emits the "changed" signal
 * each time something inside the list changes.
 *
 * #GtkRecentManager objects are expensive: be sure to create them only when
 * needed. You should use the gtk_recent_manager_new_for_screen() or the
 * gtk_recent_manager_get_default() functions instead.
 *
 * Return value: A newly created #GtkRecentManager object.
 *
 * Since: 2.10
 */
GtkRecentManager *
gtk_recent_manager_new (void)
{
  return g_object_new (GTK_TYPE_RECENT_MANAGER, NULL);
}

/**
 * gtk_recent_manager_get_default:
 *
 * Gets the recent manager for the default screen. See
 * gtk_recent_manager_get_for_screen().
 *
 * Return value: A unique #GtkRecentManager associated with the
 *   default screen. This recent manager is associated with the
 *   screen and can be used as long as the screen is open.
 *   Do not ref or unref it.
 *
 * Since: 2.10
 */
GtkRecentManager *
gtk_recent_manager_get_default (void)
{
  return gtk_recent_manager_get_for_screen (gdk_screen_get_default ());
}

/**
 * gtk_recent_manager_get_for_screen:
 * @screen: a #GdkScreen
 *
 * Gets the recent manager object associated with @screen; if this
 * function has not previously been called for the given screen,
 * a new recent manager object will be created and associated with
 * the screen. Recent manager objects are fairly expensive to create,
 * so using this function is usually a better choice than calling 
 * gtk_recent_manager_new() and setting the screen yourself; by using
 * this function a single recent manager object will be shared between
 * users.
 *
 * Return value: A unique #GtkRecentManager associated with the given
 *   screen. This recent manager is associated to the with the screen
 *   and can be used as long as the screen is open. Do not ref or
 *   unref it.
 *
 * Since: 2.10
 */
GtkRecentManager *
gtk_recent_manager_get_for_screen (GdkScreen *screen)
{
  GtkRecentManager *manager;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  g_return_val_if_fail (!screen->closed, NULL);

  manager = g_object_get_data (G_OBJECT (screen), "gtk-recent-manager-default");
  if (!manager)
    {
      GtkRecentManagerPrivate *priv;
      
      manager = gtk_recent_manager_new ();
      gtk_recent_manager_set_screen (manager, screen);

      priv = manager->priv;
      priv->is_screen_singleton = TRUE;

      g_object_set_data (G_OBJECT (screen), I_("gtk-recent-manager-default"), manager);
    }

  return manager;
}

static void
display_closed (GdkDisplay       *display,
		gboolean          is_error,
		GtkRecentManager *manager)
{
  GtkRecentManagerPrivate *priv = manager->priv;
  GdkScreen *screen = priv->screen;
  gboolean was_screen_singleton = priv->is_screen_singleton;

  if (was_screen_singleton)
    {
      g_object_set_data (G_OBJECT (screen), I_("gtk-recent-manager-default"), NULL);
      priv->is_screen_singleton = FALSE;
    }

  gtk_recent_manager_set_screen (manager, NULL);

  if (was_screen_singleton)
    g_object_unref (manager);
}

static void
unset_screen (GtkRecentManager *manager)
{
  GtkRecentManagerPrivate *priv = manager->priv;
  GdkDisplay *display;

  if (priv->screen)
    {
      display = gdk_screen_get_display (priv->screen);

      g_signal_handlers_disconnect_by_func (display,
					    (gpointer) display_closed,
					    manager);

      priv->screen = NULL;
    }
}

/**
 * gtk_recent_manager_set_screen:
 * @manager: a #GtkRecentManager
 * @screen: a #GdkScreen
 *
 * Sets the screen for a recent manager; the screen is used to
 * track the user's currently configured recently used documents
 * storage.
 * 
 * Since: 2.10
 */
void
gtk_recent_manager_set_screen (GtkRecentManager *manager,
			       GdkScreen        *screen)
{
  GtkRecentManagerPrivate *priv;
  GdkDisplay *display;

  g_return_if_fail (GTK_IS_RECENT_MANAGER (manager));
  g_return_if_fail (screen == NULL || GDK_IS_SCREEN (screen));

  priv = manager->priv;

  unset_screen (manager);

  if (screen)
    {
      display = gdk_screen_get_display (screen);

      priv->screen = screen;

      g_signal_connect (display, "closed",
		        G_CALLBACK (display_closed), manager);
    }
}

/**
 * gtk_recent_manager_set_limit:
 * @manager: a #GtkRecentManager
 * @limit: the maximum number of items to return, or -1.
 *
 * Sets the maximum number of item that the gtk_recent_manager_get_items()
 * function should return.  If @limit is set to -1, then return all the
 * items.
 *
 * Since: 2.10
 */
void
gtk_recent_manager_set_limit (GtkRecentManager *manager,
			      gint              limit)
{
  GtkRecentManagerPrivate *priv;
  
  g_return_if_fail (GTK_IS_RECENT_MANAGER (manager));
  
  priv = manager->priv;
  priv->limit = limit;
}

/**
 * gtk_recent_manager_get_limit:
 * @manager: a #GtkRecentManager
 *
 * Gets the maximum number of items that the gtk_recent_manager_get_items()
 * function should return.
 *
 * Return value: the number of items to return, or -1 for every item.
 *
 * Since: 2.10
 */
gint
gtk_recent_manager_get_limit (GtkRecentManager *manager)
{
  GtkRecentManagerPrivate *priv;
  
  g_return_val_if_fail (GTK_IS_RECENT_MANAGER (manager), DEFAULT_LIMIT);
  
  priv = manager->priv;
  return priv->limit;
}

/**
 * gtk_recent_manager_add_item:
 * @manager: a #GtkRecentManager
 * @uri: a valid URI
 *
 * Adds a new resource, pointed by @uri, into the recently used
 * resources list.
 *
 * This function automatically retrieving some of the needed
 * metadata and setting other metadata to common default values; it
 * then feeds the data to gtk_recent_manager_add_full().
 *
 * See gtk_recent_manager_add_full() if you want to explicitely
 * define the metadata for the resource pointed by @uri.
 *
 * Return value: %TRUE if the new item was successfully added
 *   to the recently used resources list
 *
 * Since: 2.10
 */
gboolean
gtk_recent_manager_add_item (GtkRecentManager  *manager,
			     const gchar       *uri)
{
  GtkRecentData recent_data;
  gboolean retval;
  
  g_return_val_if_fail (GTK_IS_RECENT_MANAGER (manager), FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);

  recent_data.display_name = NULL;
  recent_data.description = NULL;
  recent_data.mime_type = NULL;
  
#ifdef G_OS_UNIX
  if (has_case_prefix (uri, "file:/"))
    {
      gchar *filename;
      const gchar *mime_type;
      
      filename = g_filename_from_uri (uri, NULL, NULL);
      if (filename)
        {
          mime_type = xdg_mime_get_mime_type_for_file (filename, NULL);
          if (mime_type && *mime_type)
            recent_data.mime_type = g_strdup (mime_type);

          g_free (filename);
        }

      if (!recent_data.mime_type)
        recent_data.mime_type = g_strdup (GTK_RECENT_DEFAULT_MIME);
    }
  else
#endif
    recent_data.mime_type = g_strdup (GTK_RECENT_DEFAULT_MIME);
  
  recent_data.app_name = g_strdup (g_get_application_name ());
  recent_data.app_exec = g_strjoin (" ", g_get_prgname (), "%u", NULL);
  recent_data.groups = NULL;
  recent_data.is_private = FALSE;
  
  retval = gtk_recent_manager_add_full (manager, uri, &recent_data);
  
  g_free (recent_data.mime_type);
  g_free (recent_data.app_name);
  g_free (recent_data.app_exec);

  return retval;
}

/**
 * gtk_recent_manager_add_full:
 * @manager: a #GtkRecentManager
 * @uri: a valid URI
 * @recent_data: metadata of the resource
 *
 * Adds a new resource, pointed by @uri, into the recently used
 * resources list, using the metadata specified inside the #GtkRecentData
 * structure passed in @recent_data.
 *
 * The passed URI will be used to identify this resource inside the
 * list.
 *
 * In order to register the new recently used resource, metadata about
 * the resource must be passed as well as the URI; the metadata is
 * stored in a #GtkRecentData structure, which must contain the MIME
 * type of the resource pointed by the URI; the name of the application
 * that is registering the item, and a command line to be used when
 * launching the item.
 *
 * Optionally, a #GtkRecentData structure might contain a UTF-8 string
 * to be used when viewing the item instead of the last component of the
 * URI; a short description of the item; whether the item should be
 * considered private - that is, should be displayed only by the
 * applications that have registered it.
 *
 * Return value: %TRUE if the new item was successfully added to the
 * recently used resources list, %FALSE otherwise.
 *
 * Since: 2.10
 */
gboolean
gtk_recent_manager_add_full (GtkRecentManager     *manager,
			     const gchar          *uri,
			     const GtkRecentData  *data)
{
  GtkRecentManagerPrivate *priv;
  
  g_return_val_if_fail (GTK_IS_RECENT_MANAGER (manager), FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);
  g_return_val_if_fail (data != NULL, FALSE);

  /* sanity checks */
  if ((data->display_name) &&
      (!g_utf8_validate (data->display_name, -1, NULL)))
    {
      g_warning ("Attempting to add `%s' to the list of recently used "
		 "resources, but the display name is not a valid UTF-8 "
          	 "encoded string",
		 uri);
      return FALSE;
    }
  
  if ((data->description) &&
      (!g_utf8_validate (data->description, -1, NULL)))
    {
      g_warning ("Attempting to add `%s' to the list of recently used "
		 "resources, but the description is not a valid UTF-8 "
          	 "encoded string",
		 uri);
      return FALSE;
    }

 
  if (!data->mime_type)
    {
      g_warning ("Attempting to add `%s' to the list of recently used "
		 "resources, but not MIME type was defined",
		 uri);
      return FALSE;
    }
  
  if (!data->app_name)
    {
      g_warning ("Attempting to add `%s' to the list of recently used "
		 "resources, but no name of the application that is "
		 "registering it was defined",
      		 uri);
      return FALSE;
    }
  
  if (!data->app_exec)
    {
      g_warning ("Attempting to add `%s' to the list of recently used "
		 "resources, but no command line for the application "
		 "that is registering it was defined",
		 uri);
      return FALSE;
    }
  
  priv = manager->priv;

  if (!priv->recent_items)
    {
      priv->recent_items = g_bookmark_file_new ();
      priv->size = 0;
    }

  if (data->display_name)  
    g_bookmark_file_set_title (priv->recent_items, uri, data->display_name);
  
  if (data->description)
    g_bookmark_file_set_description (priv->recent_items, uri, data->description);

  g_bookmark_file_set_mime_type (priv->recent_items, uri, data->mime_type);
  
  if (data->groups && data->groups[0] != '\0')
    {
      gint j;
      
      for (j = 0; (data->groups)[j] != NULL; j++)
        g_bookmark_file_add_group (priv->recent_items, uri, (data->groups)[j]);
    }
  
  /* register the application; this will take care of updating the
   * registration count and time in case the application has
   * already registered the same document inside the list
   */
  g_bookmark_file_add_application (priv->recent_items, uri,
		  		   data->app_name,
				   data->app_exec);
  
  g_bookmark_file_set_is_private (priv->recent_items, uri,
		  		  data->is_private);
  
  /* mark us as dirty, so that when emitting the "changed" signal we
   * will dump our changes
   */
  priv->is_dirty = TRUE;
  
  gtk_recent_manager_changed (manager);
  
  return TRUE;
}

/**
 * gtk_recent_manager_remove_item:
 * @manager: a #GtkRecentManager
 * @uri: the URI of the item you wish to remove
 * @error: return location for a #GError, or %NULL
 *
 * Removes a resource pointed by @uri from the recently used resources
 * list handled by a recent manager.
 *
 * Return value: %TRUE if the item pointed by @uri has been successfully
 *   removed by the recently used resources list, and %FALSE otherwise.
 *
 * Since: 2.10
 */
gboolean
gtk_recent_manager_remove_item (GtkRecentManager  *manager,
				const gchar       *uri,
				GError           **error)
{
  GtkRecentManagerPrivate *priv;
  GError *remove_error = NULL;

  g_return_val_if_fail (GTK_IS_RECENT_MANAGER (manager), FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  
  priv = manager->priv;
  
  if (!priv->recent_items)
    {
      priv->recent_items = g_bookmark_file_new ();
      priv->size = 0;

      g_set_error (error, GTK_RECENT_MANAGER_ERROR,
		   GTK_RECENT_MANAGER_ERROR_NOT_FOUND,
		   _("Unable to find an item with URI '%s'"),
		   uri);

      return FALSE;
    }

  g_bookmark_file_remove_item (priv->recent_items, uri, &remove_error);
  if (remove_error)
    {
      g_propagate_error (error, remove_error);
      
      return FALSE;
    }

  priv->is_dirty = TRUE;

  gtk_recent_manager_changed (manager);
  
  return TRUE;
}

/**
 * gtk_recent_manager_has_item:
 * @manager: a #GtkRecentManager
 * @uri: a URI
 *
 * Checks whether there is a recently used resource registered
 * with @uri inside the recent manager.
 *
 * Return value: %TRUE if the resource was found, %FALSE otherwise.
 *
 * Since: 2.10
 */
gboolean
gtk_recent_manager_has_item (GtkRecentManager *manager,
			     const gchar      *uri)
{
  GtkRecentManagerPrivate *priv;

  g_return_val_if_fail (GTK_IS_RECENT_MANAGER (manager), FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);

  priv = manager->priv;
  g_return_val_if_fail (priv->recent_items != NULL, FALSE);

  return g_bookmark_file_has_item (priv->recent_items, uri);
}

static gboolean
build_recent_info (GBookmarkFile  *bookmarks,
		   GtkRecentInfo  *info)
{
  gchar **apps, **groups;
  gsize apps_len, groups_len, i;

  g_assert (bookmarks != NULL);
  g_assert (info != NULL);
  
  info->display_name = g_bookmark_file_get_title (bookmarks, info->uri, NULL);
  info->description = g_bookmark_file_get_description (bookmarks, info->uri, NULL);
  info->mime_type = g_bookmark_file_get_mime_type (bookmarks, info->uri, NULL);
    
  info->is_private = g_bookmark_file_get_is_private (bookmarks, info->uri, NULL);
  
  info->added = g_bookmark_file_get_added (bookmarks, info->uri, NULL);
  info->modified = g_bookmark_file_get_modified (bookmarks, info->uri, NULL);
  info->visited = g_bookmark_file_get_visited (bookmarks, info->uri, NULL);
  
  groups = g_bookmark_file_get_groups (bookmarks, info->uri, &groups_len, NULL);
  for (i = 0; i < groups_len; i++)
    {
      gchar *group_name = g_strdup (groups[i]);
      
      info->groups = g_slist_append (info->groups, group_name);
    }

  g_strfreev (groups);
  
  apps = g_bookmark_file_get_applications (bookmarks, info->uri, &apps_len, NULL);
  for (i = 0; i < apps_len; i++)
    {
      gchar *app_name, *app_exec;
      guint count;
      time_t stamp;
      RecentAppInfo *app_info;
      gboolean res;
      
      app_name = apps[i];
      
      res = g_bookmark_file_get_app_info (bookmarks, info->uri, app_name,
      					  &app_exec,
      					  &count,
      					  &stamp,
      					  NULL);
      if (!res)
        continue;
      
      app_info = recent_app_info_new (app_name);
      app_info->exec = app_exec;
      app_info->count = count;
      app_info->stamp = stamp;
      
      info->applications = g_slist_append (info->applications,
      					   app_info);
      g_hash_table_replace (info->apps_lookup, app_info->name, app_info);
    }
  
  g_strfreev (apps);
  
  return TRUE; 
}

/**
 * gtk_recent_manager_lookup_item:
 * @manager: a #GtkRecentManager
 * @uri: a URI
 * @error: a return location for a #GError, or %NULL
 *
 * Searches for a URI inside the recently used resources list, and
 * returns a structure containing informations about the resource
 * like its MIME type, or its display name.
 *
 * Return value: a #GtkRecentInfo structure containing information
 *   about the resource pointed by @uri, or %NULL if the URI was
 *   not registered in the recently used resources list.  Free with
 *   gtk_recent_info_unref().
 *
 * Since: 2.10
 */
GtkRecentInfo *
gtk_recent_manager_lookup_item (GtkRecentManager  *manager,
				const gchar       *uri,
				GError           **error)
{
  GtkRecentManagerPrivate *priv;
  GtkRecentInfo *info = NULL;
  gboolean res;
  
  g_return_val_if_fail (GTK_IS_RECENT_MANAGER (manager), NULL);
  g_return_val_if_fail (uri != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  
  priv = manager->priv;
  if (!priv->recent_items)
    {
      priv->recent_items = g_bookmark_file_new ();
      priv->size = 0;

      g_set_error (error, GTK_RECENT_MANAGER_ERROR,
		   GTK_RECENT_MANAGER_ERROR_NOT_FOUND,
		   _("Unable to find an item with URI '%s'"),
		   uri);

      return NULL;
    }
  
  if (!g_bookmark_file_has_item (priv->recent_items, uri))
    {
      g_set_error (error, GTK_RECENT_MANAGER_ERROR,
      		   GTK_RECENT_MANAGER_ERROR_NOT_FOUND,
      		   _("Unable to find an item with URI '%s'"),
      		   uri);
      return NULL;
    }
  
  info = gtk_recent_info_new (uri);
  g_return_val_if_fail (info != NULL, NULL);
  
  /* fill the RecentInfo structure with the data retrieved by our
   * parser object from the storage file 
   */
  res = build_recent_info (priv->recent_items, info);
  if (!res)
    {
      gtk_recent_info_free (info);
      
      return NULL;
    }
 
  return info;
}

/**
 * gtk_recent_manager_move_item:
 * @manager: a #GtkRecentManager
 * @uri: the URI of a recently used resource
 * @new_uri: the new URI of the recently used resource, or %NULL to
 *    remove the item pointed by @uri in the list
 * @error: a return location for a #GError, or %NULL
 *
 * Changes the location of a recently used resource from @uri to @new_uri.
 * 
 * Please note that this function will not affect the resource pointed
 * by the URIs, but only the URI used in the recently used resources list.
 *
 * Return value: %TRUE on success.
 *
 * Since: 2.10
 */ 
gboolean
gtk_recent_manager_move_item (GtkRecentManager  *recent_manager,
			      const gchar       *uri,
			      const gchar       *new_uri,
			      GError           **error)
{
  GtkRecentManagerPrivate *priv;
  GError *move_error;
  gboolean res;
  
  g_return_val_if_fail (GTK_IS_RECENT_MANAGER (recent_manager), FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  
  priv = recent_manager->priv;

  if (!g_bookmark_file_has_item (priv->recent_items, uri))
    {
      g_set_error (error, GTK_RECENT_MANAGER_ERROR,
      		   GTK_RECENT_MANAGER_ERROR_NOT_FOUND,
      		   _("Unable to find an item with URI '%s'"),
      		   uri);
      return FALSE;
    }
  
  move_error = NULL;
  res = g_bookmark_file_move_item (priv->recent_items,
                                   uri, new_uri,
                                   &move_error);
  if (move_error)
    {
      g_propagate_error (error, move_error);
      return FALSE;
    }
  
  priv->is_dirty = TRUE;

  gtk_recent_manager_changed (recent_manager);
  
  return TRUE;
}

/**
 * gtk_recent_manager_get_items:
 * @manager: a #GtkRecentManager
 *
 * Gets the list of recently used resources.
 *
 * Return value: a list of newly allocated #GtkRecentInfo objects. Use
 *   gtk_recent_info_unref() on each item inside the list, and then
 *   free the list itself using g_list_free().
 *
 * Since: 2.10
 */
GList *
gtk_recent_manager_get_items (GtkRecentManager *manager)
{
  GtkRecentManagerPrivate *priv;
  GList *retval = NULL;
  gchar **uris;
  gsize uris_len, i;
  
  g_return_val_if_fail (GTK_IS_RECENT_MANAGER (manager), NULL);
  
  priv = manager->priv;
  if (!priv->recent_items)
    return NULL;

  if (priv->limit == 0)
    return NULL;
  
  uris = g_bookmark_file_get_uris (priv->recent_items, &uris_len);
  for (i = 0; i < uris_len; i++)
    {
      GtkRecentInfo *info;
      gboolean res;
      
      info = gtk_recent_info_new (uris[i]);
      res = build_recent_info (priv->recent_items, info);
      if (!res)
        {
          g_warning ("Unable to create a RecentInfo object for "
                     "item with URI `%s'",
                     uris[i]);
          gtk_recent_info_free (info);
	  
          continue;
        }
      
      retval = g_list_prepend (retval, info);
    }
  
  g_strfreev (uris);
    
  /* clamp the list, if a limit is present */
  if ((priv->limit != -1) &&
      (g_list_length (retval) > priv->limit))
    {
      GList *clamp, *l;
      
      clamp = g_list_nth (retval, priv->limit - 1);
      
      if (!clamp)
        return retval;
      
      l = clamp->next;
      clamp->next = NULL;
      
      g_list_foreach (l, (GFunc) gtk_recent_info_free, NULL);
      g_list_free (l);
    }
  
  return retval;
}

static void
purge_recent_items_list (GtkRecentManager  *manager,
			 GError           **error)
{
  GtkRecentManagerPrivate *priv = manager->priv;

  if (!priv->recent_items)
    return;
  
  g_bookmark_file_free (priv->recent_items);
  priv->recent_items = NULL;
      
  priv->recent_items = g_bookmark_file_new ();
  priv->size = 0;
  priv->is_dirty = TRUE;
      
  /* emit the changed signal, to ensure that the purge is written */
  gtk_recent_manager_changed (manager);
}

/**
 * gtk_recent_manager_purge_items:
 * @manager: a #GtkRecentManager
 * @error: a return location for a #GError, or %NULL
 *
 * Purges every item from the recently used resources list.
 *
 * Return value: the number of items that have been removed from the
 *   recently used resources list.
 *
 * Since: 2.10
 */
gint
gtk_recent_manager_purge_items (GtkRecentManager  *manager,
				GError           **error)
{
  GtkRecentManagerPrivate *priv;
  gint count, purged;
  
  g_return_val_if_fail (GTK_IS_RECENT_MANAGER (manager), -1);

  priv = manager->priv;
  if (!priv->recent_items)
    return 0;
  
  count = g_bookmark_file_get_size (priv->recent_items);
  if (!count)
    return 0;
  
  purge_recent_items_list (manager, error);
  
  purged = count - g_bookmark_file_get_size (priv->recent_items);

  return purged;
}

static void
gtk_recent_manager_changed (GtkRecentManager *recent_manager)
{
  g_signal_emit (recent_manager, signal_changed, 0);
}

/*****************
 * GtkRecentInfo *
 *****************/
 
GType
gtk_recent_info_get_type (void)
{
  static GType info_type = 0;
  
  if (!info_type)
    info_type = g_boxed_type_register_static ("GtkRecentInfo",
    					      (GBoxedCopyFunc) gtk_recent_info_ref,
    					      (GBoxedFreeFunc) gtk_recent_info_unref);
  return info_type;
}

static GtkRecentInfo *
gtk_recent_info_new (const gchar *uri)
{
  GtkRecentInfo *info;

  g_assert (uri != NULL);

  info = g_new0 (GtkRecentInfo, 1);
  info->uri = g_strdup (uri);
  
  info->applications = NULL;
  info->apps_lookup = g_hash_table_new (g_str_hash, g_str_equal);
  
  info->groups = NULL;
  
  info->ref_count = 1;

  return info;
}

static void
gtk_recent_info_free (GtkRecentInfo *recent_info)
{
  if (!recent_info)
    return;

  g_free (recent_info->uri);
  g_free (recent_info->display_name);
  g_free (recent_info->description);
  g_free (recent_info->mime_type);
  
  if (recent_info->applications)
    {
      g_slist_foreach (recent_info->applications,
      		       (GFunc) recent_app_info_free,
      		       NULL);
      g_slist_free (recent_info->applications);
      
      recent_info->applications = NULL;
    }
  
  if (recent_info->apps_lookup)
    g_hash_table_destroy (recent_info->apps_lookup);

  if (recent_info->groups)
    {
      g_slist_foreach (recent_info->groups,
		       (GFunc) g_free,
		       NULL);
      g_slist_free (recent_info->groups);

      recent_info->groups = NULL;
    }
  
  if (recent_info->icon)
    g_object_unref (recent_info->icon);

  g_free (recent_info);
}

/**
 * gtk_recent_info_ref:
 * @info: a #GtkRecentInfo
 *
 * Increases the reference count of @recent_info by one.
 *
 * Return value: the recent info object with its reference count increased
 *   by one.
 *
 * Since: 2.10
 */
GtkRecentInfo *
gtk_recent_info_ref (GtkRecentInfo *info)
{
  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (info->ref_count > 0, NULL);
  
  info->ref_count += 1;
    
  return info;
}

/**
 * gtk_recent_info_unref:
 * @info: a #GtkRecentInfo
 *
 * Decreases the reference count of @info by one.  If the reference
 * count reaches zero, @info is deallocated, and the memory freed.
 *
 * Since: 2.10
 */
void
gtk_recent_info_unref (GtkRecentInfo *info)
{
  g_return_if_fail (info != NULL);
  g_return_if_fail (info->ref_count > 0);

  info->ref_count -= 1;
  
  if (info->ref_count == 0)
    gtk_recent_info_free (info);
}

/**
 * gtk_recent_info_get_uri:
 * @info: a #GtkRecentInfo
 *
 * Gets the URI of the resource.
 *
 * Return value: the URI of the resource.  The returned string is
 *   owned by the recent manager, and should not be freed.
 *
 * Since: 2.10
 */
G_CONST_RETURN gchar *
gtk_recent_info_get_uri (GtkRecentInfo *info)
{
  g_return_val_if_fail (info != NULL, NULL);
  
  return info->uri;
}

/**
 * gtk_recent_info_get_display_name:
 * @info: a #GtkRecentInfo
 *
 * Gets the name of the resource.  If none has been defined, the basename
 * of the resource is obtained.
 *
 * Return value: the display name of the resource.  The returned string
 *   is owned by the recent manager, and should not be freed.
 *
 * Since: 2.10
 */
G_CONST_RETURN gchar *
gtk_recent_info_get_display_name (GtkRecentInfo *info)
{
  g_return_val_if_fail (info != NULL, NULL);
  
  if (!info->display_name)
    info->display_name = gtk_recent_info_get_short_name (info);
  
  return info->display_name;
}

/**
 * gtk_recent_info_get_description:
 * @info: a #GtkRecentInfo
 *
 * Gets the (short) description of the resource.
 *
 * Return value: the description of the resource.  The returned string
 *   is owned by the recent manager, and should not be freed.
 *
 * Since: 2.10
 **/
G_CONST_RETURN gchar *
gtk_recent_info_get_description (GtkRecentInfo *info)
{
  g_return_val_if_fail (info != NULL, NULL);
  
  return info->description;
}

/**
 * gtk_recent_info_get_mime_type:
 * @info: a #GtkRecentInfo
 *
 * Gets the MIME type of the resource.
 *
 * Return value: the MIME type of the resource.  The returned string
 *   is owned by the recent manager, and should not be freed.
 *
 * Since: 2.10
 */
G_CONST_RETURN gchar *
gtk_recent_info_get_mime_type (GtkRecentInfo *info)
{
  g_return_val_if_fail (info != NULL, NULL);
  
  if (!info->mime_type)
    info->mime_type = g_strdup (GTK_RECENT_DEFAULT_MIME);
  
  return info->mime_type;
}

/**
 * gtk_recent_info_get_added:
 * @info: a #GtkRecentInfo
 *
 * Gets the timestamp (seconds from system's Epoch) when the resource
 * was added to the recently used resources list.
 *
 * Return value: the number of seconds elapsed from system's Epoch when
 *   the resource was added to the list, or -1 on failure.
 *
 * Since: 2.10
 */
time_t
gtk_recent_info_get_added (GtkRecentInfo *info)
{
  g_return_val_if_fail (info != NULL, (time_t) -1);
  
  return info->added;
}

/**
 * gtk_recent_info_get_modified:
 * @info: a #GtkRecentInfo
 *
 * Gets the timestamp (seconds from system's Epoch) when the resource
 * was last modified.
 *
 * Return value: the number of seconds elapsed from system's Epoch when
 *   the resource was last modified, or -1 on failure.
 *
 * Since: 2.10
 */
time_t
gtk_recent_info_get_modified (GtkRecentInfo *info)
{
  g_return_val_if_fail (info != NULL, (time_t) -1);
  
  return info->modified;
}

/**
 * gtk_recent_info_get_visited:
 * @info: a #GtkRecentInfo
 *
 * Gets the timestamp (seconds from system's Epoch) when the resource
 * was last visited.
 *
 * Return value: the number of seconds elapsed from system's Epoch when
 *   the resource was last visited, or -1 on failure.
 *
 * Since: 2.10
 */
time_t
gtk_recent_info_get_visited (GtkRecentInfo *info)
{
  g_return_val_if_fail (info != NULL, (time_t) -1);
  
  return info->visited;
}

/**
 * gtk_recent_info_get_private_hint:
 * @info: a #GtkRecentInfo
 *
 * Gets the value of the "private" flag.  Resources in the recently used
 * list that have this flag set to %TRUE should only be displayed by the
 * applications that have registered them.
 *
 * Return value: %TRUE if the private flag was found, %FALSE otherwise.
 *
 * Since: 2.10
 */
gboolean
gtk_recent_info_get_private_hint (GtkRecentInfo *info)
{
  g_return_val_if_fail (info != NULL, FALSE);
  
  return info->is_private;
}


static RecentAppInfo *
recent_app_info_new (const gchar *app_name)
{
  RecentAppInfo *app_info;

  g_assert (app_name != NULL);
  
  app_info = g_new0 (RecentAppInfo, 1);
  app_info->name = g_strdup (app_name);
  app_info->exec = NULL;
  app_info->count = 1;
  app_info->stamp = time (NULL);
  
  return app_info;
}

static void
recent_app_info_free (RecentAppInfo *app_info)
{
  if (!app_info)
    return;
  
  if (app_info->name)
    g_free (app_info->name);
  
  if (app_info->exec)
    g_free (app_info->exec);
  
  g_free (app_info);
}

/**
 * gtk_recent_info_get_application_info:
 * @info: a #GtkRecentInfo
 * @app_name: the name of the application that has registered this item
 * @app_exec: return location for the string containing the command line
 * @count: return location for the number of times this item was registered
 * @time_: return location for the timestamp this item was last registered
 *    for this application
 *
 * Gets the data regarding the application that has registered the resource
 * pointed by @info.
 *
 * If the command line contains any escape characters defined inside the
 * storage specification, they will be expanded.
 *
 * Return value: %TRUE if an application with @app_name has registered this
 *   resource inside the recently used list, or %FALSE otherwise.  You should
 *   free the returned command line using g_free().
 *
 * Since: 2.10
 */
gboolean
gtk_recent_info_get_application_info (GtkRecentInfo  *info,
				      const gchar    *app_name,
				      gchar         **app_exec,
				      guint          *count,
				      time_t         *time_)
{
  RecentAppInfo *ai;
  
  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (app_name != NULL, FALSE);
  
  ai = (RecentAppInfo *) g_hash_table_lookup (info->apps_lookup,
  					      app_name);
  if (!ai)
    {
      g_warning ("No registered application with name '%s' "
                 "for item with URI '%s' found",
                 app_name,
                 info->uri);
      return FALSE;
    }
  
  if (app_exec)
    *app_exec = ai->exec;
  
  if (count)
    *count = ai->count;
  
  if (time_)
    *time_ = ai->stamp;

  return TRUE;
}

/**
 * gtk_recent_info_get_applications:
 * @info: a #GtkRecentInfo
 * @length: return location for the length of the returned list, or %NULL
 *
 * Retrieves the list of applications that have registered this resource.
 *
 * Return value: a newly allocated %NULL-terminated array of strings.
 *   Use g_strfreev() to free it.
 *
 * Since: 2.10
 */ 			      
gchar **
gtk_recent_info_get_applications (GtkRecentInfo *info,
				  gsize         *length)
{
  GSList *l;
  gchar **retval;
  gsize n_apps, i;
  
  g_return_val_if_fail (info != NULL, NULL);
  
  if (!info->applications)
    {
      if (length)
        *length = 0;
      
      return NULL;    
    }
  
  n_apps = g_slist_length (info->applications);
  
  retval = g_new0 (gchar *, n_apps + 1);
  
  for (l = info->applications, i = 0;
       l != NULL;
       l = l->next)
    {
      RecentAppInfo *ai = (RecentAppInfo *) l->data;
      
      g_assert (ai != NULL);
      
      retval[i++] = g_strdup (ai->name);
    }
  retval[i] = NULL;
  
  if (length)
    *length = i;
  
  return retval;
}

/**
 * gtk_recent_info_has_application:
 * @info: a #GtkRecentInfo
 * @app_name: a string containing an application name
 *
 * Checks whether an application registered this resource using @app_name.
 *
 * Return value: %TRUE if an application with name @app_name was found,
 *   %FALSE otherwise.
 *
 * Since: 2.10
 */
gboolean
gtk_recent_info_has_application (GtkRecentInfo *info,
				 const gchar   *app_name)
{
  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (app_name != NULL, FALSE);
  
  return (NULL != g_hash_table_lookup (info->apps_lookup, app_name));
}

/**
 * gtk_recent_info_last_application:
 * @info: a #GtkRecentInfo
 *
 * Gets the name of the last application that have registered the
 * recently used resource represented by @info.
 *
 * Return value: an application name.  Use g_free() to free it.
 *
 * Since: 2.10
 */
gchar *
gtk_recent_info_last_application (GtkRecentInfo  *info)
{
  GSList *l;
  time_t last_stamp = (time_t) -1;
  gchar *name = NULL;
  
  g_return_val_if_fail (info != NULL, NULL);
  
  for (l = info->applications; l != NULL; l = l->next)
    {
      RecentAppInfo *ai = (RecentAppInfo *) l->data;
      
      if (ai->stamp > last_stamp)
        {
	  name = ai->name;
	  last_stamp = ai->stamp;
	}
    }
  
  return g_strdup (name);
}

static GdkPixbuf *
get_icon_for_mime_type (const char *mime_type,
			gint        pixel_size)
{
  GtkIconTheme *icon_theme;
  const char *separator;
  GString *icon_name;
  GdkPixbuf *pixbuf;

  separator = strchr (mime_type, '/');
  if (!separator)
    return NULL;

  icon_theme = gtk_icon_theme_get_default ();

  /* try with the three icon name variants for MIME types */

  /* canonicalize MIME type: foo/x-bar -> foo-x-bar */
  icon_name = g_string_new (NULL);
  g_string_append_len (icon_name, mime_type, separator - mime_type);
  g_string_append_c (icon_name, '-');
  g_string_append (icon_name, separator + 1);
  pixbuf = gtk_icon_theme_load_icon (icon_theme, icon_name->str,
                                     pixel_size,
                                     0,
                                     NULL);
  g_string_free (icon_name, TRUE);
  if (pixbuf)
    return pixbuf;

  /* canonicalize MIME type, and prepend "gnome-mime-" */
  icon_name = g_string_new ("gnome-mime-");
  g_string_append_len (icon_name, mime_type, separator - mime_type);
  g_string_append_c (icon_name, '-');
  g_string_append (icon_name, separator + 1);
  pixbuf = gtk_icon_theme_load_icon (icon_theme, icon_name->str,
                                     pixel_size,
                                     0,
                                     NULL);
  g_string_free (icon_name, TRUE);
  if (pixbuf)
    return pixbuf;

  /* try the MIME family icon */
  icon_name = g_string_new ("gnome-mime-");
  g_string_append_len (icon_name, mime_type, separator - mime_type);
  pixbuf = gtk_icon_theme_load_icon (icon_theme, icon_name->str,
                                     pixel_size,
                                     0,
                                     NULL);
  g_string_free (icon_name, TRUE);

  return pixbuf;
}

static GdkPixbuf *
get_icon_fallback (const gchar *icon_name,
		   gint         size)
{
  GtkIconTheme *icon_theme;
  GdkPixbuf *retval;

  icon_theme = gtk_icon_theme_get_default ();
  
  retval = gtk_icon_theme_load_icon (icon_theme, icon_name,
  				     size,
  				     GTK_ICON_LOOKUP_USE_BUILTIN,
  				     NULL);
  g_assert (retval != NULL);
  
  return retval; 
}

/**
 * gtk_recent_info_get_icon:
 * @info: a #GtkRecentInfo
 * @size: the size of the icon in pixels
 *
 * Retrieves the icon of size @size associated to the resource MIME type.
 *
 * Return value: a #GdkPixbuf containing the icon, or %NULL.
 *
 * Since: 2.10
 */
GdkPixbuf *
gtk_recent_info_get_icon (GtkRecentInfo *info,
			  gint           size)
{
  GdkPixbuf *retval = NULL;
  
  g_return_val_if_fail (info != NULL, NULL);
  
  if (info->mime_type)
    retval = get_icon_for_mime_type (info->mime_type, size);

  /* this should never fail */
  if (!retval)
    {
      if (info->mime_type &&
          strcmp (info->mime_type, "x-directory/normal") == 0)
        retval = get_icon_fallback (GTK_STOCK_DIRECTORY, size);
      else
        retval = get_icon_fallback (GTK_STOCK_FILE, size);
    }

  return retval;
}

/**
 * gtk_recent_info_is_local:
 * @info: a #GtkRecentInfo
 *
 * Checks whether the resource is local or not by looking at the
 * scheme of its URI.
 *
 * Return value: %TRUE if the resource is local.
 *
 * Since: 2.10
 */
gboolean
gtk_recent_info_is_local (GtkRecentInfo *info)
{
  g_return_val_if_fail (info != NULL, FALSE);
  
  return has_case_prefix (info->uri, "file:/");
}

/**
 * gtk_recent_info_exists:
 * @info: a #GtkRecentInfo
 *
 * Checks whether the resource pointed by @info still exists.  At
 * the moment this check is done only on resources pointing to local files.
 *
 * Return value: %TRUE if the resource exists
 *
 * Since: 2.10
 */
gboolean
gtk_recent_info_exists (GtkRecentInfo *info)
{
  gchar *filename;
  struct stat stat_buf;
  gboolean retval = FALSE;
  
  g_return_val_if_fail (info != NULL, FALSE);
  
  /* we guarantee only local resources */
  if (!gtk_recent_info_is_local (info))
    return FALSE;
  
  filename = g_filename_from_uri (info->uri, NULL, NULL);
  if (filename)
    {
      if (stat (filename, &stat_buf) == 0)
        retval = TRUE;
     
      g_free (filename);
    }
  
  return retval;
}

/**
 * gtk_recent_info_match:
 * @info_a: a #GtkRecentInfo
 * @info_b: a #GtkRecentInfo
 *
 * Checks whether two #GtkRecentInfo structures point to the same
 * resource.
 *
 * Return value: %TRUE if both #GtkRecentInfo structures point to se same
 *   resource, %FALSE otherwise.
 *
 * Since: 2.10
 */
gboolean
gtk_recent_info_match (GtkRecentInfo *info_a,
		       GtkRecentInfo *info_b)
{
  g_return_val_if_fail (info_a != NULL, FALSE);
  g_return_val_if_fail (info_b != NULL, FALSE);
  
  return (0 == strcmp (info_a->uri, info_b->uri));
}

/* taken from gnome-vfs-uri.c */
static const gchar *
get_method_string (const gchar  *substring, 
		   gchar       **method_string)
{
  const gchar *p;
  char *method;
	
  for (p = substring;
       g_ascii_isalnum (*p) || *p == '+' || *p == '-' || *p == '.';
       p++)
    ;

  if (*p == ':'
#ifdef G_OS_WIN32
                &&
      !(p == substring + 1 && g_ascii_isalpha (*substring))
#endif
							   )
    {
      /* Found toplevel method specification.  */
      method = g_strndup (substring, p - substring);
      *method_string = g_ascii_strdown (method, -1);
      g_free (method);
      p++;
    }
  else
    {
      *method_string = g_strdup ("file");
      p = substring;
    }
  
  return p;
}

/* Stolen from gnome_vfs_make_valid_utf8() */
static char *
make_valid_utf8 (const char *name)
{
  GString *string;
  const char *remainder, *invalid;
  int remaining_bytes, valid_bytes;

  string = NULL;
  remainder = name;
  remaining_bytes = name ? strlen (name) : 0;

  while (remaining_bytes != 0)
    {
      if (g_utf8_validate (remainder, remaining_bytes, &invalid))
        break;
      
      valid_bytes = invalid - remainder;
      
      if (string == NULL)
        string = g_string_sized_new (remaining_bytes);
      
      g_string_append_len (string, remainder, valid_bytes);
      g_string_append_c (string, '?');
      
      remaining_bytes -= valid_bytes + 1;
      remainder = invalid + 1;
    }
  
  if (string == NULL)
    return g_strdup (name);

  g_string_append (string, remainder);
  g_assert (g_utf8_validate (string->str, -1, NULL));

  return g_string_free (string, FALSE);
}

static gchar *
get_uri_shortname_for_display (const gchar *uri)
{
  gchar *name = NULL;
  gboolean validated = FALSE;

  if (has_case_prefix (uri, "file:/"))
    {
      gchar *local_file;
      
      local_file = g_filename_from_uri (uri, NULL, NULL);
      
      if (local_file)
        {
          name = g_filename_display_basename (local_file);
          validated = TRUE;
        }
		
      g_free (local_file);
    } 
  
  if (!name)
    {
      gchar *method;
      gchar *local_file;
      const gchar *rest;
      
      rest = get_method_string (uri, &method);
      local_file = g_filename_display_basename (rest);
      
      name = g_strdup_printf ("%s: %s", method, local_file);
      
      g_free (local_file);
      g_free (method);
    }
  
  g_assert (name != NULL);
  
  if (!validated && !g_utf8_validate (name, -1, NULL))
    {
      gchar *utf8_name;
      
      utf8_name = make_valid_utf8 (name);
      g_free (name);
      
      name = utf8_name;
    }

  return name;
}

/**
 * gtk_recent_info_get_short_name:
 * @info: an #GtkRecentInfo
 *
 * Computes a valid UTF-8 string that can be used as the name of the item in a
 * menu or list.  For example, calling this function on an item that refers to
 * "file:///foo/bar.txt" will yield "bar.txt".
 *
 * Return value: A newly-allocated string in UTF-8 encoding; free it with
 *   g_free().
 *
 * Since: 2.10
 */
gchar *
gtk_recent_info_get_short_name (GtkRecentInfo *info)
{
  gchar *short_name;

  g_return_val_if_fail (info != NULL, NULL);

  if (info->uri == NULL)
    return NULL;

  short_name = get_uri_shortname_for_display (info->uri);

  return short_name;
}

/**
 * gtk_recent_info_get_uri_display:
 * @info: a #GtkRecentInfo
 *
 * Gets a displayable version of the resource's URI.
 *
 * Return value: a UTF-8 string containing the resource's URI or %NULL
 *
 * Since: 2.10
 */
gchar *
gtk_recent_info_get_uri_display (GtkRecentInfo *info)
{
  gchar *filename, *filename_utf8;
  
  g_return_val_if_fail (info != NULL, NULL);
  
  filename = g_filename_from_uri (info->uri, NULL, NULL);
  if (!filename)
    return NULL;
      
  filename_utf8 = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
  g_free (filename);

  return filename_utf8;
}

/**
 * gtk_recent_info_get_age:
 * @info: a #GtkRecentInfo
 *
 * Gets the number of days elapsed since the last update of the resource
 * pointed by @info.
 *
 * Return value: a positive integer containing the number of days elapsed
 *   since the time this resource was last modified.  
 *
 * Since: 2.10
 */
gint
gtk_recent_info_get_age (GtkRecentInfo *info)
{
  time_t now, delta;
  gint retval;

  g_return_val_if_fail (info != NULL, -1);

  now = time (NULL);
  
  delta = now - info->modified;
  
  retval = (gint) (delta / (60 * 60 * 24));
  
  return retval;
}

/**
 * gtk_recent_info_get_groups:
 * @info: a #GtkRecentInfo
 * @length: return location for the number of groups returned, or %NULL
 *
 * Returns all groups registered for the recently used item @info.  The
 * array of returned group names will be %NULL terminated, so length might
 * optionally be %NULL.
 *
 * Return value: a newly allocated %NULL terminated array of strings.  Use
 *   g_strfreev() to free it.
 *
 * Since: 2.10
 */
gchar **
gtk_recent_info_get_groups (GtkRecentInfo *info,
			    gsize         *length)
{
  GSList *l;
  gchar **retval;
  gsize n_groups, i;
  
  g_return_val_if_fail (info != NULL, NULL);
  
  if (!info->groups)
    {
      if (length)
        *length = 0;
      
      return NULL;
    }
  
  n_groups = g_slist_length (info->groups);
  
  retval = g_new0 (gchar *, n_groups + 1);
  
  for (l = info->groups, i = 0;
       l != NULL;
       l = l->next)
    {
      gchar *group_name = (gchar *) l->data;
      
      g_assert (group_name != NULL);
      
      retval[i++] = g_strdup (group_name);
    }
  retval[i] = NULL;
  
  if (length)
    *length = i;
  
  return retval;
}

/**
 * gtk_recent_info_has_group:
 * @info: a #GtkRecentInfo
 * @group_name: name of a group
 *
 * Checks whether @group_name appears inside the groups registered for the
 * recently used item @info.
 *
 * Return value: %TRUE if the group was found.
 *
 * Since: 2.10
 */
gboolean
gtk_recent_info_has_group (GtkRecentInfo *info,
			   const gchar   *group_name)
{
  GSList *l;
  
  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (group_name != NULL, FALSE);

  if (!info->groups)
    return FALSE;

  for (l = info->groups; l != NULL; l = l->next)
    {
      gchar *g = (gchar *) l->data;

      if (strcmp (g, group_name) == 0)
        return TRUE;
    }

  return FALSE;
}

#define __GTK_RECENT_MANAGER_C__
#include "gtkaliasdef.c"
