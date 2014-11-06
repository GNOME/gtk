/*
 * Copyright © 2014 Canonical Ltd
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkcursorprivate.h"

#include "gdkmir.h"
#include "gdkmir-private.h"

typedef struct GdkMirCursor      GdkMirCursor;
typedef struct GdkMirCursorClass GdkMirCursorClass;

#define GDK_TYPE_MIR_CURSOR              (gdk_mir_cursor_get_type ())
#define GDK_MIR_CURSOR(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_MIR_CURSOR, GdkMirCursor))
#define GDK_MIR_CURSOR_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_MIR_CURSOR, GdkMirCursorClass))
#define GDK_IS_MIR_CURSOR(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_MIR_CURSOR))
#define GDK_IS_MIR_CURSOR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_MIR_CURSOR))
#define GDK_MIR_CURSOR_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_MIR_CURSOR, GdkMirCursorClass))

struct GdkMirCursor
{
  GdkCursor parent_instance;

  gchar *name;
};

struct GdkMirCursorClass
{
  GdkCursorClass parent_class;
};

G_DEFINE_TYPE (GdkMirCursor, gdk_mir_cursor, GDK_TYPE_CURSOR)

static const gchar *
get_cursor_name_for_cursor_type (GdkCursorType cursor_type)
{
  switch (cursor_type)
    {
    case GDK_BLANK_CURSOR:
      return mir_disabled_cursor_name;
    case GDK_X_CURSOR:
    case GDK_ARROW:
    case GDK_CENTER_PTR:
    case GDK_DRAFT_LARGE:
    case GDK_DRAFT_SMALL:
    case GDK_LEFT_PTR:
    case GDK_RIGHT_PTR:
    case GDK_TOP_LEFT_ARROW:
      return mir_arrow_cursor_name;
    case GDK_CLOCK:
    case GDK_WATCH:
      return mir_busy_cursor_name;
    case GDK_XTERM:
      return mir_caret_cursor_name;
    case GDK_HAND1:
    case GDK_HAND2:
      return mir_pointing_hand_cursor_name;
      return mir_open_hand_cursor_name;
    case GDK_FLEUR:
      return mir_closed_hand_cursor_name;
    case GDK_LEFT_SIDE:
    case GDK_LEFT_TEE:
    case GDK_RIGHT_SIDE:
    case GDK_RIGHT_TEE:
    case GDK_SB_LEFT_ARROW:
    case GDK_SB_RIGHT_ARROW:
      return mir_horizontal_resize_cursor_name;
    case GDK_BASED_ARROW_DOWN:
    case GDK_BASED_ARROW_UP:
    case GDK_BOTTOM_SIDE:
    case GDK_BOTTOM_TEE:
    case GDK_DOUBLE_ARROW:
    case GDK_SB_DOWN_ARROW:
    case GDK_SB_UP_ARROW:
    case GDK_TOP_SIDE:
    case GDK_TOP_TEE:
      return mir_vertical_resize_cursor_name;
    case GDK_BOTTOM_LEFT_CORNER:
    case GDK_LL_ANGLE:
    case GDK_TOP_RIGHT_CORNER:
    case GDK_UR_ANGLE:
      return mir_diagonal_resize_bottom_to_top_cursor_name;
    case GDK_BOTTOM_RIGHT_CORNER:
    case GDK_LR_ANGLE:
    case GDK_SIZING:
    case GDK_TOP_LEFT_CORNER:
    case GDK_UL_ANGLE:
      return mir_diagonal_resize_top_to_bottom_cursor_name;
      return mir_omnidirectional_resize_cursor_name;
    case GDK_SB_V_DOUBLE_ARROW:
      return mir_vsplit_resize_cursor_name;
    case GDK_SB_H_DOUBLE_ARROW:
      return mir_hsplit_resize_cursor_name;
    default:
      return mir_default_cursor_name;
    }
}


GdkCursor *
_gdk_mir_cursor_new_for_name (GdkDisplay *display, const gchar *name)
{
  GdkMirCursor *cursor;

  cursor = g_object_new (GDK_TYPE_MIR_CURSOR, "display", display, "cursor-type", GDK_CURSOR_IS_PIXMAP, NULL);
  cursor->name = g_strdup (name);

  return GDK_CURSOR (cursor);
}

GdkCursor *
_gdk_mir_cursor_new_for_type (GdkDisplay *display, GdkCursorType type)
{
  GdkMirCursor *cursor;

  cursor = g_object_new (GDK_TYPE_MIR_CURSOR, "display", display, "cursor-type", type, NULL);
  cursor->name = g_strdup (get_cursor_name_for_cursor_type (type));

  return GDK_CURSOR (cursor);
}

const gchar *
_gdk_mir_cursor_get_name (GdkCursor *cursor)
{
  GdkMirCursor *mir_cursor = GDK_MIR_CURSOR (cursor);

  return mir_cursor->name;
}

cairo_surface_t *
gdk_mir_cursor_get_surface (GdkCursor *cursor,
                            gdouble   *x_hot,
                            gdouble   *y_hot)
{
  g_printerr ("gdk_mir_cursor_get_surface\n");
  return NULL;
}

static void
gdk_mir_cursor_init (GdkMirCursor *cursor)
{
}

static void
gdk_mir_cursor_finalize (GObject *object)
{
  GdkMirCursor *mir_cursor = GDK_MIR_CURSOR (object);

  g_free (mir_cursor->name);

  G_OBJECT_CLASS (gdk_mir_cursor_parent_class)->finalize (object);
}

static void
gdk_mir_cursor_class_init (GdkMirCursorClass *klass)
{
  GdkCursorClass *cursor_class = GDK_CURSOR_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  cursor_class->get_surface = gdk_mir_cursor_get_surface;
  object_class->finalize = gdk_mir_cursor_finalize;
}
