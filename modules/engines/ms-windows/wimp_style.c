/* Wimp "Windows Impersonator" Engine
 *
 * Copyright (C) 2003 Raymond Penners <raymond@dotsphinx.com>
 * Includes code adapted from redmond95 by Owen Taylor, and
 * gtk-nativewin by Evan Martin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "wimp_style.h"
#include "xp_theme.h"

#include <windows.h>
#include <math.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gtk/gtk.h>
#include <gdk/gdkwin32.h>

#include <stdio.h>

/* Default values, not normally used
 */
static GtkRequisition default_option_indicator_size = { 9, 8 };
static GtkBorder default_option_indicator_spacing = { 7, 5, 2, 2 };

static GtkStyleClass *parent_class;

typedef enum {
  CHECK_AA,
  CHECK_BASE,
  CHECK_BLACK,
  CHECK_DARK,
  CHECK_LIGHT,
  CHECK_MID,
  CHECK_TEXT,
  RADIO_BASE,
  RADIO_BLACK,
  RADIO_DARK,
  RADIO_LIGHT,
  RADIO_MID,
  RADIO_TEXT
} Part;

#define PART_SIZE 13

static const char check_aa_bits[] = {
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static const char check_base_bits[] = {
 0x00,0x00,0x00,0x00,0xfc,0x07,0xfc,0x07,0xfc,0x07,0xfc,0x07,0xfc,0x07,0xfc,
 0x07,0xfc,0x07,0xfc,0x07,0xfc,0x07,0x00,0x00,0x00,0x00};
static const char check_black_bits[] = {
 0x00,0x00,0xfe,0x0f,0x02,0x00,0x02,0x00,0x02,0x00,0x02,0x00,0x02,0x00,0x02,
 0x00,0x02,0x00,0x02,0x00,0x02,0x00,0x02,0x00,0x00,0x00};
static const char check_dark_bits[] = {
 0xff,0x1f,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,
 0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00};
static const char check_light_bits[] = {
 0x00,0x00,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,
 0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0xfe,0x1f};
static const char check_mid_bits[] = {
 0x00,0x00,0x00,0x00,0x00,0x08,0x00,0x08,0x00,0x08,0x00,0x08,0x00,0x08,0x00,
 0x08,0x00,0x08,0x00,0x08,0x00,0x08,0xfc,0x0f,0x00,0x00};
static const char check_text_bits[] = {
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x03,0x88,0x03,0xd8,0x01,0xf8,
 0x00,0x70,0x00,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static const char radio_base_bits[] = {
 0x00,0x00,0x00,0x00,0xf0,0x01,0xf8,0x03,0xfc,0x07,0xfc,0x07,0xfc,0x07,0xfc,
 0x07,0xfc,0x07,0xf8,0x03,0xf0,0x01,0x00,0x00,0x00,0x00};
static const char radio_black_bits[] = {
 0x00,0x00,0xf0,0x01,0x0c,0x02,0x04,0x00,0x02,0x00,0x02,0x00,0x02,0x00,0x02,
 0x00,0x02,0x00,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static const char radio_dark_bits[] = {
 0xf0,0x01,0x0c,0x06,0x02,0x00,0x02,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,
 0x00,0x01,0x00,0x02,0x00,0x02,0x00,0x00,0x00,0x00,0x00};
static const char radio_light_bits[] = {
 0x00,0x00,0x00,0x00,0x00,0x08,0x00,0x08,0x00,0x10,0x00,0x10,0x00,0x10,0x00,
 0x10,0x00,0x10,0x00,0x08,0x00,0x08,0x0c,0x06,0xf0,0x01};
static const char radio_mid_bits[] = {
 0x00,0x00,0x00,0x00,0x00,0x04,0x00,0x04,0x00,0x08,0x00,0x08,0x00,0x08,0x00,
 0x08,0x00,0x08,0x00,0x04,0x0c,0x06,0xf0,0x01,0x00,0x00};
static const char radio_text_bits[] = {
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xe0,0x00,0xf0,0x01,0xf0,0x01,0xf0,
 0x01,0xe0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

static struct {
  const char *bits;
  GdkBitmap  *bmap;
} parts[] = {
  { check_aa_bits, NULL },
  { check_base_bits, NULL },
  { check_black_bits, NULL },
  { check_dark_bits, NULL },
  { check_light_bits, NULL },
  { check_mid_bits, NULL },
  { check_text_bits, NULL },
  { radio_base_bits, NULL },
  { radio_black_bits, NULL },
  { radio_dark_bits, NULL },
  { radio_light_bits, NULL },
  { radio_mid_bits, NULL },
  { radio_text_bits, NULL }
};

typedef enum
{
	CAPTION_FONT,
	MENU_FONT,
	STATUS_FONT,
	MESSAGE_FONT
} SystemFontType;

static gboolean
get_system_font(SystemFontType type, LOGFONT *out_lf)
{
  gboolean ok;

  NONCLIENTMETRICS ncm;
  ncm.cbSize = sizeof(NONCLIENTMETRICS);
  ok = SystemParametersInfo(SPI_GETNONCLIENTMETRICS,
			    sizeof(NONCLIENTMETRICS), &ncm, 0);
  if (ok)
    {
		if (type == CAPTION_FONT)
			*out_lf = ncm.lfCaptionFont;
		else if (type == MENU_FONT)
			*out_lf = ncm.lfMenuFont;
		else if (type == STATUS_FONT)
			*out_lf = ncm.lfStatusFont;
		else
	      *out_lf = ncm.lfMessageFont;
    }

  return ok;
}

static char *
sys_font_to_pango_font (SystemFontType type, char * buf)
{
  LOGFONT lf;
  int pt_size;
  const char * weight;
  const char * style;

  if (get_system_font(type, &lf))
    {
      switch (lf.lfWeight) {
      case FW_THIN:
      case FW_EXTRALIGHT:
	weight = "Ultra-Light";
	break;

      case FW_LIGHT:
	weight = "Light";
	break;

      case FW_BOLD:
	weight = "Bold";
	break;

      case FW_SEMIBOLD:
	weight = "Semi-Bold";
	break;

      case FW_ULTRABOLD:
	weight = "Ultra-Bold";
	break;

      case FW_HEAVY:
	weight = "Heavy";
	break;

      default:
	weight = "";
	break;
      }

      if (lf.lfItalic)
	style="Italic";
      else
	style="";

      pt_size = -MulDiv(lf.lfHeight, 72,
                        GetDeviceCaps(GetDC(GetDesktopWindow()),
                                      LOGPIXELSY));
      sprintf(buf, "%s %s %s %d", lf.lfFaceName, style, weight, pt_size);

      return buf;
    }

  return NULL;
}

static void
setup_system_font(GtkStyle *style)
{
  char buf[256], * font; /* It's okay, lfFaceName is smaller than 32 chars */

  if ((font = sys_font_to_pango_font(MESSAGE_FONT, buf)) != NULL)
    style->font_desc = pango_font_description_from_string(font);
}

static void
sys_color_to_gtk_color(int id, GdkColor *pcolor)
{
  DWORD color   = GetSysColor(id);
  pcolor->pixel = color;
  pcolor->red   = (GetRValue(color) << 8) | GetRValue(color);
  pcolor->green = (GetGValue(color) << 8) | GetGValue(color);
  pcolor->blue  = (GetBValue(color) << 8) | GetBValue(color);
}

static void
setup_system_styles(GtkStyle *style)
{
  char buf[1024], font_buf[256], *font_ptr;
  GdkColor menu_color;
  GdkColor menu_text_color;
  GdkColor fg_prelight;
  GdkColor bg_prelight;
  GdkColor base_prelight;
  GdkColor text_prelight;
  GdkColor tooltip_back;
  GdkColor tooltip_fore;
  GdkColor btn_fore;
  GdkColor progress_back;

  NONCLIENTMETRICS nc;
  gint paned_size = 15;

  int i;

  /* Default forgeground */
  sys_color_to_gtk_color(COLOR_WINDOWTEXT, &style->fg[GTK_STATE_NORMAL]);
  sys_color_to_gtk_color(COLOR_WINDOWTEXT, &style->fg[GTK_STATE_ACTIVE]);
  sys_color_to_gtk_color(COLOR_WINDOWTEXT, &style->fg[GTK_STATE_PRELIGHT]);
  sys_color_to_gtk_color(COLOR_HIGHLIGHTTEXT, &style->fg[GTK_STATE_SELECTED]);
  sys_color_to_gtk_color(COLOR_GRAYTEXT, &style->fg[GTK_STATE_INSENSITIVE]);

  /* Default background */
  sys_color_to_gtk_color(COLOR_3DFACE, &style->bg[GTK_STATE_NORMAL]);
  sys_color_to_gtk_color(COLOR_SCROLLBAR, &style->bg[GTK_STATE_ACTIVE]);
  sys_color_to_gtk_color(COLOR_3DFACE, &style->bg[GTK_STATE_PRELIGHT]);
  sys_color_to_gtk_color(COLOR_HIGHLIGHT, &style->bg[GTK_STATE_SELECTED]);
  sys_color_to_gtk_color(COLOR_3DFACE, &style->bg[GTK_STATE_INSENSITIVE]);

  /* Default base */
  sys_color_to_gtk_color(COLOR_WINDOW, &style->base[GTK_STATE_NORMAL]);
  sys_color_to_gtk_color(COLOR_HIGHLIGHT, &style->base[GTK_STATE_ACTIVE]);
  sys_color_to_gtk_color(COLOR_WINDOW, &style->base[GTK_STATE_PRELIGHT]);
  sys_color_to_gtk_color(COLOR_HIGHLIGHT, &style->base[GTK_STATE_SELECTED]);
  sys_color_to_gtk_color(COLOR_3DFACE, &style->base[GTK_STATE_INSENSITIVE]);

  /* Default text */
  sys_color_to_gtk_color(COLOR_WINDOWTEXT, &style->text[GTK_STATE_NORMAL]);
  sys_color_to_gtk_color(COLOR_HIGHLIGHTTEXT, &style->text[GTK_STATE_ACTIVE]);
  sys_color_to_gtk_color(COLOR_WINDOWTEXT, &style->text[GTK_STATE_PRELIGHT]);
  sys_color_to_gtk_color(COLOR_HIGHLIGHTTEXT, &style->text[GTK_STATE_SELECTED]);
  sys_color_to_gtk_color(COLOR_GRAYTEXT, &style->text[GTK_STATE_INSENSITIVE]);

  /* Prelight */
  sys_color_to_gtk_color(COLOR_HIGHLIGHTTEXT, &fg_prelight);
  sys_color_to_gtk_color(COLOR_HIGHLIGHT, &bg_prelight);
  sys_color_to_gtk_color(COLOR_HIGHLIGHT, &base_prelight);
  sys_color_to_gtk_color(COLOR_HIGHLIGHTTEXT, &text_prelight);

  sys_color_to_gtk_color(COLOR_MENUTEXT, &menu_text_color);
  sys_color_to_gtk_color(COLOR_MENU, &menu_color);

  /* tooltips */
  sys_color_to_gtk_color(COLOR_INFOTEXT, &tooltip_fore);
  sys_color_to_gtk_color(COLOR_INFOBK, &tooltip_back);

  /* text on push buttons. TODO: button shadows, backgrounds, and highlights */
  sys_color_to_gtk_color(COLOR_BTNTEXT, &btn_fore);

  /* progress bar background color */
  sys_color_to_gtk_color(COLOR_HIGHLIGHT, &progress_back);

  for (i = 0; i < 5; i++)
    {
      sys_color_to_gtk_color(COLOR_3DSHADOW, &style->dark[i]);
      sys_color_to_gtk_color(COLOR_3DHILIGHT, &style->light[i]);

      style->mid[i].red = (style->light[i].red + style->dark[i].red) / 2;
      style->mid[i].green = (style->light[i].green + style->dark[i].green) / 2;
      style->mid[i].blue = (style->light[i].blue + style->dark[i].blue) / 2;

      style->text_aa[i].red = (style->text[i].red + style->base[i].red) / 2;
      style->text_aa[i].green = (style->text[i].green + style->base[i].green) / 2;
      style->text_aa[i].blue = (style->text[i].blue + style->base[i].blue) / 2;
    }

  /* Enable coloring for menus. */
  font_ptr = sys_font_to_pango_font (MENU_FONT,font_buf);
  sprintf(buf, "style \"wimp-menu\" = \"wimp-default\"\n"
          "{fg[PRELIGHT] = { %d, %d, %d }\n"
          "bg[PRELIGHT] = { %d, %d, %d }\n"
          "text[PRELIGHT] = { %d, %d, %d }\n"
          "base[PRELIGHT] = { %d, %d, %d }\n"
          "fg[NORMAL] = { %d, %d, %d }\n"
          "bg[NORMAL] = { %d, %d, %d }\n"
          "%s = \"%s\"\n"
          "}widget_class \"*GtkMenu*\" style \"wimp-menu\"\n",
          fg_prelight.red,
          fg_prelight.green,
          fg_prelight.blue,
          bg_prelight.red,
          bg_prelight.green,
          bg_prelight.blue,
          text_prelight.red,
          text_prelight.green,
          text_prelight.blue,
          base_prelight.red,
          base_prelight.green,
          base_prelight.blue,
          menu_text_color.red,
          menu_text_color.green,
          menu_text_color.blue,
          menu_color.red,
          menu_color.green,
          menu_color.blue,
          (font_ptr ? "font_name" : "#"),
          (font_ptr ? font_ptr : " font name should go here"));
  gtk_rc_parse_string(buf);

  /* enable tooltip fonts */
  font_ptr = sys_font_to_pango_font (STATUS_FONT,font_buf);
  sprintf(buf, "style \"wimp-tooltips-caption\" = \"wimp-default\"\n"
	  "{fg[NORMAL] = { %d, %d, %d }\n"
	  "%s = \"%s\"\n"
	  "}widget \"gtk-tooltips.GtkLabel\" style \"wimp-tooltips-caption\"\n",
          tooltip_fore.red,
          tooltip_fore.green,
          tooltip_fore.blue,
          (font_ptr ? "font_name" : "#"),
          (font_ptr ? font_ptr : " font name should go here"));
  gtk_rc_parse_string(buf);

  sprintf(buf, "style \"wimp-tooltips\" = \"wimp-default\"\n"
	  "{bg[NORMAL] = { %d, %d, %d }\n"
	  "}widget \"gtk-tooltips*\" style \"wimp-tooltips\"\n",
          tooltip_back.red,
          tooltip_back.green,
          tooltip_back.blue);
  gtk_rc_parse_string(buf);

  /* enable font theming for status bars */
  font_ptr = sys_font_to_pango_font (STATUS_FONT,font_buf);
  sprintf(buf, "style \"wimp-statusbar\" = \"wimp-default\"\n"
	  "{%s = \"%s\"\n"
	  "}widget_class \"*GtkStatusbar*\" style \"wimp-statusbar\"\n",
          (font_ptr ? "font_name" : "#"),
          (font_ptr ? font_ptr : " font name should go here"));
  gtk_rc_parse_string(buf);

  /* enable coloring for text on buttons
     TODO: use GetThemeMetric for the border and outside border */
  sprintf(buf, "style \"wimp-button\" = \"wimp-default\"\n"
	  "fg[NORMAL] = { %d, %d, %d }\n"
	  "default_border = { 1, 1, 1, 1 }\n"
	  "default_outside_border = { 0, 0, 0, 0 }\n"
	  "child_displacement_x = 1\n"
  	  "child_displacement_y = 1\n"
	  "}widget_class \"*GtkButton*\" style \"wimp-button\"\n",
	  btn_fore.red,
      btn_fore.green,
      btn_fore.blue);
  gtk_rc_parse_string(buf);

  /* enable coloring for progress bars */
  sprintf(buf, "style \"wimp-progress\" = \"wimp-default\"\n"
	  "{bg[PRELIGHT] = { %d, %d, %d }\n"
	  "}widget_class \"*GtkProgress*\" style \"wimp-progress\"\n",
	  progress_back.red,
      progress_back.green,
      progress_back.blue);
  gtk_rc_parse_string(buf);

  /* scrollbar thumb width and height */
  sprintf(buf, "style \"wimp-vscrollbar\" = \"wimp-default\"\n"
	  "{GtkRange::slider-width = %d\n"
	  "GtkRange::stepper-size = %d\n"
	  "GtkRange::stepper-spacing = 0\n"
	  "GtkRange::trough_border = 0\n"
	  "}widget_class \"*GtkVScrollbar*\" style \"wimp-vscrollbar\"\n",
	  GetSystemMetrics(SM_CYVTHUMB),
	  GetSystemMetrics(SM_CXVSCROLL));
  gtk_rc_parse_string(buf);

  sprintf(buf, "style \"wimp-hscrollbar\" = \"wimp-default\"\n"
	  "{GtkRange::slider-width = %d\n"
	  "GtkRange::stepper-size = %d\n"
	  "GtkRange::stepper-spacing = 0\n"
	  "GtkRange::trough_border = 0\n"
	  "}widget_class \"*GtkHScrollbar*\" style \"wimp-hscrollbar\"\n",
	  GetSystemMetrics(SM_CXHTHUMB),
	  GetSystemMetrics(SM_CYHSCROLL));
  gtk_rc_parse_string(buf);

  /* radio/check button sizes */
  sprintf(buf, "style \"wimp-checkbutton\" = \"wimp-default\"\n"
	  "{GtkCheckButton::indicator-size = 13\n"
	  "}widget_class \"*GtkCheckButton*\" style \"wimp-checkbutton\"\n");
  gtk_rc_parse_string(buf);

  /* the width/height of the paned resizer grippies */
  nc.cbSize = sizeof(nc);
  if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(nc), &nc, 0))
    paned_size = abs(nc.lfStatusFont.lfHeight) + 4;
  sprintf(buf, "style \"wimp-paned\" = \"wimp-default\"\n"
	  "{GtkPaned::handle-size = %d\n"
	  "}widget_class \"*GtkPaned*\" style \"wimp-paned\"\n", paned_size);
  gtk_rc_parse_string(buf);
}

static gboolean
sanitize_size (GdkWindow *window,
	       gint      *width,
	       gint      *height)
{
  gboolean set_bg = FALSE;

  if ((*width == -1) && (*height == -1))
    {
      set_bg = GDK_IS_WINDOW (window);
      gdk_window_get_size (window, width, height);
    }
  else if (*width == -1)
    gdk_window_get_size (window, width, NULL);
  else if (*height == -1)
    gdk_window_get_size (window, NULL, height);

  return set_bg;
}

static XpThemeElement
map_gtk_progress_bar_to_xp(GtkProgressBar *progress_bar, gboolean trough)
{
  XpThemeElement ret;
  switch (progress_bar->orientation)
    {
    case GTK_PROGRESS_LEFT_TO_RIGHT:
    case GTK_PROGRESS_RIGHT_TO_LEFT:
      ret = trough
        ? XP_THEME_ELEMENT_PROGRESS_TROUGH_H
        : XP_THEME_ELEMENT_PROGRESS_BAR_H;
      break;
    default:
      ret = trough
        ? XP_THEME_ELEMENT_PROGRESS_TROUGH_V
        : XP_THEME_ELEMENT_PROGRESS_BAR_V;
      break;
    }
  return ret;
}

static void
draw_part (GdkDrawable  *drawable,
	   GdkGC        *gc,
	   GdkRectangle *area,
	   gint          x,
	   gint          y,
	   Part          part)
{
  if (area)
    gdk_gc_set_clip_rectangle (gc, area);

  if (!parts[part].bmap)
      parts[part].bmap = gdk_bitmap_create_from_data (drawable,
						      parts[part].bits,
						      PART_SIZE, PART_SIZE);

  gdk_gc_set_ts_origin (gc, x, y);
  gdk_gc_set_stipple (gc, parts[part].bmap);
  gdk_gc_set_fill (gc, GDK_STIPPLED);

  gdk_draw_rectangle (drawable, gc, TRUE, x, y, PART_SIZE, PART_SIZE);

  gdk_gc_set_fill (gc, GDK_SOLID);

  if (area)
    gdk_gc_set_clip_rectangle (gc, NULL);
}

static void
draw_check(GtkStyle      *style,
	   GdkWindow     *window,
	   GtkStateType   state,
	   GtkShadowType  shadow,
	   GdkRectangle  *area,
	   GtkWidget     *widget,
	   const gchar   *detail,
	   gint           x,
	   gint           y,
	   gint           width,
	   gint           height)
{
  x -= (1 + PART_SIZE - width) / 2;
  y -= (1 + PART_SIZE - height) / 2;

  if (detail && strcmp (detail, "check") == 0)	/* Menu item */
    {
      if (shadow == GTK_SHADOW_IN)
	{
	  draw_part (window, style->black_gc, area, x, y, CHECK_TEXT);
	  draw_part (window, style->dark_gc[state], area, x, y, CHECK_AA);
	}
    }
  else
    {
      if (xp_theme_draw(window, shadow == GTK_SHADOW_IN
                        ? XP_THEME_ELEMENT_PRESSED_CHECKBOX
                        : XP_THEME_ELEMENT_CHECKBOX,
                        style, x, y, width, height, state))
        {
        }
      else
        {
          draw_part (window, style->black_gc, area, x, y, CHECK_BLACK);
          draw_part (window, style->dark_gc[state], area, x, y, CHECK_DARK);
          draw_part (window, style->mid_gc[state], area, x, y, CHECK_MID);
          draw_part (window, style->light_gc[state], area, x, y, CHECK_LIGHT);
          draw_part (window, style->base_gc[state], area, x, y, CHECK_BASE);

          if (shadow == GTK_SHADOW_IN)
            {
              draw_part (window, style->text_gc[state], area, x, y, CHECK_TEXT);
              draw_part (window, style->text_aa_gc[state], area, x, y, CHECK_AA);
            }
        }
    }
}

static void
draw_expander(GtkStyle      *style,
	      GdkWindow     *window,
	      GtkStateType   state,
	      GdkRectangle  *area,
	      GtkWidget     *widget,
	      const gchar   *detail,
	      gint           x,
	      gint           y,
	      GtkExpanderStyle expander_style)
{
  gint expander_size;
  gint expander_semi_size;
  GdkColor color;
  GdkGCValues values;
  gboolean success;
  XpThemeElement xp_expander;

  gtk_widget_style_get (widget, "expander_size", &expander_size, NULL);

  switch (expander_style)
    {
    case GTK_EXPANDER_COLLAPSED:
    case GTK_EXPANDER_SEMI_COLLAPSED:
      xp_expander = XP_THEME_ELEMENT_TREEVIEW_EXPANDER_CLOSED;
      break;
    default:
      xp_expander = XP_THEME_ELEMENT_TREEVIEW_EXPANDER_OPENED;
      break;
    }

  if (xp_theme_draw(window, xp_expander, style,
                    x, y - expander_size / 2,
                    expander_size, expander_size, state))
    {
      return;
    }

  if (expander_size > 2)
    expander_size -= 2;

  if (area)
    gdk_gc_set_clip_rectangle (style->fg_gc[state], area);

  expander_semi_size = expander_size / 2;
  x -= expander_semi_size;
  y -= expander_semi_size;

  gdk_gc_get_values (style->fg_gc[state], &values);

  /* RGB values to emulate Windows Classic style */
  color.red = color.green = color.blue = 128 << 8;

  success = gdk_colormap_alloc_color
    (gtk_widget_get_default_colormap (), &color, FALSE, TRUE);

  if (success)
    gdk_gc_set_foreground (style->fg_gc[state], &color);

  gdk_draw_rectangle
    (window, style->fg_gc[state], FALSE, x, y, expander_size, expander_size);

  if (success)
    gdk_gc_set_foreground (style->fg_gc[state], &values.foreground);

  gdk_draw_line
    (window, style->fg_gc[state], x + 2, y + expander_semi_size,
     x + expander_size - 2, y + expander_semi_size);

  switch (expander_style)
    {
    case GTK_EXPANDER_COLLAPSED:
    case GTK_EXPANDER_SEMI_COLLAPSED:
      gdk_draw_line
	(window, style->fg_gc[state], x + expander_semi_size, y + 2,
	 x + expander_semi_size, y + expander_size - 2);
      break;
    }

  if (area)
    gdk_gc_set_clip_rectangle (style->fg_gc[state], NULL);
}

static void
draw_option(GtkStyle      *style,
	    GdkWindow     *window,
	    GtkStateType   state,
	    GtkShadowType  shadow,
	    GdkRectangle  *area,
	    GtkWidget     *widget,
	    const gchar   *detail,
	    gint           x,
	    gint           y,
	    gint           width,
	    gint           height)
{
  x -= (1 + PART_SIZE - width) / 2;
  y -= (1 + PART_SIZE - height) / 2;

  if (detail && strcmp (detail, "option") == 0)	/* Menu item */
    {
      if (shadow == GTK_SHADOW_IN)
	draw_part (window, style->fg_gc[state], area, x, y, RADIO_TEXT);
    }
  else
    {
      if (xp_theme_draw(window,
                        XP_THEME_ELEMENT_RADIO_BUTTON, style,
                        x, y, width, height, state))
        {
        }
      else
	{
	  draw_part (window, style->black_gc, area, x, y, RADIO_BLACK);
	  draw_part (window, style->dark_gc[state], area, x, y, RADIO_DARK);
	  draw_part (window, style->mid_gc[state], area, x, y, RADIO_MID);
	  draw_part (window, style->light_gc[state], area, x, y, RADIO_LIGHT);
	  draw_part (window, style->base_gc[state], area, x, y, RADIO_BASE);

	  if (shadow == GTK_SHADOW_IN)
	    draw_part (window, style->text_gc[state], area, x, y, RADIO_TEXT);
	}
    }
}

static void
draw_varrow (GdkWindow     *window,
	     GdkGC         *gc,
	     GtkShadowType  shadow_type,
	     GdkRectangle  *area,
	     GtkArrowType   arrow_type,
	     gint           x,
	     gint           y,
	     gint           width,
	     gint           height)
{
  gint steps, extra;
  gint y_start, y_increment;
  gint i;

  if (area)
    gdk_gc_set_clip_rectangle (gc, area);

  width = width + width % 2 - 1;	/* Force odd */

  steps = 1 + width / 2;

  extra = height - steps;

  if (arrow_type == GTK_ARROW_DOWN)
    {
      y_start = y;
      y_increment = 1;
    }
  else
    {
      y_start = y + height - 1;
      y_increment = -1;
    }

#if 0
  for (i = 0; i < extra; i++)
    {
      gdk_draw_line (window, gc,
		     x,              y_start + i * y_increment,
		     x + width - 1,  y_start + i * y_increment);
    }
#endif
  for (i = extra; i < height; i++)
    {
      gdk_draw_line (window, gc,
		     x + (i - extra),              y_start + i * y_increment,
		     x + width - (i - extra) - 1,  y_start + i * y_increment);
    }


  if (area)
    gdk_gc_set_clip_rectangle (gc, NULL);
}

static void
draw_harrow (GdkWindow     *window,
	     GdkGC         *gc,
	     GtkShadowType  shadow_type,
	     GdkRectangle  *area,
	     GtkArrowType   arrow_type,
	     gint           x,
	     gint           y,
	     gint           width,
	     gint           height)
{
  gint steps, extra;
  gint x_start, x_increment;
  gint i;

  if (area)
    gdk_gc_set_clip_rectangle (gc, area);

  height = height + height % 2 - 1;	/* Force odd */

  steps = 1 + height / 2;

  extra = width - steps;

  if (arrow_type == GTK_ARROW_RIGHT)
    {
      x_start = x;
      x_increment = 1;
    }
  else
    {
      x_start = x + width - 1;
      x_increment = -1;
    }

#if 0
  for (i = 0; i < extra; i++)
    {
      gdk_draw_line (window, gc,
		     x_start + i * x_increment, y,
		     x_start + i * x_increment, y + height - 1);
    }
#endif
  for (i = extra; i < width; i++)
    {
      gdk_draw_line (window, gc,
		     x_start + i * x_increment, y + (i - extra),
		     x_start + i * x_increment, y + height - (i - extra) - 1);
    }


  if (area)
    gdk_gc_set_clip_rectangle (gc, NULL);
}

/* This function makes up for some brokeness in gtkrange.c
 * where we never get the full arrow of the stepper button
 * and the type of button in a single drawing function.
 *
 * It doesn't work correctly when the scrollbar is squished
 * to the point we don't have room for full-sized steppers.
 */
static void
reverse_engineer_stepper_box (GtkWidget    *range,
			      GtkArrowType  arrow_type,
			      gint         *x,
			      gint         *y,
			      gint         *width,
			      gint         *height)
{
  gint slider_width = 14, stepper_size = 14;
  gint box_width;
  gint box_height;

  if (range)
    {
      gtk_widget_style_get (range,
			    "slider_width", &slider_width,
			    "stepper_size", &stepper_size,
			    NULL);
    }

  if (arrow_type == GTK_ARROW_UP || arrow_type == GTK_ARROW_DOWN)
    {
      box_width = slider_width;
      box_height = stepper_size;
    }
  else
    {
      box_width = stepper_size;
      box_height = slider_width;
    }

  *x = *x - (box_width - *width) / 2;
  *y = *y - (box_height - *height) / 2;
  *width = box_width;
  *height = box_height;
}


static void
draw_arrow (GtkStyle      *style,
	    GdkWindow     *window,
	    GtkStateType   state,
	    GtkShadowType  shadow,
	    GdkRectangle  *area,
	    GtkWidget     *widget,
	    const gchar   *detail,
	    GtkArrowType   arrow_type,
	    gboolean       fill,
	    gint           x,
	    gint           y,
	    gint           width,
	    gint           height)
{
  const gchar * name;

  name = gtk_widget_get_name (widget);

  sanitize_size (window, &width, &height);

  if (detail && strcmp (detail, "spinbutton") == 0)
    {
      if (xp_theme_is_drawable(XP_THEME_ELEMENT_SPIN_BUTTON_UP))
        {
          return;
        }
      else
        {
          x += (width - 7) / 2;

          if (arrow_type == GTK_ARROW_UP)
            y += (height - 4) / 2;
          else
            y += (1 + height - 4) / 2;
          draw_varrow (window, style->fg_gc[state], shadow, area, arrow_type,
                       x, y, 7, 4);
        }
    }
  else if (detail && (!strcmp (detail, "vscrollbar")
                      || !strcmp (detail, "hscrollbar")))
    {
      gint box_x = x;
      gint box_y = y;
      gint box_width = width;
      gint box_height = height;
      XpThemeElement xp_arrow;
      reverse_engineer_stepper_box (widget, arrow_type,
				    &box_x, &box_y, &box_width, &box_height);

      switch (arrow_type)
        {
        case GTK_ARROW_UP:
          xp_arrow = XP_THEME_ELEMENT_ARROW_UP;
          break;
        case GTK_ARROW_DOWN:
          xp_arrow = XP_THEME_ELEMENT_ARROW_DOWN;
          break;
        case GTK_ARROW_LEFT:
          xp_arrow = XP_THEME_ELEMENT_ARROW_LEFT;
          break;
        default:
          xp_arrow = XP_THEME_ELEMENT_ARROW_RIGHT;
          break;
        }
      if (xp_theme_draw(window, xp_arrow, style, box_x, box_y, box_width, box_height, state))
        {
        }
      else if (arrow_type == GTK_ARROW_UP || arrow_type == GTK_ARROW_DOWN)
        {
          x += (width - 7) / 2;
          y += (height - 5) / 2;

          draw_varrow (window, style->fg_gc[state], shadow, area, arrow_type,
                       x, y, 7, 5);
        }
      else
        {
          y += (height - 7) / 2;
          x += (width - 5) / 2;

          draw_harrow (window, style->fg_gc[state], shadow, area, arrow_type,
                       x, y, 5, 7);
        }
    }
  else
    {
      /* draw the toolbar chevrons - waiting for GTK 2.4 */
	  if (name && !strcmp (name, "gtk-toolbar-arrow"))
	  {
		  if (xp_theme_draw(window, XP_THEME_ELEMENT_CHEVRON, style, x, y, width, height, state))
				return;
	  }

      if (arrow_type == GTK_ARROW_UP || arrow_type == GTK_ARROW_DOWN)
	{
	  x += (width - 7) / 2;
	  y += (height - 5) / 2;

	  draw_varrow (window, style->fg_gc[state], shadow, area, arrow_type,
		       x, y, 7, 5);
	}
      else
	{
	  x += (width - 5) / 2;
	  y += (height - 7) / 2;

	  draw_harrow (window, style->fg_gc[state], shadow, area, arrow_type,
		       x, y, 5, 7);
	}
    }
}

static void
option_menu_get_props (GtkWidget      *widget,
		       GtkRequisition *indicator_size,
		       GtkBorder      *indicator_spacing)
{
  GtkRequisition *tmp_size = NULL;
  GtkBorder *tmp_spacing = NULL;

  if (widget)
    gtk_widget_style_get (widget,
			  "indicator_size", &tmp_size,
			  "indicator_spacing", &tmp_spacing,
			  NULL);

  if (tmp_size)
    {
      *indicator_size = *tmp_size;
      g_free (tmp_size);
    }
  else
    *indicator_size = default_option_indicator_size;

  if (tmp_spacing)
    {
      *indicator_spacing = *tmp_spacing;
      g_free (tmp_spacing);
    }
  else
    *indicator_spacing = default_option_indicator_spacing;
}

static void
draw_box (GtkStyle      *style,
	  GdkWindow     *window,
	  GtkStateType   state_type,
	  GtkShadowType  shadow_type,
	  GdkRectangle  *area,
	  GtkWidget     *widget,
	  const gchar   *detail,
	  gint           x,
	  gint           y,
	  gint           width,
	  gint           height)
{
  const gchar * name;

  name = gtk_widget_get_name (widget);

  if (detail &&
      (!strcmp (detail, "button") ||
       !strcmp (detail, "buttondefault")))
    {
      if (GTK_IS_TREE_VIEW (widget->parent) || GTK_IS_CLIST (widget->parent))
        {
          if (xp_theme_draw(window, XP_THEME_ELEMENT_LIST_HEADER, style, x, y,
                            width, height, state_type))
            return;
        }
      else
        {
          gboolean is_default = !strcmp (detail, "buttondefault");
          if (xp_theme_draw(window, is_default ? XP_THEME_ELEMENT_DEFAULT_BUTTON
                            : XP_THEME_ELEMENT_BUTTON, style, x, y,
                            width, height, state_type))
            return;
        }
    }
  else if (detail && !strcmp (detail, "spinbutton"))
    {
      if (xp_theme_is_drawable(XP_THEME_ELEMENT_SPIN_BUTTON_UP))
        {
          return;
        }
    }
  else if (detail && (!strcmp (detail, "spinbutton_up")
                      || !strcmp (detail, "spinbutton_down")))
    {
      if (xp_theme_draw(window,
                        (! strcmp (detail, "spinbutton_up"))
                        ? XP_THEME_ELEMENT_SPIN_BUTTON_UP
                        : XP_THEME_ELEMENT_SPIN_BUTTON_DOWN,
                        style, x, y, width, height, state_type))
        {
          return;
        }
    }

  else if (detail && !strcmp (detail, "slider"))
    {
      if (GTK_IS_SCROLLBAR(widget))
        {
          GtkScrollbar * scrollbar = GTK_SCROLLBAR(widget);
          if (xp_theme_draw(window,
                            (! GTK_IS_VSCROLLBAR(widget))
                            ? XP_THEME_ELEMENT_SCROLLBAR_V
                            : XP_THEME_ELEMENT_SCROLLBAR_H,
                            style, x, y, width, height, state_type))
            {
              return;
            }
        }
    }
  else if (detail && !strcmp (detail, "bar"))
    {
      if (widget && GTK_IS_PROGRESS_BAR (widget))
        {
          GtkProgressBar *progress_bar = GTK_PROGRESS_BAR(widget);
          XpThemeElement xp_progress_bar = map_gtk_progress_bar_to_xp (progress_bar, FALSE);
          if (xp_theme_draw (window, xp_progress_bar,
                             style, x, y, width, height, state_type))
            {
              return;
            }
        }
    }
  else if (detail && !strcmp (detail, "handlebox_bin")) {
	if (xp_theme_draw (window, XP_THEME_ELEMENT_REBAR, style, x, y, width, height, state_type))
	  {
		return;
  	  }
  }
  else if (name && !strcmp (name, "gtk-tooltips")) {
      if (xp_theme_draw (window, XP_THEME_ELEMENT_TOOLTIP, style, x, y, width, height, state_type))
        {
  		return;
        }
  }

  if (detail && strcmp (detail, "menuitem") == 0)
    shadow_type = GTK_SHADOW_NONE;

  if (detail && !strcmp (detail, "trough"))
    {
      if (widget && GTK_IS_PROGRESS_BAR (widget))
        {
          GtkProgressBar *progress_bar = GTK_PROGRESS_BAR(widget);
          XpThemeElement xp_progress_bar = map_gtk_progress_bar_to_xp (progress_bar, TRUE);
          if (xp_theme_draw (window, xp_progress_bar,
                             style, x, y, width, height, state_type))
            {
              return;
            }
          else
            {
              /* Blank in classic Windows */
            }
        }
      else
        {
          gboolean is_vertical = GTK_IS_VSCROLLBAR(widget);

          if (GTK_IS_RANGE(widget)
              && xp_theme_draw(window,
                               is_vertical
                               ? XP_THEME_ELEMENT_TROUGH_V
                               : XP_THEME_ELEMENT_TROUGH_H,
                               style,
                               x, y, width, height, state_type))
            {
              return;
            }
          else
            {
              GdkGCValues gc_values;
              GdkGC *gc;
              GdkPixmap *pixmap;

              sanitize_size (window, &width, &height);

              pixmap = gdk_pixmap_new (window, 2, 2, -1);

              gdk_draw_point (pixmap, style->bg_gc[GTK_STATE_NORMAL], 0, 0);
              gdk_draw_point (pixmap, style->bg_gc[GTK_STATE_NORMAL], 1, 1);
              gdk_draw_point (pixmap, style->light_gc[GTK_STATE_NORMAL], 1, 0);
              gdk_draw_point (pixmap, style->light_gc[GTK_STATE_NORMAL], 0, 1);

              gc_values.fill = GDK_TILED;
              gc_values.tile = pixmap;
              gc_values.ts_x_origin = x;
              gc_values.ts_y_origin = y;
              gc = gdk_gc_new_with_values (window, &gc_values,
                                           GDK_GC_TS_X_ORIGIN | GDK_GC_TS_Y_ORIGIN | GDK_GC_FILL | GDK_GC_TILE);

              if (area)
                gdk_gc_set_clip_rectangle (gc, area);

              gdk_draw_rectangle (window, gc, TRUE, x, y, width, height);

              gdk_gc_unref (gc);
              gdk_pixmap_unref (pixmap);

              return;
            }
        }
    }
  else if (detail && strcmp (detail, "optionmenu") == 0)
    {
      if (xp_theme_draw(window, XP_THEME_ELEMENT_EDIT_TEXT,
                        style, x, y, width, height, state_type))
        {
          return;
        }
    }

  parent_class->draw_box (style, window, state_type, shadow_type, area,
			  widget, detail, x, y, width, height);

  if (detail && strcmp (detail, "optionmenu") == 0)
    {
      GtkRequisition indicator_size;
      GtkBorder indicator_spacing;
      gint vline_x;

      option_menu_get_props (widget, &indicator_size, &indicator_spacing);

      sanitize_size (window, &width, &height);

      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
	vline_x = x + indicator_size.width + indicator_spacing.left + indicator_spacing.right;
      else
	vline_x = x + width - (indicator_size.width + indicator_spacing.left + indicator_spacing.right) - style->xthickness;

      parent_class->draw_vline (style, window, state_type, area, widget,
				detail,
				y + style->ythickness + 1,
				y + height - style->ythickness - 3,
				vline_x);
    }
}

static void
draw_tab (GtkStyle      *style,
	  GdkWindow     *window,
	  GtkStateType   state,
	  GtkShadowType  shadow,
	  GdkRectangle  *area,
	  GtkWidget     *widget,
	  const gchar   *detail,
	  gint           x,
	  gint           y,
	  gint           width,
	  gint           height)
{
  GtkRequisition indicator_size;
  GtkBorder indicator_spacing;

  gint arrow_height;

  g_return_if_fail (style != NULL);
  g_return_if_fail (window != NULL);

  if (detail && ! strcmp (detail, "optionmenutab"))
    {
      if (xp_theme_draw(window, XP_THEME_ELEMENT_COMBOBUTTON,
                        style, x-5, widget->allocation.y+1,
                        width+10, widget->allocation.height-2, state))
        {
          return;
        }
    }

  if (widget)
    gtk_widget_style_get (widget, "indicator_size", &indicator_size, NULL);

  option_menu_get_props (widget, &indicator_size, &indicator_spacing);

  x += (width - indicator_size.width) / 2;
  arrow_height = (indicator_size.width + 1) / 2;

  y += (height - arrow_height) / 2;

  draw_varrow (window, style->black_gc, shadow, area, GTK_ARROW_DOWN,
	       x, y, indicator_size.width, arrow_height);
}

/* this is an undefined magic value that, according to the mozilla folks,
   worked for all the various themes that they tried */
#define XP_EDGE_SIZE 2

static void
draw_extension(GtkStyle *style,
               GdkWindow *window,
               GtkStateType state_type,
               GtkShadowType shadow_type,
               GdkRectangle *area,
               GtkWidget *widget,
               const gchar *detail,
               gint x,
               gint y,
               gint width,
               gint height,
               GtkPositionType gap_side)
{
  if (detail && !strcmp(detail, "tab"))
    {
      GtkNotebook *notebook = GTK_NOTEBOOK(widget);
      GtkPositionType pos_type = gtk_notebook_get_tab_pos(notebook);

      if (pos_type == GTK_POS_TOP)
	height += XP_EDGE_SIZE;

#if 0
	/* FIXME: pos != TOP to be implemented */
      else if (pos_type == GTK_POS_BOTTOM)
	y -= XP_EDGE_SIZE;
      else if (pos_type == GTK_POS_RIGHT)
	width += XP_EDGE_SIZE;
      else if (pos_type == GTK_POS_LEFT)
	height -= XP_EDGE_SIZE;
#endif

      if (xp_theme_draw (window,
			 gtk_notebook_get_current_page(notebook)==0
			 ? XP_THEME_ELEMENT_TAB_ITEM_LEFT_EDGE
			 : XP_THEME_ELEMENT_TAB_ITEM,
			 style, x, y, width, height, state_type))
        {
          return;
        }
    }
  parent_class->draw_extension
    (style, window, state_type, shadow_type, area, widget, detail,
     	 x, y, width, height, gap_side);
}

static void
draw_box_gap (GtkStyle *style, GdkWindow *window, GtkStateType state_type,
              GtkShadowType shadow_type, GdkRectangle *area,
              GtkWidget *widget, const gchar *detail, gint x,
              gint y, gint width, gint height, GtkPositionType gap_side,
              gint gap_x, gint gap_width)
{
  if (detail && !strcmp(detail, "notebook"))
    {
      GtkNotebook *notebook = GTK_NOTEBOOK(widget);

		/* FIXME: pos != TOP to be implemented */
      if (gtk_notebook_get_tab_pos(notebook) == GTK_POS_TOP && xp_theme_draw(window, XP_THEME_ELEMENT_TAB_PANE, style,  x, y, width, height,
			state_type))
        {
          return;
        }
    }
  parent_class->draw_box_gap(style, window, state_type, shadow_type,
                             area, widget, detail, x, y, width, height,
                             gap_side, gap_x, gap_width);
}

static void
draw_flat_box (GtkStyle *style, GdkWindow *window,
               GtkStateType state_type, GtkShadowType shadow_type,
               GdkRectangle *area, GtkWidget *widget,
               const gchar *detail, gint x, gint y,
               gint width, gint height)
{
  if (detail && ! strcmp (detail, "checkbutton"))
    {
      if (state_type == GTK_STATE_PRELIGHT)
        {
          return;
        }
    }

  parent_class->draw_flat_box(style, window, state_type, shadow_type,
                              area, widget, detail, x, y, width, height);
}

static void
draw_shadow (GtkStyle      *style,
             GdkWindow     *window,
             GtkStateType   state_type,
             GtkShadowType  shadow_type,
             GdkRectangle  *area,
             GtkWidget     *widget,
             const gchar   *detail,
             gint           x,
             gint           y,
             gint           width,
             gint           height)
{
  if(detail && ! strcmp(detail, "entry"))
    {
      if (xp_theme_draw(window, XP_THEME_ELEMENT_EDIT_TEXT, style,
                        x, y, width, height, state_type))
        {
          return;
        }
    }
  parent_class->draw_shadow (style, window, state_type, shadow_type, area, widget,
                             detail, x, y, width, height);
}

static void
draw_hline (GtkStyle		*style,
	    GdkWindow		*window,
	    GtkStateType	 state_type,
	    GdkRectangle	*area,
	    GtkWidget		*widget,
	    const gchar		*detail,
	    gint		 x1,
	    gint		 x2,
	    gint		 y)
{
  /* TODO: GP_LINEHORIZ : LHS_FLAT, LHS_RAISED, LHS_SUNKEN*/
  parent_class->draw_hline (style, window, state_type, area, widget,
			    detail, x1, x2, y);
}

static void
draw_vline (GtkStyle		*style,
	    GdkWindow		*window,
	    GtkStateType	 state_type,
	    GdkRectangle	*area,
	    GtkWidget		*widget,
	    const gchar		*detail,
	    gint		 y1,
	    gint		 y2,
	    gint		 x)
{
  /* TODO: GP_LINEVERT : LVS_FLAT, LVS_RAISED, LVS_SUNKEN */
  parent_class->draw_vline (style, window, state_type, area, widget,
			    detail, y1, y2, x);
}

static void
draw_handle (GtkStyle        *style,
	     GdkWindow       *window,
	     GtkStateType     state_type,
	     GtkShadowType    shadow_type,
	     GdkRectangle    *area,
	     GtkWidget       *widget,
	     const gchar     *detail,
	     gint             x,
	     gint             y,
	     gint             width,
	     gint             height,
	     GtkOrientation   orientation)
{
  XpThemeElement hndl;

  if (!GTK_IS_HANDLE_BOX(widget)) {
	  if (orientation == GTK_ORIENTATION_VERTICAL)
	    hndl = XP_THEME_ELEMENT_GRIPPER_V;
	  else
	    hndl = XP_THEME_ELEMENT_GRIPPER_H;

	  if (xp_theme_draw(window, hndl, style, x, y, width, height, state_type))
	    {
	      return;
	    }
	}

  if (!GTK_IS_HANDLE_BOX(widget)) {
    /* grippers are just flat boxes when they're not a toolbar */
    parent_class->draw_box (style, window, state_type, shadow_type, area, widget,
			    detail, x, y, width, height);
  } else {
    /* TODO: Draw handle boxes as double lines: || */
    parent_class->draw_handle (style, window, state_type, shadow_type, area, widget,
			       detail, x, y, width, height, orientation);
  }
}

static void
wimp_style_init_from_rc (GtkStyle * style, GtkRcStyle * rc_style)
{
  setup_system_font (style);
  setup_system_styles (style);
  parent_class->init_from_rc(style, rc_style);
}

static void
wimp_style_init (WimpStyle * style)
{
  xp_theme_init ();
}

static void
wimp_style_class_init (WimpStyleClass *klass)
{
  GtkStyleClass *style_class = GTK_STYLE_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  style_class->init_from_rc = wimp_style_init_from_rc;
  style_class->draw_arrow = draw_arrow;
  style_class->draw_box = draw_box;
  style_class->draw_check = draw_check;
  style_class->draw_option = draw_option;
  style_class->draw_tab = draw_tab;
  style_class->draw_flat_box = draw_flat_box;
  style_class->draw_expander = draw_expander;
  style_class->draw_extension = draw_extension;
  style_class->draw_box_gap = draw_box_gap;
  style_class->draw_shadow = draw_shadow;
  style_class->draw_hline = draw_hline;
  style_class->draw_vline = draw_vline;
  style_class->draw_handle = draw_handle;
}

GType wimp_type_style = 0;

void
wimp_style_register_type (GTypeModule *module)
{
  static const GTypeInfo object_info =
  {
    sizeof (WimpStyleClass),
    (GBaseInitFunc) NULL,
    (GBaseFinalizeFunc) NULL,
    (GClassInitFunc) wimp_style_class_init,
    NULL,           /* class_finalize */
    NULL,           /* class_data */
    sizeof (WimpStyle),
    0,              /* n_preallocs */
    (GInstanceInitFunc) wimp_style_init,
  };

  wimp_type_style = g_type_module_register_type (module,
						   GTK_TYPE_STYLE,
						   "WimpStyle",
						   &object_info, 0);
}

