/* GtkIconTheme - a loader for icon themes
 * gtk-icon-theme.c Copyright (C) 2002, 2003 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gstdio.h>
#include "gtkalias.h"

#ifdef G_OS_WIN32
#ifndef S_ISDIR
#define S_ISDIR(mode) ((mode)&_S_IFDIR)
#endif
#endif /* G_OS_WIN32 */

#include "gtkicontheme.h"
#include "gtkiconfactory.h"
#include "gtkiconcache.h"
#include "gtkintl.h"
#include "gtksettings.h"
#include "gtkprivate.h"



#define DEFAULT_THEME_NAME "hicolor"

typedef struct _GtkIconData GtkIconData;

typedef enum
{
  ICON_THEME_DIR_FIXED,  
  ICON_THEME_DIR_SCALABLE,  
  ICON_THEME_DIR_THRESHOLD,
  ICON_THEME_DIR_UNTHEMED
} IconThemeDirType;

/* In reverse search order: */
typedef enum
{
  ICON_SUFFIX_NONE = 0,
  ICON_SUFFIX_XPM = 1 << 0,
  ICON_SUFFIX_SVG = 1 << 1,
  ICON_SUFFIX_PNG = 1 << 2,
  HAS_ICON_FILE = 1 << 3
} IconSuffix;


struct _GtkIconThemePrivate
{
  guint custom_theme : 1;
  guint is_screen_singleton : 1;
  guint pixbuf_supports_svg : 1;
  
  char *current_theme;
  char **search_path;
  int search_path_len;

  gboolean themes_valid;
  /* A list of all the themes needed to look up icons.
   * In search order, without duplicates
   */
  GList *themes;
  GHashTable *unthemed_icons;
  GList *unthemed_icons_caches;
  
  /* Note: The keys of this hashtable are owned by the
   * themedir and unthemed hashtables.
   */
  GHashTable *all_icons;

  /* GdkScreen for the icon theme (may be NULL)
   */
  GdkScreen *screen;
  
  /* time when we last stat:ed for theme changes */
  long last_stat_time;
  GList *dir_mtimes;
};

struct _GtkIconInfo
{
  /* Information about the source
   */
  gchar *filename;
#ifdef G_OS_WIN32
  /* System codepage version of filename, for DLL ABI backward
   * compatibility functions.
   */
  gchar *cp_filename;
#endif
  GdkPixbuf *builtin_pixbuf;

  GtkIconData *data;
  
  /* Information about the directory where
   * the source was found
   */
  IconThemeDirType dir_type;
  gint dir_size;
  gint threshold;

  /* Parameters influencing the scaled icon
   */
  gint desired_size;
  gboolean raw_coordinates;

  /* Cached information if we go ahead and try to load
   * the icon.
   */
  GdkPixbuf *pixbuf;
  GError *load_error;
  gdouble scale;
};

typedef struct
{
  char *name;
  char *display_name;
  char *comment;
  char *example;

  /* Icon caches, per theme directory, key is NULL if
   * no cache exists for that directory
   */
  GHashTable *icon_caches;
  
  /* In search order */
  GList *dirs;
} IconTheme;

struct _GtkIconData
{
  gboolean has_embedded_rect;
  gint x0, y0, x1, y1;
  
  GdkPoint *attach_points;
  gint n_attach_points;

  gchar *display_name;
};

typedef struct
{
  IconThemeDirType type;
  GQuark context;

  int size;
  int min_size;
  int max_size;
  int threshold;

  char *dir;
  char *subdir;
  
  GtkIconCache *cache;
  
  GHashTable *icons;
  GHashTable *icon_data;
} IconThemeDir;

typedef struct
{
  char *svg_filename;
  char *no_svg_filename;
} UnthemedIcon;

typedef struct
{
  gint size;
  GdkPixbuf *pixbuf;
} BuiltinIcon;

typedef struct 
{
  char *dir;
  time_t mtime; /* 0 == not existing or not a dir */
} IconThemeDirMtime;

static void  gtk_icon_theme_class_init (GtkIconThemeClass    *klass);
static void  gtk_icon_theme_init       (GtkIconTheme         *icon_theme);
static void  gtk_icon_theme_finalize   (GObject              *object);
static void  theme_dir_destroy         (IconThemeDir         *dir);

static void         theme_destroy     (IconTheme        *theme);
static GtkIconInfo *theme_lookup_icon (IconTheme        *theme,
				       const char       *icon_name,
				       int               size,
				       gboolean          allow_svg,
				       gboolean          use_default_icons);
static void         theme_list_icons  (IconTheme        *theme,
				       GHashTable       *icons,
				       GQuark            context);
static void         theme_subdir_load (GtkIconTheme     *icon_theme,
				       IconTheme        *theme,
				       GKeyFile         *theme_file,
				       char             *subdir);
static void         do_theme_change   (GtkIconTheme     *icon_theme);

static void  blow_themes               (GtkIconTheme    *icon_themes);

static void  icon_data_free            (GtkIconData          *icon_data);
static void load_icon_data (IconThemeDir *dir,
			    const char   *path,
			    const char   *name);

static IconSuffix theme_dir_get_icon_suffix (IconThemeDir *dir,
					     const gchar  *icon_name,
					     gboolean     *has_icon_file);


static GtkIconInfo *icon_info_new             (void);
static GtkIconInfo *icon_info_new_builtin     (BuiltinIcon *icon);

static IconSuffix suffix_from_name (const char *name);

static BuiltinIcon *find_builtin_icon (const gchar *icon_name,
				       gint         size,
				       gint        *min_difference_p,
				       gboolean    *has_larger_p);

static GObjectClass *parent_class = NULL;

static guint signal_changed = 0;

static GHashTable *icon_theme_builtin_icons;

GType
gtk_icon_theme_get_type (void)
{
  static GType type = 0;

  if (type == 0)
    {
      static const GTypeInfo info =
	{
	  sizeof (GtkIconThemeClass),
	  NULL,           /* base_init */
	  NULL,           /* base_finalize */
	  (GClassInitFunc) gtk_icon_theme_class_init,
	  NULL,           /* class_finalize */
	  NULL,           /* class_data */
	  sizeof (GtkIconTheme),
	  0,              /* n_preallocs */
	  (GInstanceInitFunc) gtk_icon_theme_init,
	};

      type = g_type_register_static (G_TYPE_OBJECT, "GtkIconTheme", &info, 0);
    }

  return type;
}

/**
 * gtk_icon_theme_new:
 * 
 * Creates a new icon theme object. Icon theme objects are used
 * to lookup up an icon by name in a particular icon theme.
 * Usually, you'll want to use gtk_icon_theme_get_default()
 * or gtk_icon_theme_get_for_screen() rather than creating
 * a new icon theme object for scratch.
 * 
 * Return value: the newly created #GtkIconTheme object.
 *
 * Since: 2.4
 **/
GtkIconTheme *
gtk_icon_theme_new (void)
{
  return g_object_new (GTK_TYPE_ICON_THEME, NULL);
}

/**
 * gtk_icon_theme_get_default:
 * 
 * Gets the icon theme for the default screen. See
 * gtk_icon_theme_get_for_screen().
 * 
 * Return value: A unique #GtkIconTheme associated with
 *  the default screen. This icon theme is associated with
 *  the screen and can be used as long as the screen
 *  is open.
 *
 * Since: 2.4
 **/
GtkIconTheme *
gtk_icon_theme_get_default (void)
{
  return gtk_icon_theme_get_for_screen (gdk_screen_get_default ());
}

/**
 * gtk_icon_theme_get_for_screen:
 * @screen: a #GdkScreen
 * 
 * Gets the icon theme object associated with @screen; if this
 * function has not previously been called for the given
 * screen, a new icon theme object will be created and
 * associated with the screen. Icon theme objects are
 * fairly expensive to create, so using this function
 * is usually a better choice than calling than gtk_icon_theme_new()
 * and setting the screen yourself; by using this function
 * a single icon theme object will be shared between users.
 * 
 * Return value: A unique #GtkIconTheme associated with
 *  the given screen. This icon theme is associated with
 *  the screen and can be used as long as the screen
 *  is open.
 *
 * Since: 2.4
 **/
GtkIconTheme *
gtk_icon_theme_get_for_screen (GdkScreen *screen)
{
  GtkIconTheme *icon_theme;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  g_return_val_if_fail (!screen->closed, NULL);

  icon_theme = g_object_get_data (G_OBJECT (screen), "gtk-icon-theme");
  if (!icon_theme)
    {
      GtkIconThemePrivate *priv;

      icon_theme = gtk_icon_theme_new ();
      gtk_icon_theme_set_screen (icon_theme, screen);

      priv = icon_theme->priv;
      priv->is_screen_singleton = TRUE;

      g_object_set_data (G_OBJECT (screen), "gtk-icon-theme", icon_theme);
    }

  return icon_theme;
}

static void
gtk_icon_theme_class_init (GtkIconThemeClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = gtk_icon_theme_finalize;

/**
 * GtkIconTheme::changed
 * @icon_theme: the icon theme
 * 
 * Emitted when the current icon theme is switched or GTK+ detects
 * that a change has occurred in the contents of the current
 * icon theme.
 **/
  signal_changed = g_signal_new ("changed",
				 G_TYPE_FROM_CLASS (klass),
				 G_SIGNAL_RUN_LAST,
				 G_STRUCT_OFFSET (GtkIconThemeClass, changed),
				 NULL, NULL,
				 g_cclosure_marshal_VOID__VOID,
				 G_TYPE_NONE, 0);

  g_type_class_add_private (klass, sizeof (GtkIconThemePrivate));
}


/* Callback when the display that the icon theme is attached to
 * is closed; unset the screen, and if it's the unique theme
 * for the screen, drop the reference
 */
static void
display_closed (GdkDisplay   *display,
		gboolean      is_error,
		GtkIconTheme *icon_theme)
{
  GtkIconThemePrivate *priv = icon_theme->priv;
  GdkScreen *screen = priv->screen;
  gboolean was_screen_singleton = priv->is_screen_singleton;

  if (was_screen_singleton)
    {
      g_object_set_data (G_OBJECT (screen), "gtk-icon-theme", NULL);
      priv->is_screen_singleton = FALSE;
    }

  gtk_icon_theme_set_screen (icon_theme, NULL);

  if (was_screen_singleton)
    {
      g_object_unref (icon_theme);
    }
}

static void
update_current_theme (GtkIconTheme *icon_theme)
{
  GtkIconThemePrivate *priv = icon_theme->priv;

  if (!priv->custom_theme)
    {
      gchar *theme = NULL;

      if (priv->screen)
	{
	  GtkSettings *settings = gtk_settings_get_for_screen (priv->screen);
	  g_object_get (settings, "gtk-icon-theme-name", &theme, NULL);
	}

      if (!theme)
	theme = g_strdup (DEFAULT_THEME_NAME);

      if (strcmp (priv->current_theme, theme) != 0)
	{
	  g_free (priv->current_theme);
	  priv->current_theme = theme;

	  do_theme_change (icon_theme);
	}
      else
	g_free (theme);
    }
}

/* Callback when the icon theme GtkSetting changes
 */
static void
theme_changed (GtkSettings  *settings,
	       GParamSpec   *pspec,
	       GtkIconTheme *icon_theme)
{
  update_current_theme (icon_theme);
}

static void
unset_screen (GtkIconTheme *icon_theme)
{
  GtkIconThemePrivate *priv = icon_theme->priv;
  GtkSettings *settings;
  GdkDisplay *display;
  
  if (priv->screen)
    {
      settings = gtk_settings_get_for_screen (priv->screen);
      display = gdk_screen_get_display (priv->screen);
      
      g_signal_handlers_disconnect_by_func (display,
					    (gpointer) display_closed,
					    icon_theme);
      g_signal_handlers_disconnect_by_func (settings,
					    (gpointer) theme_changed,
					    icon_theme);

      priv->screen = NULL;
    }
}

/**
 * gtk_icon_theme_set_screen:
 * @icon_theme: a #GtkIconTheme
 * @screen: a #GdkScreen
 * 
 * Sets the screen for an icon theme; the screen is used
 * to track the user's currently configured icon theme,
 * which might be different for different screens.
 *
 * Since: 2.4
 **/
void
gtk_icon_theme_set_screen (GtkIconTheme *icon_theme,
			   GdkScreen    *screen)
{
  GtkIconThemePrivate *priv;
  GtkSettings *settings;
  GdkDisplay *display;

  g_return_if_fail (GTK_ICON_THEME (icon_theme));
  g_return_if_fail (screen == NULL || GDK_IS_SCREEN (screen));

  priv = icon_theme->priv;

  unset_screen (icon_theme);
  
  if (screen)
    {
      display = gdk_screen_get_display (screen);
      settings = gtk_settings_get_for_screen (screen);
      
      priv->screen = screen;
      
      g_signal_connect (display, "closed",
			G_CALLBACK (display_closed), icon_theme);
      g_signal_connect (settings, "notify::gtk-icon-theme-name",
			G_CALLBACK (theme_changed), icon_theme);
    }

  update_current_theme (icon_theme);
}

/* Checks whether a loader for SVG files has been registered
 * with GdkPixbuf.
 */
static gboolean
pixbuf_supports_svg (void)
{
  GSList *formats = gdk_pixbuf_get_formats ();
  GSList *tmp_list;
  static gboolean found_svg = FALSE;
  static gboolean value_known = FALSE;

  if (value_known)
    return found_svg;
  
  for (tmp_list = formats; tmp_list && !found_svg; tmp_list = tmp_list->next)
    {
      gchar **mime_types = gdk_pixbuf_format_get_mime_types (tmp_list->data);
      gchar **mime_type;
      
      for (mime_type = mime_types; *mime_type && !found_svg; mime_type++)
	{
	  if (strcmp (*mime_type, "image/svg") == 0)
	    found_svg = TRUE;
	}

      g_strfreev (mime_types);
    }

  g_slist_free (formats);
  value_known = TRUE;
  
  return found_svg;
}

static void
gtk_icon_theme_init (GtkIconTheme *icon_theme)
{
  GtkIconThemePrivate *priv;
  const gchar * const *xdg_data_dirs;
  int i, j;
  
  priv = g_type_instance_get_private ((GTypeInstance *)icon_theme,
				      GTK_TYPE_ICON_THEME);
  icon_theme->priv = priv;

  priv->custom_theme = FALSE;
  priv->current_theme = g_strdup (DEFAULT_THEME_NAME);

  xdg_data_dirs = g_get_system_data_dirs ();
  for (i = 0; xdg_data_dirs[i]; i++) ;

  priv->search_path_len = 2 * i + 2;
  
  priv->search_path = g_new (char *, priv->search_path_len);
  
  i = 0;
  priv->search_path[i++] = g_build_filename (g_get_home_dir (), ".icons", NULL);
  priv->search_path[i++] = g_build_filename (g_get_user_data_dir (), "icons", NULL);
  
  for (j = 0; xdg_data_dirs[j]; j++) 
    priv->search_path[i++] = g_build_filename (xdg_data_dirs[j], "icons", NULL);

  for (j = 0; xdg_data_dirs[j]; j++) 
    priv->search_path[i++] = g_build_filename (xdg_data_dirs[j], "pixmaps", NULL);

  priv->themes_valid = FALSE;
  priv->themes = NULL;
  priv->unthemed_icons = NULL;
  priv->unthemed_icons_caches = NULL;
  
  priv->pixbuf_supports_svg = pixbuf_supports_svg ();
}

static void
free_dir_mtime (IconThemeDirMtime *dir_mtime)
{
  g_free (dir_mtime->dir);
  g_free (dir_mtime);
}

static void
do_theme_change (GtkIconTheme *icon_theme)
{
  GtkIconThemePrivate *priv = icon_theme->priv;
  
  GTK_NOTE (ICONTHEME, 
	    g_print ("change to icon theme \"%s\"\n", priv->current_theme));
  blow_themes (icon_theme);
  g_signal_emit (icon_theme, signal_changed, 0);
  
  if (priv->screen && priv->is_screen_singleton)
    {
      GtkSettings *settings = gtk_settings_get_for_screen (priv->screen);
      gtk_rc_reset_styles (settings);
    }
}

static void 
free_cache (gpointer data, 
	    gpointer user_data)
{
  GtkIconCache *cache = (GtkIconCache *)data;

  if (cache)
    _gtk_icon_cache_unref (cache);
}

static void
blow_themes (GtkIconTheme *icon_theme)
{
  GtkIconThemePrivate *priv = icon_theme->priv;
  
  if (priv->themes_valid)
    {
      g_hash_table_destroy (priv->all_icons);
      g_list_foreach (priv->themes, (GFunc)theme_destroy, NULL);
      g_list_free (priv->themes);
      g_list_foreach (priv->dir_mtimes, (GFunc)free_dir_mtime, NULL);
      g_list_free (priv->dir_mtimes);
      g_hash_table_destroy (priv->unthemed_icons);
      if (priv->unthemed_icons_caches)
	g_list_foreach (priv->unthemed_icons_caches, free_cache, NULL);
      g_list_free (priv->unthemed_icons_caches);
    }
  priv->themes = NULL;
  priv->unthemed_icons = NULL;
  priv->unthemed_icons_caches = NULL;
  priv->dir_mtimes = NULL;
  priv->all_icons = NULL;
  priv->themes_valid = FALSE;
}

static void
gtk_icon_theme_finalize (GObject *object)
{
  GtkIconTheme *icon_theme;
  GtkIconThemePrivate *priv;
  int i;

  icon_theme = GTK_ICON_THEME (object);
  priv = icon_theme->priv;

  unset_screen (icon_theme);

  g_free (priv->current_theme);
  priv->current_theme = NULL;

  for (i=0; i < priv->search_path_len; i++)
    g_free (priv->search_path[i]);

  g_free (priv->search_path);
  priv->search_path = NULL;

  blow_themes (icon_theme);

  G_OBJECT_CLASS (parent_class)->finalize (object);  
}

/**
 * gtk_icon_theme_set_search_path:
 * @icon_theme: a #GtkIconTheme
 * @path: array of directories that are searched for icon themes
 * @n_elements: number of elements in @path.
 * 
 * Sets the search path for the icon theme object. When looking
 * for an icon theme, GTK+ will search for a subdirectory of
 * one or more of the directories in @path with the same name
 * as the icon theme. (Themes from multiple of the path elements
 * are combined to allow themes to be extended by adding icons
 * in the user's home directory.)
 *
 * In addition if an icon found isn't found either in the current
 * icon theme or the default icon theme, and an image file with
 * the right name is found directly in one of the elements of
 * @path, then that image will be used for the icon name.
 * (This is legacy feature, and new icons should be put
 * into the default icon theme, which is called DEFAULT_THEME_NAME,
 * rather than directly on the icon path.)
 *
 * Since: 2.4
 **/
void
gtk_icon_theme_set_search_path (GtkIconTheme *icon_theme,
				const gchar  *path[],
				gint          n_elements)
{
  GtkIconThemePrivate *priv;
  gint i;

  g_return_if_fail (GTK_IS_ICON_THEME (icon_theme));

  priv = icon_theme->priv;
  for (i = 0; i < priv->search_path_len; i++)
    g_free (priv->search_path[i]);

  g_free (priv->search_path);

  priv->search_path = g_new (gchar *, n_elements);
  priv->search_path_len = n_elements;
  for (i = 0; i < priv->search_path_len; i++)
    priv->search_path[i] = g_strdup (path[i]);

  do_theme_change (icon_theme);
}


/**
 * gtk_icon_theme_get_search_path:
 * @icon_theme: a #GtkIconTheme
 * @path: location to store a list of icon theme path directories or %NULL
 *        The stored value should be freed with g_strfreev().
 * @n_elements: location to store number of elements
 *              in @path, or %NULL
 * 
 * Gets the current search path. See gtk_icon_theme_set_search_path().
 *
 * Since: 2.4
 **/
void
gtk_icon_theme_get_search_path (GtkIconTheme      *icon_theme,
				gchar            **path[],
				gint              *n_elements)
{
  GtkIconThemePrivate *priv;
  int i;

  g_return_if_fail (GTK_IS_ICON_THEME (icon_theme));

  priv = icon_theme->priv;

  if (n_elements)
    *n_elements = priv->search_path_len;
  
  if (path)
    {
      *path = g_new (gchar *, priv->search_path_len + 1);
      for (i = 0; i < priv->search_path_len; i++)
	(*path)[i] = g_strdup (priv->search_path[i]);
      (*path)[i] = NULL;
    }
}

/**
 * gtk_icon_theme_append_search_path:
 * @icon_theme: a #GtkIconTheme
 * @path: directory name to append to the icon path
 * 
 * Appends a directory to the search path. 
 * See gtk_icon_theme_set_search_path(). 
 *
 * Since: 2.4
 **/
void
gtk_icon_theme_append_search_path (GtkIconTheme *icon_theme,
				   const gchar  *path)
{
  GtkIconThemePrivate *priv;

  g_return_if_fail (GTK_IS_ICON_THEME (icon_theme));
  g_return_if_fail (path != NULL);

  priv = icon_theme->priv;
  
  priv->search_path_len++;
  priv->search_path = g_renew (gchar *, priv->search_path, priv->search_path_len);
  priv->search_path[priv->search_path_len-1] = g_strdup (path);

  do_theme_change (icon_theme);
}

/**
 * gtk_icon_theme_prepend_search_path:
 * @icon_theme: a #GtkIconTheme
 * @path: directory name to prepend to the icon path
 * 
 * Prepends a directory to the search path. 
 * See gtk_icon_theme_set_search_path().
 *
 * Since: 2.4
 **/
void
gtk_icon_theme_prepend_search_path (GtkIconTheme *icon_theme,
				    const gchar  *path)
{
  GtkIconThemePrivate *priv;
  int i;

  g_return_if_fail (GTK_IS_ICON_THEME (icon_theme));
  g_return_if_fail (path != NULL);

  priv = icon_theme->priv;
  
  priv->search_path_len++;
  priv->search_path = g_renew (gchar *, priv->search_path, priv->search_path_len);

  for (i = priv->search_path_len - 1; i > 0; i--)
    priv->search_path[i] = priv->search_path[i - 1];
  
  priv->search_path[0] = g_strdup (path);

  do_theme_change (icon_theme);
}

/**
 * gtk_icon_theme_set_custom_theme:
 * @icon_theme: a #GtkIconTheme
 * @theme_name: name of icon theme to use instead of configured theme
 * 
 * Sets the name of the icon theme that the #GtkIconTheme object uses
 * overriding system configuration. This function cannot be called
 * on the icon theme objects returned from gtk_icon_theme_get_default()
 * and gtk_icon_theme_get_default().
 *
 * Since: 2.4
 **/
void
gtk_icon_theme_set_custom_theme (GtkIconTheme *icon_theme,
				 const gchar  *theme_name)
{
  GtkIconThemePrivate *priv;

  g_return_if_fail (GTK_IS_ICON_THEME (icon_theme));

  priv = icon_theme->priv;

  g_return_if_fail (!priv->is_screen_singleton);
  
  if (theme_name != NULL)
    {
      priv->custom_theme = TRUE;
      if (strcmp (theme_name, priv->current_theme) != 0)
	{
	  g_free (priv->current_theme);
	  priv->current_theme = g_strdup (theme_name);

	  do_theme_change (icon_theme);
	}
    }
  else
    {
      priv->custom_theme = FALSE;

      update_current_theme (icon_theme);
    }
}

static void
insert_theme (GtkIconTheme *icon_theme, const char *theme_name)
{
  int i;
  GList *l;
  char **dirs;
  char **themes;
  GtkIconThemePrivate *priv;
  IconTheme *theme;
  char *path;
  GKeyFile *theme_file;
  GError *error = NULL;
  IconThemeDirMtime *dir_mtime;
  struct stat stat_buf;
  
  priv = icon_theme->priv;

  for (l = priv->themes; l != NULL; l = l->next)
    {
      theme = l->data;
      if (strcmp (theme->name, theme_name) == 0)
	return;
    }
  
  for (i = 0; i < priv->search_path_len; i++)
    {
      path = g_build_filename (priv->search_path[i],
			       theme_name,
			       NULL);
      dir_mtime = g_new (IconThemeDirMtime, 1);
      dir_mtime->dir = path;
      if (g_stat (path, &stat_buf) == 0 && S_ISDIR (stat_buf.st_mode))
	dir_mtime->mtime = stat_buf.st_mtime;
      else
	dir_mtime->mtime = 0;

      priv->dir_mtimes = g_list_prepend (priv->dir_mtimes, dir_mtime);
    }
  priv->dir_mtimes = g_list_reverse (priv->dir_mtimes);

  theme_file = NULL;
  for (i = 0; i < priv->search_path_len && !theme_file; i++)
    {
      path = g_build_filename (priv->search_path[i],
			       theme_name,
			       "index.theme",
			       NULL);
      if (g_file_test (path, G_FILE_TEST_IS_REGULAR)) 
	{
	  theme_file = g_key_file_new ();
	  g_key_file_set_list_separator (theme_file, ',');
	  g_key_file_load_from_file (theme_file, path, 0, &error);
	  if (error)
	    {
	      g_key_file_free (theme_file);
	      theme_file = NULL;
	      g_error_free (error);
	      error = NULL;
	    }
	}
      g_free (path);
    }

  if (theme_file == NULL)
    return;
  
  theme = g_new (IconTheme, 1);
  theme->display_name = 
    g_key_file_get_locale_string (theme_file, "Icon Theme", "Name", NULL, NULL);
  if (!theme->display_name)
    {
      g_warning ("Theme file for %s has no name\n", theme_name);
      g_free (theme);
      g_key_file_free (theme_file);
      return;
    }

  dirs = g_key_file_get_string_list (theme_file, "Icon Theme", "Directories", NULL, NULL);
  if (!dirs)
    {
      g_warning ("Theme file for %s has no directories\n", theme_name);
      g_free (theme->display_name);
      g_free (theme);
      g_key_file_free (theme_file);
      return;
    }
  
  theme->name = g_strdup (theme_name);
  theme->comment = 
    g_key_file_get_locale_string (theme_file, 
				  "Icon Theme", "Comment",
				  NULL, NULL);
  theme->example = 
    g_key_file_get_string (theme_file, 
			   "Icon Theme", "Example",
			   NULL);

  theme->icon_caches = NULL;
  theme->dirs = NULL;
  for (i = 0; dirs[i] != NULL; i++)
    theme_subdir_load (icon_theme, theme, theme_file, dirs[i]);
  
  g_strfreev (dirs);
  
  theme->dirs = g_list_reverse (theme->dirs);

  /* Prepend the finished theme */
  priv->themes = g_list_prepend (priv->themes, theme);

  themes = g_key_file_get_string_list (theme_file,
				       "Icon Theme",
				       "Inherits",
				       NULL,
				       NULL);
  if (themes)
    {
      for (i = 0; themes[i] != NULL; i++)
	insert_theme (icon_theme, themes[i]);
      
      g_strfreev (themes);
    }

  g_key_file_free (theme_file);
}

static void
free_unthemed_icon (UnthemedIcon *unthemed_icon)
{
  if (unthemed_icon->svg_filename)
    g_free (unthemed_icon->svg_filename);
  if (unthemed_icon->no_svg_filename)
    g_free (unthemed_icon->no_svg_filename);
  g_free (unthemed_icon);
}

static void
load_themes (GtkIconTheme *icon_theme)
{
  GtkIconThemePrivate *priv;
  GDir *gdir;
  int base;
  char *dir, *base_name, *dot;
  const char *file;
  char *abs_file;
  UnthemedIcon *unthemed_icon;
  IconSuffix old_suffix, new_suffix;
  GTimeVal tv;
  
  priv = icon_theme->priv;

  priv->all_icons = g_hash_table_new (g_str_hash, g_str_equal);
  
  insert_theme (icon_theme, priv->current_theme);
  
  /* Always look in the "default" icon theme */
  insert_theme (icon_theme, DEFAULT_THEME_NAME);
  priv->themes = g_list_reverse (priv->themes);


  priv->unthemed_icons = g_hash_table_new_full (g_str_hash, g_str_equal,
						g_free, (GDestroyNotify)free_unthemed_icon);

  for (base = 0; base < icon_theme->priv->search_path_len; base++)
    {
      GtkIconCache *cache;
      dir = icon_theme->priv->search_path[base];

      cache = _gtk_icon_cache_new_for_path (dir);

      if (cache != NULL)
	{
	  priv->unthemed_icons_caches = g_list_prepend (priv->unthemed_icons_caches, cache);

	  continue;
	}
	   
      gdir = g_dir_open (dir, 0, NULL);

      if (gdir == NULL)
	continue;
      
      while ((file = g_dir_read_name (gdir)))
	{
	  new_suffix = suffix_from_name (file);

	  if (new_suffix != ICON_SUFFIX_NONE)
	    {
	      abs_file = g_build_filename (dir, file, NULL);

	      base_name = g_strdup (file);
		  
	      dot = strrchr (base_name, '.');
	      if (dot)
		*dot = 0;

	      if ((unthemed_icon = g_hash_table_lookup (priv->unthemed_icons,
							base_name)))
		{
		  if (new_suffix == ICON_SUFFIX_SVG)
		    {
		      if (unthemed_icon->svg_filename)
			g_free (abs_file);
		      else
			unthemed_icon->svg_filename = abs_file;
		    }
		  else
		    {
		      if (unthemed_icon->no_svg_filename)
			{
			  old_suffix = suffix_from_name (unthemed_icon->no_svg_filename);
			  if (new_suffix > old_suffix)
			    {
			      g_free (unthemed_icon->no_svg_filename);
			      unthemed_icon->no_svg_filename = abs_file;			      
			    }
			  else
			    g_free (abs_file);
			}
		      else
			unthemed_icon->no_svg_filename = abs_file;			      
		    }

		  g_free (base_name);
		}
	      else
		{
		  unthemed_icon = g_new0 (UnthemedIcon, 1);
		  
		  if (new_suffix == ICON_SUFFIX_SVG)
		    unthemed_icon->svg_filename = abs_file;
		  else
		    unthemed_icon->no_svg_filename = abs_file;

		  g_hash_table_insert (priv->unthemed_icons,
				       base_name,
				       unthemed_icon);
		  g_hash_table_insert (priv->all_icons,
				       base_name, NULL);
		}
	    }
	}
      g_dir_close (gdir);
    }

  priv->themes_valid = TRUE;
  
  g_get_current_time(&tv);
  priv->last_stat_time = tv.tv_sec;
}

static void
ensure_valid_themes (GtkIconTheme *icon_theme)
{
  GtkIconThemePrivate *priv = icon_theme->priv;
  GTimeVal tv;
  
  if (priv->themes_valid)
    {
      g_get_current_time (&tv);

      if (ABS (tv.tv_sec - priv->last_stat_time) > 5)
	gtk_icon_theme_rescan_if_needed (icon_theme);
    }
  
  if (!priv->themes_valid)
    load_themes (icon_theme);
}

/**
 * gtk_icon_theme_lookup_icon:
 * @icon_theme: a #GtkIconTheme
 * @icon_name: the name of the icon to lookup
 * @size: desired icon size
 * @flags: flags modifying the behavior of the icon lookup
 * 
 * Looks up a named icon and returns a structure containing
 * information such as the filename of the icon. The icon
 * can then be rendered into a pixbuf using
 * gtk_icon_info_load_icon(). (gtk_icon_theme_load_icon()
 * combines these two steps if all you need is the pixbuf.)
 * 
 * Return value: a #GtkIconInfo structure containing information
 * about the icon, or %NULL if the icon wasn't found. Free with
 * gtk_icon_info_free()
 *
 * Since: 2.4
 **/
GtkIconInfo *
gtk_icon_theme_lookup_icon (GtkIconTheme       *icon_theme,
			    const gchar        *icon_name,
			    gint                size,
			    GtkIconLookupFlags  flags)
{
  GtkIconThemePrivate *priv;
  GList *l;
  GtkIconInfo *icon_info = NULL;
  UnthemedIcon *unthemed_icon;
  gboolean allow_svg;
  gboolean use_builtin;
  gboolean found_default;

  g_return_val_if_fail (GTK_IS_ICON_THEME (icon_theme), NULL);
  g_return_val_if_fail (icon_name != NULL, NULL);
  g_return_val_if_fail ((flags & GTK_ICON_LOOKUP_NO_SVG) == 0 ||
			(flags & GTK_ICON_LOOKUP_FORCE_SVG) == 0, NULL);

  priv = icon_theme->priv;

  if (flags & GTK_ICON_LOOKUP_NO_SVG)
    allow_svg = FALSE;
  else if (flags & GTK_ICON_LOOKUP_FORCE_SVG)
    allow_svg = TRUE;
  else
    allow_svg = priv->pixbuf_supports_svg;

  use_builtin = (flags & GTK_ICON_LOOKUP_USE_BUILTIN);

  ensure_valid_themes (icon_theme);

  found_default = FALSE;
  l = priv->themes;
  while (l != NULL)
    {
      IconTheme *theme = l->data;
      
      if (strcmp (theme->name, DEFAULT_THEME_NAME) == 0)
	found_default = TRUE;
      
      icon_info = theme_lookup_icon (theme, icon_name, size, allow_svg, use_builtin);
      if (icon_info)
	goto out;
      
      l = l->next;
    }

  if (!found_default)
    {
      BuiltinIcon *builtin = find_builtin_icon (icon_name, size, NULL, NULL);
      if (builtin)
	{
	  icon_info = icon_info_new_builtin (builtin);
	  goto out;
	}
    }
  
  unthemed_icon = g_hash_table_lookup (priv->unthemed_icons, icon_name);
  if (unthemed_icon)
    {
      icon_info = icon_info_new ();

      /* A SVG icon, when allowed, beats out a XPM icon, but not
       * a PNG icon
       */
      if (allow_svg &&
	  unthemed_icon->svg_filename &&
	  (!unthemed_icon->no_svg_filename ||
	   suffix_from_name (unthemed_icon->no_svg_filename) != ICON_SUFFIX_PNG))
	icon_info->filename = g_strdup (unthemed_icon->svg_filename);
      else if (unthemed_icon->no_svg_filename)
	icon_info->filename = g_strdup (unthemed_icon->no_svg_filename);
#ifdef G_OS_WIN32
      icon_info->cp_filename = g_locale_from_utf8 (icon_info->filename,
						   -1, NULL, NULL, NULL);
#endif

      icon_info->dir_type = ICON_THEME_DIR_UNTHEMED;
    }

 out:
  if (icon_info)
    icon_info->desired_size = size;
  else
    {
      static gboolean check_for_default_theme = TRUE;
      char *default_theme_path;
      gboolean found = FALSE;
      unsigned i;

      if (check_for_default_theme)
	{
	  check_for_default_theme = FALSE;

	  for (i = 0; !found && i < priv->search_path_len; i++)
	    {
	      default_theme_path = g_build_filename (priv->search_path[i],
						     DEFAULT_THEME_NAME,
						     "index.theme",
						     NULL);
	      found = g_file_test (default_theme_path, G_FILE_TEST_IS_REGULAR);
	      g_free (default_theme_path);
	    }
	  if (!found)
	    {
	      g_warning (_("Could not find the icon '%s'. The '%s' theme\n"
			   "was not found either, perhaps you need to install it.\n"
			   "You can get a copy from:\n"
			   "\t%s"),
			 icon_name, DEFAULT_THEME_NAME, "http://freedesktop.org/Software/icon-theme/releases");
	    }
	}
    }

  return icon_info;
}

/* Error quark */
GQuark
gtk_icon_theme_error_quark (void)
{
  static GQuark q = 0;
  if (q == 0)
    q = g_quark_from_static_string ("gtk-icon-theme-error-quark");

  return q;
}

/**
 * gtk_icon_theme_load_icon:
 * @icon_theme: a #GtkIconTheme
 * @icon_name: the name of the icon to lookup
 * @size: the desired icon size. The resulting icon may not be
 *        exactly this size; see gtk_icon_info_load_icon().
 * @flags: flags modifying the behavior of the icon lookup
 * @error: Location to store error information on failure, or %NULL.
 * 
 * Looks up an icon in an icon theme, scales it to the given size
 * and renders it into a pixbuf. This is a convenience function;
 * if more details about the icon are needed, use
 * gtk_icon_theme_lookup_icon() followed by gtk_icon_info_load_icon().
 * 
 * Return value: the rendered icon; this may be a newly created icon
 *  or a new reference to an internal icon, so you must not modify
 *  the icon. Use g_object_unref() to release your reference to the
 *  icon. %NULL if the icon isn't found.
 *
 * Since: 2.4
 **/
GdkPixbuf *
gtk_icon_theme_load_icon (GtkIconTheme         *icon_theme,
			  const gchar          *icon_name,
			  gint                  size,
			  GtkIconLookupFlags    flags,
			  GError              **error)
{
  GtkIconInfo *icon_info;
  GdkPixbuf *pixbuf = NULL;
  
  g_return_val_if_fail (GTK_IS_ICON_THEME (icon_theme), NULL);
  g_return_val_if_fail (icon_name != NULL, NULL);
  g_return_val_if_fail ((flags & GTK_ICON_LOOKUP_NO_SVG) == 0 ||
			(flags & GTK_ICON_LOOKUP_FORCE_SVG) == 0, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  
  icon_info  = gtk_icon_theme_lookup_icon (icon_theme, icon_name, size,
					   flags | GTK_ICON_LOOKUP_USE_BUILTIN);
  if (!icon_info)
    {
      g_set_error (error, GTK_ICON_THEME_ERROR,  GTK_ICON_THEME_NOT_FOUND,
		   _("Icon '%s' not present in theme"), icon_name);
      return NULL;
    }

  pixbuf = gtk_icon_info_load_icon (icon_info, error);
  gtk_icon_info_free (icon_info);

  return pixbuf;
}

typedef struct 
{
  const gchar *icon_name;
  gboolean found;
} CacheSearch;

static void
cache_has_icon (gpointer  key,
		gpointer  value,
		gpointer  user_data)
{
  GtkIconCache *cache = (GtkIconCache *)value;
  CacheSearch *search = (CacheSearch *)user_data;

  if (!cache || search->found)
    return;

  if (_gtk_icon_cache_has_icon (cache, search->icon_name))
    search->found = TRUE;  
}

/**
 * gtk_icon_theme_has_icon:
 * @icon_theme: a #GtkIconTheme
 * @icon_name: the name of an icon
 * 
 * Checks whether an icon theme includes an icon
 * for a particular name.
 * 
 * Return value: %TRUE if @icon_theme includes an
 *  icon for @icon_name.
 *
 * Since: 2.4
 **/
gboolean 
gtk_icon_theme_has_icon (GtkIconTheme *icon_theme,
			 const char   *icon_name)
{
  GtkIconThemePrivate *priv;
  GList *l;
  CacheSearch search;

  g_return_val_if_fail (GTK_IS_ICON_THEME (icon_theme), FALSE);
  
  priv = icon_theme->priv;
  
  ensure_valid_themes (icon_theme);

  search.icon_name = icon_name;
  search.found = FALSE;

  for (l = priv->themes; l; l = l->next)
    {
      IconTheme *theme = (IconTheme *)l->data;

      g_hash_table_foreach (theme->icon_caches, cache_has_icon, &search);

      if (search.found)
	return TRUE;
    }

  for (l = priv->unthemed_icons_caches; l; l = l->next)
    {
      GtkIconCache *cache = (GtkIconCache *)l->data;

      if (_gtk_icon_cache_has_icon (cache, icon_name))
	return TRUE;
    }

  if (g_hash_table_lookup_extended (priv->all_icons,
				    icon_name, NULL, NULL))
    return TRUE;

  if (icon_theme_builtin_icons &&
      g_hash_table_lookup_extended (icon_theme_builtin_icons,
				    icon_name, NULL, NULL))
    return TRUE;

  return FALSE;
}

static void
add_size (gpointer  key,
	  gpointer  value,
	  gpointer  user_data)
{
  gint **res_p = user_data;

  **res_p = GPOINTER_TO_INT (key);

  (*res_p)++;
}

/**
 * gtk_icon_theme_get_icon_sizes:
 * @icon_theme: a #GtkIconTheme
 * @icon_name: the name of an icon
 * 
 * Returns an array of integers describing the sizes at which
 * the icon is available without scaling. A size of -1 means 
 * that the icon is available in a scalable format. The array 
 * is zero-terminated.
 * 
 * Return value: An newly allocated array describing the sizes at
 * which the icon is available. The array should be freed with g_free()
 * when it is no longer needed.
 *
 * Since: 2.6
 **/
gint *
gtk_icon_theme_get_icon_sizes (GtkIconTheme *icon_theme,
			       const char   *icon_name)
{
  GList *l, *d;
  GHashTable *sizes;
  gint *result, *r;
  guint suffix;
  
  GtkIconThemePrivate *priv;

  g_return_val_if_fail (GTK_IS_ICON_THEME (icon_theme), NULL);
  
  priv = icon_theme->priv;

  ensure_valid_themes (icon_theme);

  sizes = g_hash_table_new (g_direct_hash, g_direct_equal);

  for (l = priv->themes; l; l = l->next)
    {
      IconTheme *theme = l->data;
      for (d = theme->dirs; d; d = d->next)
	{
	  IconThemeDir *dir = d->data;

	  suffix = theme_dir_get_icon_suffix (dir, icon_name, NULL);	  
	  if (suffix != ICON_SUFFIX_NONE)
	    {
	      if (suffix == ICON_SUFFIX_SVG)
		g_hash_table_insert (sizes, GINT_TO_POINTER (-1), NULL);
	      else
		g_hash_table_insert (sizes, GINT_TO_POINTER (dir->size), NULL);
	    }
	}
    }

  r = result = g_new0 (gint, g_hash_table_size (sizes) + 1);

  g_hash_table_foreach (sizes, add_size, &r);
  g_hash_table_destroy (sizes);
  
  return result;
}

static void
add_key_to_hash (gpointer  key,
		 gpointer  value,
		 gpointer  user_data)
{
  GHashTable *hash = user_data;

  g_hash_table_insert (hash, key, NULL);
}

static void
add_key_to_list (gpointer  key,
		 gpointer  value,
		 gpointer  user_data)
{
  GList **list = user_data;

  *list = g_list_prepend (*list, g_strdup (key));
}

/**
 * gtk_icon_theme_list_icons:
 * @icon_theme: a #GtkIconTheme
 * @context: a string identifying a particular type of icon,
 *           or %NULL to list all icons.
 * 
 * Lists the icons in the current icon theme. Only a subset
 * of the icons can be listed by providing a context string.
 * The set of values for the context string is system dependent,
 * but will typically include such values as 'apps' and
 * 'mimetypes'.
 * 
 * Return value: a #GList list holding the names of all the
 *  icons in the theme. You must first free each element
 *  in the list with g_free(), then free the list itself
 *  with g_list_free().
 *
 * Since: 2.4
 **/
GList *
gtk_icon_theme_list_icons (GtkIconTheme *icon_theme,
			   const char   *context)
{
  GtkIconThemePrivate *priv;
  GHashTable *icons;
  GList *list, *l;
  GQuark context_quark;
  
  priv = icon_theme->priv;
  
  ensure_valid_themes (icon_theme);

  if (context)
    {
      context_quark = g_quark_try_string (context);

      if (!context_quark)
	return NULL;
    }
  else
    context_quark = 0;

  icons = g_hash_table_new (g_str_hash, g_str_equal);
  
  l = priv->themes;
  while (l != NULL)
    {
      theme_list_icons (l->data, icons, context_quark);
      l = l->next;
    }

  if (context_quark == 0)
    g_hash_table_foreach (priv->unthemed_icons,
			  add_key_to_hash,
			  icons);

  list = NULL;
  
  g_hash_table_foreach (icons,
			add_key_to_list,
			&list);

  g_hash_table_destroy (icons);
  
  return list;
}

/**
 * gtk_icon_theme_get_example_icon_name:
 * @icon_theme: a #GtkIconTheme
 * 
 * Gets the name of an icon that is representative of the
 * current theme (for instance, to use when presenting
 * a list of themes to the user.)
 * 
 * Return value: the name of an example icon or %NULL.
 *  Free with g_free().
 *
 * Since: 2.4
 **/
char *
gtk_icon_theme_get_example_icon_name (GtkIconTheme *icon_theme)
{
  GtkIconThemePrivate *priv;
  GList *l;
  IconTheme *theme;

  g_return_val_if_fail (GTK_IS_ICON_THEME (icon_theme), NULL);
  
  priv = icon_theme->priv;
  
  ensure_valid_themes (icon_theme);

  l = priv->themes;
  while (l != NULL)
    {
      theme = l->data;
      if (theme->example)
	return g_strdup (theme->example);
      
      l = l->next;
    }
  
  return NULL;
}

/**
 * gtk_icon_theme_rescan_if_needed:
 * @icon_theme: a #GtkIconTheme
 * 
 * Checks to see if the icon theme has changed; if it has, any
 * currently cached information is discarded and will be reloaded
 * next time @icon_theme is accessed.
 * 
 * Return value: %TRUE if the icon theme has changed and needed
 *   to be reloaded.
 *
 * Since: 2.4
 **/
gboolean
gtk_icon_theme_rescan_if_needed (GtkIconTheme *icon_theme)
{
  GtkIconThemePrivate *priv;
  IconThemeDirMtime *dir_mtime;
  GList *d;
  int stat_res;
  struct stat stat_buf;
  GTimeVal tv;

  g_return_val_if_fail (GTK_IS_ICON_THEME (icon_theme), FALSE);

  priv = icon_theme->priv;
  
  for (d = priv->dir_mtimes; d != NULL; d = d->next)
    {
      dir_mtime = d->data;

      stat_res = g_stat (dir_mtime->dir, &stat_buf);

      /* dir mtime didn't change */
      if (stat_res == 0 && 
	  S_ISDIR (stat_buf.st_mode) &&
	  dir_mtime->mtime == stat_buf.st_mtime)
	continue;
      /* didn't exist before, and still doesn't */
      if (dir_mtime->mtime == 0 &&
	  (stat_res != 0 || !S_ISDIR (stat_buf.st_mode)))
	continue;
	  
      do_theme_change (icon_theme);
      return TRUE;
    }
  
  g_get_current_time (&tv);
  priv->last_stat_time = tv.tv_sec;

  return FALSE;
}

static void
theme_destroy (IconTheme *theme)
{
  g_free (theme->display_name);
  g_free (theme->comment);
  g_free (theme->name);
  g_free (theme->example);

  g_list_foreach (theme->dirs, (GFunc)theme_dir_destroy, NULL);
  g_list_free (theme->dirs);
  
  if (theme->icon_caches)
    g_hash_table_destroy (theme->icon_caches);

  g_free (theme);
}

static void
theme_dir_destroy (IconThemeDir *dir)
{
  if (dir->cache)
      _gtk_icon_cache_unref (dir->cache);
  else
    g_hash_table_destroy (dir->icons);
  
  if (dir->icon_data)
    g_hash_table_destroy (dir->icon_data);
  g_free (dir->dir);
  g_free (dir->subdir);
  g_free (dir);
}

static int
theme_dir_size_difference (IconThemeDir *dir, int size, gboolean *smaller)
{
  int min, max;
  switch (dir->type)
    {
    case ICON_THEME_DIR_FIXED:
      *smaller = size < dir->size;
      return abs (size - dir->size);
      break;
    case ICON_THEME_DIR_SCALABLE:
      *smaller = size < dir->min_size;
      if (size < dir->min_size)
	return dir->min_size - size;
      if (size > dir->max_size)
	return size - dir->max_size;
      return 0;
      break;
    case ICON_THEME_DIR_THRESHOLD:
      min = dir->size - dir->threshold;
      max = dir->size + dir->threshold;
      *smaller = size < min;
      if (size < min)
	return min - size;
      if (size > max)
	return size - max;
      return 0;
      break;
    case ICON_THEME_DIR_UNTHEMED:
      g_assert_not_reached ();
      break;
    }
  g_assert_not_reached ();
  return 1000;
}

static const char *
string_from_suffix (IconSuffix suffix)
{
  switch (suffix)
    {
    case ICON_SUFFIX_XPM:
      return ".xpm";
    case ICON_SUFFIX_SVG:
      return ".svg";
    case ICON_SUFFIX_PNG:
      return ".png";
    default:
      g_assert_not_reached();
    }
  return NULL;
}

static IconSuffix
suffix_from_name (const char *name)
{
  IconSuffix retval;

  if (g_str_has_suffix (name, ".png"))
    retval = ICON_SUFFIX_PNG;
  else if (g_str_has_suffix (name, ".svg"))
    retval = ICON_SUFFIX_SVG;
  else if (g_str_has_suffix (name, ".xpm"))
    retval = ICON_SUFFIX_XPM;
  else
    retval = ICON_SUFFIX_NONE;

  return retval;
}

static IconSuffix
best_suffix (IconSuffix suffix,
	     gboolean   allow_svg)
{
  if ((suffix & ICON_SUFFIX_PNG) != 0)
    return ICON_SUFFIX_PNG;
  else if (allow_svg && ((suffix & ICON_SUFFIX_SVG) != 0))
    return ICON_SUFFIX_SVG;
  else if ((suffix & ICON_SUFFIX_XPM) != 0)
    return ICON_SUFFIX_XPM;
  else
    return ICON_SUFFIX_NONE;
}


static IconSuffix
theme_dir_get_icon_suffix (IconThemeDir *dir,
			   const gchar  *icon_name,
			   gboolean     *has_icon_file)
{
  IconSuffix suffix;

  if (dir->cache)
    {
      suffix = (IconSuffix)_gtk_icon_cache_get_icon_flags (dir->cache,
							   icon_name,
							   dir->subdir);

      if (has_icon_file)
	*has_icon_file = suffix & HAS_ICON_FILE;

      suffix = suffix & ~HAS_ICON_FILE;
    }
  else
      suffix = GPOINTER_TO_UINT (g_hash_table_lookup (dir->icons, icon_name));

  return suffix;
}

static GtkIconInfo *
theme_lookup_icon (IconTheme          *theme,
		   const char         *icon_name,
		   int                 size,
		   gboolean            allow_svg,
		   gboolean            use_builtin)
{
  GList *l;
  IconThemeDir *dir, *min_dir;
  char *file;
  int min_difference, difference;
  BuiltinIcon *closest_builtin = NULL;
  gboolean smaller, has_larger;
  IconSuffix suffix;

  min_difference = G_MAXINT;
  min_dir = NULL;
  has_larger = FALSE;

  /* Builtin icons are logically part of the default theme and
   * are searched before other subdirectories of the default theme.
   */
  if (strcmp (theme->name, DEFAULT_THEME_NAME) == 0 && use_builtin)
    {
      closest_builtin = find_builtin_icon (icon_name, size,
					   &min_difference,
					   &has_larger);

      if (min_difference == 0)
	return icon_info_new_builtin (closest_builtin);
    }

  l = theme->dirs;
  while (l != NULL)
    {
      dir = l->data;

      suffix = theme_dir_get_icon_suffix (dir, icon_name, NULL);
      if (best_suffix (suffix, allow_svg) != ICON_SUFFIX_NONE)
	{
	  difference = theme_dir_size_difference (dir, size, &smaller);

	  if (difference == 0)
	    {
	      min_dir = dir;
	      break;
	    }

	  if (!has_larger)
	    {
	      if (difference < min_difference || smaller)
		{
		  min_difference = difference;
		  min_dir = dir;
		  closest_builtin = NULL;
		  has_larger = smaller;
		}
	    }
	  else
	    {
	      if (difference < min_difference && smaller)
		{
		  min_difference = difference;
		  min_dir = dir;
		  closest_builtin = NULL;
		}
	    }

	}

      l = l->next;
    }

  if (closest_builtin)
    return icon_info_new_builtin (closest_builtin);
  
  if (min_dir)
    {
      GtkIconInfo *icon_info = icon_info_new ();
      gboolean has_icon_file;
      
      suffix = theme_dir_get_icon_suffix (min_dir, icon_name, &has_icon_file);
      suffix = best_suffix (suffix, allow_svg);
      g_assert (suffix != ICON_SUFFIX_NONE);
      
      file = g_strconcat (icon_name, string_from_suffix (suffix), NULL);
      icon_info->filename = g_build_filename (min_dir->dir, file, NULL);
      g_free (file);
#ifdef G_OS_WIN32
      icon_info->cp_filename = g_locale_from_utf8 (icon_info->filename,
						   -1, NULL, NULL, NULL);
#endif
      
      if (min_dir->icon_data != NULL)
	icon_info->data = g_hash_table_lookup (min_dir->icon_data, icon_name);
      
      if (icon_info->data == NULL &&
	  min_dir->cache && has_icon_file)
	{
	  gchar *icon_file_name, *icon_file_path;

	  icon_file_name = g_strconcat (icon_name, ".icon", NULL);
	  icon_file_path = g_build_filename (min_dir->dir, icon_file_name, NULL);

	  if (g_file_test (icon_file_path, G_FILE_TEST_IS_REGULAR))
	    {
	      if (min_dir->icon_data == NULL)
		min_dir->icon_data = g_hash_table_new_full (g_str_hash, g_str_equal,
							    g_free, (GDestroyNotify)icon_data_free);
	      load_icon_data (min_dir, icon_file_path, icon_file_name);
	      
	      icon_info->data = g_hash_table_lookup (min_dir->icon_data, icon_name);
	    }
	  g_free (icon_file_name);
	  g_free (icon_file_path);
	}

      icon_info->dir_type = min_dir->type;
      icon_info->dir_size = min_dir->size;
      icon_info->threshold = min_dir->threshold;
      
      return icon_info;
    }
 
  return NULL;
}

static void
theme_list_icons (IconTheme *theme, GHashTable *icons,
		  GQuark context)
{
  GList *l = theme->dirs;
  IconThemeDir *dir;
  
  while (l != NULL)
    {
      dir = l->data;

      if (context == dir->context ||
	  context == 0)
	{
	  if (dir->cache)
	    {
	      _gtk_icon_cache_add_icons (dir->cache,
					 dir->subdir,
					 icons);
					 
	    }
	  else
	    {
	      g_hash_table_foreach (dir->icons,
				    add_key_to_hash,
				    icons);
	    }
	}
      l = l->next;
    }
}

static void
load_icon_data (IconThemeDir *dir, const char *path, const char *name)
{
  GKeyFile *icon_file;
  char *base_name;
  char **split;
  gsize length;
  char *dot;
  char *str;
  char *split_point;
  int i;
  gint *ivalues;
  GError *error = NULL;
  
  GtkIconData *data;

  icon_file = g_key_file_new ();
  g_key_file_set_list_separator (icon_file, ',');
  g_key_file_load_from_file (icon_file, path, 0, &error);
  if (error)
    {
      g_error_free (error);
      return;
    }
  else
    {
      base_name = g_strdup (name);
      dot = strrchr (base_name, '.');
      *dot = 0;
      
      data = g_new0 (GtkIconData, 1);
      g_hash_table_replace (dir->icon_data, base_name, data);
      
      ivalues = g_key_file_get_integer_list (icon_file, 
					     "Icon Data", "EmbeddedTextRectangle",
					      &length, NULL);
      if (ivalues)
	{
	  if (length == 4)
	    {
	      data->has_embedded_rect = TRUE;
	      data->x0 = ivalues[0];
	      data->y0 = ivalues[1];
	      data->x1 = ivalues[2];
	      data->y1 = ivalues[3];
	    }
	  
	  g_free (ivalues);
	}
      
      str = g_key_file_get_string (icon_file, "Icon Data", "AttachPoints", NULL);
      if (str)
	{
	  split = g_strsplit (str, "|", -1);
	  
	  data->n_attach_points = g_strv_length (split);
	  data->attach_points = g_malloc (sizeof (GdkPoint) * data->n_attach_points);

	  i = 0;
	  while (split[i] != NULL && i < data->n_attach_points)
	    {
	      split_point = strchr (split[i], ',');
	      if (split_point)
		{
		  *split_point = 0;
		  split_point++;
		  data->attach_points[i].x = atoi (split[i]);
		  data->attach_points[i].y = atoi (split_point);
		}
	      i++;
	    }
	  
	  g_strfreev (split);
	  g_free (str);
	}
      
      data->display_name = g_key_file_get_locale_string (icon_file, 
							 "Icon Data", "DisplayName",
							 NULL, NULL);
      g_key_file_free (icon_file);
    }
}

static void
scan_directory (GtkIconThemePrivate *icon_theme,
		IconThemeDir *dir, char *full_dir)
{
  GDir *gdir;
  const char *name;
  char *base_name, *dot;
  char *path;
  IconSuffix suffix, hash_suffix;

  GTK_NOTE (ICONTHEME, 
	    g_print ("scanning directory %s\n", full_dir));
  dir->icons = g_hash_table_new_full (g_str_hash, g_str_equal,
				      g_free, NULL);
  
  gdir = g_dir_open (full_dir, 0, NULL);

  if (gdir == NULL)
    return;

  while ((name = g_dir_read_name (gdir)))
    {
      if (g_str_has_suffix (name, ".icon"))
	{
	  if (dir->icon_data == NULL)
	    dir->icon_data = g_hash_table_new_full (g_str_hash, g_str_equal,
						    g_free, (GDestroyNotify)icon_data_free);
	  
	  path = g_build_filename (full_dir, name, NULL);
	  if (g_file_test (path, G_FILE_TEST_IS_REGULAR))
	    load_icon_data (dir, path, name);
	  
	  g_free (path);
	  
	  continue;
	}

      suffix = suffix_from_name (name);
      if (suffix == ICON_SUFFIX_NONE)
	continue;
      
      base_name = g_strdup (name);
      dot = strrchr (base_name, '.');
      *dot = 0;
      
      hash_suffix = GPOINTER_TO_INT (g_hash_table_lookup (dir->icons, base_name));
      g_hash_table_replace (dir->icons, base_name, GUINT_TO_POINTER (hash_suffix| suffix));
      g_hash_table_insert (icon_theme->all_icons, base_name, NULL);
    }
  
  g_dir_close (gdir);
}

static void
theme_subdir_load (GtkIconTheme *icon_theme,
		   IconTheme    *theme,
		   GKeyFile     *theme_file,
		   char         *subdir)
{
  GList *d;
  char *type_string;
  IconThemeDir *dir;
  IconThemeDirType type;
  char *context_string;
  GQuark context;
  int size;
  int min_size;
  int max_size;
  int threshold;
  char *full_dir;
  GError *error = NULL;
  GtkIconCache *cache;
  IconThemeDirMtime *dir_mtime;

  size = g_key_file_get_integer (theme_file, subdir, "Size", &error);
  if (error)
    {
      g_error_free (error);
      g_warning ("Theme directory %s of theme %s has no size field\n", 
		 subdir, theme->name);
      return;
    }
  
  type = ICON_THEME_DIR_THRESHOLD;
  type_string = g_key_file_get_string (theme_file, subdir, "Type", NULL);
  if (type_string)
    {
      if (strcmp (type_string, "Fixed") == 0)
	type = ICON_THEME_DIR_FIXED;
      else if (strcmp (type_string, "Scalable") == 0)
	type = ICON_THEME_DIR_SCALABLE;
      else if (strcmp (type_string, "Threshold") == 0)
	type = ICON_THEME_DIR_THRESHOLD;

      g_free (type_string);
    }
  
  context = 0;
  context_string = g_key_file_get_string (theme_file, subdir, "Context", NULL);
  if (context_string)
    {
      context = g_quark_from_string (context_string);
      g_free (context_string);
    }

  max_size = g_key_file_get_integer (theme_file, subdir, "MaxSize", &error);
  if (error)
    {
      max_size = size;

      g_error_free (error);
      error = NULL;
    }

  min_size = g_key_file_get_integer (theme_file, subdir, "MinSize", &error);
  if (error)
    {
      min_size = size;

      g_error_free (error);
      error = NULL;
    }
  
  threshold = g_key_file_get_integer (theme_file, subdir, "Threshold", &error);
  if (error)
    {
      threshold = 2;

      g_error_free (error);
      error = NULL;
    }

  for (d = icon_theme->priv->dir_mtimes; d; d = d->next)
    {
      dir_mtime = (IconThemeDirMtime *)d->data;

      if (dir_mtime->mtime == 0)
	continue; /* directory doesn't exist */

       full_dir = g_build_filename (dir_mtime->dir, subdir, NULL);

      /* First, see if we have a cache for the directory */
      if (!theme->icon_caches)
	theme->icon_caches = g_hash_table_new_full (g_str_hash, g_str_equal,
						    g_free, (GDestroyNotify)free_cache);
						   
      if (!g_hash_table_lookup_extended (theme->icon_caches, dir_mtime->dir, 
					 NULL, (gpointer)&cache))
	{
	  /* This will return NULL if the cache doesn't exist or is outdated */
	  cache = _gtk_icon_cache_new_for_path (dir_mtime->dir);

	  g_hash_table_insert (theme->icon_caches, g_strdup (dir_mtime->dir), cache);
	}

      if (cache != NULL || g_file_test (full_dir, G_FILE_TEST_IS_DIR))
	{
	  dir = g_new (IconThemeDir, 1);
	  dir->type = type;
	  dir->context = context;
	  dir->size = size;
	  dir->min_size = min_size;
	  dir->max_size = max_size;
	  dir->threshold = threshold;
	  dir->dir = full_dir;
	  dir->icon_data = NULL;
	  dir->subdir = g_strdup (subdir);
	  if (cache != NULL)
	    dir->cache = _gtk_icon_cache_ref (cache);
	  else
	    {
	      dir->cache = NULL;
	      scan_directory (icon_theme->priv, dir, full_dir);
	    }

	  theme->dirs = g_list_prepend (theme->dirs, dir);
	}
      else
	g_free (full_dir);
    }
}

static void
icon_data_free (GtkIconData *icon_data)
{
  g_free (icon_data->attach_points);
  g_free (icon_data->display_name);
  g_free (icon_data);
}

/*
 * GtkIconInfo
 */
GType
gtk_icon_info_get_type (void)
{
  static GType our_type = 0;
  
  if (our_type == 0)
    our_type = g_boxed_type_register_static ("GtkIconInfo",
					     (GBoxedCopyFunc) gtk_icon_info_copy,
					     (GBoxedFreeFunc) gtk_icon_info_free);

  return our_type;
}

static GtkIconInfo *
icon_info_new (void)
{
  GtkIconInfo *icon_info = g_new0 (GtkIconInfo, 1);

  icon_info->scale = -1.;

  return icon_info;
}

static GtkIconInfo *
icon_info_new_builtin (BuiltinIcon *icon)
{
  GtkIconInfo *icon_info = icon_info_new ();

  icon_info->builtin_pixbuf = g_object_ref (icon->pixbuf);
  icon_info->dir_type = ICON_THEME_DIR_THRESHOLD;
  icon_info->dir_size = icon->size;
  icon_info->threshold = 2;
  
  return icon_info;
}

/**
 * gtk_icon_info_copy:
 * @icon_info: a #GtkIconInfo
 * 
 * Make a copy of a #GtkIconInfo.
 * 
 * Return value: the new GtkIconInfo
 *
 * Since: 2.4
 **/
GtkIconInfo *
gtk_icon_info_copy (GtkIconInfo *icon_info)
{
  GtkIconInfo *copy;
  
  g_return_val_if_fail (icon_info != NULL, NULL);

  copy = g_memdup (icon_info, sizeof (GtkIconInfo));
  if (copy->builtin_pixbuf)
    g_object_ref (copy->builtin_pixbuf);
  if (copy->pixbuf)
    g_object_ref (copy->pixbuf);
  if (copy->load_error)
    copy->load_error = g_error_copy (copy->load_error);
  if (copy->filename)
    copy->filename = g_strdup (copy->filename);
#ifdef G_OS_WIN32
  if (copy->cp_filename)
    copy->cp_filename = g_strdup (copy->cp_filename);
#endif

  return copy;
}

/**
 * gtk_icon_info_free:
 * @icon_info: a #GtkIconInfo
 * 
 * Free a #GtkIconInfo and associated information
 *
 * Since: 2.4
 **/
void
gtk_icon_info_free (GtkIconInfo *icon_info)
{
  g_return_if_fail (icon_info != NULL);

  if (icon_info->filename)
    g_free (icon_info->filename);
#ifdef G_OS_WIN32
  if (icon_info->cp_filename)
    g_free (icon_info->cp_filename);
#endif
  if (icon_info->builtin_pixbuf)
    g_object_unref (icon_info->builtin_pixbuf);
  if (icon_info->pixbuf)
    g_object_unref (icon_info->pixbuf);
  
  g_free (icon_info);
}

/**
 * gtk_icon_info_get_base_size:
 * @icon_info: a #GtkIconInfo
 * 
 * Gets the base size for the icon. The base size
 * is a size for the icon that was specified by
 * the icon theme creator. This may be different
 * than the actual size of image; an example of
 * this is small emblem icons that can be attached
 * to a larger icon. These icons will be given
 * the same base size as the larger icons to which
 * they are attached.
 * 
 * Return value: the base size, or 0, if no base
 *  size is known for the icon.
 *
 * Since: 2.4
 **/
gint
gtk_icon_info_get_base_size (GtkIconInfo *icon_info)
{
  g_return_val_if_fail (icon_info != NULL, 0);

  return icon_info->dir_size;
}

/**
 * gtk_icon_info_get_filename:
 * @icon_info: a #GtkIconInfo
 * 
 * Gets the filename for the icon. If the
 * %GTK_ICON_LOOKUP_USE_BUILTIN flag was passed
 * to gtk_icon_theme_lookup_icon(), there may be
 * no filename if a builtin icon is returned; in this
 * case, you should use gtk_icon_info_get_builtin_pixbuf().
 * 
 * Return value: the filename for the icon, or %NULL
 *  if gtk_icon_info_get_builtin_pixbuf() should
 *  be used instead. The return value is owned by
 *  GTK+ and should not be modified or freed.
 *
 * Since: 2.4
 **/
G_CONST_RETURN gchar *
gtk_icon_info_get_filename (GtkIconInfo *icon_info)
{
  g_return_val_if_fail (icon_info != NULL, NULL);

  return icon_info->filename;
}

/**
 * gtk_icon_info_get_builtin_pixbuf:
 * @icon_info: a #GtkIconInfo structure
 * 
 * Gets the built-in image for this icon, if any. To allow
 * GTK+ to use built in icon images, you must pass the
 * %GTK_ICON_LOOKUP_USE_BUILTIN to
 * gtk_icon_theme_lookup_icon().
 * 
 * Return value: the built-in image pixbuf, or %NULL. No
 *  extra reference is added to the returned pixbuf, so if
 *  you want to keep it around, you must use g_object_ref().
 *  The returned image must not be modified.
 *
 * Since: 2.4
 **/
GdkPixbuf *
gtk_icon_info_get_builtin_pixbuf (GtkIconInfo *icon_info)
{
  g_return_val_if_fail (icon_info != NULL, NULL);

  return icon_info->builtin_pixbuf;
}

static GdkPixbuf *
load_svg_at_size (const gchar *filename,
		  gint         size,
		  GError      **error)
{
  GdkPixbuf *pixbuf = NULL;
  GdkPixbufLoader *loader = NULL;
  gchar *contents = NULL;
  gsize length;
  
  if (!g_file_get_contents (filename,
			    &contents, &length, error))
    goto bail;
  
  loader = gdk_pixbuf_loader_new ();
  gdk_pixbuf_loader_set_size (loader, size, size);
  
  if (!gdk_pixbuf_loader_write (loader, contents, length, error))
    {
      gdk_pixbuf_loader_close (loader, NULL);
      goto bail;
    }
  
  if (!gdk_pixbuf_loader_close (loader, error))
    goto bail;
  
  pixbuf = g_object_ref (gdk_pixbuf_loader_get_pixbuf (loader));
  
 bail:
  if (loader)
    g_object_unref (loader);
  if (contents)
    g_free (contents);
  
  return pixbuf;
}

/* This function contains the complicatd logic for deciding
 * on the size at which to load the icon and loading it at
 * that size.
 */
static gboolean
icon_info_ensure_scale_and_pixbuf (GtkIconInfo *icon_info,
				   gboolean     scale_only)
{
  int image_width, image_height;
  GdkPixbuf *source_pixbuf;

  /* First check if we already succeeded have the necessary
   * information (or failed earlier)
   */
  if (scale_only && icon_info->scale >= 0)
    return TRUE;

  if (icon_info->pixbuf)
    return TRUE;

  if (icon_info->load_error)
    return FALSE;

  /* SVG icons are a special case - we just immediately scale them
   * to the desired size
   */
  if (icon_info->filename && g_str_has_suffix (icon_info->filename, ".svg"))
    {
      icon_info->scale = icon_info->desired_size / 1000.;

      if (scale_only)
	return TRUE;
      
      icon_info->pixbuf = load_svg_at_size (icon_info->filename,
					    icon_info->desired_size,
					    &icon_info->load_error);

      return icon_info->pixbuf != NULL;
    }

  /* In many cases, the scale can be determined without actual access
   * to the icon file. This is generally true when we have a size
   * for the directory where the icon is; the image size doesn't
   * matter in that case.
   */
  if (icon_info->dir_type == ICON_THEME_DIR_FIXED)
    icon_info->scale = 1.0;
  else if (icon_info->dir_type == ICON_THEME_DIR_THRESHOLD)
    {
      if (icon_info->desired_size >= icon_info->dir_size - icon_info->threshold &&
	  icon_info->desired_size <= icon_info->dir_size + icon_info->threshold)
	icon_info->scale = 1.0;
      else if (icon_info->dir_size > 0)
	icon_info->scale =(gdouble) icon_info->desired_size / icon_info->dir_size;
    }
  else if (icon_info->dir_type == ICON_THEME_DIR_SCALABLE)
    {
      if (icon_info->dir_size > 0)
	icon_info->scale = (gdouble) icon_info->desired_size / icon_info->dir_size;
    }

  if (icon_info->scale >= 0. && scale_only)
    return TRUE;

  /* At this point, we need to actually get the icon; either from the
   * builting image or by loading the file
   */
  if (icon_info->builtin_pixbuf)
    source_pixbuf = g_object_ref (icon_info->builtin_pixbuf);
  else
    {
      source_pixbuf = gdk_pixbuf_new_from_file (icon_info->filename,
						&icon_info->load_error);
      if (!source_pixbuf)
	return FALSE;
    }

  /* Do scale calculations that depend on the image size
   */
  image_width = gdk_pixbuf_get_width (source_pixbuf);
  image_height = gdk_pixbuf_get_height (source_pixbuf);

  if (icon_info->scale < 0.0)
    {
      gint image_size = MAX (image_width, image_height);
      if (image_size > 0)
	icon_info->scale = icon_info->desired_size / (gdouble)image_size;
      else
	icon_info->scale = 1.0;
      
      if (icon_info->dir_type == ICON_THEME_DIR_UNTHEMED)
	icon_info->scale = MIN (icon_info->scale, 1.0);
    }

  /* We don't short-circuit out here for scale_only, since, now
   * we've loaded the icon, we might as well go ahead and finish
   * the job. This is a bit of a waste when we scale here
   * and never get the final pixbuf; at the cost of a bit of
   * extra complexity, we could keep the source pixbuf around
   * but not actually scale it until neede.
   */
    
  if (icon_info->scale == 1.0)
    icon_info->pixbuf = source_pixbuf;
  else
    {
      icon_info->pixbuf = gdk_pixbuf_scale_simple (source_pixbuf,
						   0.5 + image_width * icon_info->scale,
						   0.5 + image_height * icon_info->scale,
						   GDK_INTERP_BILINEAR);
      g_object_unref (source_pixbuf);
    }

  return TRUE;
}

/**
 * gtk_icon_info_load_icon:
 * @icon_info: a #GtkIconInfo structure from gtk_icon_theme_lookup_icon()
 * @error: 
 * 
 * Renders an icon previously looked up in an icon theme using
 * gtk_icon_theme_lookup_icon(); the size will be based on the size
 * passed to gtk_icon_theme_lookup_icon(). Note that the resulting
 * pixbuf may not be exactly this size; an icon theme may have icons
 * that differ slightly from their nominal sizes, and in addition GTK+
 * will avoid scaling icons that it considers sufficiently close to the
 * requested size or for which the source image would have to be scaled
 * up too far. (This maintains sharpness.) 
 * 
 * Return value: the rendered icon; this may be a newly created icon
 *  or a new reference to an internal icon, so you must not modify
 *  the icon. Use g_object_unref() to release your reference to the
 *  icon.
 *
 * Since: 2.4
 **/
GdkPixbuf *
gtk_icon_info_load_icon (GtkIconInfo *icon_info,
			 GError     **error)
{
  g_return_val_if_fail (icon_info != NULL, NULL);

  g_return_val_if_fail (icon_info != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  icon_info_ensure_scale_and_pixbuf (icon_info, FALSE);

  if (icon_info->load_error)
    {
      g_propagate_error (error, icon_info->load_error);
      return NULL;
    }

  return g_object_ref (icon_info->pixbuf);
}

/**
 * gtk_icon_info_set_raw_coordinates:
 * @icon_info: a #GtkIconInfo
 * @raw_coordinates: whether the coordinates of embedded rectangles
 *   and attached points should be returned in their original
 *   (unscaled) form.
 * 
 * Sets whether the coordinates returned by gtk_icon_info_get_embedded_rect()
 * and gtk_icon_info_get_attach_points() should be returned in their
 * original form as specified in the icon theme, instead of scaled
 * appropriately for the pixbuf returned by gtk_icon_info_load_icon().
 *
 * Raw coordinates are somewhat strange; they are specified to be with
 * respect to the unscaled pixmap for PNG and XPM icons, but for SVG
 * icons, they are in a 1000x1000 coordinate space that is scaled
 * to the final size of the icon.  You can determine if the icon is an SVG
 * icon by using gtk_icon_info_get_filename(), and seeing if it is non-%NULL
 * and ends in '.svg'.
 *
 * This function is provided primarily to allow compatibility wrappers
 * for older API's, and is not expected to be useful for applications.
 *
 * Since: 2.4
 **/
void
gtk_icon_info_set_raw_coordinates (GtkIconInfo *icon_info,
				   gboolean     raw_coordinates)
{
  g_return_if_fail (icon_info != NULL);
  
  icon_info->raw_coordinates = raw_coordinates != FALSE;
}

/* Scale coordinates from the icon data prior to returning
 * them to the user.
 */
static gboolean
icon_info_scale_point (GtkIconInfo  *icon_info,
		       gint          x,
		       gint          y,
		       gint         *x_out,
		       gint         *y_out)
{
  if (icon_info->raw_coordinates)
    {
      *x_out = x;
      *y_out = y;
    }
  else
    {
      if (!icon_info_ensure_scale_and_pixbuf (icon_info, TRUE))
	return FALSE;

      *x_out = 0.5 + x * icon_info->scale;
      *y_out = 0.5 + y * icon_info->scale;
    }

  return TRUE;
}

/**
 * gtk_icon_info_get_embedded_rect:
 * @icon_info: a #GtkIconInfo
 * @rectangle: #GdkRectangle in which to store embedded
 *   rectangle coordinates; coordinates are only stored
 *   when this function returns %TRUE.
 *
 * Gets the coordinates of a rectangle within the icon
 * that can be used for display of information such
 * as a preview of the contents of a text file.
 * See gtk_icon_info_set_raw_coordinates() for further
 * information about the coordinate system.
 * 
 * Return value: %TRUE if the icon has an embedded rectangle
 *
 * Since: 2.4
 **/
gboolean
gtk_icon_info_get_embedded_rect (GtkIconInfo  *icon_info,
				 GdkRectangle *rectangle)
{
  g_return_val_if_fail (icon_info != NULL, FALSE);

  if (icon_info->data && icon_info->data->has_embedded_rect &&
      icon_info_ensure_scale_and_pixbuf (icon_info, TRUE))
    {
      gint scaled_x0, scaled_y0;
      gint scaled_x1, scaled_y1;
      
      if (rectangle)
	{
	  icon_info_scale_point (icon_info,
				 icon_info->data->x0, icon_info->data->y0,
				 &scaled_x0, &scaled_y0);
	  icon_info_scale_point (icon_info,
				 icon_info->data->x1, icon_info->data->y1,
				 &scaled_x1, &scaled_y1);
	  
	  rectangle->x = scaled_x0;
	  rectangle->y = scaled_y0;
	  rectangle->width = scaled_x1 - rectangle->x;
	  rectangle->height = scaled_y1 - rectangle->y;
	}

      return TRUE;
    }
  else
    return FALSE;
}

/**
 * gtk_icon_info_get_attach_points:
 * @icon_info: a #GtkIconInfo
 * @points: location to store pointer to an array of points, or %NULL
 *          free the array of points with g_free().
 * @n_points: location to store the number of points in @points, or %NULL
 * 
 * Fetches the set of attach points for an icon. An attach point
 * is a location in the icon that can be used as anchor points for attaching
 * emblems or overlays to the icon.
 * 
 * Return value: %TRUE if there are any attach points for the icon.
 *
 * Since: 2.4
 **/
gboolean
gtk_icon_info_get_attach_points (GtkIconInfo *icon_info,
				 GdkPoint   **points,
				 gint        *n_points)
{
  g_return_val_if_fail (icon_info != NULL, FALSE);
  
  if (icon_info->data && icon_info->data->n_attach_points &&
      icon_info_ensure_scale_and_pixbuf (icon_info, TRUE))
    {
      if (points)
	{
	  gint i;
	  
	  *points = g_new (GdkPoint, icon_info->data->n_attach_points);
	  for (i = 0; i < icon_info->data->n_attach_points; i++)
	    icon_info_scale_point (icon_info,
				   icon_info->data->attach_points[i].x,
				   icon_info->data->attach_points[i].y,
				   &(*points)[i].x,
				   &(*points)[i].y);
	}
	  
      if (n_points)
	*n_points = icon_info->data->n_attach_points;

      return TRUE;
    }
  else
    {
      if (points)
	*points = NULL;
      if (n_points)
	*n_points = 0;
      
      return FALSE;
    }
}

/**
 * gtk_icon_info_get_display_name:
 * @icon_info: a #GtkIconInfo
 * 
 * Gets the display name for an icon. A display name is a
 * string to be used in place of the icon name in a user
 * visible context like a list of icons.
 * 
 * Return value: the display name for the icon or %NULL, if
 *  the icon doesn't have a specified display name. This value
 *  is owned @icon_info and must not be modified or free.
 *
 * Since: 2.4
 **/
G_CONST_RETURN gchar *
gtk_icon_info_get_display_name  (GtkIconInfo *icon_info)
{
  g_return_val_if_fail (icon_info != NULL, NULL);

  if (icon_info->data)
    return icon_info->data->display_name;
  else
    return NULL;
}

/*
 * Builtin icons
 */


/**
 * gtk_icon_theme_add_builtin_icon:
 * @icon_name: the name of the icon to register
 * @size: the size at which to register the icon (different
 *        images can be registered for the same icon name
 *        at different sizes.)
 * @pixbuf: #GdkPixbuf that contains the image to use
 *          for @icon_name.
 * 
 * Registers a built-in icon for icon theme lookups. The idea
 * of built-in icons is to allow an application or library
 * that uses themed icons to function requiring files to
 * be present in the file system. For instance, the default
 * images for all of GTK+'s stock icons are registered
 * as built-icons.
 *
 * In general, if you use gtk_icon_theme_add_builtin_icon()
 * you should also install the icon in the icon theme, so
 * that the icon is generally available.
 *
 * This function will generally be used with pixbufs loaded
 * via gdk_pixbuf_new_from_inline().
 *
 * Since: 2.4
 **/
void
gtk_icon_theme_add_builtin_icon (const gchar *icon_name,
				 gint         size,
				 GdkPixbuf   *pixbuf)
{
  BuiltinIcon *default_icon;
  GSList *icons;
  gpointer key;

  g_return_if_fail (icon_name != NULL);
  g_return_if_fail (GDK_IS_PIXBUF (pixbuf));
  
  if (!icon_theme_builtin_icons)
    icon_theme_builtin_icons = g_hash_table_new (g_str_hash, g_str_equal);

  icons = g_hash_table_lookup (icon_theme_builtin_icons, icon_name);
  if (!icons)
    key = g_strdup (icon_name);
  else
    key = (gpointer)icon_name;	/* Won't get stored */

  default_icon = g_new (BuiltinIcon, 1);
  default_icon->size = size;
  default_icon->pixbuf = g_object_ref (pixbuf);
  icons = g_slist_prepend (icons, default_icon);

  /* Replaces value, leaves key untouched
   */
  g_hash_table_insert (icon_theme_builtin_icons, key, icons);
}

/* Look up a builtin icon; the min_difference_p and
 * has_larger_p out parameters allow us to combine
 * this lookup with searching through the actual directories
 * of the "hicolor" icon theme. See theme_lookup_icon()
 * for how they are used.
 */
static BuiltinIcon *
find_builtin_icon (const gchar *icon_name,
		   gint         size,
		   gint        *min_difference_p,
		   gboolean    *has_larger_p)
{
  GSList *icons = NULL;
  gint min_difference = G_MAXINT;
  gboolean has_larger = FALSE;
  BuiltinIcon *min_icon = NULL;
  
  _gtk_icon_factory_ensure_default_icons ();
  
  if (!icon_theme_builtin_icons)
    return NULL;

  icons = g_hash_table_lookup (icon_theme_builtin_icons, icon_name);

  while (icons)
    {
      BuiltinIcon *default_icon = icons->data;
      int min, max, difference;
      gboolean smaller;
      
      min = default_icon->size - 2;
      max = default_icon->size + 2;
      smaller = size < min;
      if (size < min)
	difference = min - size;
      else if (size > max)
	difference = size - max;
      else
	difference = 0;
      
      if (difference == 0)
	{
	  min_icon = default_icon;
	  break;
	}
      
      if (!has_larger)
	{
	  if (difference < min_difference || smaller)
	    {
	      min_difference = difference;
	      min_icon = default_icon;
	      has_larger = smaller;
	    }
	}
      else
	{
	  if (difference < min_difference && smaller)
	    {
	      min_difference = difference;
	      min_icon = default_icon;
	    }
	}
      
      icons = icons->next;
    }

  if (min_difference_p)
    *min_difference_p = min_difference;
  if (has_larger_p)
    *has_larger_p = has_larger;

  return min_icon;
}

#ifdef G_OS_WIN32

/* DLL ABI stability backward compatibility versions */

#undef gtk_icon_theme_set_search_path

void
gtk_icon_theme_set_search_path (GtkIconTheme *icon_theme,
				const gchar  *path[],
				gint          n_elements)
{
  const gchar **utf8_path;
  gint i;

  utf8_path = g_new (const gchar *, n_elements);

  for (i = 0; i < n_elements; i++)
    utf8_path[i] = g_locale_to_utf8 (path[i], -1, NULL, NULL, NULL);

  gtk_icon_theme_set_search_path_utf8 (icon_theme, utf8_path, n_elements);

  for (i = 0; i < n_elements; i++)
    g_free ((gchar *) utf8_path[i]);

  g_free (utf8_path);
}

#undef gtk_icon_theme_get_search_path

void
gtk_icon_theme_get_search_path (GtkIconTheme      *icon_theme,
				gchar            **path[],
				gint              *n_elements)
{
  gint i, n;

  gtk_icon_theme_get_search_path_utf8 (icon_theme, path, &n);

  if (n_elements)
    *n_elements = n;

  if (path)
    {
      for (i = 0; i < n; i++)
	{
	  gchar *tem = (*path)[i];

	  (*path)[i] = g_locale_from_utf8 ((*path)[i], -1, NULL, NULL, NULL);
	  g_free (tem);
	}
    }
}

#undef gtk_icon_theme_append_search_path

void
gtk_icon_theme_append_search_path (GtkIconTheme *icon_theme,
				   const gchar  *path)
{
  gchar *utf8_path = g_locale_from_utf8 (path, -1, NULL, NULL, NULL);

  gtk_icon_theme_append_search_path_utf8 (icon_theme, utf8_path);

  g_free (utf8_path);
}

#undef gtk_icon_theme_prepend_search_path

void
gtk_icon_theme_prepend_search_path (GtkIconTheme *icon_theme,
				    const gchar  *path)
{
  gchar *utf8_path = g_locale_from_utf8 (path, -1, NULL, NULL, NULL);

  gtk_icon_theme_prepend_search_path_utf8 (icon_theme, utf8_path);

  g_free (utf8_path);
}

#undef gtk_icon_info_get_filename

G_CONST_RETURN gchar *
gtk_icon_info_get_filename (GtkIconInfo *icon_info)
{
  g_return_val_if_fail (icon_info != NULL, NULL);

  return icon_info->cp_filename;
}

#endif
