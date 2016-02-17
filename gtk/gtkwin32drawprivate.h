/* GTK - The GIMP Toolkit
 * Copyright (C) 2016 Benjamin Otte <otte@gnome.org>
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

#ifndef __GTK_WIN32_DRAW_H__
#define __GTK_WIN32_DRAW_H__

#include <gdk/gdk.h>

G_BEGIN_DECLS

enum {
  GTK_WIN32_SYS_COLOR_SCROLLBAR,
  GTK_WIN32_SYS_COLOR_BACKGROUND,
  GTK_WIN32_SYS_COLOR_ACTIVECAPTION,
  GTK_WIN32_SYS_COLOR_INACTIVECAPTION,
  GTK_WIN32_SYS_COLOR_MENU,
  GTK_WIN32_SYS_COLOR_WINDOW,
  GTK_WIN32_SYS_COLOR_WINDOWFRAME,
  GTK_WIN32_SYS_COLOR_MENUTEXT,
  GTK_WIN32_SYS_COLOR_WINDOWTEXT,
  GTK_WIN32_SYS_COLOR_CAPTIONTEXT,
  GTK_WIN32_SYS_COLOR_ACTIVEBORDER,
  GTK_WIN32_SYS_COLOR_INACTIVEBORDER,
  GTK_WIN32_SYS_COLOR_APPWORKSPACE,
  GTK_WIN32_SYS_COLOR_HIGHLIGHT,
  GTK_WIN32_SYS_COLOR_HIGHLIGHTTEXT,
  GTK_WIN32_SYS_COLOR_BTNFACE,
  GTK_WIN32_SYS_COLOR_BTNSHADOW,
  GTK_WIN32_SYS_COLOR_GRAYTEXT,
  GTK_WIN32_SYS_COLOR_BTNTEXT,
  GTK_WIN32_SYS_COLOR_INACTIVECAPTIONTEXT,
  GTK_WIN32_SYS_COLOR_BTNHIGHLIGHT,
  GTK_WIN32_SYS_COLOR_3DDKSHADOW,
  GTK_WIN32_SYS_COLOR_3DLIGHT,
  GTK_WIN32_SYS_COLOR_INFOTEXT,
  GTK_WIN32_SYS_COLOR_INFOBK,
  GTK_WIN32_SYS_COLOR_ALTERNATEBTNFACE,
  GTK_WIN32_SYS_COLOR_HOTLIGHT,
  GTK_WIN32_SYS_COLOR_GRADIENTACTIVECAPTION,
  GTK_WIN32_SYS_COLOR_GRADIENTINACTIVECAPTION,
  GTK_WIN32_SYS_COLOR_MENUHILIGHT,
  GTK_WIN32_SYS_COLOR_MENUBAR
};

void                    gtk_win32_get_sys_color         (gint            id,
                                                         GdkRGBA        *color);

G_END_DECLS

#endif /* __GTK_WIN32_DRAW_H__ */
