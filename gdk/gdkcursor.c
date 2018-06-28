/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#define GDK_PIXBUF_ENABLE_BACKEND
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "gdkcursor.h"
#include "gdkcursorprivate.h"
#include "gdktexture.h"
#include "gdkintl.h"
#include "gdkinternals.h"

#include <math.h>
#include <errno.h>

/**
 * SECTION:cursors
 * @Short_description: Named and texture cursors
 * @Title: Cursors
 *
 * These functions are used to create and destroy cursors. Cursors
 * are immutable objects, so once you created them, there is no way
 * to modify them later. Create a new cursor, when you want to change
 * something about it.
 *
 * Cursors by themselves are not very interesting, they must be
 * bound to a window for users to see them. This is done with
 * gdk_surface_set_cursor() or gdk_surface_set_device_cursor().
 * Applications will typically use higher-level GTK+ functions such
 * as gtk_widget_set_cursor() instead.
 *
 * Cursors are not bound to a given #GdkDisplay, so they can be shared.
 * However, the appearance of cursors may vary when used on different
 * platforms.
 *
 * There are multiple ways to create cursors. The platform's own cursors
 * can be created with gdk_cursor_new_from_name(). That function lists
 * the commonly available names that are shared with the CSS specification.
 * Other names may be available, depending on the platform in use.
 * Another option to create a cursor is to use gdk_cursor_new_from_texture()
 * and provide an image to use for the cursor. Depending on the #GdkDisplay
 * in use, the type of supported images may be limited. See
 * gdk_display_supports_cursor_alpha(),
 * gdk_display_supports_cursor_color(),
 * gdk_display_get_default_cursor_size() and
 * gdk_display_get_maximal_cursor_size() for the limitations that might apply.
 *
 * To ease work with unsupported cursors, a fallback cursor can be provided.
 * If a #GdkSurface cannot use a cursor because of the reasons mentioned above,
 * it will try the fallback cursor. Of course, fallback cursors can themselves
 * have fallback cursors again, so it is possible to provide a chain of
 * progressively easier to support cursors. If none of the provided cursors
 * can be supported, the default cursor will be the ultimate fallback.
 */

/**
 * GdkCursor:
 *
 * A #GdkCursor represents a cursor. Its contents are private.
 *
 * Cursors are immutable objects, so they can not change after
 * they have been constructed.
 */

enum {
  PROP_0,
  PROP_FALLBACK,
  PROP_HOTSPOT_X,
  PROP_HOTSPOT_Y,
  PROP_NAME,
  PROP_TEXTURE,
};

G_DEFINE_TYPE (GdkCursor, gdk_cursor, G_TYPE_OBJECT)

static void
gdk_cursor_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  GdkCursor *cursor = GDK_CURSOR (object);

  switch (prop_id)
    {
    case PROP_FALLBACK:
      g_value_set_object (value, cursor->fallback);
      break;
    case PROP_HOTSPOT_X:
      g_value_set_int (value, cursor->hotspot_x);
      break;
    case PROP_HOTSPOT_Y:
      g_value_set_int (value, cursor->hotspot_y);
      break;
    case PROP_NAME:
      g_value_set_string (value, cursor->name);
      break;
    case PROP_TEXTURE:
      g_value_set_object (value, cursor->texture);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_cursor_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GdkCursor *cursor = GDK_CURSOR (object);

  switch (prop_id)
    {
    case PROP_FALLBACK:
      cursor->fallback = g_value_dup_object (value);
      break;
    case PROP_HOTSPOT_X:
      cursor->hotspot_x = g_value_get_int (value);
      break;
    case PROP_HOTSPOT_Y:
      cursor->hotspot_y = g_value_get_int (value);
      break;
    case PROP_NAME:
      cursor->name = g_value_dup_string (value);
      break;
    case PROP_TEXTURE:
      cursor->texture = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_cursor_finalize (GObject *object)
{
  GdkCursor *cursor = GDK_CURSOR (object);

  g_free (cursor->name);
  g_clear_object (&cursor->texture);
  g_clear_object (&cursor->fallback);

  G_OBJECT_CLASS (gdk_cursor_parent_class)->finalize (object);
}

static void
gdk_cursor_class_init (GdkCursorClass *cursor_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (cursor_class);

  object_class->get_property = gdk_cursor_get_property;
  object_class->set_property = gdk_cursor_set_property;
  object_class->finalize = gdk_cursor_finalize;

  g_object_class_install_property (object_class,
				   PROP_FALLBACK,
				   g_param_spec_object ("fallback",
                                                        P_("Fallback"),
                                                        P_("Cursor image to fall back to if this cursor cannot be displayed"),
                                                        GDK_TYPE_CURSOR,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
				   PROP_HOTSPOT_X,
				   g_param_spec_int ("hotspot-x",
                                                     P_("Hotspot X"),
                                                     P_("Horizontal offset of the cursor hotspot"),
                                                     0, G_MAXINT, 0,
                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                     G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
				   PROP_HOTSPOT_Y,
				   g_param_spec_int ("hotspot-y",
                                                     P_("Hotspot Y"),
                                                     P_("Vertical offset of the cursor hotspot"),
                                                     0, G_MAXINT, 0,
                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                     G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
				   PROP_NAME,
				   g_param_spec_string ("name",
                                                        P_("Name"),
                                                        P_("Name of this cursor"),
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
				   PROP_TEXTURE,
				   g_param_spec_object ("texture",
                                                        P_("Texture"),
                                                        P_("The texture displayed by this cursor"),
                                                        GDK_TYPE_TEXTURE,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
}

static void
gdk_cursor_init (GdkCursor *cursor)
{
}

guint
gdk_cursor_hash (gconstpointer pointer)
{
  const GdkCursor *cursor = pointer;
  guint hash;

  if (cursor->fallback)
    hash = gdk_cursor_hash (cursor->fallback) << 16;
  else
    hash = 0;

  if (cursor->name)
    hash ^= g_str_hash (cursor->name);
  else if (cursor->texture)
    hash ^= g_direct_hash (cursor->texture);

  hash ^= (cursor->hotspot_x << 8) | cursor->hotspot_y;

  return hash;
}

gboolean
gdk_cursor_equal (gconstpointer a,
                  gconstpointer b)
{
  const GdkCursor *ca = a;
  const GdkCursor *cb = b;

  if ((ca->fallback != NULL) != (cb->fallback != NULL))
    return FALSE;
  if (ca->fallback != NULL && !gdk_cursor_equal (ca->fallback, cb->fallback))
    return FALSE;

  if (g_strcmp0 (ca->name, cb->name) != 0)
    return FALSE;

  if (ca->texture != cb->texture)
    return FALSE;

  if (ca->hotspot_x != cb->hotspot_x ||
      ca->hotspot_y != cb->hotspot_y)
    return FALSE;

  return TRUE;
}

/**
 * gdk_cursor_new_from_name:
 * @fallback: (allow-none): %NULL or the #GdkCursor to fall back to when
 *     this one cannot be supported
 * @name: the name of the cursor
 *
 * Creates a new cursor by looking up @name in the current cursor
 * theme.
 *
 * A recommended set of cursor names that will work across different
 * platforms can be found in the CSS specification:
 * - "none"
 * - ![](default_cursor.png) "default"
 * - ![](help_cursor.png) "help"
 * - ![](pointer_cursor.png) "pointer"
 * - ![](context_menu_cursor.png) "context-menu"
 * - ![](progress_cursor.png) "progress"
 * - ![](wait_cursor.png) "wait"
 * - ![](cell_cursor.png) "cell"
 * - ![](crosshair_cursor.png) "crosshair"
 * - ![](text_cursor.png) "text"
 * - ![](vertical_text_cursor.png) "vertical-text"
 * - ![](alias_cursor.png) "alias"
 * - ![](copy_cursor.png) "copy"
 * - ![](no_drop_cursor.png) "no-drop"
 * - ![](move_cursor.png) "move"
 * - ![](not_allowed_cursor.png) "not-allowed"
 * - ![](grab_cursor.png) "grab"
 * - ![](grabbing_cursor.png) "grabbing"
 * - ![](all_scroll_cursor.png) "all-scroll"
 * - ![](col_resize_cursor.png) "col-resize"
 * - ![](row_resize_cursor.png) "row-resize"
 * - ![](n_resize_cursor.png) "n-resize"
 * - ![](e_resize_cursor.png) "e-resize"
 * - ![](s_resize_cursor.png) "s-resize"
 * - ![](w_resize_cursor.png) "w-resize"
 * - ![](ne_resize_cursor.png) "ne-resize"
 * - ![](nw_resize_cursor.png) "nw-resize"
 * - ![](sw_resize_cursor.png) "sw-resize"
 * - ![](se_resize_cursor.png) "se-resize"
 * - ![](ew_resize_cursor.png) "ew-resize"
 * - ![](ns_resize_cursor.png) "ns-resize"
 * - ![](nesw_resize_cursor.png) "nesw-resize"
 * - ![](nwse_resize_cursor.png) "nwse-resize"
 * - ![](zoom_in_cursor.png) "zoom-in"
 * - ![](zoom_out_cursor.png) "zoom-out"
 *
 *
 * Returns: (nullable): a new #GdkCursor, or %NULL if there is no
 *   cursor with the given name
 */
GdkCursor*
gdk_cursor_new_from_name (const gchar *name,
                          GdkCursor   *fallback)
{
  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (fallback == NULL || GDK_IS_CURSOR (fallback), NULL);

  return g_object_new (GDK_TYPE_CURSOR,
                       "name", name,
                       "fallback", fallback,
                       NULL);
}

/**
 * gdk_cursor_new_from_texture:
 * @texture: the texture providing the pixel data
 * @hotspot_x: the horizontal offset of the “hotspot” of the cursor
 * @hotspot_y: the vertical offset of the “hotspot” of the cursor
 * @fallback: (allow-none): %NULL or the #GdkCursor to fall back to when
 *     this one cannot be supported
 *
 * Creates a new cursor from a #GdkTexture.
 *
 * Not all GDK backends support RGBA cursors. If they are not
 * supported, a monochrome approximation will be displayed.
 * The functions gdk_display_supports_cursor_alpha() and
 * gdk_display_supports_cursor_color() can be used to determine
 * whether RGBA cursors are supported;
 * gdk_display_get_default_cursor_size() and
 * gdk_display_get_maximal_cursor_size() give information about
 * cursor sizes.
 *
 * On the X backend, support for RGBA cursors requires a
 * sufficently new version of the X Render extension.
 *
 * Returns: a new #GdkCursor.
 */
GdkCursor *
gdk_cursor_new_from_texture (GdkTexture *texture,
			     int         hotspot_x,
			     int         hotspot_y,
                             GdkCursor  *fallback)
{
  g_return_val_if_fail (GDK_IS_TEXTURE (texture), NULL);
  g_return_val_if_fail (0 <= hotspot_x && hotspot_x < gdk_texture_get_width (texture), NULL);
  g_return_val_if_fail (0 <= hotspot_y && hotspot_y < gdk_texture_get_height (texture), NULL);
  g_return_val_if_fail (fallback == NULL || GDK_IS_CURSOR (fallback), NULL);

  return g_object_new (GDK_TYPE_CURSOR, 
                       "texture", texture,
                       "hotspot-x", hotspot_x,
                       "hotspot-y", hotspot_y,
                       "fallback", fallback,
                       NULL);
}

/**
 * gdk_cursor_get_fallback:
 * @cursor: a #GdkCursor.
 *
 * Returns the fallback for this @cursor. The fallback will be used if this
 * cursor is not available on a given #GdkDisplay.
 *
 * For named cursors, this can happen when using nonstandard names or when
 * using an incomplete cursor theme.
 * For textured cursors, this can happen when the texture is too large or
 * when the #GdkDisplay it is used on does not support textured cursors.
 *
 * Returns: (transfer none) (nullable): the fallback of the cursor or %NULL to use
 *     the default cursor as fallback.
 */
GdkCursor *
gdk_cursor_get_fallback (GdkCursor *cursor)
{
  g_return_val_if_fail (GDK_IS_CURSOR (cursor), NULL);

  return cursor->fallback;
}

/**
 * gdk_cursor_get_name:
 * @cursor: a #GdkCursor.
 *
 * Returns the name of the cursor. If the cursor is not a named cursor, %NULL
 * will be returned and the GdkCursor::texture property will be set.
 *
 * Returns: (transfer none) (nullable): the name of the cursor or %NULL if it is not
 *     a named cursor
 */
const char *
gdk_cursor_get_name (GdkCursor *cursor)
{
  g_return_val_if_fail (GDK_IS_CURSOR (cursor), NULL);

  return cursor->name;
}

/**
 * gdk_cursor_get_texture:
 * @cursor: a #GdkCursor.
 *
 * Returns the texture for the cursor. If the cursor is a named cursor, %NULL
 * will be returned and the GdkCursor::name property will be set.
 *
 * Returns: (transfer none) (nullable): the texture for cursor or %NULL if it is a
 *     named cursor
 */
GdkTexture *
gdk_cursor_get_texture (GdkCursor *cursor)
{
  g_return_val_if_fail (GDK_IS_CURSOR (cursor), NULL);

  return cursor->texture;
}

/**
 * gdk_cursor_get_hotspot_x:
 * @cursor: a #GdkCursor.
 *
 * Returns the horizontal offset of the hotspot. The hotspot indicates the
 * pixel that will be directly above the cursor.
 *
 * Returns: the horizontal offset of the hotspot or 0 for named cursors
 */
int
gdk_cursor_get_hotspot_x (GdkCursor *cursor)
{
  g_return_val_if_fail (GDK_IS_CURSOR (cursor), 0);

  return cursor->hotspot_x;
}

/**
 * gdk_cursor_get_hotspot_y:
 * @cursor: a #GdkCursor.
 *
 * Returns the vertical offset of the hotspot. The hotspot indicates the
 * pixel that will be directly above the cursor.
 *
 * Returns: the vertical offset of the hotspot or 0 for named cursors
 */
int
gdk_cursor_get_hotspot_y (GdkCursor *cursor)
{
  g_return_val_if_fail (GDK_IS_CURSOR (cursor), 0);

  return cursor->hotspot_y;
}

