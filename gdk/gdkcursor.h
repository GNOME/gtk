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

#ifndef __GDK_CURSOR_H__
#define __GDK_CURSOR_H__

#if !defined (__GDK_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#include <gdk/gdkversionmacros.h>
#include <gdk/gdktypes.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

#define GDK_TYPE_CURSOR              (gdk_cursor_get_type ())
#define GDK_CURSOR(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_CURSOR, GdkCursor))
#define GDK_IS_CURSOR(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_CURSOR))


/**
 * GdkCursorType:
 * @GDK_X_CURSOR: <inlinegraphic format="PNG" fileref="X_cursor.png"></inlinegraphic>
 * @GDK_ARROW: <inlinegraphic format="PNG" fileref="arrow.png"></inlinegraphic>
 * @GDK_BASED_ARROW_DOWN: <inlinegraphic format="PNG" fileref="based_arrow_down.png"></inlinegraphic>
 * @GDK_BASED_ARROW_UP: <inlinegraphic format="PNG" fileref="based_arrow_up.png"></inlinegraphic>
 * @GDK_BOAT: <inlinegraphic format="PNG" fileref="boat.png"></inlinegraphic>
 * @GDK_BOGOSITY: <inlinegraphic format="PNG" fileref="bogosity.png"></inlinegraphic>
 * @GDK_BOTTOM_LEFT_CORNER: <inlinegraphic format="PNG" fileref="bottom_left_corner.png"></inlinegraphic>
 * @GDK_BOTTOM_RIGHT_CORNER: <inlinegraphic format="PNG" fileref="bottom_right_corner.png"></inlinegraphic>
 * @GDK_BOTTOM_SIDE: <inlinegraphic format="PNG" fileref="bottom_side.png"></inlinegraphic>
 * @GDK_BOTTOM_TEE: <inlinegraphic format="PNG" fileref="bottom_tee.png"></inlinegraphic>
 * @GDK_BOX_SPIRAL: <inlinegraphic format="PNG" fileref="box_spiral.png"></inlinegraphic>
 * @GDK_CENTER_PTR: <inlinegraphic format="PNG" fileref="center_ptr.png"></inlinegraphic>
 * @GDK_CIRCLE: <inlinegraphic format="PNG" fileref="circle.png"></inlinegraphic>
 * @GDK_CLOCK: <inlinegraphic format="PNG" fileref="clock.png"></inlinegraphic>
 * @GDK_COFFEE_MUG: <inlinegraphic format="PNG" fileref="coffee_mug.png"></inlinegraphic>
 * @GDK_CROSS: <inlinegraphic format="PNG" fileref="cross.png"></inlinegraphic>
 * @GDK_CROSS_REVERSE: <inlinegraphic format="PNG" fileref="cross_reverse.png"></inlinegraphic>
 * @GDK_CROSSHAIR: <inlinegraphic format="PNG" fileref="crosshair.png"></inlinegraphic>
 * @GDK_DIAMOND_CROSS: <inlinegraphic format="PNG" fileref="diamond_cross.png"></inlinegraphic>
 * @GDK_DOT: <inlinegraphic format="PNG" fileref="dot.png"></inlinegraphic>
 * @GDK_DOTBOX: <inlinegraphic format="PNG" fileref="dotbox.png"></inlinegraphic>
 * @GDK_DOUBLE_ARROW: <inlinegraphic format="PNG" fileref="double_arrow.png"></inlinegraphic>
 * @GDK_DRAFT_LARGE: <inlinegraphic format="PNG" fileref="draft_large.png"></inlinegraphic>
 * @GDK_DRAFT_SMALL: <inlinegraphic format="PNG" fileref="draft_small.png"></inlinegraphic>
 * @GDK_DRAPED_BOX: <inlinegraphic format="PNG" fileref="draped_box.png"></inlinegraphic>
 * @GDK_EXCHANGE: <inlinegraphic format="PNG" fileref="exchange.png"></inlinegraphic>
 * @GDK_FLEUR: <inlinegraphic format="PNG" fileref="fleur.png"></inlinegraphic>
 * @GDK_GOBBLER: <inlinegraphic format="PNG" fileref="gobbler.png"></inlinegraphic>
 * @GDK_GUMBY: <inlinegraphic format="PNG" fileref="gumby.png"></inlinegraphic>
 * @GDK_HAND1: <inlinegraphic format="PNG" fileref="hand1.png"></inlinegraphic>
 * @GDK_HAND2: <inlinegraphic format="PNG" fileref="hand2.png"></inlinegraphic>
 * @GDK_HEART: <inlinegraphic format="PNG" fileref="heart.png"></inlinegraphic>
 * @GDK_ICON: <inlinegraphic format="PNG" fileref="icon.png"></inlinegraphic>
 * @GDK_IRON_CROSS: <inlinegraphic format="PNG" fileref="iron_cross.png"></inlinegraphic>
 * @GDK_LEFT_PTR: <inlinegraphic format="PNG" fileref="left_ptr.png"></inlinegraphic>
 * @GDK_LEFT_SIDE: <inlinegraphic format="PNG" fileref="left_side.png"></inlinegraphic>
 * @GDK_LEFT_TEE: <inlinegraphic format="PNG" fileref="left_tee.png"></inlinegraphic>
 * @GDK_LEFTBUTTON: <inlinegraphic format="PNG" fileref="leftbutton.png"></inlinegraphic>
 * @GDK_LL_ANGLE: <inlinegraphic format="PNG" fileref="ll_angle.png"></inlinegraphic>
 * @GDK_LR_ANGLE: <inlinegraphic format="PNG" fileref="lr_angle.png"></inlinegraphic>
 * @GDK_MAN: <inlinegraphic format="PNG" fileref="man.png"></inlinegraphic>
 * @GDK_MIDDLEBUTTON: <inlinegraphic format="PNG" fileref="middlebutton.png"></inlinegraphic>
 * @GDK_MOUSE: <inlinegraphic format="PNG" fileref="mouse.png"></inlinegraphic>
 * @GDK_PENCIL: <inlinegraphic format="PNG" fileref="pencil.png"></inlinegraphic>
 * @GDK_PIRATE: <inlinegraphic format="PNG" fileref="pirate.png"></inlinegraphic>
 * @GDK_PLUS: <inlinegraphic format="PNG" fileref="plus.png"></inlinegraphic>
 * @GDK_QUESTION_ARROW: <inlinegraphic format="PNG" fileref="question_arrow.png"></inlinegraphic>
 * @GDK_RIGHT_PTR: <inlinegraphic format="PNG" fileref="right_ptr.png"></inlinegraphic>
 * @GDK_RIGHT_SIDE: <inlinegraphic format="PNG" fileref="right_side.png"></inlinegraphic>
 * @GDK_RIGHT_TEE: <inlinegraphic format="PNG" fileref="right_tee.png"></inlinegraphic>
 * @GDK_RIGHTBUTTON: <inlinegraphic format="PNG" fileref="rightbutton.png"></inlinegraphic>
 * @GDK_RTL_LOGO: <inlinegraphic format="PNG" fileref="rtl_logo.png"></inlinegraphic>
 * @GDK_SAILBOAT: <inlinegraphic format="PNG" fileref="sailboat.png"></inlinegraphic>
 * @GDK_SB_DOWN_ARROW: <inlinegraphic format="PNG" fileref="sb_down_arrow.png"></inlinegraphic>
 * @GDK_SB_H_DOUBLE_ARROW: <inlinegraphic format="PNG" fileref="sb_h_double_arrow.png"></inlinegraphic>
 * @GDK_SB_LEFT_ARROW: <inlinegraphic format="PNG" fileref="sb_left_arrow.png"></inlinegraphic>
 * @GDK_SB_RIGHT_ARROW: <inlinegraphic format="PNG" fileref="sb_right_arrow.png"></inlinegraphic>
 * @GDK_SB_UP_ARROW: <inlinegraphic format="PNG" fileref="sb_up_arrow.png"></inlinegraphic>
 * @GDK_SB_V_DOUBLE_ARROW: <inlinegraphic format="PNG" fileref="sb_v_double_arrow.png"></inlinegraphic>
 * @GDK_SHUTTLE: <inlinegraphic format="PNG" fileref="shuttle.png"></inlinegraphic>
 * @GDK_SIZING: <inlinegraphic format="PNG" fileref="sizing.png"></inlinegraphic>
 * @GDK_SPIDER: <inlinegraphic format="PNG" fileref="spider.png"></inlinegraphic>
 * @GDK_SPRAYCAN: <inlinegraphic format="PNG" fileref="spraycan.png"></inlinegraphic>
 * @GDK_STAR: <inlinegraphic format="PNG" fileref="star.png"></inlinegraphic>
 * @GDK_TARGET: <inlinegraphic format="PNG" fileref="target.png"></inlinegraphic>
 * @GDK_TCROSS: <inlinegraphic format="PNG" fileref="tcross.png"></inlinegraphic>
 * @GDK_TOP_LEFT_ARROW: <inlinegraphic format="PNG" fileref="top_left_arrow.png"></inlinegraphic>
 * @GDK_TOP_LEFT_CORNER: <inlinegraphic format="PNG" fileref="top_left_corner.png"></inlinegraphic>
 * @GDK_TOP_RIGHT_CORNER: <inlinegraphic format="PNG" fileref="top_right_corner.png"></inlinegraphic>
 * @GDK_TOP_SIDE: <inlinegraphic format="PNG" fileref="top_side.png"></inlinegraphic>
 * @GDK_TOP_TEE: <inlinegraphic format="PNG" fileref="top_tee.png"></inlinegraphic>
 * @GDK_TREK: <inlinegraphic format="PNG" fileref="trek.png"></inlinegraphic>
 * @GDK_UL_ANGLE: <inlinegraphic format="PNG" fileref="ul_angle.png"></inlinegraphic>
 * @GDK_UMBRELLA: <inlinegraphic format="PNG" fileref="umbrella.png"></inlinegraphic>
 * @GDK_UR_ANGLE: <inlinegraphic format="PNG" fileref="ur_angle.png"></inlinegraphic>
 * @GDK_WATCH: <inlinegraphic format="PNG" fileref="watch.png"></inlinegraphic>
 * @GDK_XTERM: <inlinegraphic format="PNG" fileref="xterm.png"></inlinegraphic>
 * @GDK_LAST_CURSOR: last cursor type
 * @GDK_BLANK_CURSOR: Blank cursor. Since 2.16
 * @GDK_CURSOR_IS_PIXMAP: type of cursors constructed with
 *   gdk_cursor_new_from_pixbuf()
 *
 * The standard cursors available.
 */
typedef enum
{
  GDK_X_CURSOR 		  = 0,
  GDK_ARROW 		  = 2,
  GDK_BASED_ARROW_DOWN    = 4,
  GDK_BASED_ARROW_UP 	  = 6,
  GDK_BOAT 		  = 8,
  GDK_BOGOSITY 		  = 10,
  GDK_BOTTOM_LEFT_CORNER  = 12,
  GDK_BOTTOM_RIGHT_CORNER = 14,
  GDK_BOTTOM_SIDE 	  = 16,
  GDK_BOTTOM_TEE 	  = 18,
  GDK_BOX_SPIRAL 	  = 20,
  GDK_CENTER_PTR 	  = 22,
  GDK_CIRCLE 		  = 24,
  GDK_CLOCK	 	  = 26,
  GDK_COFFEE_MUG 	  = 28,
  GDK_CROSS 		  = 30,
  GDK_CROSS_REVERSE 	  = 32,
  GDK_CROSSHAIR 	  = 34,
  GDK_DIAMOND_CROSS 	  = 36,
  GDK_DOT 		  = 38,
  GDK_DOTBOX 		  = 40,
  GDK_DOUBLE_ARROW 	  = 42,
  GDK_DRAFT_LARGE 	  = 44,
  GDK_DRAFT_SMALL 	  = 46,
  GDK_DRAPED_BOX 	  = 48,
  GDK_EXCHANGE 		  = 50,
  GDK_FLEUR 		  = 52,
  GDK_GOBBLER 		  = 54,
  GDK_GUMBY 		  = 56,
  GDK_HAND1 		  = 58,
  GDK_HAND2 		  = 60,
  GDK_HEART 		  = 62,
  GDK_ICON 		  = 64,
  GDK_IRON_CROSS 	  = 66,
  GDK_LEFT_PTR 		  = 68,
  GDK_LEFT_SIDE 	  = 70,
  GDK_LEFT_TEE 		  = 72,
  GDK_LEFTBUTTON 	  = 74,
  GDK_LL_ANGLE 		  = 76,
  GDK_LR_ANGLE 	 	  = 78,
  GDK_MAN 		  = 80,
  GDK_MIDDLEBUTTON 	  = 82,
  GDK_MOUSE 		  = 84,
  GDK_PENCIL 		  = 86,
  GDK_PIRATE 		  = 88,
  GDK_PLUS 		  = 90,
  GDK_QUESTION_ARROW 	  = 92,
  GDK_RIGHT_PTR 	  = 94,
  GDK_RIGHT_SIDE 	  = 96,
  GDK_RIGHT_TEE 	  = 98,
  GDK_RIGHTBUTTON 	  = 100,
  GDK_RTL_LOGO 		  = 102,
  GDK_SAILBOAT 		  = 104,
  GDK_SB_DOWN_ARROW 	  = 106,
  GDK_SB_H_DOUBLE_ARROW   = 108,
  GDK_SB_LEFT_ARROW 	  = 110,
  GDK_SB_RIGHT_ARROW 	  = 112,
  GDK_SB_UP_ARROW 	  = 114,
  GDK_SB_V_DOUBLE_ARROW   = 116,
  GDK_SHUTTLE 		  = 118,
  GDK_SIZING 		  = 120,
  GDK_SPIDER		  = 122,
  GDK_SPRAYCAN 		  = 124,
  GDK_STAR 		  = 126,
  GDK_TARGET 		  = 128,
  GDK_TCROSS 		  = 130,
  GDK_TOP_LEFT_ARROW 	  = 132,
  GDK_TOP_LEFT_CORNER 	  = 134,
  GDK_TOP_RIGHT_CORNER 	  = 136,
  GDK_TOP_SIDE 		  = 138,
  GDK_TOP_TEE 		  = 140,
  GDK_TREK 		  = 142,
  GDK_UL_ANGLE 		  = 144,
  GDK_UMBRELLA 		  = 146,
  GDK_UR_ANGLE 		  = 148,
  GDK_WATCH 		  = 150,
  GDK_XTERM 		  = 152,
  GDK_LAST_CURSOR,
  GDK_BLANK_CURSOR        = -2,
  GDK_CURSOR_IS_PIXMAP 	  = -1
} GdkCursorType;

/* Cursors
 */

GDK_AVAILABLE_IN_ALL
GType      gdk_cursor_get_type           (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GdkCursor* gdk_cursor_new_for_display	 (GdkDisplay      *display,
					  GdkCursorType    cursor_type);
#ifndef GDK_MULTIHEAD_SAFE
GDK_AVAILABLE_IN_ALL
GdkCursor* gdk_cursor_new		 (GdkCursorType	   cursor_type);
#endif
GDK_AVAILABLE_IN_ALL
GdkCursor* gdk_cursor_new_from_pixbuf	 (GdkDisplay      *display,
					  GdkPixbuf       *pixbuf,
					  gint             x,
					  gint             y);
GDK_AVAILABLE_IN_3_10
GdkCursor* gdk_cursor_new_from_surface	 (GdkDisplay      *display,
					  cairo_surface_t *surface,
					  gdouble          x,
					  gdouble          y);
GDK_AVAILABLE_IN_ALL
GdkCursor*  gdk_cursor_new_from_name	 (GdkDisplay      *display,
					  const gchar     *name);
GDK_AVAILABLE_IN_ALL
GdkDisplay* gdk_cursor_get_display	 (GdkCursor	  *cursor);
GDK_DEPRECATED_IN_3_0_FOR(g_object_ref)
GdkCursor * gdk_cursor_ref               (GdkCursor       *cursor);
GDK_DEPRECATED_IN_3_0_FOR(g_object_unref)
void        gdk_cursor_unref             (GdkCursor       *cursor);
GDK_AVAILABLE_IN_ALL
GdkPixbuf*  gdk_cursor_get_image         (GdkCursor       *cursor);
GDK_AVAILABLE_IN_3_10
cairo_surface_t *gdk_cursor_get_surface  (GdkCursor       *cursor,
					  gdouble         *x_hot,
					  gdouble         *y_hot);
GDK_AVAILABLE_IN_ALL
GdkCursorType gdk_cursor_get_cursor_type (GdkCursor       *cursor);


G_END_DECLS

#endif /* __GDK_CURSOR_H__ */
