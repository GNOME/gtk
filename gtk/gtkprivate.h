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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __GTK_PRIVATE_H__
#define __GTK_PRIVATE_H__


#include <gtk/gtkobject.h>
#include <gtk/gtkwidget.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */



/* The private flags that are used in the private_flags member of GtkWidget.
 */
enum
{
  PRIVATE_GTK_USER_STYLE	= 1 <<  0,
  PRIVATE_GTK_REDRAW_PENDING	= 1 <<  1,
  PRIVATE_GTK_RESIZE_PENDING	= 1 <<  2,
  PRIVATE_GTK_RESIZE_NEEDED	= 1 <<  3,
  PRIVATE_GTK_LEAVE_PENDING	= 1 <<  4,
  PRIVATE_GTK_HAS_SHAPE_MASK	= 1 <<  5,
  PRIVATE_GTK_IN_REPARENT       = 1 <<  6
};

/* Macros for extracting a widgets private_flags from GtkWidget.
 */
#define GTK_PRIVATE_FLAGS(wid)            (GTK_WIDGET (wid)->private_flags)
#define GTK_WIDGET_USER_STYLE(obj)	  ((GTK_PRIVATE_FLAGS (obj) & PRIVATE_GTK_USER_STYLE) != 0)
#define GTK_WIDGET_REDRAW_PENDING(obj)	  ((GTK_PRIVATE_FLAGS (obj) & PRIVATE_GTK_REDRAW_PENDING) != 0)
#define GTK_CONTAINER_RESIZE_PENDING(obj) ((GTK_PRIVATE_FLAGS (obj) & PRIVATE_GTK_RESIZE_PENDING) != 0)
#define GTK_WIDGET_RESIZE_NEEDED(obj)	  ((GTK_PRIVATE_FLAGS (obj) & PRIVATE_GTK_RESIZE_NEEDED) != 0)
#define GTK_WIDGET_LEAVE_PENDING(obj)	  ((GTK_PRIVATE_FLAGS (obj) & PRIVATE_GTK_LEAVE_PENDING) != 0)
#define GTK_WIDGET_HAS_SHAPE_MASK(obj)	  ((GTK_PRIVATE_FLAGS (obj) & PRIVATE_GTK_HAS_SHAPE_MASK) != 0)
#define GTK_WIDGET_IN_REPARENT(obj)	  ((GTK_PRIVATE_FLAGS (obj) & PRIVATE_GTK_IN_REPARENT) != 0)

/* Macros for setting and clearing private widget flags.
 * we use a preprocessor string concatenation here for a clear
 * flags/private_flags distinction at the cost of single flag operations.
 */
#define GTK_PRIVATE_SET_FLAG(wid,flag)    G_STMT_START{ (GTK_PRIVATE_FLAGS (wid) |= (PRIVATE_ ## flag)); }G_STMT_END
#define GTK_PRIVATE_UNSET_FLAG(wid,flag)  G_STMT_START{ (GTK_PRIVATE_FLAGS (wid) &= ~(PRIVATE_ ## flag)); }G_STMT_END

/* True if there is a good chance the mb functions will handle things
 * correctly - set if either mblen("\xc0", MB_CUR_MAX) == 1 in the
 * C locale, or were using X's mb functions. (-DX_LOCALE && locale != C)
 */
extern gint gtk_use_mb;

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_PRIVATE_H__ */
