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

#include <config.h>

#include "gtkwin32drawprivate.h"

static void
gtk_cairo_set_source_sys_color (cairo_t *cr,
                                gint     id)
{
  GdkRGBA rgba;

  gtk_win32_get_sys_color (id, &rgba);
  gdk_cairo_set_source_rgba (cr, &rgba);
}

static void
draw_button (cairo_t *cr,
             int      part,
             int      state,
             int      width,
             int      height)
{
  gboolean is_down = (state == 3);
  int top_color = is_down ? GTK_WIN32_SYS_COLOR_BTNSHADOW : GTK_WIN32_SYS_COLOR_BTNHIGHLIGHT;
  int bot_color = is_down ? GTK_WIN32_SYS_COLOR_BTNHIGHLIGHT : GTK_WIN32_SYS_COLOR_BTNSHADOW;

  gtk_cairo_set_source_sys_color (cr, top_color);
  cairo_rectangle (cr, 0, 0, width - 1, 1);
  cairo_rectangle (cr, 0, 1, 1, height - 1);
  cairo_fill (cr);

  gtk_cairo_set_source_sys_color (cr, bot_color);
  cairo_rectangle (cr, width - 1, 0, 1, height -1);
  cairo_rectangle (cr, 0, height - 1, width, 1);
  cairo_fill (cr);

  gtk_cairo_set_source_sys_color (cr, GTK_WIN32_SYS_COLOR_BTNFACE);
  cairo_rectangle (cr, 1, 1, width - 2, height - 2);
  cairo_fill (cr);
}
            
static void
draw_check (cairo_t *cr,
            int      part,
            int      state,
            int      width,
            int      height)
{
  gtk_cairo_set_source_sys_color (cr, GTK_WIN32_SYS_COLOR_BTNHIGHLIGHT);
  cairo_set_line_width (cr, 1.0);
  cairo_rectangle (cr, 0.5, 0.5, width - 1.0, height - 1.0);
  cairo_stroke (cr);
}

static void
draw_radio (cairo_t *cr,
            int      part,
            int      state,
            int      width,
            int      height)
{
  gtk_cairo_set_source_sys_color (cr, GTK_WIN32_SYS_COLOR_BTNHIGHLIGHT);
  cairo_set_line_width (cr, 1.0);
  cairo_arc (cr, width / 2.0, height / 2.0, MIN (width, height) / 2.0 - 0.5, 0, G_PI * 2);
  cairo_stroke (cr);
}

typedef struct _GtkWin32ThemePart GtkWin32ThemePart;
struct _GtkWin32ThemePart {
  const char *class_name;
  gint        part;
  gint        size;
  void        (* draw_func)             (cairo_t        *cr,
                                         int             part,
                                         int             state,
                                         int             width,
                                         int             height);
};

static GtkWin32ThemePart theme_parts[] = {
  { "button", 1,  0, draw_button },
  { "button", 2, 13, draw_radio },
  { "button", 3, 13, draw_check }
};

static const GtkWin32ThemePart *
get_theme_part (const char *class_name,
                gint        part)
{
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (theme_parts); i++)
    {
      if (g_str_equal (theme_parts[i].class_name, class_name) &&
          theme_parts[i].part == part)
        return &theme_parts[i];
    }

  return NULL;
}

void
gtk_win32_draw_theme_background (cairo_t    *cr,
                                 const char *class_name,
                                 int         part,
                                 int         state,
                                 int         width,
                                 int         height)
{
  const GtkWin32ThemePart *theme_part;

  theme_part = get_theme_part (class_name, part);

  if (theme_part)
    theme_part->draw_func (cr, part, state, width, height);
}

void
gtk_win32_get_theme_part_size (const char *class_name,
                               int          part,
                               int          state,
                               int         *width,
                               int         *height)
{
  const GtkWin32ThemePart *theme_part;

  theme_part = get_theme_part (class_name, part);

  if (theme_part)
    {
      if (width)
        *width = theme_part->size;
      if (height)
        *height = theme_part->size;
    }
  else
    {
      if (width)
        *width = 1;
      if (height)
        *height = 1;
    }
}

struct {
  const char *name;
  GdkRGBA rgba;
} win32_default_colors[] = {
#define RGB(r, g, b) { (r)/255.0, (g)/255.0, (b)/255., 1.0 }
  { "scrollbar", RGB(212, 208, 200) },
  { "background", RGB(58, 110, 165) },
  { "activecaption", RGB(10, 36, 106) },
  { "inactivecaption", RGB(128, 128, 128) },
  { "menu", RGB(212, 208, 200) },
  { "window", RGB(255, 255, 255) },
  { "windowframe", RGB(0, 0, 0) },
  { "menutext", RGB(0, 0, 0) },
  { "windowtext", RGB(0, 0, 0) },
  { "captiontext", RGB(255, 255, 255) },
  { "activeborder", RGB(212, 208, 200) },
  { "inactiveborder", RGB(212, 208, 200) },
  { "appworkspace", RGB(128, 128, 128) },
  { "highlight", RGB(10, 36, 106) },
  { "highlighttext", RGB(255, 255, 255) },
  { "btnface", RGB(212, 208, 200) },
  { "btnshadow", RGB(128, 128, 128) },
  { "graytext", RGB(128, 128, 128) },
  { "btntext", RGB(0, 0, 0) },
  { "inactivecaptiontext", RGB(212, 208, 200) },
  { "btnhighlight", RGB(255, 255, 255) },
  { "3ddkshadow", RGB(64, 64, 64) },
  { "3dlight", RGB(212, 208, 200) },
  { "infotext", RGB(0, 0, 0) },
  { "infobk", RGB(255, 255, 225) },
  { "alternatebtnface", RGB(181, 181, 181) },
  { "hotlight", RGB(0, 0, 200) },
  { "gradientactivecaption", RGB(166, 202, 240) },
  { "gradientinactivecaption", RGB(192, 192, 192) },
  { "menuhilight", RGB(10, 36, 106) },
  { "menubar", RGB(212, 208, 200) }
#undef RGB
};

void
gtk_win32_get_sys_color (gint     id,
                         GdkRGBA *color)
{
  if (id < G_N_ELEMENTS (win32_default_colors))
    *color = win32_default_colors[id].rgba;
  else
    gdk_rgba_parse (color, "black");
}

