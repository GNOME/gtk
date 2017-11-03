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
#include "gdkdisplayprivate.h"
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
 * gdk_window_set_cursor(). 
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
 * If a #GdkWindow cannot use a cursor because of the reasons mentioned above,
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
  PROP_DISPLAY,
  PROP_FALLBACK,
  PROP_HOTSPOT_X,
  PROP_HOTSPOT_Y,
  PROP_NAME,
  PROP_TEXTURE,
};

G_DEFINE_ABSTRACT_TYPE (GdkCursor, gdk_cursor, G_TYPE_OBJECT)

static void
gdk_cursor_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  GdkCursor *cursor = GDK_CURSOR (object);

  switch (prop_id)
    {
    case PROP_DISPLAY:
      g_value_set_object (value, cursor->display);
      break;
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
    case PROP_DISPLAY:
      cursor->display = g_value_get_object (value);
      /* check that implementations actually provide the display when constructing */
      g_assert (cursor->display != NULL);
      break;
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
				   PROP_DISPLAY,
				   g_param_spec_object ("display",
                                                        P_("Display"),
                                                        P_("Display of this cursor"),
                                                        GDK_TYPE_DISPLAY,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
				   PROP_FALLBACK,
				   g_param_spec_object ("fallback",
                                                        P_("Fallback"),
                                                        P_("Cursor image to fall back to if this cursor cannot be displayed"),
                                                        GDK_TYPE_CURSOR,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
				   PROP_HOTSPOT_X,
				   g_param_spec_int ("hotspot-x",
                                                     P_("Hotspot X"),
                                                     P_("Horizontal offset of the cursor hotspot"),
                                                     0, G_MAXINT, 0,
                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
				   PROP_HOTSPOT_Y,
				   g_param_spec_int ("hotspot-y",
                                                     P_("Hotspot Y"),
                                                     P_("Vertical offset of the cursor hotspot"),
                                                     0, G_MAXINT, 0,
                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
				   PROP_NAME,
				   g_param_spec_string ("name",
                                                        P_("Name"),
                                                        P_("Name of this cursor"),
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
				   PROP_TEXTURE,
				   g_param_spec_object ("texture",
                                                        P_("Texture"),
                                                        P_("The texture displayed by this cursor"),
                                                        GDK_TYPE_TEXTURE,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
gdk_cursor_init (GdkCursor *cursor)
{
}

/**
 * gdk_cursor_new_from_name:
 * @display: the #GdkDisplay for which the cursor will be created
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
 *
 * Since: 2.8
 */
GdkCursor*
gdk_cursor_new_from_name (GdkDisplay  *display,
                          const gchar *name)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  return GDK_DISPLAY_GET_CLASS (display)->get_cursor_for_name (display, name);
}

/**
 * gdk_cursor_new_from_pixbuf:
 * @display: the #GdkDisplay for which the cursor will be created
 * @pixbuf: the #GdkPixbuf containing the cursor image
 * @x: the horizontal offset of the “hotspot” of the cursor.
 * @y: the vertical offset of the “hotspot” of the cursor.
 *
 * Creates a new cursor from a pixbuf.
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
 * If @x or @y are `-1`, the pixbuf must have
 * options named “x_hot” and “y_hot”, resp., containing
 * integer values between `0` and the width resp. height of
 * the pixbuf. (Since: 3.0)
 *
 * On the X backend, support for RGBA cursors requires a
 * sufficently new version of the X Render extension.
 *
 * Returns: a new #GdkCursor.
 *
 * Since: 2.4
 */
GdkCursor *
gdk_cursor_new_from_pixbuf (GdkDisplay *display,
                            GdkPixbuf  *pixbuf,
                            gint        x,
                            gint        y)
{
  GdkTexture *texture;
  const char *option;
  char *end;
  gint64 value;
  GdkCursor *cursor;
 
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);

  if (x == -1 && (option = gdk_pixbuf_get_option (pixbuf, "x_hot")))
    {
      errno = 0;
      end = NULL;
      value = g_ascii_strtoll (option, &end, 10);
      if (errno == 0 &&
          end != option &&
          value >= 0 && value < G_MAXINT)
        x = (gint) value;
    }
  
  if (y == -1 && (option = gdk_pixbuf_get_option (pixbuf, "y_hot")))
    {
      errno = 0;
      end = NULL;
      value = g_ascii_strtoll (option, &end, 10);
      if (errno == 0 &&
          end != option &&
          value >= 0 && value < G_MAXINT)
        y = (gint) value;
    }

  texture = gdk_texture_new_for_pixbuf (pixbuf);
  
  cursor = gdk_cursor_new_from_texture (display, texture, x, y);

  g_object_unref (texture);

  return cursor;
}

/**
 * gdk_cursor_new_from_texture:
 * @display: the #GdkDisplay for which the cursor will be created
 * @texture: the texture providing the pixel data
 * @hotspot_x: the horizontal offset of the “hotspot” of the cursor
 * @hotspot_y: the vertical offset of the “hotspot” of the cursor
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
 *
 * Since: 3.94
 */
GdkCursor *
gdk_cursor_new_from_texture (GdkDisplay *display,
			     GdkTexture *texture,
			     int         hotspot_x,
			     int         hotspot_y)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (GDK_IS_TEXTURE (texture), NULL);
  g_return_val_if_fail (0 <= hotspot_x && hotspot_x < gdk_texture_get_width (texture), NULL);
  g_return_val_if_fail (0 <= hotspot_y && hotspot_y < gdk_texture_get_height (texture), NULL);

  return GDK_DISPLAY_GET_CLASS (display)->get_cursor_for_texture (display,
								  texture,
                                                                  hotspot_x, hotspot_y);
}

/**
 * gdk_cursor_get_display:
 * @cursor: a #GdkCursor.
 *
 * Returns the display on which the #GdkCursor is defined.
 *
 * Returns: (transfer none): the #GdkDisplay associated to @cursor
 *
 * Since: 2.2
 */
GdkDisplay *
gdk_cursor_get_display (GdkCursor *cursor)
{
  g_return_val_if_fail (GDK_IS_CURSOR (cursor), NULL);

  return cursor->display;
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
 * Returns: (transfer none): the fallback of the cursor or %NULL to use
 *     the default cursor as fallback.
 *
 * Since: 3.94
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
 * Returns: (transfer none): the name of the cursor or %NULL if it is not
 *     a named cursor
 *
 * Since: 3.94
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
 * Returns: (transfer none): the texture for cursor or %NULL if it is a
 *     named cursor
 *
 * Since: 3.94
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
 *
 * Since: 3.94
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
 *
 * Since: 3.94
 */
int
gdk_cursor_get_hotspot_y (GdkCursor *cursor)
{
  g_return_val_if_fail (GDK_IS_CURSOR (cursor), 0);

  return cursor->hotspot_y;
}

