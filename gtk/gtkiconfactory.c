/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "gtkiconfactory.h"
#include "stock-icons/gtkstockpixbufs.h"
#include "gtkstock.h"

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
                                            &object_info);
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

GtkIconFactory*
gtk_icon_factory_new (void)
{
  return GTK_ICON_FACTORY (g_type_create_instance (GTK_TYPE_ICON_FACTORY));
}

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

void
gtk_push_default_icon_factory (GtkIconFactory *factory)
{
  g_return_if_fail (GTK_IS_ICON_FACTORY (factory));

  g_object_ref (G_OBJECT (factory));
  
  default_factories = g_slist_prepend (default_factories, factory);
}

GtkIconSet *
gtk_default_icon_lookup (const gchar *stock_id)
{
  GSList *iter;

  g_return_val_if_fail (stock_id != NULL, NULL);
  
  iter = default_factories;
  while (iter != NULL)
    {
      GtkIconSet *icon_set =
        gtk_icon_factory_lookup (GTK_ICON_FACTORY (iter->data),
                                 stock_id);

      if (icon_set)
        return icon_set;

      iter = g_slist_next (iter);
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
                            GtkIconSizeType size)
{
  GtkIconSet *set;

  GtkIconSource source = { NULL, NULL, 0, 0, size,
                           TRUE, TRUE, FALSE };

  set = gtk_icon_set_new ();

  source.pixbuf = gdk_pixbuf_new_from_inline (inline_data, FALSE);

  g_assert (source.pixbuf);

  gtk_icon_set_add_source (set, &source);

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

  source.pixbuf = gdk_pixbuf_new_from_inline (inline_data, FALSE);

  g_assert (source.pixbuf);

  gtk_icon_set_add_source (set, &source);

  return set;
}

static void
add_sized (GtkIconFactory *factory,
           const guchar *inline_data,
           GtkIconSizeType size,
           const gchar *stock_id)
{
  GtkIconSet *set;
  
  set = sized_icon_set_from_inline (inline_data, size);
  
  gtk_icon_factory_add (factory, stock_id, set);

  gtk_icon_set_unref (set);
}

static void
add_unsized (GtkIconFactory *factory,
             const guchar *inline_data,
             const gchar *stock_id)
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

  add_sized (factory, dialog_error, GTK_ICON_DIALOG, GTK_STOCK_DIALOG_ERROR);
  add_sized (factory, dialog_info, GTK_ICON_DIALOG, GTK_STOCK_DIALOG_INFO);
  add_sized (factory, dialog_question, GTK_ICON_DIALOG, GTK_STOCK_DIALOG_QUESTION);
  add_sized (factory, dialog_warning, GTK_ICON_DIALOG, GTK_STOCK_DIALOG_WARNING);

  add_sized (factory, stock_button_apply, GTK_ICON_BUTTON, GTK_STOCK_BUTTON_APPLY);
  add_sized (factory, stock_button_ok, GTK_ICON_BUTTON, GTK_STOCK_BUTTON_OK);
  add_sized (factory, stock_button_cancel, GTK_ICON_BUTTON, GTK_STOCK_BUTTON_CANCEL);
  add_sized (factory, stock_button_close, GTK_ICON_BUTTON, GTK_STOCK_BUTTON_CLOSE);
  add_sized (factory, stock_button_yes, GTK_ICON_BUTTON, GTK_STOCK_BUTTON_YES);
  add_sized (factory, stock_button_no, GTK_ICON_BUTTON, GTK_STOCK_BUTTON_NO);
    
  add_unsized (factory, stock_close, GTK_STOCK_CLOSE);
  add_unsized (factory, stock_exit, GTK_STOCK_QUIT);
  add_unsized (factory, stock_help, GTK_STOCK_HELP);
  add_unsized (factory, stock_new, GTK_STOCK_NEW);
  add_unsized (factory, stock_open, GTK_STOCK_OPEN);
  add_unsized (factory, stock_save, GTK_STOCK_SAVE);
}

/* Sizes */


/* FIXME this shouldn't be hard-coded forever. Eventually
 * it will somehow be user-configurable.
 */

static gint widths[] =
{
  16, /* menu */
  24, /* button */
  18, /* small toolbar */
  24, /* large toolbar */
  48  /* dialog */
};

static gint heights[] =
{
  16, /* menu */
  24, /* button */
  18, /* small toolbar */
  24, /* large toolbar */
  48  /* dialog */
};

void
gtk_get_icon_size (GtkIconSizeType semantic_size,
                   gint *width,
                   gint *height)
{
  g_return_if_fail (semantic_size < G_N_ELEMENTS (widths));

  if (width)
    *width = widths[semantic_size];

  if (height)
    *height = heights[semantic_size];
}


/* Icon Set */

static GdkPixbuf *find_in_cache (GtkIconSet      *icon_set,
                                 GtkStyle        *style,
                                 GtkTextDirection   direction,
                                 GtkStateType     state,
                                 GtkIconSizeType  size);
static void       add_to_cache  (GtkIconSet      *icon_set,
                                 GtkStyle        *style,
                                 GtkTextDirection   direction,
                                 GtkStateType     state,
                                 GtkIconSizeType  size,
                                 GdkPixbuf       *pixbuf);
static void       clear_cache   (GtkIconSet      *icon_set);
static GSList*    copy_cache    (GtkIconSet      *icon_set);

static GSList* direction_state_size_matches (GSList           *sources,
                                             GtkTextDirection  direction,
                                             GtkStateType      state,
                                             GtkIconSizeType   size);
static GSList* state_size_matches           (GSList           *sources,
                                             GtkStateType      state,
                                             GtkIconSizeType   size);
static GSList* size_matches                 (GSList           *sources,
                                             GtkIconSizeType   size);

struct _GtkIconSet
{
  guint ref_count;

  GSList *sources;

  /* Cache of the last few rendered versions of the icon. */
  GSList *cache;

  guint cache_size;
};

GtkIconSet*
gtk_icon_set_new (void)
{
  GtkIconSet *icon_set;

  icon_set = g_new (GtkIconSet, 1);

  icon_set->ref_count = 1;
  icon_set->sources = NULL;
  icon_set->cache = NULL;
  icon_set->cache_size = 0;
  
  return icon_set;
}

GtkIconSet*
gtk_icon_set_ref (GtkIconSet *icon_set)
{
  g_return_val_if_fail (icon_set != NULL, NULL);
  g_return_val_if_fail (icon_set->ref_count > 0, NULL);

  icon_set->ref_count += 1;

  return icon_set;
}

void
gtk_icon_set_unref (GtkIconSet *icon_set)
{
  g_return_if_fail (icon_set != NULL);
  g_return_if_fail (icon_set->ref_count > 0);

  icon_set->ref_count -= 1;

  if (icon_set->ref_count == 0)
    {
      GSList *iter = icon_set->sources;
      while (iter != NULL)
        {
          gtk_icon_source_free (iter->data);

          iter = g_slist_next (iter);
        }

      clear_cache (icon_set);

      g_free (icon_set);
    }
}

GtkIconSet*
gtk_icon_set_copy (GtkIconSet *icon_set)
{
  GtkIconSet *copy;
  GSList *iter;
  
  copy = gtk_icon_set_new ();

  iter = icon_set->sources;
  while (iter != NULL)
    {
      copy->sources = g_slist_prepend (copy->sources,
                                       gtk_icon_source_copy (iter->data));

      iter = g_slist_next (iter);
    }

  copy->sources = g_slist_reverse (copy->sources);

  copy->cache = copy_cache (icon_set);
  copy->cache_size = icon_set->cache_size;

  return copy;
}

GdkPixbuf*
gtk_icon_set_get_icon (GtkIconSet      *icon_set,
                       GtkStyle        *style,
                       GtkTextDirection   direction,
                       GtkStateType     state,
                       GtkIconSizeType  size,
                       GtkWidget       *widget,
                       const char      *detail)
{
  GdkPixbuf *icon;

  GSList *candidates = NULL;
  GtkIconSource *source;
  
  /* FIXME conceivably, everywhere this function
   * returns NULL, we should return some default
   * dummy icon like a question mark or the image icon
   * from netscape
   */
  
  g_return_val_if_fail (icon_set != NULL, NULL);

  if (icon_set->sources == NULL)
    return NULL;
  
  icon = find_in_cache (icon_set, style, direction,
                        state, size);

  if (icon)
    {
      g_object_ref (G_OBJECT (icon));
      add_to_cache (icon_set, style, direction, state, size, icon);
      return icon;
    }

  /* We need to find the best icon source.  Direction matters more
   * than state, state matters more than size.
   */
  candidates = direction_state_size_matches (icon_set->sources,
                                             direction,
                                             state,
                                             size);


  if (candidates == NULL)
    return NULL; /* No sources were found. */
  
  /* If multiple candidates were returned, it basically means the
   * RC file contained stupidness. We just pick one at random.
   */
  source = candidates->data;
  g_slist_free (candidates);

  if (source->pixbuf == NULL)
    {
      if (source->filename == NULL)
        {
          g_warning ("Useless GtkIconSource contains NULL filename and pixbuf");
          return NULL;
        }

      source->pixbuf = gdk_pixbuf_new_from_file (source->filename);

      /* This needs to fail silently and do something sane.
       * A good argument for returning a default icon.
       */
      if (source->pixbuf == NULL)
        return NULL;
    }

  g_assert (source->pixbuf != NULL);
  
  if (style)
    {
      icon = gtk_style_render_icon (style,
                                    source,
                                    direction,
                                    state,
                                    size,
                                    widget,
                                    detail);
    }
  else
    {
      /* Internal function used here and in the default theme. */
      icon = _gtk_default_render_icon (style,
                                       source,
                                       direction,
                                       state,
                                       size,
                                       widget,
                                       detail);
    }

  g_assert (icon != NULL);

  add_to_cache (icon_set, style, direction, state, size, icon);
  
  return icon;
}

void
gtk_icon_set_add_source (GtkIconSet *icon_set,
                         const GtkIconSource *source)
{
  g_return_if_fail (icon_set != NULL);
  g_return_if_fail (source != NULL);
  
  icon_set->sources = g_slist_prepend (icon_set->sources,
                                       gtk_icon_source_copy (source));
}

GtkIconSource *
gtk_icon_source_copy (const GtkIconSource *source)
{
  GtkIconSource *copy;
  
  g_return_val_if_fail (source != NULL, NULL);

  copy = g_new (GtkIconSource, 1);

  *copy = *source;
  
  copy->filename = g_strdup (source->filename);
  if (copy->pixbuf)
    g_object_ref (G_OBJECT (copy->pixbuf));

  return copy;
}

void
gtk_icon_source_free (GtkIconSource *source)
{
  g_return_if_fail (source != NULL);

  g_free ((char*) source->filename);
  if (source->pixbuf)
    g_object_unref (G_OBJECT (source->pixbuf));

  g_free (source);
}

void
gtk_icon_set_clear (GtkIconSet *icon_set)
{
  GSList *iter;

  g_return_if_fail (icon_set != NULL);
  
  iter = icon_set->sources;
  while (iter != NULL)
    {
      gtk_icon_source_free (iter->data);

      iter = g_slist_next (iter);
    }

  clear_cache (icon_set);
}

/* Note that the logical maximum is 20 per GtkTextDirection, so we could
 * eventually set this to >20 to never throw anything out.
 */
#define NUM_CACHED_ICONS 8

typedef struct _CachedIcon CachedIcon;

struct _CachedIcon
{
  /* These must all match to use the cached pixbuf.
   * If any don't match, we must re-render the pixbuf.  */
  GtkStyle *style;
  GtkTextDirection direction;
  GtkStateType state;
  GtkIconSizeType size;

  GdkPixbuf *pixbuf;
};

static GdkPixbuf *
find_in_cache (GtkIconSet      *icon_set,
               GtkStyle        *style,
               GtkTextDirection   direction,
               GtkStateType     state,
               GtkIconSizeType  size)
{
  GSList *iter;

  iter = icon_set->cache;
  while (iter != NULL)
    {
      CachedIcon *icon = iter->data;

      if (icon->style == style &&
          icon->direction == direction &&
          icon->state == state &&
          icon->size == size)
        return icon->pixbuf;

      iter = g_slist_next (iter);
    }

  return NULL;
}

static void
add_to_cache (GtkIconSet      *icon_set,
              GtkStyle        *style,
              GtkTextDirection   direction,
              GtkStateType     state,
              GtkIconSizeType  size,
              GdkPixbuf       *pixbuf)
{
  CachedIcon *icon;

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
  icon->size = size;
  icon->pixbuf = pixbuf;

  if (icon_set->cache_size >= NUM_CACHED_ICONS)
    {
      /* Remove oldest item in the cache */
      
      GSList *iter;
      
      iter = icon_set->cache;

      /* Find next-to-last link */
      g_assert (NUM_CACHED_ICONS > 2);
      while (iter->next->next)
        iter = iter->next;

      g_assert (iter != NULL);
      g_assert (iter->next != NULL);
      g_assert (iter->next->next == NULL);

      icon = iter->next->data;

      g_slist_free (iter->next);
      iter->next = NULL;
      
      g_object_unref (G_OBJECT (icon->pixbuf));
      if (icon->style)
        g_object_unref (G_OBJECT (icon->style));
    }
}

static void
clear_cache (GtkIconSet *icon_set)
{
  GSList *iter;

  iter = icon_set->cache;
  while (iter != NULL)
    {
      CachedIcon *icon = iter->data;

      g_object_unref (G_OBJECT (icon->pixbuf));
      if (icon->style)
        g_object_unref (G_OBJECT (icon->style));

      g_free (icon);
      
      iter = g_slist_next (iter);
    }

  g_slist_free (icon_set->cache);
  icon_set->cache = NULL;
  icon_set->cache_size = 0;
}

static GSList*
copy_cache (GtkIconSet *icon_set)
{
  GSList *iter;
  GSList *copy = NULL;
  
  iter = icon_set->cache;
  while (iter != NULL)
    {
      CachedIcon *icon = iter->data;
      CachedIcon *icon_copy = g_new (CachedIcon, 1);

      *icon_copy = *icon;

      if (icon_copy->style)
        g_object_ref (G_OBJECT (icon_copy->style));
      g_object_ref (G_OBJECT (icon_copy->pixbuf));
          
      copy = g_slist_prepend (copy, icon_copy);      
      
      iter = g_slist_next (iter);
    }

  return g_slist_reverse (copy);
}

static GSList*
direction_state_size_matches (GSList *sources,
                              GtkTextDirection direction,
                              GtkStateType state,
                              GtkIconSizeType size)
{
  GSList *direction_matches = NULL;
  GSList *direction_wild = NULL;
  GSList *candidates;
  GSList *iter;
  
  iter = sources;
  while (iter != NULL)
    {
      GtkIconSource *source = iter->data;

      if (!source->any_direction &&
          source->direction == direction)
        direction_matches = g_slist_prepend (direction_matches, source);
      else if (source->any_direction)
        direction_wild = g_slist_prepend (direction_wild, source);
      
      iter = g_slist_next (iter);
    }

  /* First look for a matching source among exact direction matches,
   * then look for a matching source among wildcard direction sources
   */
  candidates = state_size_matches (direction_matches, state, size);
  if (candidates == NULL)
    candidates = state_size_matches (direction_wild, state, size);

  g_slist_free (direction_wild);
  g_slist_free (direction_matches);

  return candidates;
}


static GSList*
state_size_matches (GSList *sources,
                    GtkStateType state,
                    GtkIconSizeType size)
{
  GSList *state_matches = NULL;
  GSList *state_wild = NULL;
  GSList *candidates;
  GSList *iter;
  
  iter = sources;
  while (iter != NULL)
    {
      GtkIconSource *source = iter->data;

      if (!source->any_state &&
          source->state == state)
        state_matches = g_slist_prepend (state_matches, source);
      else if (source->any_state)
        state_wild = g_slist_prepend (state_wild, source);
      
      iter = g_slist_next (iter);
    }

  /* First look for a matching source among exact state matches,
   * then look for a matching source among wildcard state sources
   */
  candidates = size_matches (state_matches, size);
  if (candidates == NULL)
    candidates = size_matches (state_wild, size);

  g_slist_free (state_wild);
  g_slist_free (state_matches);

  return candidates;
}

static GSList*
size_matches (GSList *sources,
              GtkIconSizeType size)
{
  GSList *size_matches = NULL;
  GSList *size_wild = NULL;
  GSList *iter;
  
  iter = sources;
  while (iter != NULL)
    {
      GtkIconSource *source = iter->data;

      if (!source->any_size &&
          source->size == size)
        size_matches = g_slist_prepend (size_matches, source);
      else if (source->any_size)
        size_wild = g_slist_prepend (size_wild, source);
      
      iter = g_slist_next (iter);
    }

  /* Prefer exact size matches, return wildcard sizes otherwise. */
  if (size_matches)
    {
      g_slist_free (size_wild);
      return size_matches;
    }
  else
    {
      return size_wild;
    }
}

