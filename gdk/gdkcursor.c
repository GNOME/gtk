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

#include "gdkcursor.h"
#include "gdkcursorprivate.h"
#include "gdkdisplayprivate.h"
#include "gdkintl.h"
#include "gdkinternals.h"


/**
 * SECTION:cursors
 * @Short_description: Standard and pixmap cursors
 * @Title: Cursors
 *
 * These functions are used to create and destroy cursors.
 * There is a number of standard cursors, but it is also
 * possible to construct new cursors from pixbufs. There
 * may be limitations as to what kinds of cursors can be
 * constructed on a given display, see
 * gdk_display_supports_cursor_alpha(),
 * gdk_display_supports_cursor_color(),
 * gdk_display_get_default_cursor_size() and
 * gdk_display_get_maximal_cursor_size().
 *
 * Cursors by themselves are not very interesting, they must be be
 * bound to a window for users to see them. This is done with
 * gdk_window_set_cursor() or by setting the cursor member of the
 * #GdkWindowAttr struct passed to gdk_window_new().
 */

/**
 * GdkCursor:
 *
 * The #GdkCursor structure represents a cursor. Its contents are private.
 */

enum {
  PROP_0,
  PROP_CURSOR_TYPE,
  PROP_DISPLAY
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
    case PROP_CURSOR_TYPE:
      g_value_set_enum (value, cursor->type);
      break;
    case PROP_DISPLAY:
      g_value_set_object (value, cursor->display);
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
    case PROP_CURSOR_TYPE:
      cursor->type = g_value_get_enum (value);
      break;
    case PROP_DISPLAY:
      cursor->display = g_value_get_object (value);
      /* check that implementations actually provide the display when constructing */
      g_assert (cursor->display != NULL);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_cursor_class_init (GdkCursorClass *cursor_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (cursor_class);

  object_class->get_property = gdk_cursor_get_property;
  object_class->set_property = gdk_cursor_set_property;

  g_object_class_install_property (object_class,
				   PROP_CURSOR_TYPE,
				   g_param_spec_enum ("cursor-type",
                                                      P_("Cursor type"),
                                                      P_("Standard cursor type"),
                                                      GDK_TYPE_CURSOR_TYPE, GDK_X_CURSOR,
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class,
				   PROP_DISPLAY,
				   g_param_spec_object ("display",
                                                        P_("Display"),
                                                        P_("Display of this cursor"),
                                                        GDK_TYPE_DISPLAY,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
gdk_cursor_init (GdkCursor *cursor)
{
}

/**
 * gdk_cursor_ref:
 * @cursor: a #GdkCursor
 *
 * Adds a reference to @cursor.
 *
 * Return value: (transfer full): Same @cursor that was passed in
 *
 * Deprecated: 3.0: Use g_object_ref() instead
 */
GdkCursor*
gdk_cursor_ref (GdkCursor *cursor)
{
  g_return_val_if_fail (cursor != NULL, NULL);

  return g_object_ref (cursor);
}

/**
 * gdk_cursor_unref:
 * @cursor: a #GdkCursor
 *
 * Removes a reference from @cursor, deallocating the cursor
 * if no references remain.
 *
 * Deprecated: 3.0: Use g_object_unref() instead
 */
void
gdk_cursor_unref (GdkCursor *cursor)
{
  g_return_if_fail (cursor != NULL);

  g_object_unref (cursor);
}

/**
 * gdk_cursor_new:
 * @cursor_type: cursor to create
 *
 * Creates a new cursor from the set of builtin cursors for the default display.
 * See gdk_cursor_new_for_display().
 *
 * To make the cursor invisible, use %GDK_BLANK_CURSOR.
 *
 * Return value: a new #GdkCursor
 */
GdkCursor*
gdk_cursor_new (GdkCursorType cursor_type)
{
  return gdk_cursor_new_for_display (gdk_display_get_default(), cursor_type);
}

/**
 * gdk_cursor_get_cursor_type:
 * @cursor:  a #GdkCursor
 *
 * Returns the cursor type for this cursor.
 *
 * Return value: a #GdkCursorType
 *
 * Since: 2.22
 **/
GdkCursorType
gdk_cursor_get_cursor_type (GdkCursor *cursor)
{
  g_return_val_if_fail (cursor != NULL, GDK_BLANK_CURSOR);

  return cursor->type;
}

/**
 * gdk_cursor_new_for_display:
 * @display: the #GdkDisplay for which the cursor will be created
 * @cursor_type: cursor to create
 *
 * Creates a new cursor from the set of builtin cursors.
 * Some useful ones are:
 * <itemizedlist>
 * <listitem><para>
 *  <inlinegraphic format="PNG" fileref="right_ptr.png"></inlinegraphic> #GDK_RIGHT_PTR (right-facing arrow)
 * </para></listitem>
 * <listitem><para>
 *  <inlinegraphic format="PNG" fileref="crosshair.png"></inlinegraphic> #GDK_CROSSHAIR (crosshair)
 * </para></listitem>
 * <listitem><para>
 *  <inlinegraphic format="PNG" fileref="xterm.png"></inlinegraphic> #GDK_XTERM (I-beam)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="watch.png"></inlinegraphic> #GDK_WATCH (busy)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="fleur.png"></inlinegraphic> #GDK_FLEUR (for moving objects)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="hand1.png"></inlinegraphic> #GDK_HAND1 (a right-pointing hand)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="hand2.png"></inlinegraphic> #GDK_HAND2 (a left-pointing hand)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="left_side.png"></inlinegraphic> #GDK_LEFT_SIDE (resize left side)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="right_side.png"></inlinegraphic> #GDK_RIGHT_SIDE (resize right side)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="top_left_corner.png"></inlinegraphic> #GDK_TOP_LEFT_CORNER (resize northwest corner)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="top_right_corner.png"></inlinegraphic> #GDK_TOP_RIGHT_CORNER (resize northeast corner)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="bottom_left_corner.png"></inlinegraphic> #GDK_BOTTOM_LEFT_CORNER (resize southwest corner)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="bottom_right_corner.png"></inlinegraphic> #GDK_BOTTOM_RIGHT_CORNER (resize southeast corner)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="top_side.png"></inlinegraphic> #GDK_TOP_SIDE (resize top side)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="bottom_side.png"></inlinegraphic> #GDK_BOTTOM_SIDE (resize bottom side)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="sb_h_double_arrow.png"></inlinegraphic> #GDK_SB_H_DOUBLE_ARROW (move vertical splitter)
 * </para></listitem>
 * <listitem><para>
 * <inlinegraphic format="PNG" fileref="sb_v_double_arrow.png"></inlinegraphic> #GDK_SB_V_DOUBLE_ARROW (move horizontal splitter)
 * </para></listitem>
 * <listitem><para>
 * #GDK_BLANK_CURSOR (Blank cursor). Since 2.16
 * </para></listitem>
 * </itemizedlist>
 *
 * Return value: a new #GdkCursor
 *
 * Since: 2.2
 **/
GdkCursor*
gdk_cursor_new_for_display (GdkDisplay    *display,
                            GdkCursorType  cursor_type)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_DISPLAY_GET_CLASS (display)->get_cursor_for_type (display, cursor_type);
}

/**
 * gdk_cursor_new_from_name:
 * @display: the #GdkDisplay for which the cursor will be created
 * @name: the name of the cursor
 *
 * Creates a new cursor by looking up @name in the current cursor
 * theme.
 *
 * Returns: a new #GdkCursor, or %NULL if there is no cursor with
 *   the given name
 *
 * Since: 2.8
 */
GdkCursor*
gdk_cursor_new_from_name (GdkDisplay  *display,
                          const gchar *name)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_DISPLAY_GET_CLASS (display)->get_cursor_for_name (display, name);
}

/**
 * gdk_cursor_new_from_pixbuf:
 * @display: the #GdkDisplay for which the cursor will be created
 * @pixbuf: the #GdkPixbuf containing the cursor image
 * @x: the horizontal offset of the 'hotspot' of the cursor.
 * @y: the vertical offset of the 'hotspot' of the cursor.
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
 * If @x or @y are <literal>-1</literal>, the pixbuf must have
 * options named "x_hot" and "y_hot", resp., containing
 * integer values between %0 and the width resp. height of
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
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);

  return GDK_DISPLAY_GET_CLASS (display)->get_cursor_for_pixbuf (display, pixbuf, x, y);
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
 * gdk_cursor_get_image:
 * @cursor: a #GdkCursor
 *
 * Returns a #GdkPixbuf with the image used to display the cursor.
 *
 * Note that depending on the capabilities of the windowing system and 
 * on the cursor, GDK may not be able to obtain the image data. In this 
 * case, %NULL is returned.
 *
 * Returns: (transfer full): a #GdkPixbuf representing @cursor, or %NULL
 *
 * Since: 2.8
 */
GdkPixbuf*  
gdk_cursor_get_image (GdkCursor *cursor)
{
  g_return_val_if_fail (GDK_IS_CURSOR (cursor), NULL);

  return GDK_CURSOR_GET_CLASS (cursor)->get_image (cursor);
}
