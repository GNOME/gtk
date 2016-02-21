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
#include <cairo.h>

#include <gtk/gtkborder.h>

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

enum {
  GTK_WIN32_SYS_METRIC_CXSCREEN = 0,
  GTK_WIN32_SYS_METRIC_CYSCREEN = 1,
  GTK_WIN32_SYS_METRIC_CXVSCROLL = 2,
  GTK_WIN32_SYS_METRIC_CYHSCROLL = 3,
  GTK_WIN32_SYS_METRIC_CYCAPTION = 4,
  GTK_WIN32_SYS_METRIC_CXBORDER = 5,
  GTK_WIN32_SYS_METRIC_CYBORDER = 6,
  GTK_WIN32_SYS_METRIC_CXDLGFRAME = 7,
  GTK_WIN32_SYS_METRIC_CYDLGFRAME = 8,
  GTK_WIN32_SYS_METRIC_CYVTHUMB = 9,
  GTK_WIN32_SYS_METRIC_CXHTHUMB = 10,
  GTK_WIN32_SYS_METRIC_CXICON = 11,
  GTK_WIN32_SYS_METRIC_CYICON = 12,
  GTK_WIN32_SYS_METRIC_CXCURSOR = 13,
  GTK_WIN32_SYS_METRIC_CYCURSOR = 14,
  GTK_WIN32_SYS_METRIC_CYMENU = 15,
  GTK_WIN32_SYS_METRIC_CXFULLSCREEN = 16,
  GTK_WIN32_SYS_METRIC_CYFULLSCREEN = 17,
  GTK_WIN32_SYS_METRIC_CYKANJIWINDOW = 18,
  GTK_WIN32_SYS_METRIC_MOUSEPRESENT = 19,
  GTK_WIN32_SYS_METRIC_CYVSCROLL = 20,
  GTK_WIN32_SYS_METRIC_CXHSCROLL = 21,
  GTK_WIN32_SYS_METRIC_DEBUG = 22,
  GTK_WIN32_SYS_METRIC_SWAPBUTTON = 23,
  GTK_WIN32_SYS_METRIC_RESERVED1 = 24,
  GTK_WIN32_SYS_METRIC_RESERVED2 = 25,
  GTK_WIN32_SYS_METRIC_RESERVED3 = 26,
  GTK_WIN32_SYS_METRIC_RESERVED4 = 27,
  GTK_WIN32_SYS_METRIC_CXMIN = 28,
  GTK_WIN32_SYS_METRIC_CYMIN = 29,
  GTK_WIN32_SYS_METRIC_CXSIZE = 30,
  GTK_WIN32_SYS_METRIC_CYSIZE = 31,
  GTK_WIN32_SYS_METRIC_CXFRAME = 32,
  GTK_WIN32_SYS_METRIC_CYFRAME = 33,
  GTK_WIN32_SYS_METRIC_CXMINTRACK = 34,
  GTK_WIN32_SYS_METRIC_CYMINTRACK = 35,
  GTK_WIN32_SYS_METRIC_CXDOUBLECLK = 36,
  GTK_WIN32_SYS_METRIC_CYDOUBLECLK = 37,
  GTK_WIN32_SYS_METRIC_CXICONSPACING = 38,
  GTK_WIN32_SYS_METRIC_CYICONSPACING = 39,
  GTK_WIN32_SYS_METRIC_MENUDROPALIGNMENT = 40,
  GTK_WIN32_SYS_METRIC_PENWINDOWS = 41,
  GTK_WIN32_SYS_METRIC_DBCSENABLED = 42,
  GTK_WIN32_SYS_METRIC_CMOUSEBUTTONS = 43,
  GTK_WIN32_SYS_METRIC_SECURE = 44,
  GTK_WIN32_SYS_METRIC_CXEDGE = 45,
  GTK_WIN32_SYS_METRIC_CYEDGE = 46,
  GTK_WIN32_SYS_METRIC_CXMINSPACING = 47,
  GTK_WIN32_SYS_METRIC_CYMINSPACING = 48,
  GTK_WIN32_SYS_METRIC_CXSMICON = 49,
  GTK_WIN32_SYS_METRIC_CYSMICON = 50,
  GTK_WIN32_SYS_METRIC_CYSMCAPTION = 51,
  GTK_WIN32_SYS_METRIC_CXSMSIZE = 52,
  GTK_WIN32_SYS_METRIC_CYSMSIZE = 53,
  GTK_WIN32_SYS_METRIC_CXMENUSIZE = 54,
  GTK_WIN32_SYS_METRIC_CYMENUSIZE = 55,
  GTK_WIN32_SYS_METRIC_ARRANGE = 56,
  GTK_WIN32_SYS_METRIC_CXMINIMIZED = 57,
  GTK_WIN32_SYS_METRIC_CYMINIMIZED = 58,
  GTK_WIN32_SYS_METRIC_CXMAXTRACK = 59,
  GTK_WIN32_SYS_METRIC_CYMAXTRACK = 60,
  GTK_WIN32_SYS_METRIC_CXMAXIMIZED = 61,
  GTK_WIN32_SYS_METRIC_CYMAXIMIZED = 62,
  GTK_WIN32_SYS_METRIC_NETWORK = 63,
  GTK_WIN32_SYS_METRIC_CLEANBOOT = 67,
  GTK_WIN32_SYS_METRIC_CXDRAG = 68,
  GTK_WIN32_SYS_METRIC_CYDRAG = 69,
  GTK_WIN32_SYS_METRIC_SHOWSOUNDS = 70,
  GTK_WIN32_SYS_METRIC_CXMENUCHECK = 71,
  GTK_WIN32_SYS_METRIC_CYMENUCHECK = 72,
  GTK_WIN32_SYS_METRIC_SLOWMACHINE = 73,
  GTK_WIN32_SYS_METRIC_MIDEASTENABLED = 74,
  GTK_WIN32_SYS_METRIC_MOUSEWHEELPRESENT = 75,
  GTK_WIN32_SYS_METRIC_XVIRTUALSCREEN = 76,
  GTK_WIN32_SYS_METRIC_YVIRTUALSCREEN = 77,
  GTK_WIN32_SYS_METRIC_CXVIRTUALSCREEN = 78,
  GTK_WIN32_SYS_METRIC_CYVIRTUALSCREEN = 79,
  GTK_WIN32_SYS_METRIC_CMONITORS = 80,
  GTK_WIN32_SYS_METRIC_SAMEDISPLAYFORMAT = 81,
  GTK_WIN32_SYS_METRIC_IMMENABLED = 82,
  GTK_WIN32_SYS_METRIC_CXFOCUSBORDER = 83,
  GTK_WIN32_SYS_METRIC_CYFOCUSBORDER = 84,
  GTK_WIN32_SYS_METRIC_TABLETPC = 86,
  GTK_WIN32_SYS_METRIC_MEDIACENTER = 87,
  GTK_WIN32_SYS_METRIC_STARTER = 88,
  GTK_WIN32_SYS_METRIC_SERVERR2 = 89,
  GTK_WIN32_SYS_METRIC_CMETRICS = 90,
  GTK_WIN32_SYS_METRIC_MOUSEHORIZONTALWHEELPRESENT = 91,
  GTK_WIN32_SYS_METRIC_CXPADDEDBORDER = 92
};

void                    gtk_win32_draw_theme_background         (cairo_t        *cr,
                                                                 const char     *class_name,
                                                                 int             part,
                                                                 int             state,
                                                                 int             width,
                                                                 int             height);
void                    gtk_win32_get_theme_part_size           (const char     *class_name,
                                                                 int             part,
                                                                 int             state,
                                                                 int            *width,
                                                                 int            *height);
void                    gtk_win32_get_theme_margins             (const char     *class_name,
                                                                 int             part,
                                                                 int             state,
                                                                 GtkBorder      *out_margins);

const char *            gtk_win32_get_sys_metric_name_for_id    (gint            id);
int                     gtk_win32_get_sys_metric_id_for_name    (const char     *name);
int                     gtk_win32_get_sys_metric                (gint            id);

const char *            gtk_win32_get_sys_color_name_for_id     (gint            id);
int                     gtk_win32_get_sys_color_id_for_name     (const char     *name);
void                    gtk_win32_get_sys_color                 (gint            id,
                                                                 GdkRGBA        *color);

G_END_DECLS

#endif /* __GTK_WIN32_DRAW_H__ */
