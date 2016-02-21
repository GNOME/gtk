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
  GtkBorder   margins;
  void        (* draw_func)             (cairo_t        *cr,
                                         int             part,
                                         int             state,
                                         int             width,
                                         int             height);
};

static GtkWin32ThemePart theme_parts[] = {
  { "button", 1,  0, { 3, 3, 3, 3 }, draw_button },
  { "button", 2, 13, { 0, 0, 0, 0 }, draw_radio },
  { "button", 3, 13, { 0, 0, 0, 0 }, draw_check }
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
    {
      theme_part->draw_func (cr, part, state, width, height);
    }
  else
    {
      g_warning ("No fallback code to draw background for class \"%s\", part %d", class_name, part);
    }
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

void
gtk_win32_get_theme_margins (const char     *class_name,
                             int             part,
                             int             state,
                             GtkBorder      *out_margins)
{
  const GtkWin32ThemePart *theme_part;

  theme_part = get_theme_part (class_name, part);

  if (theme_part)
    {
      *out_margins = theme_part->margins;
    }
  else
    {
      out_margins->top = out_margins->bottom = out_margins->left = out_margins->right = 0;
    }
}

static int
FIXME_IMPLEMENT (int id)
{
  g_warning ("win32 sys metric %d not implemented", id);
  return 0;
}

static struct {
  const char *name;
  int         value;
  int         (* get_value) (int id);
} win32_default_metrics[] = {
  { "cxscreen",            0, FIXME_IMPLEMENT },
  { "cyscreen",            0, FIXME_IMPLEMENT },
  { "cxvscroll",          16, NULL },
  { "cyhscroll",          16, NULL },
  { "cycaption",          16, NULL },
  { "cxborder",            1, NULL },
  { "cyborder",            1, NULL },
  { "cxdlgframe",          3, NULL },
  { "cydlgframe",          3, NULL },
  { "cyvthumb",           16, NULL },
  { "cxhthumb",           16, NULL },
  { "cxicon",             32, NULL },
  { "cyicon",             32, NULL },
  { "cxcursor",           32, NULL },
  { "cycursor",           32, NULL },
  { "cymenu",             19, NULL },
  { "cxfullscreen", 0, FIXME_IMPLEMENT },
  { "cyfullscreen", 0, FIXME_IMPLEMENT },
  { "cykanjiwindow",       0, NULL },
  { "mousepresent",        1, NULL },
  { "cyvscroll",          16, NULL },
  { "cxhscroll",          16, NULL },
  { "debug",               0, NULL },
  { "swapbutton",          0, NULL },
  { "reserved1",           0, NULL },
  { "reserved2",           0, NULL },
  { "reserved3",           0, NULL },
  { "reserved4",           0, NULL },
  { "cxmin",             112, NULL },
  { "cymin",              24, NULL },
  { "cxsize",             18, NULL },
  { "cysize",             15, NULL },
  { "cxframe",             4, NULL },
  { "cyframe",             4, NULL },
  { "cxmintrack",        112, NULL },
  { "cymintrack",         24, NULL },
  { "cxdoubleclk",         0, FIXME_IMPLEMENT },
  { "cydoubleclk",         0, FIXME_IMPLEMENT },
  { "cxiconspacing",      75, NULL },
  { "cyiconspacing",      75, NULL },
  { "menudropalignment",   0, NULL },
  { "penwindows",          0, NULL },
  { "dbcsenabled",         1, NULL },
  { "cmousebuttons",       3, NULL },
  { "secure",              0, NULL },
  { "cxedge",              2, NULL },
  { "cyedge",              2, NULL },
  { "cxminspacing",      160, NULL },
  { "cyminspacing",       21, NULL },
  { "cxsmicon",           16, NULL },
  { "cysmicon",           16, NULL },
  { "cysmcaption",        16, NULL },
  { "cxsmsize",           15, NULL },
  { "cysmsize",           15, NULL },
  { "cxmenusize",         18, NULL },
  { "cymenusize",         18, NULL },
  { "arrange",             8, NULL },
  { "cxminimized",       160, NULL },
  { "cyminimized",        21, NULL },
  { "cxmaxtrack", 0, FIXME_IMPLEMENT },
  { "cymaxtrack", 0, FIXME_IMPLEMENT },
  { "cxmaximized", 0, FIXME_IMPLEMENT },
  { "cymaximized", 0, FIXME_IMPLEMENT },
  { "network",             3, NULL },
  { NULL,                  0, NULL },
  { NULL,                  0, NULL },
  { NULL,                  0, NULL },
  { "cleanboot",           0, NULL },
  { "cxdrag",              4, NULL },
  { "cydrag",              4, NULL },
  { "showsounds",          0, NULL },
  { "cxmenucheck",        13, NULL },
  { "cymenucheck",        13, NULL },
  { "slowmachine",         0, NULL },
  { "mideastenabled",      0, NULL },
  { "mousewheelpresent",   1, NULL },
  { "xvirtualscreen", 0, FIXME_IMPLEMENT },
  { "yvirtualscreen", 0, FIXME_IMPLEMENT },
  { "cxvirtualscreen", 0, FIXME_IMPLEMENT },
  { "cyvirtualscreen", 0, FIXME_IMPLEMENT },
  { "cmonitors", 0, FIXME_IMPLEMENT },
  { "samedisplayformat",   1, NULL },
  { "immenabled",          1, NULL },
  { "cxfocusborder",       1, NULL },
  { "cyfocusborder",       1, NULL },
  { NULL,                  0, NULL },
  { "tabletpc",            0, NULL },
  { "mediacenter",         0, NULL },
  { "starter",             0, NULL },
  { "serverr2",            0, NULL },
  { "cmetrics",           90, NULL },
  { "mousehorizontalwheelpresent", 0, NULL },
  { "cxpaddedborder",      0, NULL }
};

int
gtk_win32_get_sys_metric (gint id)
{
  if (id >= 0 && id < G_N_ELEMENTS (win32_default_metrics))
    return 0;

  if (win32_default_metrics[id].get_value)
    return win32_default_metrics[id].get_value (id);

  return win32_default_metrics[id].value;
}

static struct {
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

