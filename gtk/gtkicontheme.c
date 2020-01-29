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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <glib.h>
#include <glib/gstdio.h>

#ifdef G_OS_WIN32
#ifndef S_ISDIR
#define S_ISDIR(mode) ((mode)&_S_IFDIR)
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include "win32/gdkwin32.h"
#endif /* G_OS_WIN32 */

#include "gtkiconthemeprivate.h"
#include "gtkcsspalettevalueprivate.h"
#include "gtkcsscolorvalueprivate.h"
#include "gtkdebug.h"
#include "gtkiconcacheprivate.h"
#include "gtkintl.h"
#include "gtkmain.h"
#include "gtksettingsprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtkprivate.h"
#include "gtksnapshot.h"
#include "gdkpixbufutilsprivate.h"
#include "gdk/gdktextureprivate.h"
#include "gdk/gdkprofilerprivate.h"

/* this is in case round() is not provided by the compiler, 
 * such as in the case of C89 compilers, like MSVC
 */
#include "fallback-c89.c"

/**
 * SECTION:gtkicontheme
 * @Short_description: Looking up icons by name
 * @Title: GtkIconTheme
 *
 * #GtkIconTheme provides a facility for looking up icons by name
 * and size. The main reason for using a name rather than simply
 * providing a filename is to allow different icons to be used
 * depending on what “icon theme” is selected
 * by the user. The operation of icon themes on Linux and Unix
 * follows the [Icon Theme Specification](http://www.freedesktop.org/Standards/icon-theme-spec)
 * There is a fallback icon theme, named `hicolor`, where applications
 * should install their icons, but additional icon themes can be installed
 * as operating system vendors and users choose.
 *
 * In many cases, named themes are used indirectly, via #GtkImage
 * rather than directly, but looking up icons
 * directly is also simple. The #GtkIconTheme object acts
 * as a database of all the icons in the current theme. You
 * can create new #GtkIconTheme objects, but it’s much more
 * efficient to use the standard icon theme for the #GdkDisplay
 * so that the icon information is shared with other people
 * looking up icons.
 * |[<!-- language="C" -->
 * GError *error = NULL;
 * GtkIconTheme *icon_theme;
 * GdkPaintable *paintable;
 *
 * icon_theme = gtk_icon_theme_get_default ();
 * paintable = gtk_icon_theme_load_icon (icon_theme,
 *                                       "my-icon-name", // icon name
 *                                       48, // icon size
 *                                       0,  // flags
 *                                       &error);
 * if (!paintable)
 *   {
 *     g_warning ("Couldn’t load icon: %s", error->message);
 *     g_error_free (error);
 *   }
 * else
 *   {
 *     // Use the icon
 *     g_object_unref (paintable);
 *   }
 * ]|
 */

/* Threading:
 *
 * GtkIconTheme is partially threadsafe, construction and setup can
 * only be done on the main thread (and this is not really fixable
 * since it uses other things like GdkDisplay and GSettings and
 * signals on those. However, once the icon theme is set up on the
 * main thread we can pass it to a thread and do basic lookups on
 * it. This will cause any parallel calls on the main thread (or any
 * other thread) to block until its done, but most of the time
 * lookups are fast. The only time its not fast is when we need
 * to rescan the theme, but then it would be slow if we didn't block
 * and did the rescan ourselves anyway.
 *
 * All private functions that take a GtkIconTheme (or one of its
 * private data types (like IconThemeDir, UnthemedIcon, etc) arg are
 * expected to be called with the icon theme lock held, unless the
 * funcion has a _unlocked suffix. Any similar function that must be
 * called on the main thread, will have a _mainthread suffix.
 *
 * So the rules for such functions are:
 *
 * *  non-_unlocked function cannot call _unlocked functions.
 * * _unlocked must lock before calling a non-_unlocked.
 * * non-_mainthread cannot call _mainthread.
 * * Public APIs must lock before calling a non-_unlocked private function
 * * Public APIs that never call _mainthread are threadsafe.
 *
 * There is a global "icon_cache" G_LOCK, which protects icon_cache
 * and lru_cache in GtkIconTheme as well as its reverse pointer
 * GtkIcon->in_cache. This is sometimes taken with the theme lock held
 * (from the theme side) and sometimes not (from the icon info side),
 * but we never take another lock after taking it, so this is safe.
 * Since this is a global (not per icon/theme) lock we should never
 * block while holding it.
 *
 * Sometimes there are "weak" references to the icon theme that can
 * call into the icon theme. For example, from the "theme-changed"
 * signal. Since these don't own the theme they can run in parallel
 * with some other thread which could be finalizing the theme. To
 * avoid this all such references are done via the GtkIconThemeRef
 * object which contains an NULL:able pointer to the theme and the
 * main lock for that theme. Using this we can safely generate a ref
 * for the theme if it still lives (or get NULL if it doesn't).
 *
 * The icon theme needs to call into the icons sometimes, for example
 * when checking if it should be cached (icon_should_cache__unlocked)
 * this takes the icon->texture_lock while the icon theme is locked.
 * In order to avoid ABBA style deadlocks this means the icon code
 * should never try to lock an icon theme.
 */


#define FALLBACK_ICON_THEME "hicolor"

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
  HAS_ICON_FILE = 1 << 3,
  ICON_SUFFIX_SYMBOLIC_PNG = 1 << 4
} IconSuffix;

#if 0
#define DEBUG_CACHE(args) g_print args
#else
#define DEBUG_CACHE(args)
#endif

#define LRU_CACHE_SIZE 100
#define MAX_LRU_TEXTURE_SIZE 128

typedef struct _GtkIconClass    GtkIconClass;
typedef struct _GtkIconThemeClass   GtkIconThemeClass;


typedef struct _GtkIconThemeRef GtkIconThemeRef;

/**
 * GtkIconTheme:
 *
 * Acts as a database of information about an icon theme.
 * Normally, you retrieve the icon theme for a particular
 * display using gtk_icon_theme_get_for_display() and it
 * will contain information about current icon theme for
 * that display, but you can also create a new #GtkIconTheme
 * object and set the icon theme name explicitly using
 * gtk_icon_theme_set_custom_theme().
 */
struct _GtkIconTheme
{
  GObject parent_instance;
  GtkIconThemeRef *ref;

  GHashTable *icon_cache;              /* Protected by icon_cache lock */

  GtkIcon *lru_cache[LRU_CACHE_SIZE];  /* Protected by icon_cache lock */
  int lru_cache_current;               /* Protected by icon_cache lock */

  gchar *current_theme;
  gchar **search_path;
  gint search_path_len;
  GList *resource_paths;

  guint custom_theme         : 1;
  guint is_display_singleton : 1;
  guint pixbuf_supports_svg  : 1;
  guint themes_valid         : 1;

  /* A list of all the themes needed to look up icons.
   * In search order, without duplicates
   */
  GList *themes;
  GHashTable *unthemed_icons;

  /* GdkDisplay for the icon theme (may be NULL) */
  GdkDisplay *display;
  GtkSettings *display_settings;

  /* time when we last stat:ed for theme changes */
  glong last_stat_time;
  GList *dir_mtimes;

  gulong theme_changed_idle;
};

struct _GtkIconThemeClass
{
  GObjectClass parent_class;

  void (* changed)  (GtkIconTheme *self);
};

typedef struct {
  gchar **icon_names;
  gint size;
  gint scale;
  GtkIconLookupFlags flags;
} IconInfoKey;

struct _GtkIconClass
{
  GObjectClass parent_class;
};

/* This lock protects both IconTheme.icon_cache and the dependent Icon.in_cache.
 * Its a global lock, so hold it only for short times. */
G_LOCK_DEFINE_STATIC(icon_cache);

/**
 * GtkIcon:
 *
 * Contains information found when looking up an icon in
 * an icon theme.
 */
struct _GtkIcon
{
  GObject parent_instance;

  /* Information about the source
   */
  IconInfoKey key;
  GtkIconTheme *in_cache; /* Protected by icon_cache lock */

  gchar *filename;
  GLoadableIcon *loadable;

  /* Cache pixbuf (if there is any) */
  GdkPixbuf *cache_pixbuf;

  /* Information about the directory where
   * the source was found
   */
  IconThemeDirType dir_type;
  gint dir_size;
  gint dir_scale;
  gint min_size;
  gint max_size;

  /* Parameters influencing the scaled icon
   */
  gint desired_size;
  gint desired_scale;
  gint rendered_size;
  gdouble unscaled_scale;
  guint forced_size     : 1;
  guint is_svg          : 1;
  guint is_resource     : 1;


  /* Cached information if we go ahead and try to load the icon.
   *
   * All access to these are protected by the texture_lock. Everything
   * above is immutable after construction and can be used without
   * locks.
   */
  GMutex texture_lock;

  GdkTexture *texture;
  GError *load_error;
  gdouble scale;
  gint symbolic_width;
  gint symbolic_height;
};

typedef struct
{
  gchar *name;
  gchar *display_name;
  gchar *comment;

  /* In search order */
  GList *dirs;
} IconTheme;

typedef struct
{
  IconThemeDirType type;
  GQuark context;

  gint size;
  gint min_size;
  gint max_size;
  gint threshold;
  gint scale;
  gboolean is_resource;

  gchar *dir;
  gchar *subdir;
  gint subdir_index;
  
  GtkIconCache *cache;
  
  GHashTable *icons;
} IconThemeDir;

typedef struct
{
  gchar *svg_filename;
  gchar *no_svg_filename;
  gboolean is_resource;
} UnthemedIcon;

typedef struct 
{
  gchar *dir;
  time_t mtime;
  GtkIconCache *cache;
  gboolean exists;
} IconThemeDirMtime;

static void       gtk_icon_theme_finalize               (GObject          *object);
static void       gtk_icon_theme_dispose                (GObject          *object);
static void       theme_dir_destroy                     (IconThemeDir     *dir);
static void       theme_destroy                         (IconTheme        *theme);
static GtkIcon *  theme_lookup_icon                     (IconTheme        *theme,
                                                         const gchar      *icon_name,
                                                         gint              size,
                                                         gint              scale,
                                                         gboolean          allow_svg);
static void       theme_list_icons                      (IconTheme        *theme,
                                                         GHashTable       *icons,
                                                         GQuark            context);
static gboolean   theme_has_icon                        (IconTheme        *theme,
                                                         const gchar      *icon_name);
static void       theme_subdir_load                     (GtkIconTheme     *self,
                                                         IconTheme        *theme,
                                                         GKeyFile         *theme_file,
                                                         gchar            *subdir);
static void       do_theme_change                       (GtkIconTheme     *self);
static void       blow_themes                           (GtkIconTheme     *self);
static gboolean   rescan_themes                         (GtkIconTheme     *self);
static IconSuffix theme_dir_get_icon_suffix             (IconThemeDir     *dir,
                                                         const gchar      *icon_name);
static GtkIcon *  icon_new                              (IconThemeDirType  type,
                                                         gint              dir_size,
                                                         gint              dir_scale);
static void       icon_compute_rendered_size            (GtkIcon          *icon);
static IconSuffix suffix_from_name                      (const gchar      *name);
static gboolean   icon_ensure_scale_and_texture__locked (GtkIcon          *icon);
static void       unset_display                         (GtkIconTheme     *self);
static void       update_current_theme__mainthread      (GtkIconTheme     *self);
static gboolean   ensure_valid_themes                   (GtkIconTheme     *self,
                                                         gboolean          non_blocking);


static guint signal_changed = 0;

/* This is like a weak ref with a lock, anyone doing
 * operations on the theme must take the lock in this,
 * but you can also take the lock even if the theme
 * has been finalized (but theme will then be NULL).
 *
 * This is used to avoid race conditions where signals
 * like theme-changed happen on the main thread while
 * the last active owning ref of the icon theme is
 * on some thread.
 */
struct _GtkIconThemeRef
{
  gatomicrefcount count;
  GMutex lock;
  GtkIconTheme *theme;
};

static GtkIconThemeRef *
gtk_icon_theme_ref_new (GtkIconTheme *theme)
{
  GtkIconThemeRef *ref = g_new0 (GtkIconThemeRef, 1);

  g_atomic_ref_count_init (&ref->count);
  g_mutex_init (&ref->lock);
  ref->theme = theme;

  return ref;
}

static GtkIconThemeRef *
gtk_icon_theme_ref_ref (GtkIconThemeRef *ref)
{
  g_atomic_ref_count_inc (&ref->count);
  return ref;
}

static void
gtk_icon_theme_ref_unref (GtkIconThemeRef *ref)
{
  if (g_atomic_ref_count_dec (&ref->count))
    {
      g_assert (ref->theme == NULL);
      g_mutex_clear (&ref->lock);
      g_free (ref);
    }
}

/* Take the lock and if available ensure the theme lives until (at
 * least) ref_release is called. */
static GtkIconTheme *
gtk_icon_theme_ref_aquire (GtkIconThemeRef *ref)
{
  g_mutex_lock (&ref->lock);
  if (ref->theme)
    g_object_ref (ref->theme);
  return ref->theme;
}

static void
gtk_icon_theme_ref_release (GtkIconThemeRef *ref)
{
  if (ref->theme)
    g_object_unref (ref->theme);
  g_mutex_unlock (&ref->lock);
}

static void
gtk_icon_theme_ref_dispose (GtkIconThemeRef *ref)
{
  gtk_icon_theme_ref_aquire (ref);
  ref->theme = NULL;
  gtk_icon_theme_ref_release (ref);
}

static void
gtk_icon_theme_lock (GtkIconTheme *self)
{
  g_mutex_lock (&self->ref->lock);
}

static gboolean
gtk_icon_theme_trylock (GtkIconTheme *self)
{
  return g_mutex_trylock (&self->ref->lock);
}

static void
gtk_icon_theme_unlock (GtkIconTheme *self)
{
  g_mutex_unlock (&self->ref->lock);
}


static guint
icon_key_hash (gconstpointer _key)
{
  const IconInfoKey *key = _key;
  guint h = 0;
  int i;
  for (i = 0; key->icon_names[i] != NULL; i++)
    h ^= g_str_hash (key->icon_names[i]);

  h ^= key->size * 0x10001;
  h ^= key->scale * 0x1000010;
  h ^= key->flags * 0x100000100;

  return h;
}

static gboolean
icon_key_equal (gconstpointer _a,
                gconstpointer _b)
{
  const IconInfoKey *a = _a;
  const IconInfoKey *b = _b;
  int i;

  if (a->size != b->size)
    return FALSE;

  if (a->scale != b->scale)
    return FALSE;

  if (a->flags != b->flags)
    return FALSE;

  for (i = 0;
       a->icon_names[i] != NULL &&
       b->icon_names[i] != NULL; i++)
    {
      if (strcmp (a->icon_names[i], b->icon_names[i]) != 0)
        return FALSE;
    }

  return a->icon_names[i] == NULL && b->icon_names[i] == NULL;
}

/****************** Icon cache ***********************
 *
 * The icon cache, this spans both GtkIconTheme and GtkIcon, so the locking is
 * a bit tricky. Never do block with the lock held, and never do any
 * callouts to other code. In particular, don't call theme or finalizers
 * because that will take the lock when removing from the icon cache.
 */

/* This is called with icon_cache lock held so must not take any locks */
static gboolean
_icon_cache_should_lru_cache (GtkIcon *info)
{
  return info->desired_size <= MAX_LRU_TEXTURE_SIZE;
}

/* This returns the old lru element because we can't unref it with
 * the lock held */
static GtkIcon *
_icon_cache_add_to_lru_cache (GtkIconTheme *theme, GtkIcon *icon)
{
  GtkIcon *old_icon = NULL;

  /* Avoid storing the same info multiple times in a row */
  if (theme->lru_cache[theme->lru_cache_current] != icon)
    {
      theme->lru_cache_current = (theme->lru_cache_current + 1) % LRU_CACHE_SIZE;
      old_icon = theme->lru_cache[theme->lru_cache_current];
      theme->lru_cache[theme->lru_cache_current] = g_object_ref (icon);
    }

  return old_icon;
}

static GtkIcon *
icon_cache_lookup (GtkIconTheme *theme, IconInfoKey *key)
{
  GtkIcon *old_icon = NULL;
  GtkIcon *icon;

  G_LOCK (icon_cache);

  icon = g_hash_table_lookup (theme->icon_cache, key);
  if (icon != NULL)
    {
      DEBUG_CACHE (("cache hit %p (%s %d 0x%x) (cache size %d)\n",
                    icon,
                    g_strjoinv (",", icon->key.icon_names),
                    icon->key.size, icon->key.flags,
                    g_hash_table_size (theme->icon_cache)));

      icon = g_object_ref (icon);

      /* Move item to front in LRU cache */
      if (_icon_cache_should_lru_cache (icon))
        old_icon = _icon_cache_add_to_lru_cache (theme, icon);
    }

  G_UNLOCK (icon_cache);

  /* Call potential finalizer outside the lock */
  if (old_icon)
    g_object_unref (old_icon);

  return icon;
}

/* The icon info was removed from the icon_hash hash table.
 * This is a callback from the icon_cache hashtable, so the icon_cache lock is already held.
 */
static void
icon_uncached_cb (GtkIcon *icon)
{
  DEBUG_CACHE (("removing %p (%s %d 0x%x) from cache (icon_them: %p)  (cache size %d)\n",
                icon,
                g_strjoinv (",", icon->key.icon_names),
                icon->key.size, icon->key.flags,
                self,
                icon_theme != NULL ? g_hash_table_size (self->icon_cache) : 0));
  g_assert (icon->in_cache != NULL);
  icon->in_cache = NULL;
}

static void
icon_cache_mark_used_if_cached (GtkIcon *icon)
{
  GtkIcon *old_icon = NULL;

  if (!_icon_cache_should_lru_cache (icon))
    return;

  G_LOCK (icon_cache);
  if (icon->in_cache)
    old_icon = _icon_cache_add_to_lru_cache (icon->in_cache, icon);
  G_UNLOCK (icon_cache);

  /* Call potential finalizers outside the lock */
  if (old_icon)
    g_object_unref (old_icon);
}

static void
icon_cache_add (GtkIconTheme *theme, GtkIcon *icon)
{
  GtkIcon *old_icon = NULL;

  G_LOCK (icon_cache);
  icon->in_cache = theme;
  g_hash_table_insert (theme->icon_cache, &icon->key, icon);

  if (_icon_cache_should_lru_cache (icon))
    old_icon = _icon_cache_add_to_lru_cache (theme, icon);
  DEBUG_CACHE (("adding %p (%s %d 0x%x) to cache (cache size %d)\n",
                icon,
                g_strjoinv (",", icon->key.icon_names),
                icon->key.size, icon->key.flags,
                g_hash_table_size (theme->icon_cache)));
  G_UNLOCK (icon_cache);

  /* Call potential finalizer outside the lock */
  if (old_icon)
    g_object_unref (old_icon);
}

static void
icon_cache_remove (GtkIcon *icon)
{
  G_LOCK (icon_cache);
  if (icon->in_cache)
    g_hash_table_remove (icon->in_cache->icon_cache, &icon->key);
  G_UNLOCK (icon_cache);
}

static void
icon_cache_clear (GtkIconTheme *theme)
{
  int i;
  GtkIcon *old_icons[LRU_CACHE_SIZE];

  G_LOCK (icon_cache);
  g_hash_table_remove_all (theme->icon_cache);
  for (i = 0; i < LRU_CACHE_SIZE; i ++)
    {
      old_icons[i] = theme->lru_cache[i];
      theme->lru_cache[i] = NULL;
    }
  G_UNLOCK (icon_cache);

  /* Call potential finalizers outside the lock */
  for (i = 0; i < LRU_CACHE_SIZE; i ++)
    {
      if (old_icons[i] != NULL)
        g_object_unref (old_icons[i]);
    }
}

/****************** End of icon cache ***********************/

G_DEFINE_TYPE (GtkIconTheme, gtk_icon_theme, G_TYPE_OBJECT)

/**
 * gtk_icon_theme_new:
 * 
 * Creates a new icon theme object. Icon theme objects are used
 * to lookup up an icon by name in a particular icon theme.
 * Usually, you’ll want to use gtk_icon_theme_get_default()
 * or gtk_icon_theme_get_for_display() rather than creating
 * a new icon theme object for scratch.
 * 
 * Returns: the newly created #GtkIconTheme object.
 */
GtkIconTheme *
gtk_icon_theme_new (void)
{
  return g_object_new (GTK_TYPE_ICON_THEME, NULL);
}

/**
 * gtk_icon_theme_get_default:
 * 
 * Gets the icon theme for the default display. See
 * gtk_icon_theme_get_for_display().
 *
 * Returns: (transfer none): A unique #GtkIconTheme associated with
 *     the default display. This icon theme is associated with
 *     the display and can be used as long as the display
 *     is open. Do not ref or unref it.
 */
GtkIconTheme *
gtk_icon_theme_get_default (void)
{
  return gtk_icon_theme_get_for_display (gdk_display_get_default ());
}

static void
load_theme_thread  (GTask        *task,
                    gpointer      source_object,
                    gpointer      task_data,
                    GCancellable *cancellable)
{
  GtkIconTheme *self = GTK_ICON_THEME (source_object);

  gtk_icon_theme_lock (self);
  ensure_valid_themes (self, FALSE);
  gtk_icon_theme_unlock (self);
  g_task_return_pointer (task, NULL, NULL);
}

static void
gtk_icon_theme_load_in_thread (GtkIconTheme *self)
{
  GTask *task;

  task = g_task_new (self, NULL, NULL, NULL);
  g_task_set_task_data (task, g_object_ref (self), g_object_unref);
  g_task_run_in_thread (task, load_theme_thread);
}

/**
 * gtk_icon_theme_get_for_display:
 * @display: a #GdkDisplay
 * 
 * Gets the icon theme object associated with @display; if this
 * function has not previously been called for the given
 * display, a new icon theme object will be created and
 * associated with the display. Icon theme objects are
 * fairly expensive to create, so using this function
 * is usually a better choice than calling than gtk_icon_theme_new()
 * and setting the display yourself; by using this function
 * a single icon theme object will be shared between users.
 *
 * Returns: (transfer none): A unique #GtkIconTheme associated with
 *  the given display. This icon theme is associated with
 *  the display and can be used as long as the display
 *  is open. Do not ref or unref it.
 */
GtkIconTheme *
gtk_icon_theme_get_for_display (GdkDisplay *display)
{
  GtkIconTheme *self;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  self = g_object_get_data (G_OBJECT (display), "gtk-icon-theme");
  if (!self)
    {
      self = gtk_icon_theme_new ();
      gtk_icon_theme_set_display (self, display);

      self->is_display_singleton = TRUE;

      g_object_set_data (G_OBJECT (display), I_("gtk-icon-theme"), self);

      /* Queue early read of the default themes, we read the icon theme name in _set_display(). */
      gtk_icon_theme_load_in_thread (self);
    }

  return self;
}

static void
gtk_icon_theme_class_init (GtkIconThemeClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = gtk_icon_theme_finalize;
  gobject_class->dispose = gtk_icon_theme_dispose;

  /**
   * GtkIconTheme::changed:
   * @self: the icon theme
   *
   * Emitted when the current icon theme is switched or GTK+ detects
   * that a change has occurred in the contents of the current
   * icon theme.
   */
  signal_changed = g_signal_new (I_("changed"),
                                 G_TYPE_FROM_CLASS (klass),
                                 G_SIGNAL_RUN_LAST,
                                 G_STRUCT_OFFSET (GtkIconThemeClass, changed),
                                 NULL, NULL,
                                 NULL,
                                 G_TYPE_NONE, 0);
}


/* Callback when the display that the icon theme is attached
 * to is closed; unset the display, and if it’s the unique theme
 * for the display, drop the reference
 */
static void
display_closed__mainthread_unlocked (GdkDisplay      *display,
                                     gboolean         is_error,
                                     GtkIconThemeRef *ref)
{
  GtkIconTheme *self = gtk_icon_theme_ref_aquire (ref);
  gboolean was_display_singleton;

  if (self)
    {
      /* This is only set at construction and accessed here in the main thread, so no locking necessary */
      was_display_singleton = self->is_display_singleton;
      if (was_display_singleton)
        {
          g_object_set_data (G_OBJECT (display), I_("gtk-icon-theme"), NULL);
          self->is_display_singleton = FALSE;
        }

      unset_display (self);
      update_current_theme__mainthread (self);

      if (was_display_singleton)
        g_object_unref (self);
    }

  gtk_icon_theme_ref_release (ref);
}

static void
update_current_theme__mainthread (GtkIconTheme *self)
{
#define theme_changed(_old, _new) \
  ((_old && !_new) || (!_old && _new) || \
   (_old && _new && strcmp (_old, _new) != 0))

  if (!self->custom_theme)
    {
      gchar *theme = NULL;
      gboolean changed = FALSE;

      if (self->display)
        {
          GtkSettings *settings = gtk_settings_get_for_display (self->display);
          g_object_get (settings, "gtk-icon-theme-name", &theme, NULL);
        }

      if (theme_changed (self->current_theme, theme))
        {
          g_free (self->current_theme);
          self->current_theme = theme;
          changed = TRUE;
        }
      else
        g_free (theme);

      if (changed)
        do_theme_change (self);
    }
#undef theme_changed
}

/* Callback when the icon theme GtkSetting changes
 */
static void
theme_changed__mainthread_unlocked (GtkSettings  *settings,
                                    GParamSpec   *pspec,
                                    GtkIconThemeRef *ref)
{
  GtkIconTheme *self = gtk_icon_theme_ref_aquire (ref);

  if (self)
    {
      update_current_theme__mainthread (self);

      /* Queue early read of the new theme */
      gtk_icon_theme_load_in_thread (self);
    }

  gtk_icon_theme_ref_release (ref);
}

static void
unset_display (GtkIconTheme *self)
{
  if (self->display)
    {
      g_signal_handlers_disconnect_by_func (self->display,
                                            (gpointer) display_closed__mainthread_unlocked,
                                            self->ref);
      g_signal_handlers_disconnect_by_func (self->display_settings,
                                            (gpointer) theme_changed__mainthread_unlocked,
                                            self->ref);

      self->display = NULL;
      self->display_settings = NULL;
    }
}

/**
 * gtk_icon_theme_set_display:
 * @self: a #GtkIconTheme
 * @display: a #GdkDisplay
 * 
 * Sets the display for an icon theme; the display is used
 * to track the user’s currently configured icon theme,
 * which might be different for different displays.
 */
void
gtk_icon_theme_set_display (GtkIconTheme *self,
                            GdkDisplay   *display)
{
  g_return_if_fail (GTK_ICON_THEME (self));
  g_return_if_fail (display == NULL || GDK_IS_DISPLAY (display));

  gtk_icon_theme_lock (self);

  unset_display (self);

  if (display)
    {
      self->display = display;
      self->display_settings = gtk_settings_get_for_display (display);

      g_signal_connect_data (display, "closed",
                             G_CALLBACK (display_closed__mainthread_unlocked),
                             gtk_icon_theme_ref_ref (self->ref),
                             (GClosureNotify)gtk_icon_theme_ref_unref,
                             0);
      g_signal_connect_data (self->display_settings, "notify::gtk-icon-theme-name",
                             G_CALLBACK (theme_changed__mainthread_unlocked),
                             gtk_icon_theme_ref_ref (self->ref),
                             (GClosureNotify)gtk_icon_theme_ref_unref,
                             0);
    }

  update_current_theme__mainthread (self);

  gtk_icon_theme_unlock (self);
}

/* Checks whether a loader for SVG files has been registered
 * with GdkPixbuf.
 */
static gboolean
pixbuf_supports_svg (void)
{
  GSList *formats;
  GSList *tmp_list;
  static gint found_svg = -1;

  if (found_svg != -1)
    return found_svg;

  formats = gdk_pixbuf_get_formats ();

  found_svg = FALSE; 
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
  
  return found_svg;
}

static void
gtk_icon_theme_init (GtkIconTheme *self)
{
  const gchar * const *xdg_data_dirs;
  int i, j;

  self->ref = gtk_icon_theme_ref_new (self);

  self->icon_cache = g_hash_table_new_full (icon_key_hash, icon_key_equal, NULL,
                                            (GDestroyNotify)icon_uncached_cb);

  self->custom_theme = FALSE;

  xdg_data_dirs = g_get_system_data_dirs ();
  for (i = 0; xdg_data_dirs[i]; i++) ;

  self->search_path_len = 2 * i + 2;
  
  self->search_path = g_new (char *, self->search_path_len);
  
  i = 0;
  self->search_path[i++] = g_build_filename (g_get_user_data_dir (), "icons", NULL);
  self->search_path[i++] = g_build_filename (g_get_home_dir (), ".icons", NULL);
  
  for (j = 0; xdg_data_dirs[j]; j++) 
    self->search_path[i++] = g_build_filename (xdg_data_dirs[j], "icons", NULL);

  for (j = 0; xdg_data_dirs[j]; j++) 
    self->search_path[i++] = g_build_filename (xdg_data_dirs[j], "pixmaps", NULL);

  self->resource_paths = g_list_append (NULL, g_strdup ("/org/gtk/libgtk/icons/"));

  self->themes_valid = FALSE;
  self->themes = NULL;
  self->unthemed_icons = NULL;

  self->pixbuf_supports_svg = pixbuf_supports_svg ();
}

static void
free_dir_mtime (IconThemeDirMtime *dir_mtime)
{
  if (dir_mtime->cache)
    gtk_icon_cache_unref (dir_mtime->cache);

  g_free (dir_mtime->dir);
  g_slice_free (IconThemeDirMtime, dir_mtime);
}

static gboolean
theme_changed_idle__mainthread_unlocked (gpointer user_data)
{
  GtkIconThemeRef *ref = (GtkIconThemeRef *)user_data;
  GtkIconTheme *self;
  GdkDisplay *display = NULL;

  self = gtk_icon_theme_ref_aquire (ref);
  if (self)
    {
      g_object_ref (self); /* Ensure theme lives during the changed signal emissions */

      self->theme_changed_idle = 0;

      if (self->display && self->is_display_singleton)
        display = g_object_ref (self->display);
    }
  gtk_icon_theme_ref_release (ref);

  if (self)
    {
      /* Emit signals outside locks. */
      g_signal_emit (self, signal_changed, 0);

      if (display)
        {
          gtk_style_context_reset_widgets (self->display);
          g_object_unref (display);
        }

      g_object_unref (self);
    }

  return FALSE;
}

static void
queue_theme_changed (GtkIconTheme *self)
{
  if (!self->theme_changed_idle)
    {
      self->theme_changed_idle = g_idle_add_full (GTK_PRIORITY_RESIZE - 2,
                                                  theme_changed_idle__mainthread_unlocked,
                                                  gtk_icon_theme_ref_ref (self->ref),
                                                  (GDestroyNotify)gtk_icon_theme_ref_unref);
      g_source_set_name_by_id (self->theme_changed_idle, "[gtk] theme_changed_idle");
    }
}

static void
do_theme_change (GtkIconTheme *self)
{
  icon_cache_clear (self);

  if (!self->themes_valid)
    return;

  GTK_DISPLAY_NOTE (self->display, ICONTHEME,
            g_message ("change to icon theme \"%s\"", self->current_theme));
  blow_themes (self);

  queue_theme_changed (self);

}

static void
blow_themes (GtkIconTheme *self)
{
  if (self->themes_valid)
    {
      g_list_free_full (self->themes, (GDestroyNotify) theme_destroy);
      g_list_free_full (self->dir_mtimes, (GDestroyNotify) free_dir_mtime);
      g_hash_table_destroy (self->unthemed_icons);
    }
  self->themes = NULL;
  self->unthemed_icons = NULL;
  self->dir_mtimes = NULL;
  self->themes_valid = FALSE;
}

static void
gtk_icon_theme_dispose (GObject *object)
{
  GtkIconTheme *self = GTK_ICON_THEME (object);

  /* We make sure all outstanding GtkIconThemeRefs to us are NULLed
   * out so that no other threads than the one running finalize will
   * refer to the icon theme after this. This could happen if
   * we finalize on a thread and on the main thread some display or
   * setting signal is emitted.
   *
   * It is possible that before we aquire the lock this happens
   * and the other thread refs the icon theme for some reason, but
   * this is ok as it is allowed to resurrect objects in dispose
   * (but not in finalize).
   */
  gtk_icon_theme_ref_dispose (self->ref);

  G_OBJECT_CLASS (gtk_icon_theme_parent_class)->dispose (object);
}

static void
gtk_icon_theme_finalize (GObject *object)
{
  GtkIconTheme *self = GTK_ICON_THEME (object);
  int i;

  /* We don't actually need to take the lock here, because by now
     there can be no other threads that own a ref to this object, but
     technically this is considered "locked" */

  icon_cache_clear (self);

  if (self->theme_changed_idle)
    g_source_remove (self->theme_changed_idle);

  unset_display (self);

  g_free (self->current_theme);

  for (i = 0; i < self->search_path_len; i++)
    g_free (self->search_path[i]);
  g_free (self->search_path);

  g_list_free_full (self->resource_paths, g_free);

  blow_themes (self);

  gtk_icon_theme_ref_unref (self->ref);

  G_OBJECT_CLASS (gtk_icon_theme_parent_class)->finalize (object);
}

/**
 * gtk_icon_theme_set_search_path:
 * @self: a #GtkIconTheme
 * @path: (array length=n_elements) (element-type filename): array of
 *     directories that are searched for icon themes
 * @n_elements: number of elements in @path.
 * 
 * Sets the search path for the icon theme object. When looking
 * for an icon theme, GTK+ will search for a subdirectory of
 * one or more of the directories in @path with the same name
 * as the icon theme containing an index.theme file. (Themes from
 * multiple of the path elements are combined to allow themes to be
 * extended by adding icons in the user’s home directory.)
 *
 * In addition if an icon found isn’t found either in the current
 * icon theme or the default icon theme, and an image file with
 * the right name is found directly in one of the elements of
 * @path, then that image will be used for the icon name.
 * (This is legacy feature, and new icons should be put
 * into the fallback icon theme, which is called hicolor,
 * rather than directly on the icon path.)
 */
void
gtk_icon_theme_set_search_path (GtkIconTheme *self,
                                const gchar  *path[],
                                gint          n_elements)
{
  gint i;

  g_return_if_fail (GTK_IS_ICON_THEME (self));

  gtk_icon_theme_lock (self);

  for (i = 0; i < self->search_path_len; i++)
    g_free (self->search_path[i]);

  g_free (self->search_path);

  self->search_path = g_new (gchar *, n_elements);
  self->search_path_len = n_elements;

  for (i = 0; i < self->search_path_len; i++)
    self->search_path[i] = g_strdup (path[i]);

  do_theme_change (self);

  gtk_icon_theme_unlock (self);
}

/**
 * gtk_icon_theme_get_search_path:
 * @self: a #GtkIconTheme
 * @path: (allow-none) (array length=n_elements) (element-type filename) (out):
 *     location to store a list of icon theme path directories or %NULL.
 *     The stored value should be freed with g_strfreev().
 * @n_elements: location to store number of elements in @path, or %NULL
 *
 * Gets the current search path. See gtk_icon_theme_set_search_path().
 */
void
gtk_icon_theme_get_search_path (GtkIconTheme  *self,
                                gchar        **path[],
                                gint          *n_elements)
{
  gint i;

  g_return_if_fail (GTK_IS_ICON_THEME (self));

  gtk_icon_theme_lock (self);

  if (n_elements)
    *n_elements = self->search_path_len;

  if (path)
    {
      *path = g_new (gchar *, self->search_path_len + 1);
      for (i = 0; i < self->search_path_len; i++)
        (*path)[i] = g_strdup (self->search_path[i]);
      (*path)[i] = NULL;
    }

  gtk_icon_theme_unlock (self);
}

/**
 * gtk_icon_theme_append_search_path:
 * @self: a #GtkIconTheme
 * @path: (type filename): directory name to append to the icon path
 * 
 * Appends a directory to the search path. 
 * See gtk_icon_theme_set_search_path(). 
 */
void
gtk_icon_theme_append_search_path (GtkIconTheme *self,
                                   const gchar  *path)
{
  g_return_if_fail (GTK_IS_ICON_THEME (self));
  g_return_if_fail (path != NULL);

  gtk_icon_theme_lock (self);

  self->search_path_len++;

  self->search_path = g_renew (gchar *, self->search_path, self->search_path_len);
  self->search_path[self->search_path_len-1] = g_strdup (path);

  do_theme_change (self);

  gtk_icon_theme_unlock (self);
}

/**
 * gtk_icon_theme_prepend_search_path:
 * @self: a #GtkIconTheme
 * @path: (type filename): directory name to prepend to the icon path
 * 
 * Prepends a directory to the search path. 
 * See gtk_icon_theme_set_search_path().
 */
void
gtk_icon_theme_prepend_search_path (GtkIconTheme *self,
                                    const gchar  *path)
{
  gint i;

  g_return_if_fail (GTK_IS_ICON_THEME (self));
  g_return_if_fail (path != NULL);

  gtk_icon_theme_lock (self);

  self->search_path_len++;
  self->search_path = g_renew (gchar *, self->search_path, self->search_path_len);

  for (i = self->search_path_len - 1; i > 0; i--)
    self->search_path[i] = self->search_path[i - 1];

  self->search_path[0] = g_strdup (path);

  do_theme_change (self);

  gtk_icon_theme_unlock (self);
}

/**
 * gtk_icon_theme_add_resource_path:
 * @self: a #GtkIconTheme
 * @path: a resource path
 *
 * Adds a resource path that will be looked at when looking
 * for icons, similar to search paths.
 *
 * This function should be used to make application-specific icons
 * available as part of the icon theme.
 *
 * The resources are considered as part of the hicolor icon theme
 * and must be located in subdirectories that are defined in the
 * hicolor icon theme, such as `@path/16x16/actions/run.png`.
 * Icons that are directly placed in the resource path instead
 * of a subdirectory are also considered as ultimate fallback.
 */
void
gtk_icon_theme_add_resource_path (GtkIconTheme *self,
                                  const gchar  *path)
{
  g_return_if_fail (GTK_IS_ICON_THEME (self));
  g_return_if_fail (path != NULL);

  gtk_icon_theme_lock (self);

  self->resource_paths = g_list_append (self->resource_paths, g_strdup (path));

  do_theme_change (self);

  gtk_icon_theme_unlock (self);
}

/**
 * gtk_icon_theme_set_custom_theme:
 * @self: a #GtkIconTheme
 * @theme_name: (allow-none): name of icon theme to use instead of
 *   configured theme, or %NULL to unset a previously set custom theme
 * 
 * Sets the name of the icon theme that the #GtkIconTheme object uses
 * overriding system configuration. This function cannot be called
 * on the icon theme objects returned from gtk_icon_theme_get_default()
 * and gtk_icon_theme_get_for_display().
 */
void
gtk_icon_theme_set_custom_theme (GtkIconTheme *self,
                                 const gchar  *theme_name)
{
  g_return_if_fail (GTK_IS_ICON_THEME (self));

  g_return_if_fail (!self->is_display_singleton);

  gtk_icon_theme_lock (self);

  if (theme_name != NULL)
    {
      self->custom_theme = TRUE;
      if (!self->current_theme || strcmp (theme_name, self->current_theme) != 0)
        {
          g_free (self->current_theme);
          self->current_theme = g_strdup (theme_name);

          do_theme_change (self);
        }
    }
  else
    {
      if (self->custom_theme)
        {
          self->custom_theme = FALSE;
          update_current_theme__mainthread (self);
        }
    }

  gtk_icon_theme_unlock (self);
}

static const gchar builtin_hicolor_index[] =
"[Icon Theme]\n"
"Name=Hicolor\n"
"Hidden=True\n"
"Directories=16x16/actions,16x16/status,22x22/actions,24x24/actions,24x24/status,32x32/actions,32x32/status,48x48/status,64x64/actions\n"
"[16x16/actions]\n"
"Size=16\n"
"Type=Threshold\n"
"[16x16/status]\n"
"Size=16\n"
"Type=Threshold\n"
"[22x22/actions]\n"
"Size=22\n"
"Type=Threshold\n"
"[24x24/actions]\n"
"Size=24\n"
"Type=Threshold\n"
"[24x24/status]\n"
"Size=24\n"
"Type=Threshold\n"
"[32x32/actions]\n"
"Size=32\n"
"Type=Threshold\n"
"[32x32/status]\n"
"Size=32\n"
"Type=Threshold\n"
"[48x48/status]\n"
"Size=48\n"
"Type=Threshold\n"
"[64x64/actions]\n"
"Size=64\n"
"Type=Threshold\n";

static void
insert_theme (GtkIconTheme *self,
              const gchar  *theme_name)
{
  gint i;
  GList *l;
  gchar **dirs;
  gchar **scaled_dirs;
  gchar **themes;
  IconTheme *theme = NULL;
  gchar *path;
  GKeyFile *theme_file;
  GError *error = NULL;
  IconThemeDirMtime *dir_mtime;
  GStatBuf stat_buf;

  for (l = self->themes; l != NULL; l = l->next)
    {
      theme = l->data;
      if (strcmp (theme->name, theme_name) == 0)
        return;
    }
  
  for (i = 0; i < self->search_path_len; i++)
    {
      path = g_build_filename (self->search_path[i],
                               theme_name,
                               NULL);
      dir_mtime = g_slice_new (IconThemeDirMtime);
      dir_mtime->cache = NULL;
      dir_mtime->dir = path;
      if (g_stat (path, &stat_buf) == 0 && S_ISDIR (stat_buf.st_mode)) {
        dir_mtime->mtime = stat_buf.st_mtime;
        dir_mtime->exists = TRUE;
      } else {
        dir_mtime->mtime = 0;
        dir_mtime->exists = FALSE;
      }

      self->dir_mtimes = g_list_prepend (self->dir_mtimes, dir_mtime);
    }

  theme_file = NULL;
  for (i = 0; i < self->search_path_len && !theme_file; i++)
    {
      path = g_build_filename (self->search_path[i],
                               theme_name,
                               "index.theme",
                               NULL);
      if (g_file_test (path, G_FILE_TEST_IS_REGULAR)) 
        {
          theme_file = g_key_file_new ();
          g_key_file_set_list_separator (theme_file, ',');
          if (!g_key_file_load_from_file (theme_file, path, 0, &error))
            {
              g_key_file_free (theme_file);
              theme_file = NULL;
              g_error_free (error);
              error = NULL;
            }
        }
      g_free (path);
    }

  if (theme_file || strcmp (theme_name, FALLBACK_ICON_THEME) == 0)
    {
      theme = g_new0 (IconTheme, 1);
      theme->name = g_strdup (theme_name);
      self->themes = g_list_prepend (self->themes, theme);
      if (!theme_file)
        {
          theme_file = g_key_file_new ();
          g_key_file_set_list_separator (theme_file, ',');
          g_key_file_load_from_data (theme_file, builtin_hicolor_index, -1, 0, NULL);
        }
    }

  if (theme_file == NULL)
    return;

  theme->display_name = 
    g_key_file_get_locale_string (theme_file, "Icon Theme", "Name", NULL, NULL);
  if (!theme->display_name)
    g_warning ("Theme file for %s has no name", theme_name);

  dirs = g_key_file_get_string_list (theme_file, "Icon Theme", "Directories", NULL, NULL);
  if (!dirs)
    {
      g_warning ("Theme file for %s has no directories", theme_name);
      self->themes = g_list_remove (self->themes, theme);
      g_free (theme->name);
      g_free (theme->display_name);
      g_free (theme);
      g_key_file_free (theme_file);
      return;
    }

  scaled_dirs = g_key_file_get_string_list (theme_file, "Icon Theme", "ScaledDirectories", NULL, NULL);

  theme->comment = 
    g_key_file_get_locale_string (theme_file, 
                                  "Icon Theme", "Comment",
                                  NULL, NULL);

  theme->dirs = NULL;
  for (i = 0; dirs[i] != NULL; i++)
    theme_subdir_load (self, theme, theme_file, dirs[i]);

  if (scaled_dirs)
    {
      for (i = 0; scaled_dirs[i] != NULL; i++)
        theme_subdir_load (self, theme, theme_file, scaled_dirs[i]);
    }
  g_strfreev (dirs);
  g_strfreev (scaled_dirs);

  theme->dirs = g_list_reverse (theme->dirs);

  themes = g_key_file_get_string_list (theme_file,
                                       "Icon Theme",
                                       "Inherits",
                                       NULL,
                                       NULL);
  if (themes)
    {
      for (i = 0; themes[i] != NULL; i++)
        insert_theme (self, themes[i]);

      g_strfreev (themes);
    }

  g_key_file_free (theme_file);
}

static void
free_unthemed_icon (UnthemedIcon *unthemed_icon)
{
  g_free (unthemed_icon->svg_filename);
  g_free (unthemed_icon->no_svg_filename);
  g_slice_free (UnthemedIcon, unthemed_icon);
}

static gchar *
strip_suffix (const gchar *filename)
{
  const gchar *dot;

  if (g_str_has_suffix (filename, ".symbolic.png"))
    return g_strndup (filename, strlen(filename)-13);

  dot = strrchr (filename, '.');

  if (dot == NULL)
    return g_strdup (filename);

  return g_strndup (filename, dot - filename);
}

static void
add_unthemed_icon (GtkIconTheme *self,
                   const gchar  *dir,
                   const gchar  *file,
                   gboolean      is_resource)
{
  IconSuffix new_suffix, old_suffix;
  gchar *abs_file;
  gchar *base_name;
  UnthemedIcon *unthemed_icon;

  new_suffix = suffix_from_name (file);

  if (new_suffix == ICON_SUFFIX_NONE)
    return;

  abs_file = g_build_filename (dir, file, NULL);
  base_name = strip_suffix (file);

  unthemed_icon = g_hash_table_lookup (self->unthemed_icons, base_name);

  if (unthemed_icon)
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
      unthemed_icon = g_slice_new0 (UnthemedIcon);

      unthemed_icon->is_resource = is_resource;

      if (new_suffix == ICON_SUFFIX_SVG)
        unthemed_icon->svg_filename = abs_file;
      else
        unthemed_icon->no_svg_filename = abs_file;

      /* takes ownership of base_name */
      g_hash_table_replace (self->unthemed_icons, base_name, unthemed_icon);
    }
}

static void
load_themes (GtkIconTheme *self)
{
  GDir *gdir;
  gint base;
  gchar *dir;
  const gchar *file;
  GTimeVal tv;
  IconThemeDirMtime *dir_mtime;
  GStatBuf stat_buf;
  GList *d;

  if (self->current_theme)
    insert_theme (self, self->current_theme);

  /* Always look in the Adwaita, gnome and hicolor icon themes.
   * Looking in hicolor is mandated by the spec, looking in Adwaita
   * and gnome is a pragmatic solution to prevent missing icons in
   * GTK+ applications when run under, e.g. KDE.
   */
#if 0
  insert_theme (self, DEFAULT_self);
  insert_theme (self, "gnome");
#endif
  insert_theme (self, FALLBACK_ICON_THEME);
  self->themes = g_list_reverse (self->themes);


  self->unthemed_icons = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                g_free, (GDestroyNotify)free_unthemed_icon);

  for (base = 0; base < self->search_path_len; base++)
    {
      dir = self->search_path[base];

      dir_mtime = g_slice_new (IconThemeDirMtime);
      self->dir_mtimes = g_list_prepend (self->dir_mtimes, dir_mtime);

      dir_mtime->dir = g_strdup (dir);
      dir_mtime->mtime = 0;
      dir_mtime->exists = FALSE;
      dir_mtime->cache = NULL;

      if (g_stat (dir, &stat_buf) != 0 || !S_ISDIR (stat_buf.st_mode))
        continue;
      dir_mtime->mtime = stat_buf.st_mtime;
      dir_mtime->exists = TRUE;

      dir_mtime->cache = gtk_icon_cache_new_for_path (dir);
      if (dir_mtime->cache != NULL)
        continue;

      gdir = g_dir_open (dir, 0, NULL);
      if (gdir == NULL)
        continue;

      while ((file = g_dir_read_name (gdir)))
        add_unthemed_icon (self, dir, file, FALSE);

      g_dir_close (gdir);
    }
  self->dir_mtimes = g_list_reverse (self->dir_mtimes);

  for (d = self->resource_paths; d; d = d->next)
    {
      gchar **children;
      gint i;

      dir = d->data;
      children = g_resources_enumerate_children (dir, 0, NULL);
      if (!children)
        continue;

      for (i = 0; children[i]; i++)
        add_unthemed_icon (self, dir, children[i], TRUE);

      g_strfreev (children);
    }

  self->themes_valid = TRUE;

  g_get_current_time (&tv);
  self->last_stat_time = tv.tv_sec;

  GTK_DISPLAY_NOTE (self->display, ICONTHEME, {
    GList *l;
    GString *s;
    s = g_string_new ("Current icon themes ");
    for (l = self->themes; l; l = l->next)
      {
        IconTheme *theme = l->data;
        g_string_append (s, theme->name);
        g_string_append_c (s, ' ');
      }
    g_message ("%s", s->str);
    g_string_free (s, TRUE);
  });
}

static gboolean
ensure_valid_themes (GtkIconTheme *self,
                     gboolean non_blocking)
{
  GTimeVal tv;
  gboolean was_valid = self->themes_valid;

  if (self->themes_valid)
    {
      g_get_current_time (&tv);

      if (ABS (tv.tv_sec - self->last_stat_time) > 5)
        {
          if (non_blocking)
            return FALSE;

          if (rescan_themes (self))
            {
              icon_cache_clear (self);
              blow_themes (self);
            }
        }
    }

  if (!self->themes_valid)
    {
      gint64 before;
      if (non_blocking)
        return FALSE;

       before = g_get_monotonic_time ();

      load_themes (self);

      if (gdk_profiler_is_running ())
        gdk_profiler_add_mark (before * 1000, (g_get_monotonic_time () - before) * 1000, "icon theme load", NULL);

      if (was_valid)
        queue_theme_changed (self);
    }

  return TRUE;
}

static inline gboolean
icon_name_is_symbolic (const gchar *icon_name,
                       int          icon_name_len)
{

  if (icon_name_len < 0)
    icon_name_len = strlen (icon_name);

  if (icon_name_len > strlen ("-symbolic"))
    {
      if (strcmp (icon_name + icon_name_len - strlen ("-symbolic"), "-symbolic") == 0)
        return TRUE;
    }

  if (icon_name_len > strlen ("-symbolic-ltr"))
    {
      if (strcmp (icon_name + icon_name_len - strlen ("-symbolic-ltr"), "-symbolic-ltr") == 0 ||
          strcmp (icon_name + icon_name_len - strlen ("-symbolic-rtl"), "-symbolic-rtl") == 0)
        return TRUE;
    }

  return FALSE;
}

static inline gboolean
icon_uri_is_symbolic (const gchar *icon_name,
                      int          icon_name_len)
{
  if (icon_name_len < 0)
    icon_name_len = strlen (icon_name);

  if (icon_name_len > strlen ("-symbolic.svg"))
    {
      if (strcmp (icon_name + icon_name_len - strlen ("-symbolic.svg"), "-symbolic.svg") == 0 ||
          strcmp (icon_name + icon_name_len - strlen (".symbolic.png"), ".symbolic.png") == 0)
        return TRUE;
    }

  if (icon_name_len > strlen ("-symbolic-ltr.svg"))
    {
      if (strcmp (icon_name + icon_name_len - strlen ("-symbolic.ltr.svg"), "-symbolic-ltr.svg") == 0 ||
          strcmp (icon_name + icon_name_len - strlen ("-symbolic.rtl.svg"), "-symbolic-rtl.svg") == 0)
        return TRUE;
    }

  return FALSE;
}

static GtkIcon *
real_choose_icon (GtkIconTheme       *self,
                  const gchar        *icon_names[],
                  gint                size,
                  gint                scale,
                  GtkIconLookupFlags  flags,
                  gboolean            non_blocking,
                  gboolean            *would_block)
{
  GList *l;
  GtkIcon *icon = NULL;
  GtkIcon *unscaled_icon;
  UnthemedIcon *unthemed_icon = NULL;
  const gchar *icon_name = NULL;
  gboolean allow_svg;
  IconTheme *theme = NULL;
  gint i;
  IconInfoKey key;

  if (!ensure_valid_themes (self, non_blocking))
    {
      *would_block = TRUE;
      return NULL;
    }

  key.icon_names = (gchar **)icon_names;
  key.size = size;
  key.scale = scale;
  key.flags = flags;

  icon = icon_cache_lookup (self, &key);
  if (icon)
    return icon;

  if (flags & GTK_ICON_LOOKUP_NO_SVG)
    allow_svg = FALSE;
  else if (flags & GTK_ICON_LOOKUP_FORCE_SVG)
    allow_svg = TRUE;
  else
    allow_svg = self->pixbuf_supports_svg;

  /* This is used in the icontheme unit test */
  GTK_DISPLAY_NOTE (self->display, ICONTHEME,
            for (i = 0; icon_names[i]; i++)
              g_message ("\tlookup name: %s", icon_names[i]));

  /* For symbolic icons, do a search in all registered themes first;
   * a theme that inherits them from a parent theme might provide
   * an alternative full-color version, but still expect the symbolic icon
   * to show up instead.
   *
   * In other words: We prefer symbolic icons in inherited themes over
   * generic icons in the theme.
   */
  for (l = self->themes; l; l = l->next)
    {
      theme = l->data;
      for (i = 0; icon_names[i] && icon_name_is_symbolic (icon_names[i], -1); i++)
        {
          icon_name = icon_names[i];
          icon = theme_lookup_icon (theme, icon_name, size, scale, allow_svg);
          if (icon)
            goto out;
        }
    }

  for (l = self->themes; l; l = l->next)
    {
      theme = l->data;

      for (i = 0; icon_names[i]; i++)
        {
          icon_name = icon_names[i];
          icon = theme_lookup_icon (theme, icon_name, size, scale, allow_svg);
          if (icon)
            goto out;
        }
    }

  theme = NULL;

  for (i = 0; icon_names[i]; i++)
    {
      unthemed_icon = g_hash_table_lookup (self->unthemed_icons, icon_names[i]);
      if (unthemed_icon)
        break;
    }
#ifdef G_OS_WIN32
  /* Still not found an icon, check if reference to a Win32 resource */
  if (!unthemed_icon)
    {
      gchar **resources;
      HICON hIcon = NULL;

      resources = g_strsplit (icon_names[0], ",", 0);
      if (resources[0])
        {
          wchar_t *wfile = g_utf8_to_utf16 (resources[0], -1, NULL, NULL, NULL);
          ExtractIconExW (wfile, resources[1] ? atoi (resources[1]) : 0, &hIcon, NULL, 1);
          g_free (wfile);
        }

      if (hIcon)
        {
          icon = icon_new (ICON_THEME_DIR_UNTHEMED, size, 1);
          icon->cache_pixbuf = gdk_win32_icon_to_pixbuf_libgtk_only (hIcon, NULL, NULL);
          DestroyIcon (hIcon);
        }
      g_strfreev (resources);
    }
#endif

  if (unthemed_icon)
    {
      icon = icon_new (ICON_THEME_DIR_UNTHEMED, size, 1);

      /* A SVG icon, when allowed, beats out a XPM icon, but not a PNG icon */
      if (allow_svg &&
          unthemed_icon->svg_filename &&
          (!unthemed_icon->no_svg_filename ||
           suffix_from_name (unthemed_icon->no_svg_filename) < ICON_SUFFIX_PNG))
        icon->filename = g_strdup (unthemed_icon->svg_filename);
      else if (unthemed_icon->no_svg_filename)
        icon->filename = g_strdup (unthemed_icon->no_svg_filename);
      else
        {
          static gboolean warned_once = FALSE;

          if (!warned_once)
            {
              g_warning ("Found an icon but could not load it. "
                         "Most likely gdk-pixbuf does not provide SVG support.");
              warned_once = TRUE;
            }

          g_clear_object (&icon);
          goto out;
        }

      icon->is_svg = suffix_from_name (icon->filename) == ICON_SUFFIX_SVG;
      icon->is_resource = unthemed_icon->is_resource;
    }

 out:
  if (icon)
    {
      icon->desired_size = size;
      icon->desired_scale = scale;
      icon->forced_size = (flags & GTK_ICON_LOOKUP_FORCE_SIZE) != 0;

      /* In case we're not scaling the icon we want to reuse the exact same
       * size as a scale==1 lookup would be, rather than not scaling at all
       * and causing a different layout
       */
      icon->unscaled_scale = 1.0;
      if (scale != 1 && !icon->forced_size && theme != NULL)
        {
          unscaled_icon = theme_lookup_icon (theme, icon_name, size, 1, allow_svg);
          if (unscaled_icon)
            {
              icon->unscaled_scale =
                (gdouble) unscaled_icon->dir_size * scale / (icon->dir_size * icon->dir_scale);
              g_object_unref (unscaled_icon);
            }
        }

      icon_compute_rendered_size (icon);

      icon->key.icon_names = g_strdupv ((char **)icon_names);
      icon->key.size = size;
      icon->key.scale = scale;
      icon->key.flags = flags;

      icon_cache_add (self, icon);
    }
  else
    {
      static gboolean check_for_default_theme = TRUE;
      gchar *default_theme_path;
      gboolean found = FALSE;

      if (check_for_default_theme)
        {
          check_for_default_theme = FALSE;

          for (i = 0; !found && i < self->search_path_len; i++)
            {
              default_theme_path = g_build_filename (self->search_path[i],
                                                     FALLBACK_ICON_THEME,
                                                     "index.theme",
                                                     NULL);
              found = g_file_test (default_theme_path, G_FILE_TEST_IS_REGULAR);
              g_free (default_theme_path);
            }

          if (!found)
            {
              g_warning ("Could not find the icon '%s'. The '%s' theme\n"
                         "was not found either, perhaps you need to install it.\n"
                         "You can get a copy from:\n"
                         "\t%s",
                         icon_names[0], FALLBACK_ICON_THEME, "http://icon-theme.freedesktop.org/releases");
            }
        }
    }

  return icon;
}

static void
icon_name_list_add_icon (GPtrArray   *icons,
                         const gchar *dir_suffix,
                         gchar       *icon_name)
{
  if (dir_suffix)
    g_ptr_array_add (icons, g_strconcat (icon_name, dir_suffix, NULL));
  g_ptr_array_add (icons, icon_name);
}

static GtkIcon *
choose_icon (GtkIconTheme       *self,
             const gchar        *icon_names[],
             gint                size,
             gint                scale,
             GtkIconLookupFlags  flags,
             gboolean            non_blocking,
             gboolean            *would_block)
{
  gboolean has_regular = FALSE, has_symbolic = FALSE;
  GtkIcon *icon;
  GPtrArray *new_names;
  const gchar *dir_suffix;
  guint i;

  if (flags & GTK_ICON_LOOKUP_DIR_LTR)
    dir_suffix = "-ltr";
  else if (flags & GTK_ICON_LOOKUP_DIR_RTL)
    dir_suffix = "-rtl";
  else
    dir_suffix = NULL;

  for (i = 0; icon_names[i]; i++)
    {
      if (icon_name_is_symbolic (icon_names[i], -1))
        has_symbolic = TRUE;
      else
        has_regular = TRUE;
    }

  if ((flags & GTK_ICON_LOOKUP_FORCE_REGULAR) && has_symbolic)
    {
      new_names = g_ptr_array_new_with_free_func (g_free);
      for (i = 0; icon_names[i]; i++)
        {
          if (icon_name_is_symbolic (icon_names[i], -1))
            icon_name_list_add_icon (new_names, dir_suffix, g_strndup (icon_names[i], strlen (icon_names[i]) - strlen ("-symbolic")));
          else
            icon_name_list_add_icon (new_names, dir_suffix, g_strdup (icon_names[i]));
        }
      for (i = 0; icon_names[i]; i++)
        {
          if (icon_name_is_symbolic (icon_names[i], -1))
            icon_name_list_add_icon (new_names, dir_suffix, g_strdup (icon_names[i]));
        }
      g_ptr_array_add (new_names, NULL);

      icon = real_choose_icon (self,
                                    (const gchar **) new_names->pdata,
                                    size,
                                    scale,
                                    flags & ~(GTK_ICON_LOOKUP_FORCE_REGULAR | GTK_ICON_LOOKUP_FORCE_SYMBOLIC),
                                    non_blocking, would_block);

      g_ptr_array_free (new_names, TRUE);
    }
  else if ((flags & GTK_ICON_LOOKUP_FORCE_SYMBOLIC) && has_regular)
    {
      new_names = g_ptr_array_new_with_free_func (g_free);
      for (i = 0; icon_names[i]; i++)
        {
          if (!icon_name_is_symbolic (icon_names[i], -1))
            icon_name_list_add_icon (new_names, dir_suffix, g_strconcat (icon_names[i], "-symbolic", NULL));
          else
            icon_name_list_add_icon (new_names, dir_suffix, g_strdup (icon_names[i]));
        }
      for (i = 0; icon_names[i]; i++)
        {
          if (!icon_name_is_symbolic (icon_names[i], -1))
            icon_name_list_add_icon (new_names, dir_suffix, g_strdup (icon_names[i]));
        }
      g_ptr_array_add (new_names, NULL);

      icon = real_choose_icon (self,
                                    (const gchar **) new_names->pdata,
                                    size,
                                    scale,
                                    flags & ~(GTK_ICON_LOOKUP_FORCE_REGULAR | GTK_ICON_LOOKUP_FORCE_SYMBOLIC),
                                    non_blocking, would_block);

      g_ptr_array_free (new_names, TRUE);
    }
  else if (dir_suffix)
    {
      new_names = g_ptr_array_new_with_free_func (g_free);
      for (i = 0; icon_names[i]; i++)
        {
          icon_name_list_add_icon (new_names, dir_suffix, g_strdup (icon_names[i]));
        }
      g_ptr_array_add (new_names, NULL);

      icon = real_choose_icon (self,
                                    (const gchar **) new_names->pdata,
                                    size,
                                    scale,
                                    flags & ~(GTK_ICON_LOOKUP_FORCE_REGULAR | GTK_ICON_LOOKUP_FORCE_SYMBOLIC),
                                    non_blocking, would_block);

      g_ptr_array_free (new_names, TRUE);
    }
  else
    {
      icon = real_choose_icon (self,
                                    icon_names,
                                    size,
                                    scale,
                                    flags & ~(GTK_ICON_LOOKUP_FORCE_REGULAR | GTK_ICON_LOOKUP_FORCE_SYMBOLIC),
                                    non_blocking, would_block);
    }

  return icon;
}

/**
 * gtk_icon_theme_lookup_icon_scale:
 * @self: a #GtkIconTheme
 * @icon_name: the name of the icon to lookup
 * @size: desired icon size
 * @scale: the desired scale
 * @flags: flags modifying the behavior of the icon lookup
 *
 * Looks up a named icon for a particular window scale and returns a
 * #GtkIcon containing information such as the filename of the
 * icon. The icon can then be rendered into a pixbuf using
 * gtk_icon_load_icon(). (gtk_icon_theme_load_icon() combines
 * these two steps if all you need is the pixbuf.)
 *
 * Returns: (nullable) (transfer full): a #GtkIcon object
 *     containing information about the icon, or %NULL if the
 *     icon wasn’t found.
 */
GtkIcon *
gtk_icon_theme_lookup_icon (GtkIconTheme       *self,
                            const gchar        *icon_name,
                            gint                size,
                            gint                scale,
                            GtkIconLookupFlags  flags)
{
  GtkIcon *icon;

  g_return_val_if_fail (GTK_IS_ICON_THEME (self), NULL);
  g_return_val_if_fail (icon_name != NULL, NULL);
  g_return_val_if_fail ((flags & GTK_ICON_LOOKUP_NO_SVG) == 0 ||
                        (flags & GTK_ICON_LOOKUP_FORCE_SVG) == 0, NULL);
  g_return_val_if_fail (scale >= 1, NULL);

  GTK_DISPLAY_NOTE (self->display, ICONTHEME,
                    g_message ("looking up icon %s for scale %d", icon_name, scale));

  gtk_icon_theme_lock (self);

  if (flags & GTK_ICON_LOOKUP_GENERIC_FALLBACK)
    {
      gchar **names, **nonsymbolic_names;
      gint dashes, i;
      gchar *p, *nonsymbolic_icon_name;
      gboolean is_symbolic;
      int icon_name_len = strlen (icon_name);

      is_symbolic = icon_name_is_symbolic (icon_name, icon_name_len);
      if (is_symbolic)
        nonsymbolic_icon_name = g_strndup (icon_name, icon_name_len - strlen ("-symbolic"));
      else
        nonsymbolic_icon_name = g_strdup (icon_name);
 
      dashes = 0;
      for (p = (gchar *) nonsymbolic_icon_name; *p; p++)
        if (*p == '-')
          dashes++;

      nonsymbolic_names = g_new (gchar *, dashes + 2);
      nonsymbolic_names[0] = nonsymbolic_icon_name;

      for (i = 1; i <= dashes; i++)
        {
          nonsymbolic_names[i] = g_strdup (nonsymbolic_names[i - 1]);
          p = strrchr (nonsymbolic_names[i], '-');
          *p = '\0';
        }
      nonsymbolic_names[dashes + 1] = NULL;

      if (is_symbolic)
        {
          names = g_new (gchar *, 2 * dashes + 3);
          for (i = 0; nonsymbolic_names[i] != NULL; i++)
            {
              names[i] = g_strconcat (nonsymbolic_names[i], "-symbolic", NULL);
              names[dashes + 1 + i] = nonsymbolic_names[i];
            }

          names[dashes + 1 + i] = NULL;
          g_free (nonsymbolic_names);
        }
      else
        {
          names = nonsymbolic_names;
        }

      icon = choose_icon (self, (const gchar **) names, size, scale, flags, FALSE, NULL);

      g_strfreev (names);
    }
  else
    {
      const gchar *names[2];

      names[0] = icon_name;
      names[1] = NULL;

      icon = choose_icon (self, names, size, scale, flags, FALSE, NULL);
    }

  gtk_icon_theme_unlock (self);

  return icon;
}

/**
 * gtk_icon_theme_choose_icon:
 * @self: a #GtkIconTheme
 * @icon_names: (array zero-terminated=1): %NULL-terminated
 *     array of icon names to lookup
 * @size: desired icon size
 * @scale: desired scale
 * @flags: flags modifying the behavior of the icon lookup
 * 
 * Looks up a named icon for a particular window scale and returns
 * a #GtkIcon containing information such as the filename of the
 * icon. The icon can then be rendered into a pixbuf using
 * gtk_icon_load_icon(). (gtk_icon_theme_load_icon()
 * combines these two steps if all you need is the pixbuf.)
 *
 * If @icon_names contains more than one name, this function 
 * tries them all in the given order before falling back to 
 * inherited icon themes.
 *
 * Returns: (nullable) (transfer full): a #GtkIcon object
 *     containing information about the icon, or %NULL if the
 *     icon wasn’t found.
 */
GtkIcon *
gtk_icon_theme_choose_icon (GtkIconTheme       *self,
                            const gchar        *icon_names[],
                            gint                size,
                            gint                scale,
                            GtkIconLookupFlags  flags)
{
  GtkIcon *icon;

  g_return_val_if_fail (GTK_IS_ICON_THEME (self), NULL);
  g_return_val_if_fail (icon_names != NULL, NULL);
  g_return_val_if_fail ((flags & GTK_ICON_LOOKUP_NO_SVG) == 0 ||
                        (flags & GTK_ICON_LOOKUP_FORCE_SVG) == 0, NULL);
  g_return_val_if_fail (scale >= 1, NULL);
  g_warn_if_fail ((flags & GTK_ICON_LOOKUP_GENERIC_FALLBACK) == 0);

  gtk_icon_theme_lock (self);

  icon = choose_icon (self, icon_names, size, scale, flags, FALSE, NULL);

  gtk_icon_theme_unlock (self);

  return icon;
}

typedef struct {
  char **icon_names;
  int size;
  int scale;
  GtkIconLookupFlags flags;
} ChooseIconData;

static ChooseIconData *
choose_icon_data_new (const char        *icon_names[],
                     int                size,
                     int                scale,
                     GtkIconLookupFlags flags)
{
  ChooseIconData *data = g_new0 (ChooseIconData, 1);
  data->icon_names = g_strdupv ((char **)icon_names);
  data->size = size;
  data->scale = scale;
  data->flags = flags;
  return data;
}

static void
choose_icon_data_free (ChooseIconData *data)
{
  g_strfreev (data->icon_names);
  g_free (data);
}

static void
choose_icon_thread  (GTask        *task,
                     gpointer      source_object,
                     gpointer      task_data,
                     GCancellable *cancellable)
{
  ChooseIconData *data = task_data;
  GtkIconTheme *self = GTK_ICON_THEME (source_object);
  GtkIcon *icon;

  icon = gtk_icon_theme_choose_icon (self,
                                     (const char **)data->icon_names,
                                     data->size,
                                     data->scale,
                                     data->flags);

  if (icon)
    {
      g_mutex_lock (&icon->texture_lock);
      (void)icon_ensure_scale_and_texture__locked (icon);

      if (icon->texture)
        g_task_return_pointer (task, g_object_ref (icon), g_object_unref);
      else if (icon->load_error)
        g_task_return_error (task, g_error_copy (icon->load_error));
      else
        g_task_return_new_error (task,
                                 GTK_ICON_THEME_ERROR,  GTK_ICON_THEME_NOT_FOUND,
                                 _("Icon not present in theme %s"), self->current_theme);

      g_mutex_unlock (&icon->texture_lock);
    }
  else
    g_task_return_new_error (task,
                             GTK_ICON_THEME_ERROR,  GTK_ICON_THEME_NOT_FOUND,
                             _("Icon not present in theme %s"), self->current_theme);
}

static void
load_icon_thread  (GTask        *task,
                   gpointer      source_object,
                   gpointer      task_data,
                   GCancellable *cancellable)
{
  GtkIcon *icon = task_data;

  g_mutex_lock (&icon->texture_lock);
  (void)icon_ensure_scale_and_texture__locked (icon);
  g_mutex_unlock (&icon->texture_lock);
  g_task_return_pointer (task, g_object_ref (icon), g_object_unref);
}

/**
 * gtk_icon_theme_choose_icon_async:
 * @self: a #GtkIconTheme
 * @icon_names: (array zero-terminated=1): %NULL-terminated array of
 *     icon names to lookup
 * @size: desired icon size
 * @scale: desired scale
 * @flags: flags modifying the behavior of the icon lookup
 * @cancellable: (allow-none): optional #GCancellable object, %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the
 *     request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously lookup, load, render and scale an icon .
 *
 * For more details, see gtk_icon_theme_choose_icon() which is the synchronous
 * version of this call.
 *
 * Returns: (nullable) (transfer full): a #GtkIcon object
 *     containing information about the icon, or %NULL if the
 *     icon wasn’t found.
 */
void
gtk_icon_theme_choose_icon_async (GtkIconTheme       *self,
                                  const gchar        *icon_names[],
                                  gint                size,
                                  gint                scale,
                                  GtkIconLookupFlags  flags,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  GTask *task;
  GtkIcon *icon = NULL;
  gboolean would_block = FALSE;

  g_return_if_fail (GTK_IS_ICON_THEME (self));
  g_return_if_fail (icon_names != NULL);
  g_return_if_fail ((flags & GTK_ICON_LOOKUP_NO_SVG) == 0 ||
                    (flags & GTK_ICON_LOOKUP_FORCE_SVG) == 0);
  g_warn_if_fail ((flags & GTK_ICON_LOOKUP_GENERIC_FALLBACK) == 0);

  task = g_task_new (self, cancellable, callback, user_data);

  if (gtk_icon_theme_trylock (self))
    {
      icon = choose_icon (self, icon_names, size, scale, flags, TRUE, &would_block);
      gtk_icon_theme_unlock (self);
    }
  else
    would_block = TRUE;

  if (icon == NULL)
    {
      if (would_block)
        {
          /* Don't have valid theme data, do everything in a thread */
          g_task_set_task_data (task, choose_icon_data_new (icon_names, size, scale, flags), (GDestroyNotify)choose_icon_data_free);
          g_task_run_in_thread (task, choose_icon_thread);
        }
      else
        {
          g_task_return_new_error (task,
                                   GTK_ICON_THEME_ERROR,  GTK_ICON_THEME_NOT_FOUND,
                                   _("Icon not present in theme %s"), self->current_theme);
        }
    }
  else
    {
      gboolean done = FALSE;
      if (g_mutex_trylock (&icon->texture_lock))
        {
          if (icon->texture)
            {
              done = TRUE;
              g_task_return_pointer (task, icon, g_object_unref);
            }
          else if (icon->load_error)
            {
              done = TRUE;
              g_task_return_error (task, g_error_copy (icon->load_error));
              g_object_unref (icon);
            }
          g_mutex_unlock (&icon->texture_lock);
        }

      if (!done)
        {
          /* Not here, load it in a thread */
          g_task_set_task_data (task, icon, g_object_unref);
          g_task_run_in_thread (task, load_icon_thread);
        }
    }
}

/**
 * gtk_icon_theme_choose_icon_finish:
 * @self: a #GtkIconTheme
 * @res: a #GAsyncResult
 * @error: (allow-none): location to store error information on failure,
 *     or %NULL.
 *
 * Finishes an async icon load, see gtk_icon_load_icon_async().
 *
 * Returns: (transfer full): the rendered icon; this may be a newly
 *     created icon or a new reference to an internal icon, so you must
 *     not modify the icon. Use g_object_unref() to release your reference
 *     to the icon.
 */
GtkIcon *
gtk_icon_theme_choose_icon_finish (GtkIconTheme *self,
                                   GAsyncResult *result,
                                   GError       **error)
{
  GTask *task = G_TASK (result);

  g_return_val_if_fail (g_task_is_valid (result, self), NULL);

  return g_task_propagate_pointer (task, error);
}

/* Error quark */
GQuark
gtk_icon_theme_error_quark (void)
{
  return g_quark_from_static_string ("gtk-icon-theme-error-quark");
}

void
gtk_icon_theme_lookup_symbolic_colors (GtkCssStyle *style,
                                       GdkRGBA     *color_out,
                                       GdkRGBA     *success_out,
                                       GdkRGBA     *warning_out,
                                       GdkRGBA     *error_out)
{
  GtkCssValue *palette, *color;
  const GdkRGBA *lookup;

  color = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_COLOR);
  palette = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ICON_PALETTE);
  *color_out = *gtk_css_color_value_get_rgba (color);

  lookup = gtk_css_palette_value_get_color (palette, "success");
  if (lookup)
    *success_out = *lookup;
  else
    *success_out = *color_out;

  lookup = gtk_css_palette_value_get_color (palette, "warning");
  if (lookup)
    *warning_out = *lookup;
  else
    *warning_out = *color_out;

  lookup = gtk_css_palette_value_get_color (palette, "error");
  if (lookup)
    *error_out = *lookup;
  else
    *error_out = *color_out;
}


/**
 * gtk_icon_theme_has_icon:
 * @self: a #GtkIconTheme
 * @icon_name: the name of an icon
 *
 * Checks whether an icon theme includes an icon
 * for a particular name.
 *
 * Returns: %TRUE if @self includes an
 *  icon for @icon_name.
 */
gboolean 
gtk_icon_theme_has_icon (GtkIconTheme *self,
                         const gchar  *icon_name)
{
  GList *l;
  gboolean res = FALSE;

  g_return_val_if_fail (GTK_IS_ICON_THEME (self), FALSE);
  g_return_val_if_fail (icon_name != NULL, FALSE);

  gtk_icon_theme_lock (self);

  ensure_valid_themes (self, FALSE);

  for (l = self->dir_mtimes; l; l = l->next)
    {
      IconThemeDirMtime *dir_mtime = l->data;
      GtkIconCache *cache = dir_mtime->cache;

      if (cache && gtk_icon_cache_has_icon (cache, icon_name))
        {
          res = TRUE;
          goto out;
        }
    }

  for (l = self->themes; l; l = l->next)
    {
      if (theme_has_icon (l->data, icon_name))
        {
          res = TRUE;
          goto out;
        }
    }

 out:
  gtk_icon_theme_unlock (self);

  return res;
}

static void
add_size (gpointer key,
          gpointer value,
          gpointer user_data)
{
  gint **res_p = user_data;

  **res_p = GPOINTER_TO_INT (key);

  (*res_p)++;
}

/**
 * gtk_icon_theme_get_icon_sizes:
 * @self: a #GtkIconTheme
 * @icon_name: the name of an icon
 * 
 * Returns an array of integers describing the sizes at which
 * the icon is available without scaling. A size of -1 means 
 * that the icon is available in a scalable format. The array 
 * is zero-terminated.
 *
 * Returns: (array zero-terminated=1) (transfer full): A newly
 * allocated array describing the sizes at which the icon is
 * available. The array should be freed with g_free() when it is no
 * longer needed.
 */
gint *
gtk_icon_theme_get_icon_sizes (GtkIconTheme *self,
                               const gchar  *icon_name)
{
  GList *l, *d;
  GHashTable *sizes;
  gint *result, *r;
  guint suffix;

  g_return_val_if_fail (GTK_IS_ICON_THEME (self), NULL);

  gtk_icon_theme_lock (self);

  ensure_valid_themes (self, FALSE);

  sizes = g_hash_table_new (g_direct_hash, g_direct_equal);

  for (l = self->themes; l; l = l->next)
    {
      IconTheme *theme = l->data;
      for (d = theme->dirs; d; d = d->next)
        {
          IconThemeDir *dir = d->data;

          if (dir->type != ICON_THEME_DIR_SCALABLE && g_hash_table_lookup_extended (sizes, GINT_TO_POINTER (dir->size), NULL, NULL))
            continue;

          suffix = theme_dir_get_icon_suffix (dir, icon_name);
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

  gtk_icon_theme_unlock (self);

  return result;
}

static void
add_key_to_hash (gpointer key,
                 gpointer value,
                 gpointer user_data)
{
  GHashTable *hash = user_data;

  g_hash_table_insert (hash, key, NULL);
}

static void
add_key_to_list (gpointer key,
                 gpointer value,
                 gpointer user_data)
{
  GList **list = user_data;

  *list = g_list_prepend (*list, g_strdup (key));
}

/**
 * gtk_icon_theme_list_icons:
 * @self: a #GtkIconTheme
 * @context: (allow-none): a string identifying a particular type of
 *           icon, or %NULL to list all icons.
 *
 * Lists the icons in the current icon theme. Only a subset
 * of the icons can be listed by providing a context string.
 * The set of values for the context string is system dependent,
 * but will typically include such values as “Applications” and
 * “MimeTypes”. Contexts are explained in the
 * [Icon Theme Specification](http://www.freedesktop.org/wiki/Specifications/icon-theme-spec).
 * The standard contexts are listed in the
 * [Icon Naming Specification](http://www.freedesktop.org/wiki/Specifications/icon-naming-spec).
 *
 * Returns: (element-type utf8) (transfer full): a #GList list
 *     holding the names of all the icons in the theme. You must
 *     first free each element in the list with g_free(), then
 *     free the list itself with g_list_free().
 */
GList *
gtk_icon_theme_list_icons (GtkIconTheme *self,
                           const gchar  *context)
{
  GHashTable *icons;
  GList *list, *l;
  GQuark context_quark;

  gtk_icon_theme_lock (self);

  ensure_valid_themes (self, FALSE);

  if (context)
    {
      context_quark = g_quark_try_string (context);

      if (!context_quark)
        goto out;
    }
  else
    context_quark = 0;

  icons = g_hash_table_new (g_str_hash, g_str_equal);
  
  l = self->themes;
  while (l != NULL)
    {
      theme_list_icons (l->data, icons, context_quark);
      l = l->next;
    }

  if (context_quark == 0)
    g_hash_table_foreach (self->unthemed_icons,
                          add_key_to_hash,
                          icons);

  list = NULL;
  
  g_hash_table_foreach (icons,
                        add_key_to_list,
                        &list);

  g_hash_table_destroy (icons);

 out:

  gtk_icon_theme_unlock (self);

  return list;
}

static gboolean
rescan_themes (GtkIconTheme *self)
{
  IconThemeDirMtime *dir_mtime;
  GList *d;
  gint stat_res;
  GStatBuf stat_buf;
  GTimeVal tv;

  for (d = self->dir_mtimes; d != NULL; d = d->next)
    {
      dir_mtime = d->data;

      stat_res = g_stat (dir_mtime->dir, &stat_buf);

      /* dir mtime didn't change */
      if (stat_res == 0 && dir_mtime->exists &&
          S_ISDIR (stat_buf.st_mode) &&
          dir_mtime->mtime == stat_buf.st_mtime)
        continue;
      /* didn't exist before, and still doesn't */
      if (!dir_mtime->exists &&
          (stat_res != 0 || !S_ISDIR (stat_buf.st_mode)))
        continue;

      return TRUE;
    }

  g_get_current_time (&tv);
  self->last_stat_time = tv.tv_sec;

  return FALSE;
}

/**
 * gtk_icon_theme_rescan_if_needed:
 * @self: a #GtkIconTheme
 * 
 * Checks to see if the icon theme has changed; if it has, any
 * currently cached information is discarded and will be reloaded
 * next time @self is accessed.
 * 
 * Returns: %TRUE if the icon theme has changed and needed
 *     to be reloaded.
 */
gboolean
gtk_icon_theme_rescan_if_needed (GtkIconTheme *self)
{
  gboolean retval;

  g_return_val_if_fail (GTK_IS_ICON_THEME (self), FALSE);

  gtk_icon_theme_lock (self);

  retval = rescan_themes (self);
  if (retval)
      do_theme_change (self);

  gtk_icon_theme_unlock (self);

  return retval;
}

static void
theme_destroy (IconTheme *theme)
{
  g_free (theme->display_name);
  g_free (theme->comment);
  g_free (theme->name);

  g_list_free_full (theme->dirs, (GDestroyNotify) theme_dir_destroy);
  
  g_free (theme);
}

static void
theme_dir_destroy (IconThemeDir *dir)
{
  if (dir->cache)
    gtk_icon_cache_unref (dir->cache);
  if (dir->icons)
    g_hash_table_destroy (dir->icons);
  
  g_free (dir->dir);
  g_free (dir->subdir);
  g_free (dir);
}

static int
theme_dir_size_difference (IconThemeDir *dir,
                           gint          size,
                           gint          scale)
{
  gint scaled_size, scaled_dir_size;
  gint min, max;

  scaled_size = size * scale;
  scaled_dir_size = dir->size * dir->scale;

  switch (dir->type)
    {
    case ICON_THEME_DIR_FIXED:
      return abs (scaled_size - scaled_dir_size);

    case ICON_THEME_DIR_SCALABLE:
      if (scaled_size < (dir->min_size * dir->scale))
        return (dir->min_size * dir->scale) - scaled_size;
      if (size > (dir->max_size * dir->scale))
        return scaled_size - (dir->max_size * dir->scale);
      return 0;

    case ICON_THEME_DIR_THRESHOLD:
      min = (dir->size - dir->threshold) * dir->scale;
      max = (dir->size + dir->threshold) * dir->scale;
      if (scaled_size < min)
        return min - scaled_size;
      if (scaled_size > max)
        return scaled_size - max;
      return 0;

    case ICON_THEME_DIR_UNTHEMED:
    default:
      g_assert_not_reached ();
      return 1000;
    }
}

static const gchar *
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
    case ICON_SUFFIX_SYMBOLIC_PNG:
      return ".symbolic.png";
    case ICON_SUFFIX_NONE:
    case HAS_ICON_FILE:
    default:
      g_assert_not_reached();
      return NULL;
    }
}

static inline IconSuffix
suffix_from_name (const gchar *name)
{
  const gsize name_len = strlen (name);

  if (name_len > 4)
    {
      if (name_len > strlen (".symbolic.png"))
        {
          if (strcmp (name + name_len - strlen (".symbolic.png"), ".symbolic.png") == 0)
            return ICON_SUFFIX_SYMBOLIC_PNG;
        }

      if (strcmp (name + name_len - strlen (".png"), ".png") == 0)
        return ICON_SUFFIX_PNG;

      if (strcmp (name + name_len - strlen (".svg"), ".svg") == 0)
        return ICON_SUFFIX_SVG;

      if (strcmp (name + name_len - strlen (".xpm"), ".xpm") == 0)
        return ICON_SUFFIX_XPM;
    }

  return ICON_SUFFIX_NONE;
}

static IconSuffix
best_suffix (IconSuffix suffix,
             gboolean   allow_svg)
{
  if ((suffix & ICON_SUFFIX_SYMBOLIC_PNG) != 0)
    return ICON_SUFFIX_SYMBOLIC_PNG;
  else if ((suffix & ICON_SUFFIX_PNG) != 0)
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
                           const gchar  *icon_name)
{
  IconSuffix suffix, symbolic_suffix;

  if (dir->cache)
    {
      int icon_name_len = strlen (icon_name);

      if (icon_name_is_symbolic (icon_name, icon_name_len))
        {
          /* Look for foo-symbolic.symbolic.png, as the cache only stores the ".png" suffix */
          char *icon_name_with_prefix = g_strconcat (icon_name, ".symbolic", NULL);
          symbolic_suffix = (IconSuffix)gtk_icon_cache_get_icon_flags (dir->cache,
                                                                        icon_name_with_prefix,
                                                                        dir->subdir_index);
          g_free (icon_name_with_prefix);

          if (symbolic_suffix & ICON_SUFFIX_PNG)
            suffix = ICON_SUFFIX_SYMBOLIC_PNG;
          else
            suffix = (IconSuffix)gtk_icon_cache_get_icon_flags (dir->cache,
                                                                icon_name,
                                                                dir->subdir_index);
        }
      else
        suffix = (IconSuffix)gtk_icon_cache_get_icon_flags (dir->cache,
                                                            icon_name,
                                                            dir->subdir_index);

      suffix = suffix & ~HAS_ICON_FILE;
    }
  else
    suffix = GPOINTER_TO_UINT (g_hash_table_lookup (dir->icons, icon_name));

  GTK_NOTE (ICONTHEME, g_message ("get icon suffix%s: %u", dir->cache ? " (cached)" : "", suffix));

  return suffix;
}

/* returns TRUE if dir_a is a better match */
static gboolean
compare_dir_matches (IconThemeDir *dir_a, gint difference_a,
                     IconThemeDir *dir_b, gint difference_b,
                     gint requested_size,
                     gint requested_scale)
{
  gint diff_a;
  gint diff_b;

  if (difference_a == 0)
    {
      if (difference_b != 0)
        return TRUE;
      
      /* a and b both exact matches */
    }
  else
    {
      /* If scaling, *always* prefer downscaling */
      if (dir_a->size >= requested_size &&
          dir_b->size < requested_size)
        return TRUE;

      if (dir_a->size < requested_size &&
          dir_b->size >= requested_size)
        return FALSE;
 
      /* Otherwise prefer the closest match */

      if (difference_a < difference_b)
        return TRUE;

      if (difference_a > difference_b)
        return FALSE;

      /* same pixel difference */
    }

  if (dir_a->scale == requested_scale &&
      dir_b->scale != requested_scale)
    return TRUE;

  if (dir_a->scale != requested_scale &&
      dir_b->scale == requested_scale)
    return FALSE;

  /* a and b both match the scale */

  if (dir_a->type != ICON_THEME_DIR_SCALABLE &&
      dir_b->type == ICON_THEME_DIR_SCALABLE)
    return TRUE;

  if (dir_a->type == ICON_THEME_DIR_SCALABLE &&
      dir_b->type != ICON_THEME_DIR_SCALABLE)
    return FALSE;
      
  /* a and b both are scalable */

  diff_a = abs (requested_size * requested_scale - dir_a->size * dir_a->scale);
  diff_b = abs (requested_size * requested_scale - dir_b->size * dir_b->scale);

  return diff_a <= diff_b;
}

static GtkIcon *
theme_lookup_icon (IconTheme   *theme,
                   const gchar *icon_name,
                   gint         size,
                   gint         scale,
                   gboolean     allow_svg)
{
  GList *dirs, *l;
  IconThemeDir *dir, *min_dir;
  gchar *file;
  gint min_difference, difference;
  IconSuffix suffix;
  IconSuffix min_suffix;

  min_difference = G_MAXINT;
  min_dir = NULL;

  dirs = theme->dirs;

  l = dirs;
  while (l != NULL)
    {
      dir = l->data;

      GTK_NOTE (ICONTHEME, g_message ("look up icon dir %s", dir->dir));
      suffix = theme_dir_get_icon_suffix (dir, icon_name);
      if (best_suffix (suffix, allow_svg) != ICON_SUFFIX_NONE)
        {
          difference = theme_dir_size_difference (dir, size, scale);
          if (min_dir == NULL ||
              compare_dir_matches (dir, difference,
                                   min_dir, min_difference,
                                   size, scale))
            {
              min_dir = dir;
              min_suffix = suffix;
              min_difference = difference;
            }
        }

      l = l->next;
    }

  if (min_dir)
    {
      GtkIcon *icon;

      icon = icon_new (min_dir->type, min_dir->size, min_dir->scale);
      icon->min_size = min_dir->min_size;
      icon->max_size = min_dir->max_size;

      suffix = min_suffix;
      suffix = best_suffix (suffix, allow_svg);
      g_assert (suffix != ICON_SUFFIX_NONE);

      if (min_dir->dir)
        {
          file = g_strconcat (icon_name, string_from_suffix (suffix), NULL);
          icon->filename = g_build_filename (min_dir->dir, file, NULL);

          icon->is_svg = suffix == ICON_SUFFIX_SVG;
          icon->is_resource = min_dir->is_resource;
          g_free (file);
        }
      else
        {
          icon->filename = NULL;
        }

      if (min_dir->cache)
        {
          icon->cache_pixbuf = gtk_icon_cache_get_icon (min_dir->cache, icon_name,
                                                              min_dir->subdir_index);
        }

      return icon;
    }

  return NULL;
}

static void
theme_list_icons (IconTheme  *theme, 
                  GHashTable *icons,
                  GQuark      context)
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
            gtk_icon_cache_add_icons (dir->cache, dir->subdir, icons);
          else
            g_hash_table_foreach (dir->icons, add_key_to_hash, icons);
        }
      l = l->next;
    }
}

static gboolean
theme_has_icon (IconTheme   *theme,
                const gchar *icon_name)
{
  GList *l;

  for (l = theme->dirs; l; l = l->next)
    {
      IconThemeDir *dir = l->data;

      if (dir->cache)
        {
          if (gtk_icon_cache_has_icon (dir->cache, icon_name))
            return TRUE;
        }
      else
        {
          if (g_hash_table_lookup (dir->icons, icon_name) != NULL)
            return TRUE;
        }
    }

  return FALSE;
}

static GHashTable *
scan_directory (GtkIconTheme  *self,
                char          *full_dir)
{
  GDir *gdir;
  const gchar *name;
  GHashTable *icons = NULL;

  GTK_DISPLAY_NOTE (self->display, ICONTHEME,
                    g_message ("scanning directory %s", full_dir));

  gdir = g_dir_open (full_dir, 0, NULL);

  if (gdir == NULL)
    return NULL;

  while ((name = g_dir_read_name (gdir)))
    {
      gchar *base_name;
      IconSuffix suffix, hash_suffix;

      suffix = suffix_from_name (name);
      if (suffix == ICON_SUFFIX_NONE)
        continue;

      if (!icons)
        icons = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

      base_name = strip_suffix (name);

      hash_suffix = GPOINTER_TO_INT (g_hash_table_lookup (icons, base_name));
      /* takes ownership of base_name */
      g_hash_table_replace (icons, base_name, GUINT_TO_POINTER (hash_suffix|suffix));
    }

  g_dir_close (gdir);

  return icons;
}

static void
theme_subdir_load (GtkIconTheme *self,
                   IconTheme    *theme,
                   GKeyFile     *theme_file,
                   gchar        *subdir)
{
  GList *d;
  gchar *type_string;
  IconThemeDir *dir;
  IconThemeDirType type;
  gchar *context_string;
  GQuark context;
  gint size;
  gint min_size;
  gint max_size;
  gint threshold;
  gchar *full_dir;
  GError *error = NULL;
  IconThemeDirMtime *dir_mtime;
  gint scale;

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

  if (g_key_file_has_key (theme_file, subdir, "MaxSize", NULL))
    max_size = g_key_file_get_integer (theme_file, subdir, "MaxSize", NULL);
  else
    max_size = size;

  if (g_key_file_has_key (theme_file, subdir, "MinSize", NULL))
    min_size = g_key_file_get_integer (theme_file, subdir, "MinSize", NULL);
  else
    min_size = size;

  if (g_key_file_has_key (theme_file, subdir, "Threshold", NULL))
    threshold = g_key_file_get_integer (theme_file, subdir, "Threshold", NULL);
  else
    threshold = 2;

  if (g_key_file_has_key (theme_file, subdir, "Scale", NULL))
    scale = g_key_file_get_integer (theme_file, subdir, "Scale", NULL);
  else
    scale = 1;

  for (d = self->dir_mtimes; d; d = d->next)
    {
      dir_mtime = (IconThemeDirMtime *)d->data;

      if (!dir_mtime->exists)
        continue; /* directory doesn't exist */

      full_dir = g_build_filename (dir_mtime->dir, subdir, NULL);

      /* First, see if we have a cache for the directory */
      if (dir_mtime->cache != NULL || g_file_test (full_dir, G_FILE_TEST_IS_DIR))
        {
          gboolean has_icons;
          GtkIconCache *dir_cache;
          GHashTable *icon_table = NULL;

          if (dir_mtime->cache == NULL)
            {
              /* This will return NULL if the cache doesn't exist or is outdated */
              dir_mtime->cache = gtk_icon_cache_new_for_path (dir_mtime->dir);
            }

          if (dir_mtime->cache != NULL)
            {
              dir_cache = dir_mtime->cache;
              has_icons = gtk_icon_cache_has_icons (dir_cache, subdir);
            }
          else
            {
              dir_cache = NULL;
              icon_table = scan_directory (self, full_dir);
              has_icons = icon_table != NULL;
            }

          if (!has_icons)
            {
              g_assert (!icon_table);
              g_free (full_dir);
              continue;
            }

          dir = g_new0 (IconThemeDir, 1);
          dir->type = type;
          dir->is_resource = FALSE;
          dir->context = context;
          dir->size = size;
          dir->min_size = min_size;
          dir->max_size = max_size;
          dir->threshold = threshold;
          dir->dir = full_dir;
          dir->subdir = g_strdup (subdir);
          dir->scale = scale;
          dir->icons = icon_table;

          if (dir_cache)
            {
              dir->cache = gtk_icon_cache_ref (dir_cache);
              dir->subdir_index = gtk_icon_cache_get_directory_index (dir->cache, dir->subdir);
            }
          else
            {
              dir_cache = NULL;
              dir->subdir_index = -1;
            }

          theme->dirs = g_list_prepend (theme->dirs, dir);
        }
      else
        g_free (full_dir);
    }

  if (strcmp (theme->name, FALLBACK_ICON_THEME) == 0)
    {
      for (d = self->resource_paths; d; d = d->next)
        {
          int i;
          char **children;

          /* Force a trailing / here, to avoid extra copies in GResource */
          full_dir = g_build_filename ((const gchar *)d->data, subdir, " ", NULL);
          full_dir[strlen (full_dir) - 1] = '\0';

          children = g_resources_enumerate_children (full_dir, 0, NULL);

          if (!children)
            {
              g_free (full_dir);
              continue;
            }

          dir = g_new0 (IconThemeDir, 1);
          dir->icons = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
          dir->type = type;
          dir->is_resource = TRUE;
          dir->context = context;
          dir->size = size;
          dir->min_size = min_size;
          dir->max_size = max_size;
          dir->threshold = threshold;
          dir->dir = full_dir;
          dir->subdir = g_strdup (subdir);
          dir->scale = scale;
          dir->cache = NULL;
          dir->subdir_index = -1;

          for (i = 0; children[i]; i++)
            {
              gchar *base_name;
              IconSuffix suffix, hash_suffix;

              suffix = suffix_from_name (children[i]);
              if (suffix == ICON_SUFFIX_NONE)
                continue;

              base_name = strip_suffix (children[i]);

              hash_suffix = GPOINTER_TO_INT (g_hash_table_lookup (dir->icons, base_name));
              /* takes ownership of base_name */
              g_hash_table_replace (dir->icons, base_name, GUINT_TO_POINTER (hash_suffix|suffix));
            }
          g_strfreev (children);

          if (g_hash_table_size (dir->icons) > 0)
            theme->dirs = g_list_prepend (theme->dirs, dir);
          else
            theme_dir_destroy (dir);
        }
    }
}

/*
 * GtkIcon
 */

static void icon_paintable_init (GdkPaintableInterface *iface);


G_DEFINE_TYPE_WITH_CODE (GtkIcon, gtk_icon, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                icon_paintable_init))

static void
gtk_icon_init (GtkIcon *icon)
{
  icon->scale = -1.;
  g_mutex_init (&icon->texture_lock);
}

static GtkIcon *
icon_new (IconThemeDirType type,
               gint             dir_size,
               gint             dir_scale)
{
  GtkIcon *icon;

  icon = g_object_new (GTK_TYPE_ICON, NULL);

  icon->dir_type = type;
  icon->dir_size = dir_size;
  icon->dir_scale = dir_scale;
  icon->unscaled_scale = 1.0;
  icon->is_svg = FALSE;
  icon->is_resource = FALSE;
  icon->rendered_size = -1;

  return icon;
}

static void
icon_compute_rendered_size (GtkIcon *icon)
{
  int rendered_size;

  if (icon->forced_size ||
      icon->dir_type == ICON_THEME_DIR_UNTHEMED)
    {
      rendered_size = icon->desired_size;
    }
  else if (icon->dir_type == ICON_THEME_DIR_FIXED ||
           icon->dir_type == ICON_THEME_DIR_THRESHOLD)
    {
      rendered_size = icon->dir_size * icon->dir_scale * icon->unscaled_scale /  icon->desired_scale;
    }
  else /* Scalable */
    {
      gdouble dir_scale = icon->dir_scale;
      gint scaled_desired_size;

      scaled_desired_size = icon->desired_size * icon->desired_scale;

      /* See icon_ensure_scale_and_texture() comment for why we do this */
      if (icon->is_svg)
        dir_scale = icon->desired_scale;

      if (scaled_desired_size < icon->min_size * dir_scale)
        rendered_size = icon->min_size * dir_scale;
      else if (scaled_desired_size > icon->max_size * dir_scale)
        rendered_size = icon->max_size * dir_scale;
      else
        rendered_size = scaled_desired_size;

      rendered_size /= icon->desired_scale;
    }

  icon->rendered_size = rendered_size;
}

static void
gtk_icon_finalize (GObject *object)
{
  GtkIcon *icon = (GtkIcon *) object;

  icon_cache_remove (icon);

  g_strfreev (icon->key.icon_names);

  g_free (icon->filename);

  g_clear_object (&icon->loadable);
  g_clear_object (&icon->texture);
  g_clear_object (&icon->cache_pixbuf);
  g_clear_error (&icon->load_error);

  g_mutex_clear (&icon->texture_lock);

  G_OBJECT_CLASS (gtk_icon_parent_class)->finalize (object);
}

static void
gtk_icon_class_init (GtkIconClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = gtk_icon_finalize;
}

/**
 * gtk_icon_get_base_size:
 * @self: a #GtkIcon
 * 
 * Gets the base size for the icon. The base size
 * is a size for the icon that was specified by
 * the icon theme creator. This may be different
 * than the actual size of image;
 * These icons will be given
 * the same base size as the larger icons to which
 * they are attached.
 *
 * Note that for scaled icons the base size does
 * not include the base scale.
 *
 * Returns: the base size, or 0, if no base
 *     size is known for the icon.
 */
gint
gtk_icon_get_base_size (GtkIcon *icon)
{
  g_return_val_if_fail (icon != NULL, 0);

  return icon->dir_size;
}

/**
 * gtk_icon_get_base_scale:
 * @self: a #GtkIcon
 *
 * Gets the base scale for the icon. The base scale is a scale
 * for the icon that was specified by the icon theme creator.
 * For instance an icon drawn for a high-dpi monitor with window
 * scale 2 for a base size of 32 will be 64 pixels tall and have
 * a base scale of 2.
 *
 * Returns: the base scale
 */
gint
gtk_icon_get_base_scale (GtkIcon *icon)
{
  g_return_val_if_fail (icon != NULL, 0);

  return icon->dir_scale;
}

/**
 * gtk_icon_get_filename:
 * @self: a #GtkIcon
 * 
 * Gets the filename for the icon. If the %GTK_ICON_LOOKUP_USE_BUILTIN
 * flag was passed to gtk_icon_theme_lookup_icon(), there may be no
 * filename if a builtin icon is returned; in this case, you should
 * use gtk_icon_get_builtin_pixbuf().
 * 
 * Returns: (nullable) (type filename): the filename for the icon, or %NULL
 *     if gtk_icon_get_builtin_pixbuf() should be used instead.
 *     The return value is owned by GTK+ and should not be modified
 *     or freed.
 */
const gchar *
gtk_icon_get_filename (GtkIcon *icon)
{
  g_return_val_if_fail (icon != NULL, NULL);

  return icon->filename;
}

/**
 * gtk_icon_is_symbolic:
 * @self: a #GtkIcon
 *
 * Checks if the icon is symbolic or not. This currently uses only
 * the file name and not the file contents for determining this.
 * This behaviour may change in the future.
 *
 * Returns: %TRUE if the icon is symbolic, %FALSE otherwise
 */
gboolean
gtk_icon_is_symbolic (GtkIcon *icon)
{
  g_return_val_if_fail (GTK_IS_ICON (icon), FALSE);

  return icon->filename != NULL &&
         icon_uri_is_symbolic (icon->filename, -1);
}

static GLoadableIcon *
icon_get_loadable (GtkIcon *icon)
{
  GFile *file;
  GLoadableIcon *loadable;

  if (icon->loadable)
    return g_object_ref (icon->loadable);

  if (icon->is_resource)
    {
      char *uri = g_strconcat ("resource://", icon->filename, NULL);
      file = g_file_new_for_uri (uri);
      g_free (uri);
    }
  else
    file = g_file_new_for_path (icon->filename);

  loadable = G_LOADABLE_ICON (g_file_icon_new (file));

  g_object_unref (file);

  return loadable;
}

/* This function contains the complicated logic for deciding
 * on the size at which to load the icon and loading it at
 * that size.
 */
static gboolean
icon_ensure_scale_and_texture__locked (GtkIcon *icon)
{
  gint image_width, image_height, image_size;
  gint scaled_desired_size;
  GdkPixbuf *source_pixbuf;
  gdouble dir_scale;
  gint64 before;

  icon_cache_mark_used_if_cached (icon);

  if (icon->texture)
    return TRUE;

  if (icon->load_error)
    return FALSE;

  before = g_get_monotonic_time ();

  scaled_desired_size = icon->desired_size * icon->desired_scale;

  dir_scale = icon->dir_scale;

  /* In many cases, the scale can be determined without actual access
   * to the icon file. This is generally true when we have a size
   * for the directory where the icon is; the image size doesn't
   * matter in that case.
   */
  if (icon->forced_size ||
      icon->dir_type == ICON_THEME_DIR_UNTHEMED)
    icon->scale = -1;
  else if (icon->dir_type == ICON_THEME_DIR_FIXED ||
           icon->dir_type == ICON_THEME_DIR_THRESHOLD)
    icon->scale = icon->unscaled_scale;
  else if (icon->dir_type == ICON_THEME_DIR_SCALABLE)
    {
      /* For svg icons, treat scalable directories as if they had
       * a Scale=<desired_scale> entry. In particular, this means
       * spinners that are restriced to size 32 will loaded at size
       * up to 64 with Scale=2.
       */
      if (icon->is_svg)
        dir_scale = icon->desired_scale;

      if (scaled_desired_size < icon->min_size * dir_scale)
        icon->scale = (gdouble) icon->min_size / (gdouble) icon->dir_size;
      else if (scaled_desired_size > icon->max_size * dir_scale)
        icon->scale = (gdouble) icon->max_size / (gdouble) icon->dir_size;
      else
        icon->scale = (gdouble) scaled_desired_size / (icon->dir_size * dir_scale);
    }

  /* At this point, we need to actually get the icon; either from the
   * builtin image or by loading the file
   */
  source_pixbuf = NULL;
  if (icon->cache_pixbuf)
    source_pixbuf = g_object_ref (icon->cache_pixbuf);
  else if (icon->is_resource)
    {
      if (icon->is_svg)
        {
          gint size;

          if (icon->forced_size || icon->dir_type == ICON_THEME_DIR_UNTHEMED)
            size = scaled_desired_size;
          else
            size = icon->dir_size * dir_scale * icon->scale;

          if (gtk_icon_is_symbolic (icon))
            source_pixbuf = gtk_make_symbolic_pixbuf_from_resource (icon->filename,
                                                                    size, size,
                                                                    icon->desired_scale,
                                                                    &icon->load_error);
          else if (size == 0)
            source_pixbuf = _gdk_pixbuf_new_from_resource_scaled (icon->filename,
                                                                  "svg",
                                                                  icon->desired_scale,
                                                                  &icon->load_error);
          else
            source_pixbuf = _gdk_pixbuf_new_from_resource_at_scale (icon->filename,
                                                                    "svg",
                                                                    size, size, TRUE,
                                                                    &icon->load_error);
        }
      else
        source_pixbuf = _gdk_pixbuf_new_from_resource (icon->filename,
                                                       "png",
                                                       &icon->load_error);
    }
  else
    {
      GLoadableIcon *loadable;
      GInputStream *stream;

      loadable = icon_get_loadable (icon);
      stream = g_loadable_icon_load (loadable,
                                     scaled_desired_size,
                                     NULL, NULL,
                                     &icon->load_error);
      g_object_unref (loadable);

      if (stream)
        {
          /* SVG icons are a special case - we just immediately scale them
           * to the desired size
           */
          if (icon->is_svg)
            {
              gint size;

              if (icon->forced_size || icon->dir_type == ICON_THEME_DIR_UNTHEMED)
                size = scaled_desired_size;
              else
                size = icon->dir_size * dir_scale * icon->scale;

              if (gtk_icon_is_symbolic (icon))
                source_pixbuf = gtk_make_symbolic_pixbuf_from_path (icon->filename,
                                                                    size, size,
                                                                    icon->desired_scale,
                                                                    &icon->load_error);
              else if (size == 0)
                source_pixbuf = _gdk_pixbuf_new_from_stream_scaled (stream,
                                                                    "svg",
                                                                    icon->desired_scale,
                                                                    NULL,
                                                                    &icon->load_error);
              else
                source_pixbuf = _gdk_pixbuf_new_from_stream_at_scale (stream,
                                                                      "svg",
                                                                      size, size,
                                                                      TRUE, NULL,
                                                                     &icon->load_error);
            }
          else
            source_pixbuf = _gdk_pixbuf_new_from_stream (stream,
                                                         "png",
                                                         NULL,
                                                         &icon->load_error);
          g_object_unref (stream);
        }
    }

  if (!source_pixbuf)
    {
      static gboolean warn_about_load_failure = TRUE;

      if (warn_about_load_failure)
        {
          const char *path;

          if (icon->filename)
            path = icon->filename;
          else if (G_IS_FILE (icon->loadable))
            path = g_file_peek_path (G_FILE (icon->loadable));
          else
            path = "icon theme";

          g_warning ("Could not load a pixbuf from %s.\n"
                     "This may indicate that pixbuf loaders or the mime database could not be found.",
                     path);

          warn_about_load_failure = FALSE;
        }

      return FALSE;
    }

  /* Do scale calculations that depend on the image size
   */
  image_width = gdk_pixbuf_get_width (source_pixbuf);
  image_height = gdk_pixbuf_get_height (source_pixbuf);
  image_size = MAX (image_width, image_height);

  if (icon->is_svg)
    icon->scale = image_size / 1000.;
  else if (icon->scale < 0.0)
    {
      if (image_size > 0 && scaled_desired_size > 0)
        icon->scale = (gdouble)scaled_desired_size / (gdouble)image_size;
      else
        icon->scale = 1.0;
    }

  if (icon->is_svg ||
      icon->scale == 1.0)
    {
      icon->texture = gdk_texture_new_for_pixbuf (source_pixbuf);
      g_object_unref (source_pixbuf);
    }
  else
    {
      GdkPixbuf *scaled = gdk_pixbuf_scale_simple (source_pixbuf,
                                                   MAX (1, 0.5 + image_width * icon->scale),
                                                   MAX (1, 0.5 + image_height * icon->scale),
                                                   GDK_INTERP_BILINEAR);
      icon->texture = gdk_texture_new_for_pixbuf (scaled);
      g_object_unref (scaled);
      g_object_unref (source_pixbuf);
    }

  g_assert (icon->texture != NULL);


  if (gdk_profiler_is_running ())
    {
      char *message = g_strdup_printf ("%s size %d@%d", icon->filename, icon->desired_size, icon->desired_scale);
      gdk_profiler_add_mark (before * 1000, (g_get_monotonic_time () - before) * 1000, "icon load", message);
      g_free (message);
    }

  return TRUE;
}

GdkTexture *
gtk_icon_download_texture (GtkIcon *self,
                           GError **error)
{
  GdkTexture *texture = NULL;

  g_mutex_lock (&self->texture_lock);

  icon_ensure_scale_and_texture__locked (self);

  if (self->texture)
    texture = g_object_ref (self->texture);
  else
    {
      if (self->load_error)
        {
          if (error)
            *error = g_error_copy (self->load_error);
        }
      else
        {
          g_set_error_literal (error,
                               GTK_ICON_THEME_ERROR,
                               GTK_ICON_THEME_NOT_FOUND,
                               _("Failed to load icon"));
        }
    }

  g_mutex_unlock (&self->texture_lock);

  return texture;
}

static void
init_color_matrix (graphene_matrix_t *color_matrix,
                   graphene_vec4_t *color_offset,
                   const GdkRGBA *foreground_color,
                   const GdkRGBA *success_color,
                   const GdkRGBA *warning_color,
                   const GdkRGBA *error_color)
{
  GdkRGBA fg_default = { 0.7450980392156863, 0.7450980392156863, 0.7450980392156863, 1.0};
  GdkRGBA success_default = { 0.3046921492332342,0.6015716792553597, 0.023437857633325704, 1.0};
  GdkRGBA warning_default = {0.9570458533607996, 0.47266346227206835, 0.2421911955443656, 1.0 };
  GdkRGBA error_default = { 0.796887159533074, 0 ,0, 1.0 };
  const GdkRGBA *fg = foreground_color ? foreground_color : &fg_default;
  const GdkRGBA *sc = success_color ? success_color : &success_default;
  const GdkRGBA *wc = warning_color ? warning_color : &warning_default;
  const GdkRGBA *ec = error_color ? error_color : &error_default;

  graphene_matrix_init_from_float (color_matrix,
                                   (float[16]) {
                                     sc->red - fg->red, sc->green - fg->green, sc->blue - fg->blue, 0,
                                       wc->red - fg->red, wc->green - fg->green, wc->blue - fg->blue, 0,
                                       ec->red - fg->red, ec->green - fg->green, ec->blue - fg->blue, 0,
                                       0, 0, 0, fg->alpha
                                       });
  graphene_vec4_init (color_offset, fg->red, fg->green, fg->blue, 0);
}


GdkTexture *
gtk_icon_download_colored_texture (GtkIcon *self,
                                   const GdkRGBA *foreground_color,
                                   const GdkRGBA *success_color,
                                   const GdkRGBA *warning_color,
                                   const GdkRGBA *error_color,
                                   GError **error)
{
  GdkTexture *texture, *colored_texture;
  graphene_matrix_t matrix;
  graphene_vec4_t offset;
  cairo_surface_t *surface;

  texture = gtk_icon_download_texture (self, error);

  if (texture == NULL || gtk_icon_is_symbolic (self))
    return texture;

  init_color_matrix (&matrix, &offset,
                     foreground_color, success_color,
                     warning_color, error_color);

  surface = gdk_texture_download_surface (texture);
  gdk_cairo_image_surface_recolor (surface, &matrix, &offset);
  colored_texture = gdk_texture_new_for_surface (surface);
  cairo_surface_destroy (surface);
  g_object_unref (texture);

  return colored_texture;
}

static void
icon_paintable_snapshot (GdkPaintable *paintable,
                         GdkSnapshot  *snapshot,
                         double        width,
                         double        height)
{
  GtkIcon *icon = GTK_ICON (paintable);
  GdkTexture *texture;

  texture = gtk_icon_download_texture (icon, NULL);
  if (texture)
    {
      if (icon->desired_scale != 1)
        {
          gtk_snapshot_save (snapshot);
          gtk_snapshot_scale (snapshot, 1.0 / icon->desired_scale, 1.0 / icon->desired_scale);
        }

      gtk_snapshot_append_texture (snapshot, texture,
                                   &GRAPHENE_RECT_INIT (0, 0, width * icon->desired_scale, height * icon->desired_scale));

      if (icon->desired_scale != 1)
        gtk_snapshot_restore (snapshot);

      g_object_unref (texture);
    }
}

void
gtk_icon_snapshot_with_colors (GtkIcon *icon,
                               GdkSnapshot  *snapshot,
                               double        width,
                               double        height,
                               const GdkRGBA *foreground_color,
                               const GdkRGBA *success_color,
                               const GdkRGBA *warning_color,
                               const GdkRGBA *error_color)
{
  GdkTexture *texture;

  texture = gtk_icon_download_texture (icon, NULL);
  if (texture)
    {
      gboolean symbolic = gtk_icon_is_symbolic (icon);

      if (icon->desired_scale != 1)
        {
          gtk_snapshot_save (snapshot);
          gtk_snapshot_scale (snapshot, 1.0 / icon->desired_scale, 1.0 / icon->desired_scale);
        }

      if (symbolic)
        {
          graphene_matrix_t matrix;
          graphene_vec4_t offset;

          init_color_matrix (&matrix, &offset,
                             foreground_color, success_color,
                             warning_color, error_color);

          gtk_snapshot_push_color_matrix (snapshot, &matrix, &offset);
        }

      gtk_snapshot_append_texture (snapshot, texture,
                                   &GRAPHENE_RECT_INIT (0, 0, width * icon->desired_scale, height * icon->desired_scale));

      if (symbolic)
        gtk_snapshot_pop (snapshot);

      if (icon->desired_scale != 1)
        gtk_snapshot_restore (snapshot);

      g_object_unref (texture);
    }
}


static GdkPaintableFlags
icon_paintable_get_flags (GdkPaintable *paintable)
{
  return GDK_PAINTABLE_STATIC_SIZE | GDK_PAINTABLE_STATIC_CONTENTS;
}

static int
icon_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  GtkIcon *icon = GTK_ICON (paintable);

  return icon->rendered_size;
}

static int
icon_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  GtkIcon *icon = GTK_ICON (paintable);

  return icon->rendered_size;
}

static void
icon_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = icon_paintable_snapshot;
  iface->get_flags = icon_paintable_get_flags;
  iface->get_intrinsic_width = icon_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = icon_paintable_get_intrinsic_height;
}

static GtkIcon *
gtk_icon_new_for_file (GFile *file,
                            gint   size,
                            gint   scale)
{
  GtkIcon *icon;

  icon = icon_new (ICON_THEME_DIR_UNTHEMED, size, 1);
  icon->loadable = G_LOADABLE_ICON (g_file_icon_new (file));
  icon->is_resource = g_file_has_uri_scheme (file, "resource");

  if (icon->is_resource)
    {
      gchar *uri;

      uri = g_file_get_uri (file);
      icon->filename = g_strdup (uri + 11); /* resource:// */
      g_free (uri);
    }
  else
    {
      icon->filename = g_file_get_path (file);
    }

  icon->is_svg = suffix_from_name (icon->filename) == ICON_SUFFIX_SVG;

  icon->desired_size = size;
  icon->desired_scale = scale;
  icon->forced_size = FALSE;

  icon->rendered_size = size;

  return icon;
}

static GtkIcon *
gtk_icon_new_for_pixbuf (GtkIconTheme *icon_theme,
                         GdkPixbuf    *pixbuf)
{
  GtkIcon *icon;
  gint width, height, max;

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);
  max = MAX (width, height);

  icon = icon_new (ICON_THEME_DIR_UNTHEMED, 0, 1);
  icon->texture = gdk_texture_new_for_pixbuf (pixbuf);
  icon->desired_size = max;
  icon->desired_scale = 1.0;
  icon->scale = 1.0;
  icon->rendered_size = max;

  return icon;
}

/**
 * gtk_icon_theme_lookup_by_gicon:
 * @self: a #GtkIconTheme
 * @icon: the #GIcon to look up
 * @size: desired icon size
 * @scale: the desired scale
 * @flags: flags modifying the behavior of the icon lookup
 *
 * Looks up an icon and returns a #GtkIcon containing information
 * such as the filename of the icon. The icon can then be rendered into
 * a pixbuf using gtk_icon_load_icon().
 *
 * Returns: (nullable) (transfer full): a #GtkIcon containing
 *     information about the icon, or %NULL if the icon wasn’t
 *     found. Unref with g_object_unref()
 */
GtkIcon *
gtk_icon_theme_lookup_by_gicon (GtkIconTheme       *self,
                                GIcon              *gicon,
                                gint                size,
                                gint                scale,
                                GtkIconLookupFlags  flags)
{
  GtkIcon *icon;

  g_return_val_if_fail (GTK_IS_ICON_THEME (self), NULL);
  g_return_val_if_fail (G_IS_ICON (gicon), NULL);
  g_warn_if_fail ((flags & GTK_ICON_LOOKUP_GENERIC_FALLBACK) == 0);

  if (GDK_IS_PIXBUF (gicon))
    {
      GdkPixbuf *pixbuf;

      pixbuf = GDK_PIXBUF (gicon);

      if ((flags & GTK_ICON_LOOKUP_FORCE_SIZE) != 0)
        {
          gint width, height, max;
          gdouble pixbuf_scale;

          width = gdk_pixbuf_get_width (pixbuf);
          height = gdk_pixbuf_get_height (pixbuf);
          max = MAX (width, height);
          pixbuf_scale = (gdouble) size * scale / (gdouble) max;

           if (pixbuf_scale != 1.0)
             {
              GdkPixbuf *scaled;
              scaled = gdk_pixbuf_scale_simple (pixbuf,
                                                0.5 + width * pixbuf_scale,
                                                0.5 + height * pixbuf_scale,
                                                GDK_INTERP_BILINEAR);

              icon = gtk_icon_new_for_pixbuf (self, scaled);
              g_object_unref (scaled);
             }
           else
             {
              icon = gtk_icon_new_for_pixbuf (self, pixbuf);
             }
        }
      else
        {
          icon = gtk_icon_new_for_pixbuf (self, pixbuf);
        }

      return icon;
    }
  else if (G_IS_FILE_ICON (gicon))
    {
      GFile *file = g_file_icon_get_file (G_FILE_ICON (gicon));

      icon = gtk_icon_new_for_file (file, size, scale);
      icon->forced_size = (flags & GTK_ICON_LOOKUP_FORCE_SIZE) != 0;

      return icon;
    }
  else if (G_IS_LOADABLE_ICON (gicon))
    {
      icon = icon_new (ICON_THEME_DIR_UNTHEMED, size, 1);
      icon->loadable = G_LOADABLE_ICON (g_object_ref (gicon));
      icon->is_svg = FALSE;
      icon->desired_size = size;
      icon->desired_scale = scale;
      icon->forced_size = (flags & GTK_ICON_LOOKUP_FORCE_SIZE) != 0;

      return icon;
    }
  else if (G_IS_THEMED_ICON (gicon))
    {
      const gchar **names;

      names = (const gchar **)g_themed_icon_get_names (G_THEMED_ICON (gicon));
      icon = gtk_icon_theme_choose_icon (self, names, size, scale, flags);

      return icon;
    }

  return NULL;
}
