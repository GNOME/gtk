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

static GSList *all_icon_factories = NULL;

struct _GtkIconSource
{
  /* Either filename or pixbuf can be NULL. If both are non-NULL,
   * the pixbuf is assumed to be the already-loaded contents of the
   * file.
   */
  gchar *filename;
  GdkPixbuf *pixbuf;

  GtkTextDirection direction;
  GtkStateType state;
  GtkIconSize size;

  /* If TRUE, then the parameter is wildcarded, and the above
   * fields should be ignored. If FALSE, the parameter is
   * specified, and the above fields should be valid.
   */
  guint any_direction : 1;
  guint any_state : 1;
  guint any_size : 1;
};

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
  all_icon_factories = g_slist_prepend (all_icon_factories, factory);
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

  all_icon_factories = g_slist_remove (all_icon_factories, factory);
  
  g_hash_table_foreach (factory->icons, free_icon_set, NULL);
  
  g_hash_table_destroy (factory->icons);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * gtk_icon_factory_new:
 *
 * Creates a new #GtkIconFactory. An icon factory manages a collection
 * of #GtkIconSet<!>s; a #GtkIconSet manages a set of variants of a
 * particular icon (i.e. a #GtkIconSet contains variants for different
 * sizes and widget states). Icons in an icon factory are named by a
 * stock ID, which is a simple string identifying the icon. Each
 * #GtkStyle has a list of #GtkIconFactory<!>s derived from the current
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

static void
ensure_default_icons (void)
{
  if (gtk_default_icons == NULL)
    {
      gtk_default_icons = gtk_icon_factory_new ();

      get_default_icons (gtk_default_icons);
    }
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

  ensure_default_icons ();
  
  return gtk_icon_factory_lookup (gtk_default_icons, stock_id);
}

static GtkIconSet *
sized_icon_set_from_inline (const guchar *inline_data,
                            GtkIconSize   size)
{
  GtkIconSet *set;

  GtkIconSource source = { NULL, NULL, 0, 0, 0,
                           TRUE, TRUE, FALSE };

  source.size = size;

  set = gtk_icon_set_new ();

  source.pixbuf = gdk_pixbuf_new_from_inline (-1, inline_data, FALSE, NULL);
  
  g_assert (source.pixbuf);

  gtk_icon_set_add_source (set, &source);

  g_object_unref (G_OBJECT (source.pixbuf));
  
  return set;
}


static GtkIconSet *
sized_with_fallback_icon_set_from_inline (const guchar *fallback_data,
                                          const guchar *inline_data,
                                          GtkIconSize   size)
{
  GtkIconSet *set;

  GtkIconSource source = { NULL, NULL, 0, 0, 0,
                           TRUE, TRUE, FALSE };

  source.size = size;

  set = gtk_icon_set_new ();

  source.pixbuf = gdk_pixbuf_new_from_inline (-1, inline_data, FALSE, NULL);

  g_assert (source.pixbuf);

  gtk_icon_set_add_source (set, &source);

  g_object_unref (G_OBJECT (source.pixbuf));
  
  source.any_size = TRUE;

  source.pixbuf = gdk_pixbuf_new_from_inline (-1, fallback_data, FALSE, NULL);

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

  source.pixbuf = gdk_pixbuf_new_from_inline (-1, inline_data, FALSE, NULL);

  g_assert (source.pixbuf);

  gtk_icon_set_add_source (set, &source);

  g_object_unref (G_OBJECT (source.pixbuf));
  
  return set;
}

static void
add_sized (GtkIconFactory *factory,
           const guchar   *inline_data,
           GtkIconSize     size,
           const gchar    *stock_id)
{
  GtkIconSet *set;
  
  set = sized_icon_set_from_inline (inline_data, size);
  
  gtk_icon_factory_add (factory, stock_id, set);

  gtk_icon_set_unref (set);
}

static void
add_sized_with_fallback (GtkIconFactory *factory,
                         const guchar   *fallback_data,
                         const guchar   *inline_data,
                         GtkIconSize     size,
                         const gchar    *stock_id)
{
  GtkIconSet *set;
  
  set = sized_with_fallback_icon_set_from_inline (fallback_data, inline_data, size);
  
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

  add_unsized (factory, stock_missing_image, GTK_STOCK_MISSING_IMAGE);
  
  add_sized (factory, dialog_error, GTK_ICON_SIZE_DIALOG, GTK_STOCK_DIALOG_ERROR);
  add_sized (factory, dialog_info, GTK_ICON_SIZE_DIALOG, GTK_STOCK_DIALOG_INFO);
  add_sized (factory, dialog_question, GTK_ICON_SIZE_DIALOG, GTK_STOCK_DIALOG_QUESTION);
  add_sized (factory, dialog_warning, GTK_ICON_SIZE_DIALOG, GTK_STOCK_DIALOG_WARNING);
  
  /* dnd size only */
  add_sized (factory, stock_new, GTK_ICON_SIZE_DND, GTK_STOCK_DND);
  add_sized (factory, stock_dnd_multiple, GTK_ICON_SIZE_DND, GTK_STOCK_DND_MULTIPLE);
  
  /* Only have button sizes */
  add_sized (factory, stock_button_apply, GTK_ICON_SIZE_BUTTON, GTK_STOCK_APPLY);
  add_sized (factory, stock_button_cancel, GTK_ICON_SIZE_BUTTON, GTK_STOCK_CANCEL);
  add_sized (factory, stock_button_no, GTK_ICON_SIZE_BUTTON, GTK_STOCK_NO);
  add_sized (factory, stock_button_ok, GTK_ICON_SIZE_BUTTON, GTK_STOCK_OK);
  add_sized (factory, stock_button_yes, GTK_ICON_SIZE_BUTTON, GTK_STOCK_YES);

  /* Generic + button sizes */
  add_sized_with_fallback (factory,
                           stock_close,
                           stock_button_close,
                           GTK_ICON_SIZE_BUTTON,
                           GTK_STOCK_CLOSE);

  /* Generic + menu sizes */  

  add_sized_with_fallback (factory,
                           stock_print_preview,
                           stock_menu_print_preview,
                           GTK_ICON_SIZE_MENU,
                           GTK_STOCK_PRINT_PREVIEW);

  add_sized_with_fallback (factory,
                           stock_sort_descending,
                           stock_menu_sort_descending,
                           GTK_ICON_SIZE_MENU,
                           GTK_STOCK_SORT_DESCENDING);
  

  add_sized_with_fallback (factory,
                           stock_sort_ascending,
                           stock_menu_sort_ascending,
                           GTK_ICON_SIZE_MENU,
                           GTK_STOCK_SORT_ASCENDING);
  
/* Generic size only */

  add_unsized (factory, stock_add, GTK_STOCK_ADD);
  add_unsized (factory, stock_align_center, GTK_STOCK_JUSTIFY_CENTER);
  add_unsized (factory, stock_align_justify, GTK_STOCK_JUSTIFY_FILL);
  add_unsized (factory, stock_align_left, GTK_STOCK_JUSTIFY_LEFT);
  add_unsized (factory, stock_align_right, GTK_STOCK_JUSTIFY_RIGHT);
  add_unsized (factory, stock_bottom, GTK_STOCK_GOTO_BOTTOM);  
  add_unsized (factory, stock_cdrom, GTK_STOCK_CDROM);
  add_unsized (factory, stock_clear, GTK_STOCK_CLEAR);
  add_unsized (factory, stock_colorselector, GTK_STOCK_SELECT_COLOR);
  add_unsized (factory, stock_convert, GTK_STOCK_CONVERT);
  add_unsized (factory, stock_copy, GTK_STOCK_COPY);
  add_unsized (factory, stock_cut, GTK_STOCK_CUT);
  add_unsized (factory, stock_down_arrow, GTK_STOCK_GO_DOWN);
  add_unsized (factory, stock_exec, GTK_STOCK_EXECUTE);
  add_unsized (factory, stock_exit, GTK_STOCK_QUIT);
  add_unsized (factory, stock_first, GTK_STOCK_GOTO_FIRST);
  add_unsized (factory, stock_font, GTK_STOCK_SELECT_FONT);
  add_unsized (factory, stock_help, GTK_STOCK_HELP);
  add_unsized (factory, stock_home, GTK_STOCK_HOME);
  add_unsized (factory, stock_index, GTK_STOCK_INDEX);
  add_unsized (factory, stock_jump_to, GTK_STOCK_JUMP_TO);
  add_unsized (factory, stock_last, GTK_STOCK_GOTO_LAST);
  add_unsized (factory, stock_left_arrow, GTK_STOCK_GO_BACK);
  add_unsized (factory, stock_new, GTK_STOCK_NEW);
  add_unsized (factory, stock_open, GTK_STOCK_OPEN);
  add_unsized (factory, stock_paste, GTK_STOCK_PASTE);
  add_unsized (factory, stock_preferences, GTK_STOCK_PREFERENCES);
  add_unsized (factory, stock_print, GTK_STOCK_PRINT);
  add_unsized (factory, stock_properties, GTK_STOCK_PROPERTIES);
  add_unsized (factory, stock_redo, GTK_STOCK_REDO);
  add_unsized (factory, stock_refresh, GTK_STOCK_REFRESH);
  add_unsized (factory, stock_remove, GTK_STOCK_REMOVE);
  add_unsized (factory, stock_revert, GTK_STOCK_REVERT_TO_SAVED);
  add_unsized (factory, stock_right_arrow, GTK_STOCK_GO_FORWARD);
  add_unsized (factory, stock_save, GTK_STOCK_FLOPPY);
  add_unsized (factory, stock_save, GTK_STOCK_SAVE);
  add_unsized (factory, stock_save_as, GTK_STOCK_SAVE_AS);
  add_unsized (factory, stock_search, GTK_STOCK_FIND);
  add_unsized (factory, stock_search_replace, GTK_STOCK_FIND_AND_REPLACE);
  add_unsized (factory, stock_spellcheck, GTK_STOCK_SPELL_CHECK);
  add_unsized (factory, stock_stop, GTK_STOCK_STOP);
  add_unsized (factory, stock_text_bold, GTK_STOCK_BOLD);
  add_unsized (factory, stock_text_italic, GTK_STOCK_ITALIC);
  add_unsized (factory, stock_text_strikeout, GTK_STOCK_STRIKETHROUGH);
  add_unsized (factory, stock_text_underline, GTK_STOCK_UNDERLINE);
  add_unsized (factory, stock_top, GTK_STOCK_GOTO_TOP);
  add_unsized (factory, stock_trash, GTK_STOCK_DELETE);
  add_unsized (factory, stock_undelete, GTK_STOCK_UNDELETE);
  add_unsized (factory, stock_undo, GTK_STOCK_UNDO);
  add_unsized (factory, stock_up_arrow, GTK_STOCK_GO_UP);
  add_unsized (factory, stock_zoom_1, GTK_STOCK_ZOOM_100);
  add_unsized (factory, stock_zoom_fit, GTK_STOCK_ZOOM_FIT);
  add_unsized (factory, stock_zoom_in, GTK_STOCK_ZOOM_IN);
  add_unsized (factory, stock_zoom_out, GTK_STOCK_ZOOM_OUT);
}

/* Sizes */

typedef struct _IconSize IconSize;

struct _IconSize
{
  gint size;
  gchar *name;
  
  gint width;
  gint height;
};

typedef struct _IconAlias IconAlias;

struct _IconAlias
{
  gchar *name;
  gint   target;
};

static GHashTable *icon_aliases = NULL;
static IconSize *icon_sizes = NULL;
static gint      icon_sizes_allocated = 0;
static gint      icon_sizes_used = 0;

static void
init_icon_sizes (void)
{
  if (icon_sizes == NULL)
    {
#define NUM_BUILTIN_SIZES 7
      gint i;

      icon_aliases = g_hash_table_new (g_str_hash, g_str_equal);
      
      icon_sizes = g_new (IconSize, NUM_BUILTIN_SIZES);
      icon_sizes_allocated = NUM_BUILTIN_SIZES;
      icon_sizes_used = NUM_BUILTIN_SIZES;

      icon_sizes[GTK_ICON_SIZE_INVALID].size = 0;
      icon_sizes[GTK_ICON_SIZE_INVALID].name = NULL;
      icon_sizes[GTK_ICON_SIZE_INVALID].width = 0;
      icon_sizes[GTK_ICON_SIZE_INVALID].height = 0;

      /* the name strings aren't copied since we don't ever remove
       * icon sizes, so we don't need to know whether they're static.
       * Even if we did I suppose removing the builtin sizes would be
       * disallowed.
       */
      
      icon_sizes[GTK_ICON_SIZE_MENU].size = GTK_ICON_SIZE_MENU;
      icon_sizes[GTK_ICON_SIZE_MENU].name = "gtk-menu";
      icon_sizes[GTK_ICON_SIZE_MENU].width = 16;
      icon_sizes[GTK_ICON_SIZE_MENU].height = 16;

      icon_sizes[GTK_ICON_SIZE_BUTTON].size = GTK_ICON_SIZE_BUTTON;
      icon_sizes[GTK_ICON_SIZE_BUTTON].name = "gtk-button";
      icon_sizes[GTK_ICON_SIZE_BUTTON].width = 24;
      icon_sizes[GTK_ICON_SIZE_BUTTON].height = 24;

      icon_sizes[GTK_ICON_SIZE_SMALL_TOOLBAR].size = GTK_ICON_SIZE_SMALL_TOOLBAR;
      icon_sizes[GTK_ICON_SIZE_SMALL_TOOLBAR].name = "gtk-small-toolbar";
      icon_sizes[GTK_ICON_SIZE_SMALL_TOOLBAR].width = 18;
      icon_sizes[GTK_ICON_SIZE_SMALL_TOOLBAR].height = 18;
      
      icon_sizes[GTK_ICON_SIZE_LARGE_TOOLBAR].size = GTK_ICON_SIZE_LARGE_TOOLBAR;
      icon_sizes[GTK_ICON_SIZE_LARGE_TOOLBAR].name = "gtk-large-toolbar";
      icon_sizes[GTK_ICON_SIZE_LARGE_TOOLBAR].width = 24;
      icon_sizes[GTK_ICON_SIZE_LARGE_TOOLBAR].height = 24;

      icon_sizes[GTK_ICON_SIZE_DND].size = GTK_ICON_SIZE_DND;
      icon_sizes[GTK_ICON_SIZE_DND].name = "gtk-dnd";
      icon_sizes[GTK_ICON_SIZE_DND].width = 32;
      icon_sizes[GTK_ICON_SIZE_DND].height = 32;

      icon_sizes[GTK_ICON_SIZE_DIALOG].size = GTK_ICON_SIZE_DIALOG;
      icon_sizes[GTK_ICON_SIZE_DIALOG].name = "gtk-dialog";
      icon_sizes[GTK_ICON_SIZE_DIALOG].width = 48;
      icon_sizes[GTK_ICON_SIZE_DIALOG].height = 48;

      g_assert ((GTK_ICON_SIZE_DIALOG + 1) == NUM_BUILTIN_SIZES);

      /* Alias everything to itself. */
      i = 1; /* skip invalid size */
      while (i < NUM_BUILTIN_SIZES)
        {
          gtk_icon_size_register_alias (icon_sizes[i].name, icon_sizes[i].size);
          
          ++i;
        }
      
#undef NUM_BUILTIN_SIZES
    }
}

/**
 * gtk_icon_size_lookup:
 * @size: an icon size
 * @width: location to store icon width
 * @height: location to store icon height
 *
 * Obtains the pixel size of a semantic icon size, normally @size would be
 * #GTK_ICON_SIZE_MENU, #GTK_ICON_SIZE_BUTTON, etc.  This function
 * isn't normally needed, gtk_widget_render_icon() is the usual
 * way to get an icon for rendering, then just look at the size of
 * the rendered pixbuf. The rendered pixbuf may not even correspond to
 * the width/height returned by gtk_icon_size_lookup(), because themes
 * are free to render the pixbuf however they like, including changing
 * the usual size.
 * 
 * Return value: %TRUE if @size was a valid size
 **/
gboolean
gtk_icon_size_lookup (GtkIconSize  size,
                      gint        *widthp,
                      gint        *heightp)
{
  init_icon_sizes ();

  if (size >= icon_sizes_used)
    return FALSE;

  if (size == GTK_ICON_SIZE_INVALID)
    return FALSE;
  
  if (widthp)
    *widthp = icon_sizes[size].width;

  if (heightp)
    *heightp = icon_sizes[size].height;

  return TRUE;
}

/**
 * gtk_icon_size_register:
 * @name: name of the icon size
 * @width: the icon width
 * @height: the icon height
 *
 * Registers a new icon size, along the same lines as #GTK_ICON_SIZE_MENU,
 * etc. Returns the integer value for the size.
 *
 * Returns: integer value representing the size
 * 
 **/
GtkIconSize
gtk_icon_size_register (const gchar *name,
                        gint         width,
                        gint         height)
{
  g_return_val_if_fail (name != NULL, 0);
  g_return_val_if_fail (width > 0, 0);
  g_return_val_if_fail (height > 0, 0);
  
  init_icon_sizes ();

  if (icon_sizes_used == icon_sizes_allocated)
    {
      icon_sizes_allocated *= 2;
      icon_sizes = g_renew (IconSize, icon_sizes, icon_sizes_allocated);
    }
  
  icon_sizes[icon_sizes_used].size = icon_sizes_used;
  icon_sizes[icon_sizes_used].name = g_strdup (name);
  icon_sizes[icon_sizes_used].width = width;
  icon_sizes[icon_sizes_used].height = height;

  ++icon_sizes_used;

  /* alias to self. */
  gtk_icon_size_register_alias (name, icon_sizes_used - 1);
  
  return icon_sizes_used - 1;
}

/**
 * gtk_icon_size_register_alias:
 * @alias: an alias for @target
 * @target: an existing icon size
 *
 * Registers @alias as another name for @target.
 * So calling gtk_icon_size_from_name() with @alias as argument
 * will return @target.
 *
 **/
void
gtk_icon_size_register_alias (const gchar *alias,
                              GtkIconSize  target)
{
  IconAlias *ia;
  
  g_return_if_fail (alias != NULL);

  init_icon_sizes ();

  if (g_hash_table_lookup (icon_aliases, alias))
    g_warning ("gtk_icon_size_register_alias: Icon size name '%s' already exists", alias);

  if (!gtk_icon_size_lookup (target, NULL, NULL))
    g_warning ("gtk_icon_size_register_alias: Icon size %d does not exist", target);
  
  ia = g_new (IconAlias, 1);
  ia->name = g_strdup (alias);
  ia->target = target;

  g_hash_table_insert (icon_aliases, ia->name, ia);
}

/** 
 * gtk_icon_size_from_name:
 * @name: the name to look up.
 * @returns: the icon size with the given name.
 * 
 * Looks up the icon size associated with @name.
 **/
GtkIconSize
gtk_icon_size_from_name (const gchar *name)
{
  IconAlias *ia;

  init_icon_sizes ();
  
  ia = g_hash_table_lookup (icon_aliases, name);

  if (ia)
    return ia->target;
  else
    return GTK_ICON_SIZE_INVALID;
}

/**
 * gtk_icon_size_get_name:
 * @size: a #GtkIconSize.
 * @returns: the name of the given icon size.
 * 
 * Gets the canonical name of the given icon size. The returned string 
 * is statically allocated and should not be freed.
 **/
G_CONST_RETURN gchar*
gtk_icon_size_get_name (GtkIconSize  size)
{
  if (size >= icon_sizes_used)
    return NULL;
  else
    return icon_sizes[size].name;
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
                                     GtkIconSize       size);
static void       add_to_cache      (GtkIconSet       *icon_set,
                                     GtkStyle         *style,
                                     GtkTextDirection  direction,
                                     GtkStateType      state,
                                     GtkIconSize       size,
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
 * Creates a new #GtkIconSet with @pixbuf as the default/fallback
 * source image. If you don't add any additional #GtkIconSource to the
 * icon set, all variants of the icon will be created from @pixbuf,
 * using scaling, pixelation, etc. as required to adjust the icon size
 * or make the icon look insensitive/prelighted.
 * 
 * Return value: a new #GtkIconSet
 **/
GtkIconSet *
gtk_icon_set_new_from_pixbuf (GdkPixbuf *pixbuf)
{
  GtkIconSet *set;

  GtkIconSource source = { NULL, NULL, 0, 0, 0,
                           TRUE, TRUE, TRUE };

  g_return_val_if_fail (pixbuf != NULL, NULL);

  set = gtk_icon_set_new ();

  source.pixbuf = pixbuf;

  gtk_icon_set_add_source (set, &source);
  
  return set;
}


/**
 * gtk_icon_set_ref:
 * @icon_set: a #GtkIconSet.
 * 
 * Increments the reference count on @icon_set.
 * 
 * Return value: @icon_set.
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
sizes_equivalent (GtkIconSize lhs,
                  GtkIconSize rhs)
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
                           GtkIconSize       size)
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
      GError *error = NULL;
      
      g_assert (source->filename);
      source->pixbuf = gdk_pixbuf_new_from_file (source->filename, &error);

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

static GdkPixbuf*
render_fallback_image (GtkStyle          *style,
                       GtkTextDirection   direction,
                       GtkStateType       state,
                       GtkIconSize        size,
                       GtkWidget         *widget,
                       const char        *detail)
{
  /* This icon can be used for any direction/state/size */
  static GtkIconSource fallback_source = { NULL, NULL, 0, 0, 0, TRUE, TRUE, TRUE };

  if (fallback_source.pixbuf == NULL)
    fallback_source.pixbuf = gdk_pixbuf_new_from_inline (-1, stock_missing_image, FALSE, NULL);
  
  return gtk_style_render_icon (style,
                                &fallback_source,
                                direction,
                                state,
                                size,
                                widget,
                                detail);
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
 * gtk_widget_render_icon() is better, since it automatically provides
 * most of the arguments from the current widget settings.  This
 * function never returns %NULL; if the icon can't be rendered
 * (perhaps because an image file fails to load), a default "missing
 * image" icon will be returned instead.
 * 
 * Return value: a #GdkPixbuf to be displayed
 **/
GdkPixbuf*
gtk_icon_set_render_icon (GtkIconSet        *icon_set,
                          GtkStyle          *style,
                          GtkTextDirection   direction,
                          GtkStateType       state,
                          GtkIconSize        size,
                          GtkWidget         *widget,
                          const char        *detail)
{
  GdkPixbuf *icon;
  GtkIconSource *source;
  
  g_return_val_if_fail (icon_set != NULL, NULL);
  g_return_val_if_fail (GTK_IS_STYLE (style), NULL);

  if (icon_set->sources == NULL)
    return render_fallback_image (style, direction, state, size, widget, detail);
  
  icon = find_in_cache (icon_set, style, direction,
                        state, size);

  if (icon)
    {
      g_object_ref (G_OBJECT (icon));
      return icon;
    }

  
  source = find_and_prep_icon_source (icon_set, direction, state, size);

  if (source == NULL)
    return render_fallback_image (style, direction, state, size, widget, detail);

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
 * a #GtkIconSource.
 * 
 * This function copies @source, so you can reuse the same source immediately
 * without affecting the icon set.
 *
 * An example of when you'd use this function: a web browser's "Back
 * to Previous Page" icon might point in a different direction in
 * Hebrew and in English; it might look different when insensitive;
 * and it might change size depending on toolbar mode (small/large
 * icons). So a single icon set would contain all those variants of
 * the icon, and you might add a separate source for each one.
 *
 * You should nearly always add a "default" icon source with all
 * fields wildcarded, which will be used as a fallback if no more
 * specific source matches. #GtkIconSet always prefers more specific
 * icon sources to more generic icon sources. The order in which you
 * add the sources to the icon set does not matter.
 *
 * gtk_icon_set_new_from_pixbuf() creates a new icon set with a
 * default icon source based on the given pixbuf.
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
 * gtk_icon_set_get_sizes:
 * @icon_set: a #GtkIconSet
 * @sizes: return location for array of sizes
 * @n_sizes: location to store number of elements in returned array
 *
 * Obtains a list of icon sizes this icon set can render. The returned
 * array must be freed with g_free().
 * 
 **/
void
gtk_icon_set_get_sizes (GtkIconSet   *icon_set,
                        GtkIconSize **sizes,
                        gint         *n_sizes)
{
  GSList *tmp_list;
  gboolean all_sizes = FALSE;
  GSList *specifics = NULL;
  
  g_return_if_fail (icon_set != NULL);
  g_return_if_fail (sizes != NULL);
  g_return_if_fail (n_sizes != NULL);
  
  tmp_list = icon_set->sources;
  while (tmp_list != NULL)
    {
      GtkIconSource *source;

      source = tmp_list->data;

      if (source->any_size)
        {
          all_sizes = TRUE;
          break;
        }
      else
        specifics = g_slist_prepend (specifics, GINT_TO_POINTER (source->size));
      
      tmp_list = g_slist_next (tmp_list);
    }

  if (all_sizes)
    {
      /* Need to find out what sizes exist */
      gint i;

      init_icon_sizes ();
      
      *sizes = g_new (GtkIconSize, icon_sizes_used);
      *n_sizes = icon_sizes_used - 1;
      
      i = 1;      
      while (i < icon_sizes_used)
        {
          (*sizes)[i - 1] = icon_sizes[i].size;
          ++i;
        }
    }
  else
    {
      gint i;
      
      *n_sizes = g_slist_length (specifics);
      *sizes = g_new (GtkIconSize, *n_sizes);

      i = 0;
      tmp_list = specifics;
      while (tmp_list != NULL)
        {
          (*sizes)[i] = GPOINTER_TO_INT (tmp_list->data);

          ++i;
          tmp_list = g_slist_next (tmp_list);
        }
    }

  g_slist_free (specifics);
}


/**
 * gtk_icon_source_new:
 * 
 * Creates a new #GtkIconSource. A #GtkIconSource contains a #GdkPixbuf (or
 * image filename) that serves as the base image for one or more of the
 * icons in a #GtkIconSet, along with a specification for which icons in the
 * icon set will be based on that pixbuf or image file. An icon set contains
 * a set of icons that represent "the same" logical concept in different states,
 * different global text directions, and different sizes.
 * 
 * So for example a web browser's "Back to Previous Page" icon might
 * point in a different direction in Hebrew and in English; it might
 * look different when insensitive; and it might change size depending
 * on toolbar mode (small/large icons). So a single icon set would
 * contain all those variants of the icon. #GtkIconSet contains a list
 * of #GtkIconSource from which it can derive specific icon variants in
 * the set. 
 *
 * In the simplest case, #GtkIconSet contains one source pixbuf from
 * which it derives all variants. The convenience function
 * gtk_icon_set_new_from_pixbuf() handles this case; if you only have
 * one source pixbuf, just use that function.
 *
 * If you want to use a different base pixbuf for different icon
 * variants, you create multiple icon sources, mark which variants
 * they'll be used to create, and add them to the icon set with
 * gtk_icon_set_add_source().
 *
 * By default, the icon source has all parameters wildcarded. That is,
 * the icon source will be used as the base icon for any desired text
 * direction, widget state, or icon size.
 * 
 * Return value: a new #GtkIconSource
 **/
GtkIconSource*
gtk_icon_source_new (void)
{
  GtkIconSource *src;
  
  src = g_new0 (GtkIconSource, 1);

  src->direction = GTK_TEXT_DIR_NONE;
  src->size = GTK_ICON_SIZE_INVALID;
  src->state = GTK_STATE_NORMAL;
  
  src->any_direction = TRUE;
  src->any_state = TRUE;
  src->any_size = TRUE;
  
  return src;
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
  copy->size = source->size;
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
  if (source->pixbuf)
    g_object_unref (G_OBJECT (source->pixbuf));

  g_free (source);
}

/**
 * gtk_icon_source_set_filename:
 * @source: a #GtkIconSource
 * @filename: image file to use
 *
 * Sets the name of an image file to use as a base image when creating
 * icon variants for #GtkIconSet. The filename must be absolute. 
 **/
void
gtk_icon_source_set_filename (GtkIconSource *source,
			      const gchar   *filename)
{
  g_return_if_fail (source != NULL);
  g_return_if_fail (filename == NULL || g_path_is_absolute (filename));

  if (source->filename == filename)
    return;
  
  if (source->filename)
    g_free (source->filename);

  source->filename = g_strdup (filename);  
}

/**
 * gtk_icon_source_set_pixbuf:
 * @source: a #GtkIconSource
 * @pixbuf: pixbuf to use as a source
 *
 * Sets a pixbuf to use as a base image when creating icon variants
 * for #GtkIconSet. If an icon source has both a filename and a pixbuf
 * set, the pixbuf will take priority.
 * 
 **/
void
gtk_icon_source_set_pixbuf (GtkIconSource *source,
                            GdkPixbuf     *pixbuf)
{
  g_return_if_fail (source != NULL);

  if (pixbuf)
    g_object_ref (G_OBJECT (pixbuf));

  if (source->pixbuf)
    g_object_unref (G_OBJECT (source->pixbuf));

  source->pixbuf = pixbuf;
}

/**
 * gtk_icon_source_get_filename:
 * @source: a #GtkIconSource
 * 
 * Retrieves the source filename, or %NULL if none is set.  The
 * filename is not a copy, and should not be modified or expected to
 * persist beyond the lifetime of the icon source.
 * 
 * Return value: image filename
 **/
G_CONST_RETURN gchar*
gtk_icon_source_get_filename (const GtkIconSource *source)
{
  g_return_val_if_fail (source != NULL, NULL);
  
  return source->filename;
}

/**
 * gtk_icon_source_get_pixbuf:
 * @source: a #GtkIconSource
 * 
 * Retrieves the source pixbuf, or %NULL if none is set.
 * The reference count on the pixbuf is not incremented.
 * 
 * Return value: source pixbuf
 **/
GdkPixbuf*
gtk_icon_source_get_pixbuf (const GtkIconSource *source)
{
  g_return_val_if_fail (source != NULL, NULL);
  
  return source->pixbuf;
}

/**
 * gtk_icon_source_set_direction_wildcarded:
 * @source: a #GtkIconSource
 * @setting: %TRUE to wildcard the text direction
 *
 * If the text direction is wildcarded, this source can be used
 * as the base image for an icon in any #GtkTextDirection.
 * If the text direction is not wildcarded, then the
 * text direction the icon source applies to should be set
 * with gtk_icon_source_set_direction(), and the icon source
 * will only be used with that text direction.
 *
 * #GtkIconSet prefers non-wildcarded sources (exact matches) over
 * wildcarded sources, and will use an exact match when possible.
 * 
 **/
void
gtk_icon_source_set_direction_wildcarded (GtkIconSource *source,
                                          gboolean       setting)
{
  g_return_if_fail (source != NULL);

  source->any_direction = setting != FALSE;
}

/**
 * gtk_icon_source_set_state_wildcarded:
 * @source: a #GtkIconSource
 * @setting: %TRUE to wildcard the widget state
 *
 * If the widget state is wildcarded, this source can be used as the
 * base image for an icon in any #GtkStateType.  If the widget state
 * is not wildcarded, then the state the source applies to should be
 * set with gtk_icon_source_set_state() and the icon source will
 * only be used with that specific state.
 *
 * #GtkIconSet prefers non-wildcarded sources (exact matches) over
 * wildcarded sources, and will use an exact match when possible.
 *
 * #GtkIconSet will normally transform wildcarded source images to
 * produce an appropriate icon for a given state, for example
 * lightening an image on prelight, but will not modify source images
 * that match exactly.
 **/
void
gtk_icon_source_set_state_wildcarded (GtkIconSource *source,
                                      gboolean       setting)
{
  g_return_if_fail (source != NULL);

  source->any_state = setting != FALSE;
}


/**
 * gtk_icon_source_set_size_wildcarded:
 * @source: a #GtkIconSource
 * @setting: %TRUE to wildcard the widget state
 *
 * If the icon size is wildcarded, this source can be used as the base
 * image for an icon of any size.  If the size is not wildcarded, then
 * the size the source applies to should be set with
 * gtk_icon_source_set_size() and the icon source will only be used
 * with that specific size.
 *
 * #GtkIconSet prefers non-wildcarded sources (exact matches) over
 * wildcarded sources, and will use an exact match when possible.
 *
 * #GtkIconSet will normally scale wildcarded source images to produce
 * an appropriate icon at a given size, but will not change the size
 * of source images that match exactly.
 **/
void
gtk_icon_source_set_size_wildcarded (GtkIconSource *source,
                                     gboolean       setting)
{
  g_return_if_fail (source != NULL);

  source->any_size = setting != FALSE;  
}

/**
 * gtk_icon_source_get_size_wildcarded:
 * @source: a #GtkIconSource
 * 
 * Gets the value set by gtk_icon_source_set_size_wildcarded().
 * 
 * Return value: %TRUE if this icon source is a base for any icon size variant
 **/
gboolean
gtk_icon_source_get_size_wildcarded (const GtkIconSource *source)
{
  g_return_val_if_fail (source != NULL, TRUE);
  
  return source->any_size;
}

/**
 * gtk_icon_source_get_state_wildcarded:
 * @source: a #GtkIconSource
 * 
 * Gets the value set by gtk_icon_source_set_state_wildcarded().
 * 
 * Return value: %TRUE if this icon source is a base for any widget state variant
 **/
gboolean
gtk_icon_source_get_state_wildcarded (const GtkIconSource *source)
{
  g_return_val_if_fail (source != NULL, TRUE);

  return source->any_state;
}

/**
 * gtk_icon_source_get_direction_wildcarded:
 * @source: a #GtkIconSource
 * 
 * Gets the value set by gtk_icon_source_set_direction_wildcarded().
 * 
 * Return value: %TRUE if this icon source is a base for any text direction variant
 **/
gboolean
gtk_icon_source_get_direction_wildcarded (const GtkIconSource *source)
{
  g_return_val_if_fail (source != NULL, TRUE);

  return source->any_direction;
}

/**
 * gtk_icon_source_set_direction:
 * @source: a #GtkIconSource
 * @direction: text direction this source applies to
 *
 * Sets the text direction this icon source is intended to be used
 * with.
 * 
 * Setting the text direction on an icon source makes no difference
 * if the text direction is wildcarded. Therefore, you should usually
 * call gtk_icon_source_set_direction_wildcarded() to un-wildcard it
 * in addition to calling this function.
 * 
 **/
void
gtk_icon_source_set_direction (GtkIconSource   *source,
                               GtkTextDirection direction)
{
  g_return_if_fail (source != NULL);

  source->direction = direction;
}

/**
 * gtk_icon_source_set_state:
 * @source: a #GtkIconSource
 * @state: widget state this source applies to
 *
 * Sets the widget state this icon source is intended to be used
 * with.
 * 
 * Setting the widget state on an icon source makes no difference
 * if the state is wildcarded. Therefore, you should usually
 * call gtk_icon_source_set_state_wildcarded() to un-wildcard it
 * in addition to calling this function.
 * 
 **/
void
gtk_icon_source_set_state (GtkIconSource *source,
                           GtkStateType   state)
{
  g_return_if_fail (source != NULL);

  source->state = state;
}

/**
 * gtk_icon_source_set_size:
 * @source: a #GtkIconSource
 * @size: icon size this source applies to
 *
 * Sets the icon size this icon source is intended to be used
 * with.
 * 
 * Setting the icon size on an icon source makes no difference
 * if the size is wildcarded. Therefore, you should usually
 * call gtk_icon_source_set_size_wildcarded() to un-wildcard it
 * in addition to calling this function.
 * 
 **/
void
gtk_icon_source_set_size (GtkIconSource *source,
                          GtkIconSize    size)
{
  g_return_if_fail (source != NULL);

  source->size = size;
}

/**
 * gtk_icon_source_get_direction:
 * @source: a #GtkIconSource
 * 
 * Obtains the text direction this icon source applies to. The return
 * value is only useful/meaningful if the text direction is <emphasis>not</emphasis> 
 * wildcarded.
 * 
 * Return value: text direction this source matches
 **/
GtkTextDirection
gtk_icon_source_get_direction (const GtkIconSource *source)
{
  g_return_val_if_fail (source != NULL, 0);

  return source->direction;
}

/**
 * gtk_icon_source_get_state:
 * @source: a #GtkIconSource
 * 
 * Obtains the widget state this icon source applies to. The return
 * value is only useful/meaningful if the widget state is <emphasis>not</emphasis>
 * wildcarded.
 * 
 * Return value: widget state this source matches
 **/
GtkStateType
gtk_icon_source_get_state (const GtkIconSource *source)
{
  g_return_val_if_fail (source != NULL, 0);

  return source->state;
}

/**
 * gtk_icon_source_get_size:
 * @source: a #GtkIconSource
 * 
 * Obtains the icon size this source applies to. The return value
 * is only useful/meaningful if the icon size is <emphasis>not</emphasis> wildcarded.
 * 
 * Return value: icon size this source matches.
 **/
GtkIconSize
gtk_icon_source_get_size (const GtkIconSource *source)
{
  g_return_val_if_fail (source != NULL, 0);

  return source->size;
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
  GtkIconSize size;

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
  g_object_unref (G_OBJECT (icon->pixbuf));

  g_free (icon);
}

static GdkPixbuf *
find_in_cache (GtkIconSet      *icon_set,
               GtkStyle        *style,
               GtkTextDirection direction,
               GtkStateType     state,
               GtkIconSize      size)
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
          icon->size == size)
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
              GtkIconSize      size,
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
  icon->size = size;
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

      icon_copy->size = icon->size;
      
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

static void
listify_foreach (gpointer key, gpointer value, gpointer data)
{
  GSList **list = data;

  *list = g_slist_prepend (*list, key);
}

static GSList *
g_hash_table_get_keys (GHashTable *table)
{
  GSList *list = NULL;

  g_hash_table_foreach (table, listify_foreach, &list);

  return list;
}

/**
 * _gtk_icon_factory_list_ids:
 * 
 * Gets all known IDs stored in an existing icon factory.
 * The strings in the returned list aren't copied.
 * The list itself should be freed.
 * 
 * Return value: List of ids in icon factories
 **/
GSList*
_gtk_icon_factory_list_ids (void)
{
  GSList *tmp_list;
  GSList *ids;

  ids = NULL;

  ensure_default_icons ();
  
  tmp_list = all_icon_factories;
  while (tmp_list != NULL)
    {
      GSList *these_ids;
      
      GtkIconFactory *factory = GTK_ICON_FACTORY (tmp_list->data);

      these_ids = g_hash_table_get_keys (factory->icons);
      
      ids = g_slist_concat (ids, these_ids);
      
      tmp_list = g_slist_next (tmp_list);
    }

  return ids;
}
