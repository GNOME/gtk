/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "gtkiconfactory.h"
#include "stock-icons/gtkstockpixbufs.h"
#include "gtkstock.h"
#include "gtkintl.h"
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>

static gpointer parent_class = NULL;

static void gtk_icon_factory_init       (GtkIconFactory      *icon_factory);
static void gtk_icon_factory_class_init (GtkIconFactoryClass *klass);
static void gtk_icon_factory_finalize   (GObject             *object);
static void get_default_icons           (GtkIconFactory      *icon_factory);

GType
gtk_icon_factory_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GtkIconFactoryClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gtk_icon_factory_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GtkIconFactory),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gtk_icon_factory_init,
      };
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "GtkIconFactory",
                                            &object_info, 0);
    }
  
  return object_type;
}

static void
gtk_icon_factory_init (GtkIconFactory *factory)
{
  factory->icons = g_hash_table_new (g_str_hash, g_str_equal);
}

static void
gtk_icon_factory_class_init (GtkIconFactoryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gtk_icon_factory_finalize;
}

static void
free_icon_set (gpointer key, gpointer value, gpointer data)
{
  g_free (key);
  gtk_icon_set_unref (value);
}

static void
gtk_icon_factory_finalize (GObject *object)
{
  GtkIconFactory *factory = GTK_ICON_FACTORY (object);

  g_hash_table_foreach (factory->icons, free_icon_set, NULL);
  
  g_hash_table_destroy (factory->icons);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * gtk_icon_factory_new:
 *
 * Creates a new #GtkIconFactory. An icon factory manages a collection
 * of #GtkIconSet; a #GtkIconSet manages a set of variants of a
 * particular icon (i.e. a #GtkIconSet contains variants for different
 * sizes and widget states). Icons in an icon factory are named by a
 * stock ID, which is a simple string identifying the icon. Each
 * #GtkStyle has a list of #GtkIconFactory derived from the current
 * theme; those icon factories are consulted first when searching for
 * an icon. If the theme doesn't set a particular icon, GTK+ looks for
 * the icon in a list of default icon factories, maintained by
 * gtk_icon_factory_add_default() and
 * gtk_icon_factory_remove_default(). Applications with icons should
 * add a default icon factory with their icons, which will allow
 * themes to override the icons for the application.
 * 
 * Return value: a new #GtkIconFactory
 **/
GtkIconFactory*
gtk_icon_factory_new (void)
{
  return GTK_ICON_FACTORY (g_object_new (GTK_TYPE_ICON_FACTORY, NULL));
}

/**
 * gtk_icon_factory_add:
 * @factory: a #GtkIconFactory
 * @stock_id: icon name
 * @icon_set: icon set
 *
 * Adds the given @icon_set to the icon factory, under the name
 * @stock_id.  @stock_id should be namespaced for your application,
 * e.g. "myapp-whatever-icon".  Normally applications create a
 * #GtkIconFactory, then add it to the list of default factories with
 * gtk_icon_factory_add_default(). Then they pass the @stock_id to
 * widgets such as #GtkImage to display the icon. Themes can provide
 * an icon with the same name (such as "myapp-whatever-icon") to
 * override your application's default icons. If an icon already
 * existed in @factory for @stock_id, it is unreferenced and replaced
 * with the new @icon_set.
 * 
 **/
void
gtk_icon_factory_add (GtkIconFactory *factory,
                      const gchar    *stock_id,
                      GtkIconSet     *icon_set)
{
  gpointer old_key = NULL;
  gpointer old_value = NULL;

  g_return_if_fail (GTK_IS_ICON_FACTORY (factory));
  g_return_if_fail (stock_id != NULL);
  g_return_if_fail (icon_set != NULL);  

  g_hash_table_lookup_extended (factory->icons, stock_id,
                                &old_key, &old_value);

  if (old_value == icon_set)
    return;
  
  gtk_icon_set_ref (icon_set);

  /* GHashTable key memory management is so fantastically broken. */
  if (old_key)
    g_hash_table_insert (factory->icons, old_key, icon_set);
  else
    g_hash_table_insert (factory->icons, g_strdup (stock_id), icon_set);

  if (old_value)
    gtk_icon_set_unref (old_value);
}

/**
 * gtk_icon_factory_lookup:
 * @factory: a #GtkIconFactory
 * @stock_id: an icon name
 * 
 * Looks up @stock_id in the icon factory, returning an icon set
 * if found, otherwise %NULL. For display to the user, you should
 * use gtk_style_lookup_icon_set() on the #GtkStyle for the
 * widget that will display the icon, instead of using this
 * function directly, so that themes are taken into account.
 * 
 * Return value: icon set of @stock_id.
 **/
GtkIconSet *
gtk_icon_factory_lookup (GtkIconFactory *factory,
                         const gchar    *stock_id)
{
  g_return_val_if_fail (GTK_IS_ICON_FACTORY (factory), NULL);
  g_return_val_if_fail (stock_id != NULL, NULL);
  
  return g_hash_table_lookup (factory->icons, stock_id);
}

static GtkIconFactory *gtk_default_icons = NULL;
static GSList *default_factories = NULL;

/**
 * gtk_icon_factory_add_default:
 * @factory: a #GtkIconFactory
 * 
 * Adds an icon factory to the list of icon factories searched by
 * gtk_style_lookup_icon_set(). This means that, for example,
 * gtk_image_new_from_stock() will be able to find icons in @factory.
 * There will normally be an icon factory added for each library or
 * application that comes with icons. The default icon factories
 * can be overridden by themes.
 * 
 **/
void
gtk_icon_factory_add_default (GtkIconFactory *factory)
{
  g_return_if_fail (GTK_IS_ICON_FACTORY (factory));

  g_object_ref (G_OBJECT (factory));
  
  default_factories = g_slist_prepend (default_factories, factory);
}

/**
 * gtk_icon_factory_remove_default:
 * @factory: a #GtkIconFactory previously added with gtk_icon_factory_add_default()
 *
 * Removes an icon factory from the list of default icon
 * factories. Not normally used; you might use it for a library that
 * can be unloaded or shut down.
 * 
 **/
void
gtk_icon_factory_remove_default (GtkIconFactory  *factory)
{
  g_return_if_fail (GTK_IS_ICON_FACTORY (factory));

  default_factories = g_slist_remove (default_factories, factory);

  g_object_unref (G_OBJECT (factory));
}

/**
 * gtk_icon_factory_lookup_default:
 * @stock_id: an icon name
 *
 * Looks for an icon in the list of default icon factories.  For
 * display to the user, you should use gtk_style_lookup_icon_set() on
 * the #GtkStyle for the widget that will display the icon, instead of
 * using this function directly, so that themes are taken into
 * account.
 * 
 * 
 * Return value: a #GtkIconSet, or %NULL
 **/
GtkIconSet *
gtk_icon_factory_lookup_default (const gchar *stock_id)
{
  GSList *tmp_list;

  g_return_val_if_fail (stock_id != NULL, NULL);
  
  tmp_list = default_factories;
  while (tmp_list != NULL)
    {
      GtkIconSet *icon_set =
        gtk_icon_factory_lookup (GTK_ICON_FACTORY (tmp_list->data),
                                 stock_id);

      if (icon_set)
        return icon_set;

      tmp_list = g_slist_next (tmp_list);
    }

  if (gtk_default_icons == NULL)
    {
      gtk_default_icons = gtk_icon_factory_new ();

      get_default_icons (gtk_default_icons);
    }
  
  return gtk_icon_factory_lookup (gtk_default_icons, stock_id);
}

static GtkIconSet *
sized_icon_set_from_inline (const guchar *inline_data,
                            const gchar  *size)
{
  GtkIconSet *set;

  GtkIconSource source = { NULL, NULL, 0, 0, NULL,
                           TRUE, TRUE, FALSE };

  source.size = (gchar*) size;

  set = gtk_icon_set_new ();

  source.pixbuf = gdk_pixbuf_new_from_inline (inline_data, FALSE, -1, NULL);

  g_assert (source.pixbuf);

  gtk_icon_set_add_source (set, &source);

  g_object_unref (G_OBJECT (source.pixbuf));
  
  return set;
}

static GtkIconSet *
unsized_icon_set_from_inline (const guchar *inline_data)
{
  GtkIconSet *set;

  /* This icon can be used for any direction/state/size */
  GtkIconSource source = { NULL, NULL, 0, 0, 0,
                           TRUE, TRUE, TRUE };

  set = gtk_icon_set_new ();

  source.pixbuf = gdk_pixbuf_new_from_inline (inline_data, FALSE, -1, NULL);

  g_assert (source.pixbuf);

  gtk_icon_set_add_source (set, &source);

  g_object_unref (G_OBJECT (source.pixbuf));
  
  return set;
}

static void
add_sized (GtkIconFactory *factory,
           const guchar   *inline_data,
           const gchar    *size,
           const gchar    *stock_id)
{
  GtkIconSet *set;
  
  set = sized_icon_set_from_inline (inline_data, size);
  
  gtk_icon_factory_add (factory, stock_id, set);

  gtk_icon_set_unref (set);
}

static void
add_unsized (GtkIconFactory *factory,
             const guchar   *inline_data,
             const gchar    *stock_id)
{
  GtkIconSet *set;
  
  set = unsized_icon_set_from_inline (inline_data);
  
  gtk_icon_factory_add (factory, stock_id, set);

  gtk_icon_set_unref (set);
}

static void
get_default_icons (GtkIconFactory *factory)
{
  /* KEEP IN SYNC with gtkstock.c */

  add_sized (factory, dialog_error, GTK_ICON_SIZE_DIALOG, GTK_STOCK_DIALOG_ERROR);
  add_sized (factory, dialog_info, GTK_ICON_SIZE_DIALOG, GTK_STOCK_DIALOG_INFO);
  add_sized (factory, dialog_question, GTK_ICON_SIZE_DIALOG, GTK_STOCK_DIALOG_QUESTION);
  add_sized (factory, dialog_warning, GTK_ICON_SIZE_DIALOG, GTK_STOCK_DIALOG_WARNING);

  add_sized (factory, stock_button_apply, GTK_ICON_SIZE_BUTTON, GTK_STOCK_BUTTON_APPLY);
  add_sized (factory, stock_button_ok, GTK_ICON_SIZE_BUTTON, GTK_STOCK_BUTTON_OK);
  add_sized (factory, stock_button_cancel, GTK_ICON_SIZE_BUTTON, GTK_STOCK_BUTTON_CANCEL);
  add_sized (factory, stock_button_close, GTK_ICON_SIZE_BUTTON, GTK_STOCK_BUTTON_CLOSE);
  add_sized (factory, stock_button_yes, GTK_ICON_SIZE_BUTTON, GTK_STOCK_BUTTON_YES);
  add_sized (factory, stock_button_no, GTK_ICON_SIZE_BUTTON, GTK_STOCK_BUTTON_NO);
    
  add_unsized (factory, stock_close, GTK_STOCK_CLOSE);
  add_unsized (factory, stock_exit, GTK_STOCK_QUIT);
  add_unsized (factory, stock_help, GTK_STOCK_HELP);
  add_unsized (factory, stock_new, GTK_STOCK_NEW);
  add_unsized (factory, stock_open, GTK_STOCK_OPEN);
  add_unsized (factory, stock_save, GTK_STOCK_SAVE);
}

/* Sizes */

static GHashTable *icon_sizes = NULL;

typedef struct _IconSize IconSize;

struct _IconSize
{
  gchar *name;
  
  gboolean is_alias;

  union
  {
    gchar *target;
    struct
    {
      gint width;
      gint height;
    } size;
  } d;
};

static IconSize*
icon_size_new (const gchar *name)
{
  IconSize *is;

  is = g_new0 (IconSize, 1);

  is->name = g_strdup (name);

  return is;
}

static void
icon_size_free (IconSize *is)
{
  g_free (is->name);
  
  if (is->is_alias)
    g_free (is->d.target);

  g_free (is);
}

static void
icon_size_insert (IconSize *is)
{
  gpointer old_key, old_value;

  /* Remove old ones */
  if (g_hash_table_lookup_extended (icon_sizes,
                                    is->name,
                                    &old_key, &old_value))
    {
      g_hash_table_remove (icon_sizes, is->name);
      icon_size_free (old_value);
    }
  
  g_hash_table_insert (icon_sizes,
                       is->name, is);

}

static IconSize*
icon_size_lookup (const gchar *name)
{
  IconSize *is;

  is = g_hash_table_lookup (icon_sizes,
                            name);

  while (is && is->is_alias)
    {
      is = g_hash_table_lookup (icon_sizes,
                                is->d.target);

    }

  return is;
}

static void
icon_size_add (const gchar *name,
               gint         width,
               gint         height)
{
  IconSize *is;
  
  is = icon_size_new (name);
  is->d.size.width = width;
  is->d.size.height = height;
  
  icon_size_insert (is);
}

static void
icon_alias_add (const gchar *name,
                const gchar *target)
{
  IconSize *is;
  
  is = icon_size_new (name);
  is->is_alias = TRUE;

  is->d.target = g_strdup (target);

  icon_size_insert (is);
}

static void
init_icon_sizes (void)
{
  if (icon_sizes == NULL)
    {
      icon_sizes = g_hash_table_new (g_str_hash, g_str_equal);

      icon_size_add (GTK_ICON_SIZE_MENU, 16, 16);
      icon_size_add (GTK_ICON_SIZE_BUTTON, 24, 24);
      icon_size_add (GTK_ICON_SIZE_SMALL_TOOLBAR, 18, 18);
      icon_size_add (GTK_ICON_SIZE_LARGE_TOOLBAR, 24, 24);
      icon_size_add (GTK_ICON_SIZE_DIALOG, 48, 48);
    }
}

/**
 * gtk_icon_size_lookup:
 * @alias: name of an icon size
 * @width: location to store icon width
 * @height: location to store icon height
 *
 * Obtains the pixel size of an icon, normally @alias would be
 * #GTK_ICON_SIZE_MENU, #GTK_ICON_SIZE_BUTTON, etc.  This function
 * isn't normally needed, gtk_widget_render_icon() is the usual
 * way to get an icon for rendering, then just look at the size of
 * the rendered pixbuf. The rendered pixbuf may not even correspond to
 * the width/height returned by gtk_icon_size_lookup(), because themes
 * are free to render the pixbuf however they like.
 * 
 * Return value: %TRUE if @alias was known.
 **/
gboolean
gtk_icon_size_lookup (const gchar *alias,
                      gint        *widthp,
                      gint        *heightp)
{
  IconSize *is;
  
  g_return_val_if_fail (alias != NULL, FALSE);
  
  init_icon_sizes ();
  
  is = icon_size_lookup (alias);

  if (is == NULL)
    return FALSE;

  if (widthp)
    *widthp = is->d.size.width;

  if (heightp)
    *heightp = is->d.size.height;

  return TRUE;
}

/**
 * gtk_icon_size_register:
 * @alias: name of the icon size
 * @width: the icon width
 * @height: the icon height
 *
 * Registers a new icon size, along the same lines as #GTK_ICON_SIZE_MENU,
 * etc.
 * 
 **/
void
gtk_icon_size_register (const gchar *alias,
                        gint         width,
                        gint         height)
{
  g_return_if_fail (alias != NULL);
  g_return_if_fail (width > 0);
  g_return_if_fail (height > 0);
  
  init_icon_sizes ();

  icon_size_add (alias, width, height);
}

/**
 * gtk_icon_size_register_alias:
 * @alias: an alias for @target
 * @target: an existing icon size name
 *
 * Registers @alias as another name for @target, usable when calling
 * gtk_icon_size_lookup().
 * 
 **/
void
gtk_icon_size_register_alias (const gchar *alias,
                              const gchar *target)
{
  g_return_if_fail (alias != NULL);
  g_return_if_fail (target != NULL);

  init_icon_sizes ();

  icon_alias_add (alias, target);
}

/* Icon Set */


/* Clear icon set contents, drop references to all contained
 * GdkPixbuf objects and forget all GtkIconSources. Used to
 * recycle an icon set.
 */
static GdkPixbuf *find_in_cache     (GtkIconSet       *icon_set,
                                     GtkStyle         *style,
                                     GtkTextDirection  direction,
                                     GtkStateType      state,
                                     const gchar      *size);
static void       add_to_cache      (GtkIconSet       *icon_set,
                                     GtkStyle         *style,
                                     GtkTextDirection  direction,
                                     GtkStateType      state,
                                     const gchar      *size,
                                     GdkPixbuf        *pixbuf);
static void       clear_cache       (GtkIconSet       *icon_set,
                                     gboolean          style_detach);
static GSList*    copy_cache        (GtkIconSet       *icon_set,
                                     GtkIconSet       *copy_recipient);
static void       attach_to_style   (GtkIconSet       *icon_set,
                                     GtkStyle         *style);
static void       detach_from_style (GtkIconSet       *icon_set,
                                     GtkStyle         *style);
static void       style_dnotify     (gpointer          data);

struct _GtkIconSet
{
  guint ref_count;

  GSList *sources;

  /* Cache of the last few rendered versions of the icon. */
  GSList *cache;

  guint cache_size;

  guint cache_serial;
};

static guint cache_serial = 0;

/**
 * gtk_icon_set_new:
 * 
 * Creates a new #GtkIconSet. A #GtkIconSet represents a single icon
 * in various sizes and widget states. It can provide a #GdkPixbuf
 * for a given size and state on request, and automatically caches
 * some of the rendered #GdkPixbuf objects.
 *
 * Normally you would use gtk_widget_render_icon() instead of
 * using #GtkIconSet directly. The one case where you'd use
 * #GtkIconSet is to create application-specific icon sets to place in
 * a #GtkIconFactory.
 * 
 * Return value: a new #GtkIconSet
 **/
GtkIconSet*
gtk_icon_set_new (void)
{
  GtkIconSet *icon_set;

  icon_set = g_new (GtkIconSet, 1);

  icon_set->ref_count = 1;
  icon_set->sources = NULL;
  icon_set->cache = NULL;
  icon_set->cache_size = 0;
  icon_set->cache_serial = cache_serial;
  
  return icon_set;
}

/**
 * gtk_icon_set_new_from_pixbuf:
 * @pixbuf: a #GdkPixbuf
 * 
 * Creates a new #GtkIconSet seeded with @pixbuf.
 * 
 * Return value: a new #GtkIconSet
 **/
GtkIconSet *
gtk_icon_set_new_from_pixbuf (GdkPixbuf *pixbuf)
{
  GtkIconSet *set;

  GtkIconSource source = { NULL, NULL, 0, 0, NULL,
                           TRUE, TRUE, TRUE };

  g_return_val_if_fail (pixbuf != NULL, NULL);

  set = gtk_icon_set_new ();

  source.pixbuf = pixbuf;

  gtk_icon_set_add_source (set, &source);
  
  return set;
}


/**
 * gtk_icon_set_ref:
 * @icon_set: a #GtkIconSet
 * 
 * Increments the reference count on @icon_set
 * 
 * Return value: @icon_set is returned
 **/
GtkIconSet*
gtk_icon_set_ref (GtkIconSet *icon_set)
{
  g_return_val_if_fail (icon_set != NULL, NULL);
  g_return_val_if_fail (icon_set->ref_count > 0, NULL);

  icon_set->ref_count += 1;

  return icon_set;
}

/**
 * gtk_icon_set_unref:
 * @icon_set: a #GtkIconSet
 * 
 * Decrements the reference count on @icon_set, and frees memory
 * if the reference count reaches 0.
 **/
void
gtk_icon_set_unref (GtkIconSet *icon_set)
{
  g_return_if_fail (icon_set != NULL);
  g_return_if_fail (icon_set->ref_count > 0);

  icon_set->ref_count -= 1;

  if (icon_set->ref_count == 0)
    {
      GSList *tmp_list = icon_set->sources;
      while (tmp_list != NULL)
        {
          gtk_icon_source_free (tmp_list->data);

          tmp_list = g_slist_next (tmp_list);
        }

      clear_cache (icon_set, TRUE);

      g_free (icon_set);
    }
}

/**
 * gtk_icon_set_copy:
 * @icon_set: a #GtkIconSet
 * 
 * Copies @icon_set by value. 
 * 
 * Return value: a new #GtkIconSet identical to the first.
 **/
GtkIconSet*
gtk_icon_set_copy (GtkIconSet *icon_set)
{
  GtkIconSet *copy;
  GSList *tmp_list;
  
  copy = gtk_icon_set_new ();

  tmp_list = icon_set->sources;
  while (tmp_list != NULL)
    {
      copy->sources = g_slist_prepend (copy->sources,
                                       gtk_icon_source_copy (tmp_list->data));

      tmp_list = g_slist_next (tmp_list);
    }

  copy->sources = g_slist_reverse (copy->sources);

  copy->cache = copy_cache (icon_set, copy);
  copy->cache_size = icon_set->cache_size;
  copy->cache_serial = icon_set->cache_serial;
  
  return copy;
}


static gboolean
sizes_equivalent (const gchar *rhs, const gchar *lhs)
{
  gint r_w, r_h, l_w, l_h;

  gtk_icon_size_lookup (rhs, &r_w, &r_h);
  gtk_icon_size_lookup (lhs, &l_w, &l_h);

  return r_w == l_w && r_h == l_h;
}

static GtkIconSource*
find_and_prep_icon_source (GtkIconSet       *icon_set,
                           GtkTextDirection  direction,
                           GtkStateType      state,
                           const gchar      *size)
{
  GtkIconSource *source;
  GSList *tmp_list;


  /* We need to find the best icon source.  Direction matters more
   * than state, state matters more than size. icon_set->sources
   * is sorted according to wildness, so if we take the first
   * match we find it will be the least-wild match (if there are
   * multiple matches for a given "wildness" then the RC file contained
   * dumb stuff, and we end up with an arbitrary matching source)
   */
  
  source = NULL;
  tmp_list = icon_set->sources;
  while (tmp_list != NULL)
    {
      GtkIconSource *s = tmp_list->data;
      
      if ((s->any_direction || (s->direction == direction)) &&
          (s->any_state || (s->state == state)) &&
          (s->any_size || (sizes_equivalent (size, s->size))))
        {
          source = s;
          break;
        }
      
      tmp_list = g_slist_next (tmp_list);
    }

  if (source == NULL)
    return NULL;
  
  if (source->pixbuf == NULL)
    {
      GError *error;
      gchar *full;
      
      g_assert (source->filename);

      if (*source->filename != G_DIR_SEPARATOR)
        full = gtk_rc_find_pixmap_in_path (NULL, source->filename);
      else
        full = g_strdup (source->filename);

      error = NULL;
      source->pixbuf = gdk_pixbuf_new_from_file (full, &error);

      g_free (full);
      
      if (source->pixbuf == NULL)
        {
          /* Remove this icon source so we don't keep trying to
           * load it.
           */
          g_warning (_("Error loading icon: %s"), error->message);
          g_error_free (error);
          
          icon_set->sources = g_slist_remove (icon_set->sources, source);

          gtk_icon_source_free (source);

          /* Try to fall back to other sources */
          if (icon_set->sources != NULL)
            return find_and_prep_icon_source (icon_set,
                                              direction,
                                              state,
                                              size);
          else
            return NULL;
        }
    }

  return source;
}

/**
 * gtk_icon_set_render_icon:
 * @icon_set: a #GtkIconSet
 * @style: a #GtkStyle associated with @widget, or %NULL
 * @direction: text direction
 * @state: widget state
 * @size: icon size
 * @widget: widget that will display the icon, or %NULL
 * @detail: detail to pass to the theme engine, or %NULL
 * 
 * Renders an icon using gtk_style_render_icon(). In most cases,
 * gtk_widget_render_icon() is better, since it automatically
 * provides most of the arguments from the current widget settings.
 * A %NULL return value is possible if an icon file fails to load
 * or the like.
 * 
 * Return value: a #GdkPixbuf to be displayed, or %NULL
 **/
GdkPixbuf*
gtk_icon_set_render_icon (GtkIconSet        *icon_set,
                          GtkStyle          *style,
                          GtkTextDirection   direction,
                          GtkStateType       state,
                          const gchar       *size,
                          GtkWidget         *widget,
                          const char        *detail)
{
  GdkPixbuf *icon;
  GtkIconSource *source;
  
  /* FIXME conceivably, everywhere this function
   * returns NULL, we should return some default
   * dummy icon like a question mark or the image icon
   * from netscape
   */
  
  g_return_val_if_fail (icon_set != NULL, NULL);
  g_return_val_if_fail (GTK_IS_STYLE (style), NULL);

  if (icon_set->sources == NULL)
    return NULL;
  
  icon = find_in_cache (icon_set, style, direction,
                        state, size);

  if (icon)
    {
      g_object_ref (G_OBJECT (icon));
      return icon;
    }

  
  source = find_and_prep_icon_source (icon_set, direction, state, size);

  if (source == NULL)
    return NULL;

  g_assert (source->pixbuf != NULL);
  
  icon = gtk_style_render_icon (style,
                                source,
                                direction,
                                state,
                                size,
                                widget,
                                detail);

  if (icon == NULL)
    {
      g_warning ("Theme engine failed to render icon");
      return NULL;
    }
  
  add_to_cache (icon_set, style, direction, state, size, icon);
  
  return icon;
}

/* Order sources by their "wildness", so that "wilder" sources are
 * greater than "specific" sources; for determining ordering,
 * direction beats state beats size.
 */

static int
icon_source_compare (gconstpointer ap, gconstpointer bp)
{
  const GtkIconSource *a = ap;
  const GtkIconSource *b = bp;

  if (!a->any_direction && b->any_direction)
    return -1;
  else if (a->any_direction && !b->any_direction)
    return 1;
  else if (!a->any_state && b->any_state)
    return -1;
  else if (a->any_state && !b->any_state)
    return 1;
  else if (!a->any_size && b->any_size)
    return -1;
  else if (a->any_size && !b->any_size)
    return 1;
  else
    return 0;
}

/**
 * gtk_icon_set_add_source:
 * @icon_set: a #GtkIconSet
 * @source: a #GtkIconSource
 *
 * Icon sets have a list of #GtkIconSource, which they use as base
 * icons for rendering icons in different states and sizes. Icons are
 * scaled, made to look insensitive, etc. in
 * gtk_icon_set_render_icon(), but #GtkIconSet needs base images to
 * work with. The base images and when to use them are described by
 * #GtkIconSource.
 * 
 **/
void
gtk_icon_set_add_source (GtkIconSet *icon_set,
                         const GtkIconSource *source)
{
  g_return_if_fail (icon_set != NULL);
  g_return_if_fail (source != NULL);

  if (source->pixbuf == NULL &&
      source->filename == NULL)
    {
      g_warning ("Useless GtkIconSource contains NULL filename and pixbuf");
      return;
    }
  
  icon_set->sources = g_slist_insert_sorted (icon_set->sources,
                                             gtk_icon_source_copy (source),
                                             icon_source_compare);
}

/**
 * gtk_icon_source_copy:
 * @source: a #GtkIconSource
 * 
 * Creates a copy of @source; mostly useful for language bindings.
 * 
 * Return value: a new #GtkIconSource
 **/
GtkIconSource*
gtk_icon_source_copy (const GtkIconSource *source)
{
  GtkIconSource *copy;
  
  g_return_val_if_fail (source != NULL, NULL);

  copy = g_new (GtkIconSource, 1);

  *copy = *source;
  
  copy->filename = g_strdup (source->filename);
  copy->size = g_strdup (source->size);
  if (copy->pixbuf)
    g_object_ref (G_OBJECT (copy->pixbuf));

  return copy;
}

/**
 * gtk_icon_source_free:
 * @source: a #GtkIconSource
 * 
 * Frees a dynamically-allocated icon source, along with its
 * filename, size, and pixbuf fields if those are not %NULL.
 **/
void
gtk_icon_source_free (GtkIconSource *source)
{
  g_return_if_fail (source != NULL);

  g_free ((char*) source->filename);
  g_free ((char*) source->size);
  if (source->pixbuf)
    g_object_unref (G_OBJECT (source->pixbuf));

  g_free (source);
}

/* Note that the logical maximum is 20 per GtkTextDirection, so we could
 * eventually set this to >20 to never throw anything out.
 */
#define NUM_CACHED_ICONS 8

typedef struct _CachedIcon CachedIcon;

struct _CachedIcon
{
  /* These must all match to use the cached pixbuf.
   * If any don't match, we must re-render the pixbuf.
   */
  GtkStyle *style;
  GtkTextDirection direction;
  GtkStateType state;
  gchar *size;

  GdkPixbuf *pixbuf;
};

static void
ensure_cache_up_to_date (GtkIconSet *icon_set)
{
  if (icon_set->cache_serial != cache_serial)
    clear_cache (icon_set, TRUE);
}

static void
cached_icon_free (CachedIcon *icon)
{
  g_free (icon->size);
  g_object_unref (G_OBJECT (icon->pixbuf));

  g_free (icon);
}

static GdkPixbuf *
find_in_cache (GtkIconSet      *icon_set,
               GtkStyle        *style,
               GtkTextDirection direction,
               GtkStateType     state,
               const gchar     *size)
{
  GSList *tmp_list;
  GSList *prev;

  ensure_cache_up_to_date (icon_set);
  
  prev = NULL;
  tmp_list = icon_set->cache;
  while (tmp_list != NULL)
    {
      CachedIcon *icon = tmp_list->data;

      if (icon->style == style &&
          icon->direction == direction &&
          icon->state == state &&
          strcmp (icon->size, size) == 0)
        {
          if (prev)
            {
              /* Move this icon to the front of the list. */
              prev->next = tmp_list->next;
              tmp_list->next = icon_set->cache;
              icon_set->cache = tmp_list;
            }
          
          return icon->pixbuf;
        }
          
      prev = tmp_list;
      tmp_list = g_slist_next (tmp_list);
    }

  return NULL;
}

static void
add_to_cache (GtkIconSet      *icon_set,
              GtkStyle        *style,
              GtkTextDirection direction,
              GtkStateType     state,
              const gchar     *size,
              GdkPixbuf       *pixbuf)
{
  CachedIcon *icon;

  ensure_cache_up_to_date (icon_set);
  
  g_object_ref (G_OBJECT (pixbuf));

  /* We have to ref the style, since if the style was finalized
   * its address could be reused by another style, creating a
   * really weird bug
   */
  
  if (style)
    g_object_ref (G_OBJECT (style));
  

  icon = g_new (CachedIcon, 1);
  icon_set->cache = g_slist_prepend (icon_set->cache, icon);

  icon->style = style;
  icon->direction = direction;
  icon->state = state;
  icon->size = g_strdup (size);
  icon->pixbuf = pixbuf;

  if (icon->style)
    attach_to_style (icon_set, icon->style);
  
  if (icon_set->cache_size >= NUM_CACHED_ICONS)
    {
      /* Remove oldest item in the cache */
      
      GSList *tmp_list;
      
      tmp_list = icon_set->cache;

      /* Find next-to-last link */
      g_assert (NUM_CACHED_ICONS > 2);
      while (tmp_list->next->next)
        tmp_list = tmp_list->next;

      g_assert (tmp_list != NULL);
      g_assert (tmp_list->next != NULL);
      g_assert (tmp_list->next->next == NULL);

      /* Free the last icon */
      icon = tmp_list->next->data;

      g_slist_free (tmp_list->next);
      tmp_list->next = NULL;

      cached_icon_free (icon);
    }
}

static void
clear_cache (GtkIconSet *icon_set,
             gboolean    style_detach)
{
  GSList *tmp_list;
  GtkStyle *last_style = NULL;

  tmp_list = icon_set->cache;
  while (tmp_list != NULL)
    {
      CachedIcon *icon = tmp_list->data;

      if (style_detach)
        {
          /* simple optimization for the case where the cache
           * contains contiguous icons from the same style.
           * it's safe to call detach_from_style more than
           * once on the same style though.
           */
          if (last_style != icon->style)
            {
              detach_from_style (icon_set, icon->style);
              last_style = icon->style;
            }
        }
      
      cached_icon_free (icon);      
      
      tmp_list = g_slist_next (tmp_list);
    }

  g_slist_free (icon_set->cache);
  icon_set->cache = NULL;
  icon_set->cache_size = 0;
}

static GSList*
copy_cache (GtkIconSet *icon_set,
            GtkIconSet *copy_recipient)
{
  GSList *tmp_list;
  GSList *copy = NULL;

  ensure_cache_up_to_date (icon_set);
  
  tmp_list = icon_set->cache;
  while (tmp_list != NULL)
    {
      CachedIcon *icon = tmp_list->data;
      CachedIcon *icon_copy = g_new (CachedIcon, 1);

      *icon_copy = *icon;

      if (icon_copy->style)
        attach_to_style (copy_recipient, icon_copy->style);
        
      g_object_ref (G_OBJECT (icon_copy->pixbuf));

      icon_copy->size = g_strdup (icon->size);
      
      copy = g_slist_prepend (copy, icon_copy);      
      
      tmp_list = g_slist_next (tmp_list);
    }

  return g_slist_reverse (copy);
}

static void
attach_to_style (GtkIconSet *icon_set,
                 GtkStyle   *style)
{
  GHashTable *table;

  table = g_object_get_qdata (G_OBJECT (style),
                              g_quark_try_string ("gtk-style-icon-sets"));

  if (table == NULL)
    {
      table = g_hash_table_new (NULL, NULL);
      g_object_set_qdata_full (G_OBJECT (style),
                               g_quark_from_static_string ("gtk-style-icon-sets"),
                               table,
                               style_dnotify);
    }

  g_hash_table_insert (table, icon_set, icon_set);
}

static void
detach_from_style (GtkIconSet *icon_set,
                   GtkStyle   *style)
{
  GHashTable *table;

  table = g_object_get_qdata (G_OBJECT (style),
                              g_quark_try_string ("gtk-style-icon-sets"));

  if (table != NULL)
    g_hash_table_remove (table, icon_set);
}

static void
iconsets_foreach (gpointer key,
                  gpointer value,
                  gpointer user_data)
{
  GtkIconSet *icon_set = key;

  /* We only need to remove cache entries for the given style;
   * but that complicates things because in destroy notify
   * we don't know which style got destroyed, and 95% of the
   * time all cache entries will have the same style,
   * so this is faster anyway.
   */
  
  clear_cache (icon_set, FALSE);
}

static void
style_dnotify (gpointer data)
{
  GHashTable *table = data;
  
  g_hash_table_foreach (table, iconsets_foreach, NULL);

  g_hash_table_destroy (table);
}

/* This allows the icon set to detect that its cache is out of date. */
void
_gtk_icon_set_invalidate_caches (void)
{
  ++cache_serial;
}
