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

