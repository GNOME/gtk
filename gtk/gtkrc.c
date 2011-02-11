/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

#include "config.h"

#include <locale.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/stat.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


#include <glib.h>
#include <glib/gstdio.h>
#include "gdk/gdk.h"

#include "gtkversion.h"
#include "gtkrc.h"
#include "gtkbindings.h"
#include "gtkintl.h"
#include "gtkiconfactory.h"
#include "gtkmain.h"
#include "gtkmodules.h"
#include "gtkprivate.h"
#include "gtksettingsprivate.h"
#include "gtkwindow.h"

#ifdef G_OS_WIN32
#include <io.h>
#endif

enum 
{
  PATH_ELT_PSPEC,
  PATH_ELT_UNRESOLVED,
  PATH_ELT_TYPE
};

typedef struct
{
  gint type;
  union 
  {
    GType         class_type;
    gchar        *class_name;
    GPatternSpec *pspec;
  } elt;
} PathElt;

#define GTK_RC_STYLE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_RC_STYLE, GtkRcStylePrivate))

typedef struct _GtkRcStylePrivate GtkRcStylePrivate;

struct _GtkRcStylePrivate
{
  GSList *color_hashes;
};

static void        gtk_rc_style_finalize             (GObject         *object);
static void        gtk_rc_style_real_merge           (GtkRcStyle      *dest,
                                                      GtkRcStyle      *src);
static GtkRcStyle* gtk_rc_style_real_create_rc_style (GtkRcStyle      *rc_style);
static GtkStyle*   gtk_rc_style_real_create_style    (GtkRcStyle      *rc_style);
static gint	   gtk_rc_properties_cmp	     (gconstpointer    bsearch_node1,
						      gconstpointer    bsearch_node2);

static void	   insert_rc_property		     (GtkRcStyle      *style,
						      GtkRcProperty   *property,
						      gboolean         replace);


static const GScannerConfig gtk_rc_scanner_config =
{
  (
   " \t\r\n"
   )			/* cset_skip_characters */,
  (
   "_"
   G_CSET_a_2_z
   G_CSET_A_2_Z
   )			/* cset_identifier_first */,
  (
   G_CSET_DIGITS
   "-_"
   G_CSET_a_2_z
   G_CSET_A_2_Z
   )			/* cset_identifier_nth */,
  ( "#\n" )		/* cpair_comment_single */,
  
  TRUE			/* case_sensitive */,
  
  TRUE			/* skip_comment_multi */,
  TRUE			/* skip_comment_single */,
  TRUE			/* scan_comment_multi */,
  TRUE			/* scan_identifier */,
  FALSE			/* scan_identifier_1char */,
  FALSE			/* scan_identifier_NULL */,
  TRUE			/* scan_symbols */,
  TRUE			/* scan_binary */,
  TRUE			/* scan_octal */,
  TRUE			/* scan_float */,
  TRUE			/* scan_hex */,
  TRUE			/* scan_hex_dollar */,
  TRUE			/* scan_string_sq */,
  TRUE			/* scan_string_dq */,
  TRUE			/* numbers_2_int */,
  FALSE			/* int_2_float */,
  FALSE			/* identifier_2_string */,
  TRUE			/* char_2_token */,
  TRUE			/* symbol_2_token */,
  FALSE			/* scope_0_fallback */,
};
 
static const gchar symbol_names[] = 
  "include\0"
  "NORMAL\0"
  "ACTIVE\0"
  "PRELIGHT\0"
  "SELECTED\0"
  "INSENSITIVE\0"
  "fg\0"
  "bg\0"
  "text\0"
  "base\0"
  "xthickness\0"
  "ythickness\0"
  "font\0"
  "fontset\0"
  "font_name\0"
  "bg_pixmap\0"
  "pixmap_path\0"
  "style\0"
  "binding\0"
  "bind\0"
  "widget\0"
  "widget_class\0"
  "class\0"
  "lowest\0"
  "gtk\0"
  "application\0"
  "theme\0"
  "rc\0"
  "highest\0"
  "engine\0"
  "module_path\0"
  "stock\0"
  "im_module_file\0"
  "LTR\0"
  "RTL\0"
  "color\0"
  "unbind\0";

static const struct
{
  guint name_offset;
  guint token;
} symbols[] = {
  {   0, GTK_RC_TOKEN_INCLUDE },
  {   8, GTK_RC_TOKEN_NORMAL },
  {  15, GTK_RC_TOKEN_ACTIVE },
  {  22, GTK_RC_TOKEN_PRELIGHT },
  {  31, GTK_RC_TOKEN_SELECTED },
  {  40, GTK_RC_TOKEN_INSENSITIVE },
  {  52, GTK_RC_TOKEN_FG },
  {  55, GTK_RC_TOKEN_BG },
  {  58, GTK_RC_TOKEN_TEXT },
  {  63, GTK_RC_TOKEN_BASE },
  {  68, GTK_RC_TOKEN_XTHICKNESS },
  {  79, GTK_RC_TOKEN_YTHICKNESS },
  {  90, GTK_RC_TOKEN_FONT },
  {  95, GTK_RC_TOKEN_FONTSET },
  { 103, GTK_RC_TOKEN_FONT_NAME },
  { 113, GTK_RC_TOKEN_BG_PIXMAP },
  { 123, GTK_RC_TOKEN_PIXMAP_PATH },
  { 135, GTK_RC_TOKEN_STYLE },
  { 141, GTK_RC_TOKEN_BINDING },
  { 149, GTK_RC_TOKEN_BIND },
  { 154, GTK_RC_TOKEN_WIDGET },
  { 161, GTK_RC_TOKEN_WIDGET_CLASS },
  { 174, GTK_RC_TOKEN_CLASS },
  { 180, GTK_RC_TOKEN_LOWEST },
  { 187, GTK_RC_TOKEN_GTK },
  { 191, GTK_RC_TOKEN_APPLICATION },
  { 203, GTK_RC_TOKEN_THEME },
  { 209, GTK_RC_TOKEN_RC },
  { 212, GTK_RC_TOKEN_HIGHEST },
  { 220, GTK_RC_TOKEN_ENGINE },
  { 227, GTK_RC_TOKEN_MODULE_PATH },
  { 239, GTK_RC_TOKEN_STOCK },
  { 245, GTK_RC_TOKEN_IM_MODULE_FILE },
  { 260, GTK_RC_TOKEN_LTR },
  { 264, GTK_RC_TOKEN_RTL },
  { 268, GTK_RC_TOKEN_COLOR },
  { 274, GTK_RC_TOKEN_UNBIND }
};

static GHashTable *realized_style_ht = NULL;

static gchar *im_module_file = NULL;

static gchar **gtk_rc_default_files = NULL;

/* RC file handling */

static gchar *
gtk_rc_make_default_dir (const gchar *type)
{
  const gchar *var;
  gchar *path;

  var = g_getenv ("GTK_EXE_PREFIX");

  if (var)
    path = g_build_filename (var, "lib", "gtk-3.0", GTK_BINARY_VERSION, type, NULL);
  else
    path = g_build_filename (GTK_LIBDIR, "gtk-3.0", GTK_BINARY_VERSION, type, NULL);

  return path;
}

/**
 * gtk_rc_get_im_module_path:
 * @returns: (type filename): a newly-allocated string containing the
 *    path in which to look for IM modules.
 *
 * Obtains the path in which to look for IM modules. See the documentation
 * of the <link linkend="im-module-path"><envar>GTK_PATH</envar></link>
 * environment variable for more details about looking up modules. This
 * function is useful solely for utilities supplied with GTK+ and should
 * not be used by applications under normal circumstances.
 */
gchar *
gtk_rc_get_im_module_path (void)
{
  gchar **paths = _gtk_get_module_path ("immodules");
  gchar *result = g_strjoinv (G_SEARCHPATH_SEPARATOR_S, paths);
  g_strfreev (paths);

  return result;
}

/**
 * gtk_rc_get_im_module_file:
 * @returns: (type filename): a newly-allocated string containing the
 *    name of the file listing the IM modules available for loading
 *
 * Obtains the path to the IM modules file. See the documentation
 * of the <link linkend="im-module-file"><envar>GTK_IM_MODULE_FILE</envar></link>
 * environment variable for more details.
 */
gchar *
gtk_rc_get_im_module_file (void)
{
  const gchar *var = g_getenv ("GTK_IM_MODULE_FILE");
  gchar *result = NULL;

  if (var)
    result = g_strdup (var);

  if (!result)
    {
      if (im_module_file)
        result = g_strdup (im_module_file);
      else
        result = gtk_rc_make_default_dir ("immodules.cache");
    }

  return result;
}

gchar *
gtk_rc_get_theme_dir (void)
{
  const gchar *var;
  gchar *path;

  var = g_getenv ("GTK_DATA_PREFIX");

  if (var)
    path = g_build_filename (var, "share", "themes", NULL);
  else
    path = g_build_filename (GTK_DATA_PREFIX, "share", "themes", NULL);

  return path;
}

/**
 * gtk_rc_get_module_dir:
 * 
 * Returns a directory in which GTK+ looks for theme engines.
 * For full information about the search for theme engines,
 * see the docs for <envar>GTK_PATH</envar> in
 * <xref linkend="gtk-running"/>.
 * 
 * return value: (type filename): the directory. (Must be freed with g_free())
 **/
gchar *
gtk_rc_get_module_dir (void)
{
  return gtk_rc_make_default_dir ("engines");
}

/**
 * gtk_rc_add_default_file:
 * @filename: (type filename): the pathname to the file. If @filename
 *    is not absolute, it is searched in the current directory.
 *
 * Adds a file to the list of files to be parsed at the
 * end of gtk_init().
 *
 * Deprecated:3.0: Use #GtkStyleContext with a custom #GtkStyleProvider instead
 **/
void
gtk_rc_add_default_file (const gchar *filename)
{
}

/**
 * gtk_rc_set_default_files:
 * @filenames: (array zero-terminated=1) (element-type filename): A
 *     %NULL-terminated list of filenames.
 *
 * Sets the list of files that GTK+ will read at the
 * end of gtk_init().
 *
 * Deprecated:3.0: Use #GtkStyleContext with a custom #GtkStyleProvider instead
 **/
void
gtk_rc_set_default_files (gchar **filenames)
{
}

/**
 * gtk_rc_get_default_files:
 *
 * Retrieves the current list of RC files that will be parsed
 * at the end of gtk_init().
 *
 * Return value: (transfer none) (array zero-terminated=1) (element-type filename):
 *      A %NULL-terminated array of filenames.  This memory is owned
 *     by GTK+ and must not be freed by the application.  If you want
 *     to store this information, you should make a copy.
 *
 * Deprecated:3.0: Use #GtkStyleContext instead
 **/
gchar **
gtk_rc_get_default_files (void)
{
  return gtk_rc_default_files;
}

void
gtk_rc_parse_string (const gchar *rc_string)
{
  g_return_if_fail (rc_string != NULL);
}

void
gtk_rc_parse (const gchar *filename)
{
  g_return_if_fail (filename != NULL);
}

/* Handling of RC styles */

G_DEFINE_TYPE (GtkRcStyle, gtk_rc_style, G_TYPE_OBJECT)

static void
gtk_rc_style_init (GtkRcStyle *style)
{
  GtkRcStylePrivate *priv = GTK_RC_STYLE_GET_PRIVATE (style);
  guint i;

  style->name = NULL;
  style->font_desc = NULL;

  for (i = 0; i < 5; i++)
    {
      static const GdkColor init_color = { 0, 0, 0, 0, };

      style->bg_pixmap_name[i] = NULL;
      style->color_flags[i] = 0;
      style->fg[i] = init_color;
      style->bg[i] = init_color;
      style->text[i] = init_color;
      style->base[i] = init_color;
    }
  style->xthickness = -1;
  style->ythickness = -1;
  style->rc_properties = NULL;

  style->rc_style_lists = NULL;
  style->icon_factories = NULL;

  priv->color_hashes = NULL;
}

static void
gtk_rc_style_class_init (GtkRcStyleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  
  object_class->finalize = gtk_rc_style_finalize;

  klass->parse = NULL;
  klass->create_rc_style = gtk_rc_style_real_create_rc_style;
  klass->merge = gtk_rc_style_real_merge;
  klass->create_style = gtk_rc_style_real_create_style;

  g_type_class_add_private (object_class, sizeof (GtkRcStylePrivate));
}

static void
gtk_rc_style_finalize (GObject *object)
{
  GSList *tmp_list1, *tmp_list2;
  GtkRcStyle *rc_style;
  GtkRcStylePrivate *rc_priv;
  gint i;

  rc_style = GTK_RC_STYLE (object);
  rc_priv = GTK_RC_STYLE_GET_PRIVATE (rc_style);

  g_free (rc_style->name);
  if (rc_style->font_desc)
    pango_font_description_free (rc_style->font_desc);
      
  for (i = 0; i < 5; i++)
    g_free (rc_style->bg_pixmap_name[i]);
  
  /* Now remove all references to this rc_style from
   * realized_style_ht
   */
  tmp_list1 = rc_style->rc_style_lists;
  while (tmp_list1)
    {
      GSList *rc_styles = tmp_list1->data;
      GtkStyle *style = g_hash_table_lookup (realized_style_ht, rc_styles);
      g_object_unref (style);

      /* Remove the list of styles from the other rc_styles
       * in the list
       */
      tmp_list2 = rc_styles;
      while (tmp_list2)
        {
          GtkRcStyle *other_style = tmp_list2->data;

          if (other_style != rc_style)
            other_style->rc_style_lists = g_slist_remove_all (other_style->rc_style_lists,
							      rc_styles);
          tmp_list2 = tmp_list2->next;
        }

      /* And from the hash table itself
       */
      g_hash_table_remove (realized_style_ht, rc_styles);
      g_slist_free (rc_styles);

      tmp_list1 = tmp_list1->next;
    }
  g_slist_free (rc_style->rc_style_lists);

  if (rc_style->rc_properties)
    {
      guint i;

      for (i = 0; i < rc_style->rc_properties->len; i++)
	{
	  GtkRcProperty *node = &g_array_index (rc_style->rc_properties, GtkRcProperty, i);

	  g_free (node->origin);
	  g_value_unset (&node->value);
	}
      g_array_free (rc_style->rc_properties, TRUE);
      rc_style->rc_properties = NULL;
    }

  g_slist_foreach (rc_style->icon_factories, (GFunc) g_object_unref, NULL);
  g_slist_free (rc_style->icon_factories);

  g_slist_foreach (rc_priv->color_hashes, (GFunc) g_hash_table_unref, NULL);
  g_slist_free (rc_priv->color_hashes);

  G_OBJECT_CLASS (gtk_rc_style_parent_class)->finalize (object);
}

GtkRcStyle *
gtk_rc_style_new (void)
{
  GtkRcStyle *style;
  
  style = g_object_new (GTK_TYPE_RC_STYLE, NULL);
  
  return style;
}

/**
 * gtk_rc_style_copy:
 * @orig: the style to copy
 *
 * Makes a copy of the specified #GtkRcStyle. This function
 * will correctly copy an RC style that is a member of a class
 * derived from #GtkRcStyle.
 *
 * Return value: (transfer full): the resulting #GtkRcStyle
 **/
GtkRcStyle *
gtk_rc_style_copy (GtkRcStyle *orig)
{
  GtkRcStyle *style;

  g_return_val_if_fail (GTK_IS_RC_STYLE (orig), NULL);
  
  style = GTK_RC_STYLE_GET_CLASS (orig)->create_rc_style (orig);
  GTK_RC_STYLE_GET_CLASS (style)->merge (style, orig);

  return style;
}

static GtkRcStyle *
gtk_rc_style_real_create_rc_style (GtkRcStyle *style)
{
  return g_object_new (G_OBJECT_TYPE (style), NULL);
}

static gint
gtk_rc_properties_cmp (gconstpointer bsearch_node1,
		       gconstpointer bsearch_node2)
{
  const GtkRcProperty *prop1 = bsearch_node1;
  const GtkRcProperty *prop2 = bsearch_node2;

  if (prop1->type_name == prop2->type_name)
    return prop1->property_name < prop2->property_name ? -1 : prop1->property_name == prop2->property_name ? 0 : 1;
  else
    return prop1->type_name < prop2->type_name ? -1 : 1;
}

static void
insert_rc_property (GtkRcStyle    *style,
		    GtkRcProperty *property,
		    gboolean       replace)
{
  guint i;
  GtkRcProperty *new_property = NULL;
  GtkRcProperty key = { 0, 0, NULL, { 0, }, };

  key.type_name = property->type_name;
  key.property_name = property->property_name;

  if (!style->rc_properties)
    style->rc_properties = g_array_new (FALSE, FALSE, sizeof (GtkRcProperty));

  i = 0;
  while (i < style->rc_properties->len)
    {
      gint cmp = gtk_rc_properties_cmp (&key, &g_array_index (style->rc_properties, GtkRcProperty, i));

      if (cmp == 0)
	{
	  if (replace)
	    {
	      new_property = &g_array_index (style->rc_properties, GtkRcProperty, i);
	      
	      g_free (new_property->origin);
	      g_value_unset (&new_property->value);
	      
	      *new_property = key;
	      break;
	    }
	  else
	    return;
	}
      else if (cmp < 0)
	break;

      i++;
    }

  if (!new_property)
    {
      g_array_insert_val (style->rc_properties, i, key);
      new_property = &g_array_index (style->rc_properties, GtkRcProperty, i);
    }

  new_property->origin = g_strdup (property->origin);
  g_value_init (&new_property->value, G_VALUE_TYPE (&property->value));
  g_value_copy (&property->value, &new_property->value);
}

static void
gtk_rc_style_real_merge (GtkRcStyle *dest,
			 GtkRcStyle *src)
{
  gint i;

  for (i = 0; i < 5; i++)
    {
      if (!dest->bg_pixmap_name[i] && src->bg_pixmap_name[i])
	dest->bg_pixmap_name[i] = g_strdup (src->bg_pixmap_name[i]);
      
      if (!(dest->color_flags[i] & GTK_RC_FG) && 
	  src->color_flags[i] & GTK_RC_FG)
	{
	  dest->fg[i] = src->fg[i];
	  dest->color_flags[i] |= GTK_RC_FG;
	}
      if (!(dest->color_flags[i] & GTK_RC_BG) && 
	  src->color_flags[i] & GTK_RC_BG)
	{
	  dest->bg[i] = src->bg[i];
	  dest->color_flags[i] |= GTK_RC_BG;
	}
      if (!(dest->color_flags[i] & GTK_RC_TEXT) && 
	  src->color_flags[i] & GTK_RC_TEXT)
	{
	  dest->text[i] = src->text[i];
	  dest->color_flags[i] |= GTK_RC_TEXT;
	}
      if (!(dest->color_flags[i] & GTK_RC_BASE) && 
	  src->color_flags[i] & GTK_RC_BASE)
	{
	  dest->base[i] = src->base[i];
	  dest->color_flags[i] |= GTK_RC_BASE;
	}
    }

  if (dest->xthickness < 0 && src->xthickness >= 0)
    dest->xthickness = src->xthickness;
  if (dest->ythickness < 0 && src->ythickness >= 0)
    dest->ythickness = src->ythickness;

  if (src->font_desc)
    {
      if (!dest->font_desc)
	dest->font_desc = pango_font_description_copy (src->font_desc);
      else
	pango_font_description_merge (dest->font_desc, src->font_desc, FALSE);
    }

  if (src->rc_properties)
    {
      guint i;

      for (i = 0; i < src->rc_properties->len; i++)
	insert_rc_property (dest,
			    &g_array_index (src->rc_properties, GtkRcProperty, i),
			    FALSE);
    }
}

static GtkStyle *
gtk_rc_style_real_create_style (GtkRcStyle *rc_style)
{
  return gtk_style_new ();
}

/**
 * gtk_rc_reset_styles:
 * @settings: a #GtkSettings
 * 
 * This function recomputes the styles for all widgets that use a
 * particular #GtkSettings object. (There is one #GtkSettings object
 * per #GdkScreen, see gtk_settings_get_for_screen()); It is useful
 * when some global parameter has changed that affects the appearance
 * of all widgets, because when a widget gets a new style, it will
 * both redraw and recompute any cached information about its
 * appearance. As an example, it is used when the default font size
 * set by the operating system changes. Note that this function
 * doesn't affect widgets that have a style set explicitely on them
 * with gtk_widget_set_style().
 *
 * Since: 2.4
 **/
void
gtk_rc_reset_styles (GtkSettings *settings)
{
  gtk_style_context_reset_widgets (_gtk_settings_get_screen (settings));
}

/**
 * gtk_rc_reparse_all_for_settings:
 * @settings: a #GtkSettings
 * @force_load: load whether or not anything changed
 * 
 * If the modification time on any previously read file
 * for the given #GtkSettings has changed, discard all style information
 * and then reread all previously read RC files.
 * 
 * Return value: %TRUE if the files were reread.
 **/
gboolean
gtk_rc_reparse_all_for_settings (GtkSettings *settings,
				 gboolean     force_load)
{
  return FALSE;
}

/**
 * gtk_rc_reparse_all:
 * 
 * If the modification time on any previously read file for the
 * default #GtkSettings has changed, discard all style information
 * and then reread all previously read RC files.
 * 
 * Return value:  %TRUE if the files were reread.
 **/
gboolean
gtk_rc_reparse_all (void)
{
  return FALSE;
}

/**
 * gtk_rc_get_style:
 * @widget: a #GtkWidget
 *
 * Finds all matching RC styles for a given widget,
 * composites them together, and then creates a
 * #GtkStyle representing the composite appearance.
 * (GTK+ actually keeps a cache of previously
 * created styles, so a new style may not be
 * created.)
 *
 * Returns: (transfer none): the resulting style. No refcount is added
 *   to the returned style, so if you want to save this style around,
 *   you should add a reference yourself.
 *
 * Deprecated:3.0: Use #GtkStyleContext instead
 **/
GtkStyle *
gtk_rc_get_style (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  gtk_widget_ensure_style (widget);

  return gtk_widget_get_style (widget);
}

/**
 * gtk_rc_get_style_by_paths:
 * @settings: a #GtkSettings object
 * @widget_path: (allow-none): the widget path to use when looking up the
 *     style, or %NULL if no matching against the widget path should be done
 * @class_path: (allow-none): the class path to use when looking up the style,
 *     or %NULL if no matching against the class path should be done.
 * @type: a type that will be used along with parent types of this type
 *     when matching against class styles, or #G_TYPE_NONE
 *
 * Creates up a #GtkStyle from styles defined in a RC file by providing
 * the raw components used in matching. This function may be useful
 * when creating pseudo-widgets that should be themed like widgets but
 * don't actually have corresponding GTK+ widgets. An example of this
 * would be items inside a GNOME canvas widget.
 *
 * The action of gtk_rc_get_style() is similar to:
 * |[
 *  gtk_widget_path (widget, NULL, &path, NULL);
 *  gtk_widget_class_path (widget, NULL, &class_path, NULL);
 *  gtk_rc_get_style_by_paths (gtk_widget_get_settings (widget),
 *                             path, class_path,
 *                             G_OBJECT_TYPE (widget));
 * ]|
 *
 * Return value: (transfer none): A style created by matching with the
 *     supplied paths, or %NULL if nothing matching was specified and the
 *     default style should be used. The returned value is owned by GTK+
 *     as part of an internal cache, so you must call g_object_ref() on
 *     the returned value if you want to keep a reference to it.
 *
 * Deprecated:3.0: Use #GtkStyleContext instead
 **/
GtkStyle *
gtk_rc_get_style_by_paths (GtkSettings *settings,
			   const char  *widget_path,
			   const char  *class_path,
			   GType        type)
{
  GtkWidgetPath *path;
  GtkStyle *style;

  path = gtk_widget_path_new ();

  /* For compatibility, we return a GtkStyle based on a GtkStyleContext
   * with a GtkWidgetPath appropriate for the supplied information.
   *
   * GtkWidgetPath is composed of a list of GTypes with optional names;
   * In GTK+-2.0, widget_path consisted of the widget names, or
   * the class names for unnamed widgets, while class_path had the
   * class names always. So, use class_path to determine the GTypes
   * and extract widget names from widget_path as applicable.
   */
  if (class_path == NULL)
    {
      gtk_widget_path_append_type (path, type == G_TYPE_NONE ? GTK_TYPE_WIDGET : type);
    }
  else
    {
      const gchar *widget_p, *widget_next;
      const gchar *class_p, *class_next;

      widget_next = widget_path;
      class_next = class_path;

      while (*class_next)
	{
	  GType component_type;
	  gchar *component_class;
	  gchar *component_name;
	  gint pos;

	  class_p = class_next;
	  if (*class_p == '.')
	    class_p++;

	  widget_p = widget_next; /* Might be NULL */
	  if (widget_p && *widget_p == '.')
	    widget_p++;

	  class_next = strchr (class_p, '.');
	  if (class_next == NULL)
	    class_next = class_p + strlen (class_p);

	  if (widget_p)
	    {
	      widget_next = strchr (widget_p, '.');
	      if (widget_next == NULL)
		widget_next = widget_p + strlen (widget_p);
	    }

	  component_class = g_strndup (class_p, class_next - class_p);
	  if (widget_p && *widget_p)
	    component_name = g_strndup (widget_p, widget_next - widget_p);
	  else
	    component_name = NULL;

	  component_type = g_type_from_name (component_class);
	  if (component_type == G_TYPE_INVALID)
	    component_type = GTK_TYPE_WIDGET;

	  pos = gtk_widget_path_append_type (path, component_type);
	  if (component_name != NULL && strcmp (component_name, component_name) != 0)
	    gtk_widget_path_iter_set_name (path, pos, component_name);

	  g_free (component_class);
	  g_free (component_name);
	}
    }

  style = _gtk_style_new_for_path (_gtk_settings_get_screen (settings),
				   path);

  gtk_widget_path_free (path);

  return style;
}

/**
 * gtk_rc_scanner_new: (skip)
 *
 * Deprecated:3.0: Use #GtkCssProvider instead
 */
GScanner*
gtk_rc_scanner_new (void)
{
  return g_scanner_new (&gtk_rc_scanner_config);
}

/*********************
 * Parsing functions *
 *********************/

static gboolean
lookup_color (GtkRcStyle *style,
              const char *color_name,
              GdkColor   *color)
{
  GtkRcStylePrivate *priv = GTK_RC_STYLE_GET_PRIVATE (style);
  GSList *iter;

  for (iter = priv->color_hashes; iter != NULL; iter = iter->next)
    {
      GHashTable *hash  = iter->data;
      GdkColor   *match = g_hash_table_lookup (hash, color_name);

      if (match)
        {
          color->red = match->red;
          color->green = match->green;
          color->blue = match->blue;
          return TRUE;
        }
    }

  return FALSE;
}

/**
 * gtk_rc_find_pixmap_in_path:
 * @settings: a #GtkSettings
 * @scanner: Scanner used to get line number information for the
 *   warning message, or %NULL
 * @pixmap_file: name of the pixmap file to locate.
 * 
 * Looks up a file in pixmap path for the specified #GtkSettings.
 * If the file is not found, it outputs a warning message using
 * g_warning() and returns %NULL.
 *
 * Return value: (type filename): the filename. 
 **/
gchar*
gtk_rc_find_pixmap_in_path (GtkSettings  *settings,
			    GScanner     *scanner,
			    const gchar  *pixmap_file)
{
  g_warning (_("Unable to locate image file in pixmap_path: \"%s\""),
             pixmap_file);
  return NULL;
}

/**
 * gtk_rc_find_module_in_path:
 * @module_file: name of a theme engine
 * 
 * Searches for a theme engine in the GTK+ search path. This function
 * is not useful for applications and should not be used.
 * 
 * Return value: (type filename): The filename, if found (must be
 *   freed with g_free()), otherwise %NULL.
 **/
gchar*
gtk_rc_find_module_in_path (const gchar *module_file)
{
  return _gtk_find_module (module_file, "engines");
}

/**
 * gtk_rc_parse_state:
 * @scanner:
 * @state: (out):
 *
 * Deprecated:3.0: Use #GtkCssProvider instead
 */
guint
gtk_rc_parse_state (GScanner	 *scanner,
		    GtkStateType *state)
{
  guint old_scope;
  guint token;

  g_return_val_if_fail (scanner != NULL, G_TOKEN_ERROR);
  g_return_val_if_fail (state != NULL, G_TOKEN_ERROR);
  
  /* we don't know where we got called from, so we reset the scope here.
   * if we bail out due to errors, we *don't* reset the scope, so the
   * error messaging code can make sense of our tokens.
   */
  old_scope = g_scanner_set_scope (scanner, 0);
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_LEFT_BRACE)
    return G_TOKEN_LEFT_BRACE;
  
  token = g_scanner_get_next_token (scanner);
  switch (token)
    {
    case GTK_RC_TOKEN_ACTIVE:
      *state = GTK_STATE_ACTIVE;
      break;
    case GTK_RC_TOKEN_INSENSITIVE:
      *state = GTK_STATE_INSENSITIVE;
      break;
    case GTK_RC_TOKEN_NORMAL:
      *state = GTK_STATE_NORMAL;
      break;
    case GTK_RC_TOKEN_PRELIGHT:
      *state = GTK_STATE_PRELIGHT;
      break;
    case GTK_RC_TOKEN_SELECTED:
      *state = GTK_STATE_SELECTED;
      break;
    default:
      return /* G_TOKEN_SYMBOL */ GTK_RC_TOKEN_NORMAL;
    }
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_RIGHT_BRACE)
    return G_TOKEN_RIGHT_BRACE;
  
  g_scanner_set_scope (scanner, old_scope);

  return G_TOKEN_NONE;
}

/**
 * gtk_rc_parse_priority:
 * @scanner:
 * @priority: (out):
 *
 * Deprecated:3.0: Use #GtkCssProvider instead
 */
guint
gtk_rc_parse_priority (GScanner	           *scanner,
		       GtkPathPriorityType *priority)
{
  guint old_scope;
  guint token;

  g_return_val_if_fail (scanner != NULL, G_TOKEN_ERROR);
  g_return_val_if_fail (priority != NULL, G_TOKEN_ERROR);

  /* we don't know where we got called from, so we reset the scope here.
   * if we bail out due to errors, we *don't* reset the scope, so the
   * error messaging code can make sense of our tokens.
   */
  old_scope = g_scanner_set_scope (scanner, 0);
  
  token = g_scanner_get_next_token (scanner);
  if (token != ':')
    return ':';
  
  token = g_scanner_get_next_token (scanner);
  switch (token)
    {
    case GTK_RC_TOKEN_LOWEST:
      *priority = GTK_PATH_PRIO_LOWEST;
      break;
    case GTK_RC_TOKEN_GTK:
      *priority = GTK_PATH_PRIO_GTK;
      break;
    case GTK_RC_TOKEN_APPLICATION:
      *priority = GTK_PATH_PRIO_APPLICATION;
      break;
    case GTK_RC_TOKEN_THEME:
      *priority = GTK_PATH_PRIO_THEME;
      break;
    case GTK_RC_TOKEN_RC:
      *priority = GTK_PATH_PRIO_RC;
      break;
    case GTK_RC_TOKEN_HIGHEST:
      *priority = GTK_PATH_PRIO_HIGHEST;
      break;
    default:
      return /* G_TOKEN_SYMBOL */ GTK_RC_TOKEN_APPLICATION;
    }
  
  g_scanner_set_scope (scanner, old_scope);

  return G_TOKEN_NONE;
}

/**
 * gtk_rc_parse_color:
 * @scanner: a #GScanner
 * @color: (out): a pointer to a #GdkColor structure in which to store
 *     the result
 *
 * Parses a color in the <link linkend="color=format">format</link> expected
 * in a RC file. 
 *
 * Note that theme engines should use gtk_rc_parse_color_full() in 
 * order to support symbolic colors.
 *
 * Returns: %G_TOKEN_NONE if parsing succeeded, otherwise the token
 *     that was expected but not found
 *
 * Deprecated:3.0: Use #GtkCssProvider instead
 */
guint
gtk_rc_parse_color (GScanner *scanner,
		    GdkColor *color)
{
  return gtk_rc_parse_color_full (scanner, NULL, color);
}

/**
 * gtk_rc_parse_color_full:
 * @scanner: a #GScanner
 * @style: (allow-none): a #GtkRcStyle, or %NULL
 * @color: (out): a pointer to a #GdkColor structure in which to store
 *     the result
 *
 * Parses a color in the <link linkend="color=format">format</link> expected
 * in a RC file. If @style is not %NULL, it will be consulted to resolve
 * references to symbolic colors.
 *
 * Returns: %G_TOKEN_NONE if parsing succeeded, otherwise the token
 *     that was expected but not found
 *
 * Since: 2.12
 *
 * Deprecated:3.0: Use #GtkCssProvider instead
 */
guint
gtk_rc_parse_color_full (GScanner   *scanner,
                         GtkRcStyle *style,
                         GdkColor   *color)
{
  guint token;

  g_return_val_if_fail (scanner != NULL, G_TOKEN_ERROR);

  /* we don't need to set our own scope here, because
   * we don't need own symbols
   */
  
  token = g_scanner_get_next_token (scanner);
  switch (token)
    {
      gint token_int;
      GdkColor c1, c2;
      gboolean negate;
      gdouble l;

    case G_TOKEN_LEFT_CURLY:
      token = g_scanner_get_next_token (scanner);
      if (token == G_TOKEN_INT)
	token_int = scanner->value.v_int;
      else if (token == G_TOKEN_FLOAT)
	token_int = scanner->value.v_float * 65535.0;
      else
	return G_TOKEN_FLOAT;
      color->red = CLAMP (token_int, 0, 65535);
      
      token = g_scanner_get_next_token (scanner);
      if (token != G_TOKEN_COMMA)
	return G_TOKEN_COMMA;
      
      token = g_scanner_get_next_token (scanner);
      if (token == G_TOKEN_INT)
	token_int = scanner->value.v_int;
      else if (token == G_TOKEN_FLOAT)
	token_int = scanner->value.v_float * 65535.0;
      else
	return G_TOKEN_FLOAT;
      color->green = CLAMP (token_int, 0, 65535);
      
      token = g_scanner_get_next_token (scanner);
      if (token != G_TOKEN_COMMA)
	return G_TOKEN_COMMA;
      
      token = g_scanner_get_next_token (scanner);
      if (token == G_TOKEN_INT)
	token_int = scanner->value.v_int;
      else if (token == G_TOKEN_FLOAT)
	token_int = scanner->value.v_float * 65535.0;
      else
	return G_TOKEN_FLOAT;
      color->blue = CLAMP (token_int, 0, 65535);
      
      token = g_scanner_get_next_token (scanner);
      if (token != G_TOKEN_RIGHT_CURLY)
	return G_TOKEN_RIGHT_CURLY;
      return G_TOKEN_NONE;
      
    case G_TOKEN_STRING:
      if (!gdk_color_parse (scanner->value.v_string, color))
	{
          g_scanner_warn (scanner, "Invalid color constant '%s'",
                          scanner->value.v_string);
          return G_TOKEN_STRING;
	}
      return G_TOKEN_NONE;

    case '@':
      token = g_scanner_get_next_token (scanner);
      if (token != G_TOKEN_IDENTIFIER)
	return G_TOKEN_IDENTIFIER;

      if (!style || !lookup_color (style, scanner->value.v_identifier, color))
        {
          g_scanner_warn (scanner, "Invalid symbolic color '%s'",
                          scanner->value.v_identifier);
          return G_TOKEN_IDENTIFIER;
        }

      return G_TOKEN_NONE;

    case G_TOKEN_IDENTIFIER:
      if (strcmp (scanner->value.v_identifier, "mix") == 0)
        {
          token = g_scanner_get_next_token (scanner);
          if (token != G_TOKEN_LEFT_PAREN)
            return G_TOKEN_LEFT_PAREN;

          negate = FALSE;
          if (g_scanner_peek_next_token (scanner) == '-')
            {
              g_scanner_get_next_token (scanner); /* eat sign */
              negate = TRUE;
            }

          token = g_scanner_get_next_token (scanner);
          if (token != G_TOKEN_FLOAT)
            return G_TOKEN_FLOAT;

          l = negate ? -scanner->value.v_float : scanner->value.v_float;

          token = g_scanner_get_next_token (scanner);
          if (token != G_TOKEN_COMMA)
            return G_TOKEN_COMMA;

          token = gtk_rc_parse_color_full (scanner, style, &c1);
          if (token != G_TOKEN_NONE)
            return token;

	  token = g_scanner_get_next_token (scanner);
	  if (token != G_TOKEN_COMMA)
            return G_TOKEN_COMMA;

	  token = gtk_rc_parse_color_full (scanner, style, &c2);
	  if (token != G_TOKEN_NONE)
            return token;

	  token = g_scanner_get_next_token (scanner);
	  if (token != G_TOKEN_RIGHT_PAREN)
            return G_TOKEN_RIGHT_PAREN;

	  color->red   = l * c1.red   + (1.0 - l) * c2.red;
	  color->green = l * c1.green + (1.0 - l) * c2.green;
	  color->blue  = l * c1.blue  + (1.0 - l) * c2.blue;

	  return G_TOKEN_NONE;
	}
      else if (strcmp (scanner->value.v_identifier, "shade") == 0)
        {
	  token = g_scanner_get_next_token (scanner);
          if (token != G_TOKEN_LEFT_PAREN)
            return G_TOKEN_LEFT_PAREN;

          negate = FALSE;
          if (g_scanner_peek_next_token (scanner) == '-')
            {
              g_scanner_get_next_token (scanner); /* eat sign */
              negate = TRUE;
            }

          token = g_scanner_get_next_token (scanner);
          if (token != G_TOKEN_FLOAT)
            return G_TOKEN_FLOAT;

          l = negate ? -scanner->value.v_float : scanner->value.v_float;

          token = g_scanner_get_next_token (scanner);
          if (token != G_TOKEN_COMMA)
            return G_TOKEN_COMMA;

          token = gtk_rc_parse_color_full (scanner, style, &c1);
          if (token != G_TOKEN_NONE)
            return token;

          token = g_scanner_get_next_token (scanner);
          if (token != G_TOKEN_RIGHT_PAREN)
            return G_TOKEN_RIGHT_PAREN;

          _gtk_style_shade (&c1, color, l);

          return G_TOKEN_NONE;
        }
      else if (strcmp (scanner->value.v_identifier, "lighter") == 0 ||
               strcmp (scanner->value.v_identifier, "darker") == 0)
        {
          if (scanner->value.v_identifier[0] == 'l')
            l = 1.3;
          else
	    l = 0.7;

	  token = g_scanner_get_next_token (scanner);
          if (token != G_TOKEN_LEFT_PAREN)
            return G_TOKEN_LEFT_PAREN;

          token = gtk_rc_parse_color_full (scanner, style, &c1);
          if (token != G_TOKEN_NONE)
            return token;

          token = g_scanner_get_next_token (scanner);
          if (token != G_TOKEN_RIGHT_PAREN)
            return G_TOKEN_RIGHT_PAREN;

          _gtk_style_shade (&c1, color, l);

          return G_TOKEN_NONE;
        }
      else
        return G_TOKEN_IDENTIFIER;

    default:
      return G_TOKEN_STRING;
    }
}
