/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity Theme Rendering */

/*
 * Copyright (C) 2001 Havoc Pennington
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

/**
 * \file theme.c    Making Metacity look pretty
 *
 * The window decorations drawn by Metacity are described by files on disk
 * known internally as "themes" (externally as "window border themes" on
 * http://art.gnome.org/themes/metacity/ or "Metacity themes"). This file
 * contains most of the code necessary to support themes; it does not
 * contain the XML parser, which is in theme-parser.c.
 *
 * \bug This is a big file with lots of different subsystems, which might
 * be better split out into separate files.
 */

/**
 * \defgroup tokenizer   The theme expression tokenizer
 *
 * Themes can use a simple expression language to represent the values of
 * things. This is the tokeniser used for that language.
 *
 * \bug We could remove almost all this code by using GScanner instead,
 * but we would also have to find every expression in every existing theme
 * we could and make sure the parse trees were the same.
 */

/**
 * \defgroup parser  The theme expression parser
 *
 * Themes can use a simple expression language to represent the values of
 * things. This is the parser used for that language.
 */

#include <config.h>
#include "theme.h"
#include "theme-parser.h"
#include "util.h"
#include "gradient.h"
#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "gtk-compat.h"

#define GDK_COLOR_RGBA(color)                                           \
                         ((guint32) (0xff                         |     \
                                     (((color).red / 256) << 24)   |    \
                                     (((color).green / 256) << 16) |    \
                                     (((color).blue / 256) << 8)))

#define GDK_COLOR_RGB(color)                                            \
                         ((guint32) ((((color).red / 256) << 16)   |    \
                                     (((color).green / 256) << 8)  |    \
                                     (((color).blue / 256))))

#define ALPHA_TO_UCHAR(d) ((unsigned char) ((d) * 255))

#define DEBUG_FILL_STRUCT(s) memset ((s), 0xef, sizeof (*(s)))
#define CLAMP_UCHAR(v) ((guchar) (CLAMP (((int)v), (int)0, (int)255)))
#define INTENSITY(r, g, b) ((r) * 0.30 + (g) * 0.59 + (b) * 0.11)

static void gtk_style_shade		(GdkColor	 *a,
					 GdkColor	 *b,
					 gdouble	  k);
static void rgb_to_hls			(gdouble	 *r,
					 gdouble	 *g,
					 gdouble	 *b);
static void hls_to_rgb			(gdouble	 *h,
					 gdouble	 *l,
					 gdouble	 *s);

/**
 * The current theme. (Themes are singleton.)
 */
static MetaTheme *meta_current_theme = NULL;

static GdkPixbuf *
colorize_pixbuf (GdkPixbuf *orig,
                 GdkColor  *new_color)
{
  GdkPixbuf *pixbuf;
  double intensity;
  int x, y;
  const guchar *src;
  guchar *dest;
  int orig_rowstride;
  int dest_rowstride;
  int width, height;
  gboolean has_alpha;
  const guchar *src_pixels;
  guchar *dest_pixels;
  
  pixbuf = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (orig), gdk_pixbuf_get_has_alpha (orig),
			   gdk_pixbuf_get_bits_per_sample (orig),
			   gdk_pixbuf_get_width (orig), gdk_pixbuf_get_height (orig));

  if (pixbuf == NULL)
    return NULL;
  
  orig_rowstride = gdk_pixbuf_get_rowstride (orig);
  dest_rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);
  has_alpha = gdk_pixbuf_get_has_alpha (orig);
  src_pixels = gdk_pixbuf_get_pixels (orig);
  dest_pixels = gdk_pixbuf_get_pixels (pixbuf);
  
  for (y = 0; y < height; y++)
    {
      src = src_pixels + y * orig_rowstride;
      dest = dest_pixels + y * dest_rowstride;

      for (x = 0; x < width; x++)
        {
          double dr, dg, db;
          
          intensity = INTENSITY (src[0], src[1], src[2]) / 255.0;

          if (intensity <= 0.5)
            {
              /* Go from black at intensity = 0.0 to new_color at intensity = 0.5 */
              dr = (new_color->red * intensity * 2.0) / 65535.0;
              dg = (new_color->green * intensity * 2.0) / 65535.0;
              db = (new_color->blue * intensity * 2.0) / 65535.0;
            }
          else
            {
              /* Go from new_color at intensity = 0.5 to white at intensity = 1.0 */
              dr = (new_color->red + (65535 - new_color->red) * (intensity - 0.5) * 2.0) / 65535.0;
              dg = (new_color->green + (65535 - new_color->green) * (intensity - 0.5) * 2.0) / 65535.0;
              db = (new_color->blue + (65535 - new_color->blue) * (intensity - 0.5) * 2.0) / 65535.0;
            }
          
          dest[0] = CLAMP_UCHAR (255 * dr);
          dest[1] = CLAMP_UCHAR (255 * dg);
          dest[2] = CLAMP_UCHAR (255 * db);
          
          if (has_alpha)
            {
              dest[3] = src[3];
              src += 4;
              dest += 4;
            }
          else
            {
              src += 3;
              dest += 3;
            }
        }
    }

  return pixbuf;
}

static void
color_composite (const GdkColor *bg,
                 const GdkColor *fg,
                 double          alpha_d,
                 GdkColor       *color)
{
  guint16 alpha;

  *color = *bg;
  alpha = alpha_d * 0xffff;
  color->red = color->red + (((fg->red - color->red) * alpha + 0x8000) >> 16);
  color->green = color->green + (((fg->green - color->green) * alpha + 0x8000) >> 16);
  color->blue = color->blue + (((fg->blue - color->blue) * alpha + 0x8000) >> 16);
}

/**
 * Sets all the fields of a border to dummy values.
 *
 * \param border The border whose fields should be reset.
 */
static void
init_border (GtkBorder *border)
{
  border->top = -1;
  border->bottom = -1;
  border->left = -1;
  border->right = -1;
}

/**
 * Creates a new, empty MetaFrameLayout. The fields will be set to dummy
 * values.
 *
 * \return The newly created MetaFrameLayout.
 */
MetaFrameLayout*
meta_frame_layout_new  (void)
{
  MetaFrameLayout *layout;

  layout = g_new0 (MetaFrameLayout, 1);

  layout->refcount = 1;

  /* Fill with -1 values to detect invalid themes */
  layout->left_width = -1;
  layout->right_width = -1;
  layout->bottom_height = -1;

  init_border (&layout->title_border);

  layout->title_vertical_pad = -1;
  
  layout->right_titlebar_edge = -1;
  layout->left_titlebar_edge = -1;

  layout->button_sizing = META_BUTTON_SIZING_LAST;
  layout->button_aspect = 1.0;
  layout->button_width = -1;
  layout->button_height = -1;

  layout->has_title = TRUE;
  layout->title_scale = 1.0;
  
  init_border (&layout->button_border);

  return layout;
}

/**
 *
 */
static gboolean
validate_border (const GtkBorder *border,
                 const char     **bad)
{
  *bad = NULL;
  
  if (border->top < 0)
    *bad = _("top");
  else if (border->bottom < 0)
    *bad = _("bottom");
  else if (border->left < 0)
    *bad = _("left");
  else if (border->right < 0)
    *bad = _("right");

  return *bad == NULL;
}

/**
 * Ensures that the theme supplied a particular dimension. When a
 * MetaFrameLayout is created, all its integer fields are set to -1
 * by meta_frame_layout_new(). After an instance of this type
 * should have been initialised, this function checks that
 * a given field is not still at -1. It is never called directly, but
 * rather via the CHECK_GEOMETRY_VALUE and CHECK_GEOMETRY_BORDER
 * macros.
 *
 * \param      val    The value to check
 * \param      name   The name to use in the error message
 * \param[out] error  Set to an error if val was not initialised
 */
static gboolean
validate_geometry_value (int         val,
                         const char *name,
                         GError    **error)
{
  if (val < 0)
    {
      g_set_error (error, META_THEME_ERROR,
                   META_THEME_ERROR_FRAME_GEOMETRY,
                   _("frame geometry does not specify \"%s\" dimension"),
                   name);
      return FALSE;
    }
  else
    return TRUE;
}

static gboolean
validate_geometry_border (const GtkBorder *border,
                          const char      *name,
                          GError         **error)
{
  const char *bad;

  if (!validate_border (border, &bad))
    {
      g_set_error (error, META_THEME_ERROR,
                   META_THEME_ERROR_FRAME_GEOMETRY,
                   _("frame geometry does not specify dimension \"%s\" for border \"%s\""),
                   bad, name);
      return FALSE;
    }
  else
    return TRUE;
}

gboolean
meta_frame_layout_validate (const MetaFrameLayout *layout,
                            GError               **error)
{
  g_return_val_if_fail (layout != NULL, FALSE);

#define CHECK_GEOMETRY_VALUE(vname) if (!validate_geometry_value (layout->vname, #vname, error)) return FALSE

#define CHECK_GEOMETRY_BORDER(bname) if (!validate_geometry_border (&layout->bname, #bname, error)) return FALSE

  CHECK_GEOMETRY_VALUE (left_width);
  CHECK_GEOMETRY_VALUE (right_width);
  CHECK_GEOMETRY_VALUE (bottom_height);

  CHECK_GEOMETRY_BORDER (title_border);

  CHECK_GEOMETRY_VALUE (title_vertical_pad);

  CHECK_GEOMETRY_VALUE (right_titlebar_edge);
  CHECK_GEOMETRY_VALUE (left_titlebar_edge);

  switch (layout->button_sizing)
    {
    case META_BUTTON_SIZING_ASPECT:
      if (layout->button_aspect < (0.1) ||
          layout->button_aspect > (15.0))
        {
          g_set_error (error, META_THEME_ERROR,
                       META_THEME_ERROR_FRAME_GEOMETRY,
                       _("Button aspect ratio %g is not reasonable"),
                       layout->button_aspect);
          return FALSE;
        }
      break;
    case META_BUTTON_SIZING_FIXED:
      CHECK_GEOMETRY_VALUE (button_width);
      CHECK_GEOMETRY_VALUE (button_height);
      break;
    case META_BUTTON_SIZING_LAST:
      g_set_error (error, META_THEME_ERROR,
                   META_THEME_ERROR_FRAME_GEOMETRY,
                   _("Frame geometry does not specify size of buttons"));
      return FALSE;
    }

  CHECK_GEOMETRY_BORDER (button_border);

  return TRUE;
}

MetaFrameLayout*
meta_frame_layout_copy (const MetaFrameLayout *src)
{
  MetaFrameLayout *layout;

  layout = g_new0 (MetaFrameLayout, 1);

  *layout = *src;

  layout->refcount = 1;

  return layout;
}

void
meta_frame_layout_ref (MetaFrameLayout *layout)
{
  g_return_if_fail (layout != NULL);

  layout->refcount += 1;
}

void
meta_frame_layout_unref (MetaFrameLayout *layout)
{
  g_return_if_fail (layout != NULL);
  g_return_if_fail (layout->refcount > 0);

  layout->refcount -= 1;

  if (layout->refcount == 0)
    {
      DEBUG_FILL_STRUCT (layout);
      g_free (layout);
    }
}

void
meta_frame_layout_get_borders (const MetaFrameLayout *layout,
                               int                    text_height,
                               MetaFrameFlags         flags,
                               int                   *top_height,
                               int                   *bottom_height,
                               int                   *left_width,
                               int                   *right_width)
{
  int buttons_height, title_height;
  
  g_return_if_fail (top_height != NULL);
  g_return_if_fail (bottom_height != NULL);
  g_return_if_fail (left_width != NULL);
  g_return_if_fail (right_width != NULL);

  if (!layout->has_title)
    text_height = 0;
  
  buttons_height = layout->button_height +
    layout->button_border.top + layout->button_border.bottom;
  title_height = text_height +
    layout->title_vertical_pad +
    layout->title_border.top + layout->title_border.bottom;

  if (top_height)
    {
      *top_height = MAX (buttons_height, title_height);
    }

  if (left_width)
    *left_width = layout->left_width;
  if (right_width)
    *right_width = layout->right_width;

  if (bottom_height)
    {
      if (flags & META_FRAME_SHADED)
        *bottom_height = 0;
      else
        *bottom_height = layout->bottom_height;
    }

  if (flags & META_FRAME_FULLSCREEN)
    {
      if (top_height)
        *top_height = 0;
      if (bottom_height)
        *bottom_height = 0;
      if (left_width)
        *left_width = 0;
      if (right_width)
        *right_width = 0;
    }
}

static MetaButtonSpace*
rect_for_function (MetaFrameGeometry *fgeom,
                   MetaFrameFlags     flags,
                   MetaButtonFunction function,
                   MetaTheme         *theme)
{

  /* Firstly, check version-specific things. */
  
  if (META_THEME_ALLOWS(theme, META_THEME_SHADE_STICK_ABOVE_BUTTONS))
    {
      switch (function)
        {
        case META_BUTTON_FUNCTION_SHADE:
          if ((flags & META_FRAME_ALLOWS_SHADE) && !(flags & META_FRAME_SHADED))
            return &fgeom->shade_rect;
          else
            return NULL;
        case META_BUTTON_FUNCTION_ABOVE:
          if (!(flags & META_FRAME_ABOVE))
            return &fgeom->above_rect;
          else
            return NULL;
        case META_BUTTON_FUNCTION_STICK:
          if (!(flags & META_FRAME_STUCK))
            return &fgeom->stick_rect;
          else
            return NULL;
        case META_BUTTON_FUNCTION_UNSHADE:
          if ((flags & META_FRAME_ALLOWS_SHADE) && (flags & META_FRAME_SHADED))
            return &fgeom->unshade_rect;
          else
            return NULL;
        case META_BUTTON_FUNCTION_UNABOVE:
          if (flags & META_FRAME_ABOVE)
            return &fgeom->unabove_rect;
          else
            return NULL;
        case META_BUTTON_FUNCTION_UNSTICK:
          if (flags & META_FRAME_STUCK)
            return &fgeom->unstick_rect;
        default:
          /* just go on to the next switch block */;
        }
    }

  /* now consider the buttons which exist in all versions */

  switch (function)
    {
    case META_BUTTON_FUNCTION_MENU:
      if (flags & META_FRAME_ALLOWS_MENU)
        return &fgeom->menu_rect;
      else
        return NULL;
    case META_BUTTON_FUNCTION_MINIMIZE:
      if (flags & META_FRAME_ALLOWS_MINIMIZE)
        return &fgeom->min_rect;
      else
        return NULL;
    case META_BUTTON_FUNCTION_MAXIMIZE:
      if (flags & META_FRAME_ALLOWS_MAXIMIZE)
        return &fgeom->max_rect;
      else
        return NULL;
    case META_BUTTON_FUNCTION_CLOSE:
      if (flags & META_FRAME_ALLOWS_DELETE)
        return &fgeom->close_rect;
      else
        return NULL;
    case META_BUTTON_FUNCTION_STICK:
    case META_BUTTON_FUNCTION_SHADE:
    case META_BUTTON_FUNCTION_ABOVE:
    case META_BUTTON_FUNCTION_UNSTICK:
    case META_BUTTON_FUNCTION_UNSHADE:
    case META_BUTTON_FUNCTION_UNABOVE:
      /* we are being asked for a >v1 button which hasn't been handled yet,
       * so obviously we're not in a theme which supports that version.
       * therefore, we don't show the button. return NULL and all will
       * be well.
       */
      return NULL;
      
    case META_BUTTON_FUNCTION_LAST:
      return NULL;
    }

  return NULL;
}

static gboolean
strip_button (MetaButtonSpace *func_rects[MAX_BUTTONS_PER_CORNER],
              GdkRectangle    *bg_rects[MAX_BUTTONS_PER_CORNER],
              int             *n_rects,
              MetaButtonSpace *to_strip)
{
  int i;
  
  i = 0;
  while (i < *n_rects)
    {
      if (func_rects[i] == to_strip)
        {
          *n_rects -= 1;

          /* shift the other rects back in the array */
          while (i < *n_rects)
            {
              func_rects[i] = func_rects[i+1];
              bg_rects[i] = bg_rects[i+1];

              ++i;
            }

          func_rects[i] = NULL;
          bg_rects[i] = NULL;
          
          return TRUE;
        }

      ++i;
    }

  return FALSE; /* did not strip anything */
}

void
meta_frame_layout_calc_geometry (const MetaFrameLayout  *layout,
                                 int                     text_height,
                                 MetaFrameFlags          flags,
                                 int                     client_width,
                                 int                     client_height,
                                 const MetaButtonLayout *button_layout,
                                 MetaFrameGeometry      *fgeom,
                                 MetaTheme              *theme)
{
  int i, n_left, n_right, n_left_spacers, n_right_spacers;
  int x;
  int button_y;
  int title_right_edge;
  int width, height;
  int button_width, button_height;
  int min_size_for_rounding;
  
  /* the left/right rects in order; the max # of rects
   * is the number of button functions
   */
  MetaButtonSpace *left_func_rects[MAX_BUTTONS_PER_CORNER];
  MetaButtonSpace *right_func_rects[MAX_BUTTONS_PER_CORNER];
  GdkRectangle *left_bg_rects[MAX_BUTTONS_PER_CORNER];
  gboolean left_buttons_has_spacer[MAX_BUTTONS_PER_CORNER];
  GdkRectangle *right_bg_rects[MAX_BUTTONS_PER_CORNER];
  gboolean right_buttons_has_spacer[MAX_BUTTONS_PER_CORNER];
  
  meta_frame_layout_get_borders (layout, text_height,
                                 flags,
                                 &fgeom->top_height,
                                 &fgeom->bottom_height,
                                 &fgeom->left_width,
                                 &fgeom->right_width);

  width = client_width + fgeom->left_width + fgeom->right_width;

  height = ((flags & META_FRAME_SHADED) ? 0: client_height) +
    fgeom->top_height + fgeom->bottom_height;

  fgeom->width = width;
  fgeom->height = height;

  fgeom->top_titlebar_edge = layout->title_border.top;
  fgeom->bottom_titlebar_edge = layout->title_border.bottom;
  fgeom->left_titlebar_edge = layout->left_titlebar_edge;
  fgeom->right_titlebar_edge = layout->right_titlebar_edge;

  /* gcc warnings */
  button_width = -1;
  button_height = -1;
  
  switch (layout->button_sizing)
    {
    case META_BUTTON_SIZING_ASPECT:
      button_height = fgeom->top_height - layout->button_border.top - layout->button_border.bottom;
      button_width = button_height / layout->button_aspect;
      break;
    case META_BUTTON_SIZING_FIXED:
      button_width = layout->button_width;
      button_height = layout->button_height;
      break;
    case META_BUTTON_SIZING_LAST:
      g_assert_not_reached ();
      break;
    }

  /* FIXME all this code sort of pretends that duplicate buttons
   * with the same function are allowed, but that breaks the
   * code in frames.c, so isn't really allowed right now.
   * Would need left_close_rect, right_close_rect, etc.
   */
  
  /* Init all button rects to 0, lame hack */
  memset (ADDRESS_OF_BUTTON_RECTS (fgeom), '\0',
          LENGTH_OF_BUTTON_RECTS);
  
  n_left = 0;
  n_right = 0;
  n_left_spacers = 0;
  n_right_spacers = 0;

  if (!layout->hide_buttons)
    {
      /* Try to fill in rects */
      for (i = 0; i < MAX_BUTTONS_PER_CORNER && button_layout->left_buttons[i] != META_BUTTON_FUNCTION_LAST; i++)
        {
          left_func_rects[n_left] = rect_for_function (fgeom, flags,
                                                       button_layout->left_buttons[i],
                                                       theme);
          if (left_func_rects[n_left] != NULL)
            {
              left_buttons_has_spacer[n_left] = button_layout->left_buttons_has_spacer[i];
              if (button_layout->left_buttons_has_spacer[i])
                ++n_left_spacers;

              ++n_left;
            }
        }
      
      for (i = 0; i < MAX_BUTTONS_PER_CORNER && button_layout->right_buttons[i] != META_BUTTON_FUNCTION_LAST; i++)
        {
          right_func_rects[n_right] = rect_for_function (fgeom, flags,
                                                         button_layout->right_buttons[i],
                                                         theme);
          if (right_func_rects[n_right] != NULL)
            {
              right_buttons_has_spacer[n_right] = button_layout->right_buttons_has_spacer[i];
              if (button_layout->right_buttons_has_spacer[i])
                ++n_right_spacers;

              ++n_right;
            }
        }
    }

  for (i = 0; i < MAX_BUTTONS_PER_CORNER; i++)
    {
      left_bg_rects[i] = NULL;
      right_bg_rects[i] = NULL;
    }

  for (i = 0; i < n_left; i++)
    {
      if (i == 0) /* prefer left background if only one button */
        left_bg_rects[i] = &fgeom->left_left_background;
      else if (i == (n_left - 1))
        left_bg_rects[i] = &fgeom->left_right_background;
      else
        left_bg_rects[i] = &fgeom->left_middle_backgrounds[i - 1];
    }

  for (i = 0; i < n_right; i++)
    {
      /* prefer right background if only one button */
      if (i == (n_right - 1))
        right_bg_rects[i] = &fgeom->right_right_background;
      else if (i == 0)
        right_bg_rects[i] = &fgeom->right_left_background;
      else
        right_bg_rects[i] = &fgeom->right_middle_backgrounds[i - 1];
    }
  
  /* Be sure buttons fit */
  while (n_left > 0 || n_right > 0)
    {
      int space_used_by_buttons;
      int space_available;

      space_available = fgeom->width - layout->left_titlebar_edge - layout->right_titlebar_edge;
      
      space_used_by_buttons = 0;

      space_used_by_buttons += button_width * n_left;
      space_used_by_buttons += (button_width * 0.75) * n_left_spacers;
      space_used_by_buttons += layout->button_border.left * n_left;
      space_used_by_buttons += layout->button_border.right * n_left;

      space_used_by_buttons += button_width * n_right;
      space_used_by_buttons += (button_width * 0.75) * n_right_spacers;
      space_used_by_buttons += layout->button_border.left * n_right;
      space_used_by_buttons += layout->button_border.right * n_right;

      if (space_used_by_buttons <= space_available)
        break; /* Everything fits, bail out */
      
      /* First try to remove separators */
      if (n_left_spacers > 0)
        {
          left_buttons_has_spacer[--n_left_spacers] = FALSE;
          continue;
        }
      else if (n_right_spacers > 0)
        {
          right_buttons_has_spacer[--n_right_spacers] = FALSE;
          continue;
        }

      /* Otherwise we need to shave out a button. Shave
       * above, stick, shade, min, max, close, then menu (menu is most useful);
       * prefer the default button locations.
       */
      if (strip_button (left_func_rects, left_bg_rects,
                        &n_left, &fgeom->above_rect))
        continue;
      else if (strip_button (right_func_rects, right_bg_rects,
                             &n_right, &fgeom->above_rect))
        continue;
      else if (strip_button (left_func_rects, left_bg_rects,
                        &n_left, &fgeom->stick_rect))
        continue;
      else if (strip_button (right_func_rects, right_bg_rects,
                             &n_right, &fgeom->stick_rect))
        continue;
      else if (strip_button (left_func_rects, left_bg_rects,
                        &n_left, &fgeom->shade_rect))
        continue;
      else if (strip_button (right_func_rects, right_bg_rects,
                             &n_right, &fgeom->shade_rect))
        continue;
      else if (strip_button (left_func_rects, left_bg_rects,
                        &n_left, &fgeom->min_rect))
        continue;
      else if (strip_button (right_func_rects, right_bg_rects,
                             &n_right, &fgeom->min_rect))
        continue;
      else if (strip_button (left_func_rects, left_bg_rects,
                             &n_left, &fgeom->max_rect))
        continue;
      else if (strip_button (right_func_rects, right_bg_rects,
                             &n_right, &fgeom->max_rect))
        continue;
      else if (strip_button (left_func_rects, left_bg_rects,
                             &n_left, &fgeom->close_rect))
        continue;
      else if (strip_button (right_func_rects, right_bg_rects,
                             &n_right, &fgeom->close_rect))
        continue;
      else if (strip_button (right_func_rects, right_bg_rects,
                             &n_right, &fgeom->menu_rect))
        continue;
      else if (strip_button (left_func_rects, left_bg_rects,
                             &n_left, &fgeom->menu_rect))
        continue;
      else
        {
          meta_bug ("Could not find a button to strip. n_left = %d n_right = %d\n",
                    n_left, n_right);
        }
    }
  
  /* center buttons vertically */
  button_y = (fgeom->top_height -
              (button_height + layout->button_border.top + layout->button_border.bottom)) / 2 + layout->button_border.top;

  /* right edge of farthest-right button */
  x = width - layout->right_titlebar_edge;
  
  i = n_right - 1;
  while (i >= 0)
    {
      MetaButtonSpace *rect;

      if (x < 0) /* if we go negative, leave the buttons we don't get to as 0-width */
        break;
      
      rect = right_func_rects[i];
      rect->visible.x = x - layout->button_border.right - button_width;
      if (right_buttons_has_spacer[i])
        rect->visible.x -= (button_width * 0.75);

      rect->visible.y = button_y;
      rect->visible.width = button_width;
      rect->visible.height = button_height;

      if (flags & META_FRAME_MAXIMIZED)
        {
          rect->clickable.x = rect->visible.x;
          rect->clickable.y = 0;
          rect->clickable.width = rect->visible.width;
          rect->clickable.height = button_height + button_y;

          if (i == n_right - 1)
            rect->clickable.width += layout->right_titlebar_edge + layout->right_width + layout->button_border.right;

        }
      else
        g_memmove (&(rect->clickable), &(rect->visible), sizeof(rect->clickable));

      *(right_bg_rects[i]) = rect->visible;
      
      x = rect->visible.x - layout->button_border.left;
      
      --i;
    }

  /* save right edge of titlebar for later use */
  title_right_edge = x - layout->title_border.right;

  /* Now x changes to be position from the left and we go through
   * the left-side buttons
   */
  x = layout->left_titlebar_edge;
  for (i = 0; i < n_left; i++)
    {
      MetaButtonSpace *rect;

      rect = left_func_rects[i];
      
      rect->visible.x = x + layout->button_border.left;
      rect->visible.y = button_y;
      rect->visible.width = button_width;
      rect->visible.height = button_height;

      if (flags & META_FRAME_MAXIMIZED)
        {
          if (i==0)
            {
              rect->clickable.x = 0;
              rect->clickable.width = button_width + x;
            }
          else
            {
              rect->clickable.x = rect->visible.x;
              rect->clickable.width = button_width;
            }

            rect->clickable.y = 0;
            rect->clickable.height = button_height + button_y;
          }
        else
          g_memmove (&(rect->clickable), &(rect->visible), sizeof(rect->clickable));


      x = rect->visible.x + rect->visible.width + layout->button_border.right;
      if (left_buttons_has_spacer[i])
        x += (button_width * 0.75);

      *(left_bg_rects[i]) = rect->visible;
    }

  /* We always fill as much vertical space as possible with title rect,
   * rather than centering it like the buttons
   */
  fgeom->title_rect.x = x + layout->title_border.left;
  fgeom->title_rect.y = layout->title_border.top;
  fgeom->title_rect.width = title_right_edge - fgeom->title_rect.x;
  fgeom->title_rect.height = fgeom->top_height - layout->title_border.top - layout->title_border.bottom;

  /* Nuke title if it won't fit */
  if (fgeom->title_rect.width < 0 ||
      fgeom->title_rect.height < 0)
    {
      fgeom->title_rect.width = 0;
      fgeom->title_rect.height = 0;
    }

  if (flags & META_FRAME_SHADED)
    min_size_for_rounding = 0;
  else
    min_size_for_rounding = 5;
  
  fgeom->top_left_corner_rounded_radius = 0;
  fgeom->top_right_corner_rounded_radius = 0;
  fgeom->bottom_left_corner_rounded_radius = 0;
  fgeom->bottom_right_corner_rounded_radius = 0;

  if (fgeom->top_height + fgeom->left_width >= min_size_for_rounding)
    fgeom->top_left_corner_rounded_radius = layout->top_left_corner_rounded_radius;
  if (fgeom->top_height + fgeom->right_width >= min_size_for_rounding)
    fgeom->top_right_corner_rounded_radius = layout->top_right_corner_rounded_radius;

  if (fgeom->bottom_height + fgeom->left_width >= min_size_for_rounding)
    fgeom->bottom_left_corner_rounded_radius = layout->bottom_left_corner_rounded_radius;
  if (fgeom->bottom_height + fgeom->right_width >= min_size_for_rounding)
    fgeom->bottom_right_corner_rounded_radius = layout->bottom_right_corner_rounded_radius;
}

MetaGradientSpec*
meta_gradient_spec_new (MetaGradientType type)
{
  MetaGradientSpec *spec;

  spec = g_new (MetaGradientSpec, 1);

  spec->type = type;
  spec->color_specs = NULL;

  return spec;
}

static void
free_color_spec (gpointer spec, gpointer user_data)
{
  meta_color_spec_free (spec);
}

void
meta_gradient_spec_free (MetaGradientSpec *spec)
{
  g_return_if_fail (spec != NULL);

  g_slist_foreach (spec->color_specs, free_color_spec, NULL);
  g_slist_free (spec->color_specs);
  
  DEBUG_FILL_STRUCT (spec);
  g_free (spec);
}

GdkPixbuf*
meta_gradient_spec_render (const MetaGradientSpec *spec,
                           GtkWidget              *widget,
                           int                     width,
                           int                     height)
{
  int n_colors;
  GdkColor *colors;
  GSList *tmp;
  int i;
  GdkPixbuf *pixbuf;

  n_colors = g_slist_length (spec->color_specs);

  if (n_colors == 0)
    return NULL;

  colors = g_new (GdkColor, n_colors);

  i = 0;
  tmp = spec->color_specs;
  while (tmp != NULL)
    {
      meta_color_spec_render (tmp->data, widget, &colors[i]);

      tmp = tmp->next;
      ++i;
    }

  pixbuf = meta_gradient_create_multi (width, height,
                                       colors, n_colors,
                                       spec->type);

  g_free (colors);

  return pixbuf;
}

gboolean
meta_gradient_spec_validate (MetaGradientSpec *spec,
                             GError          **error)
{
  g_return_val_if_fail (spec != NULL, FALSE);
  
  if (g_slist_length (spec->color_specs) < 2)
    {
      g_set_error (error, META_THEME_ERROR,
                   META_THEME_ERROR_FAILED,
                   _("Gradients should have at least two colors"));
      return FALSE;
    }

  return TRUE;
}

MetaAlphaGradientSpec*
meta_alpha_gradient_spec_new (MetaGradientType       type,
                              int                    n_alphas)
{
  MetaAlphaGradientSpec *spec;

  g_return_val_if_fail (n_alphas > 0, NULL);
  
  spec = g_new0 (MetaAlphaGradientSpec, 1);

  spec->type = type;
  spec->alphas = g_new0 (unsigned char, n_alphas);
  spec->n_alphas = n_alphas;

  return spec;
}

void
meta_alpha_gradient_spec_free (MetaAlphaGradientSpec *spec)
{
  g_return_if_fail (spec != NULL);

  g_free (spec->alphas);
  g_free (spec);
}

MetaColorSpec*
meta_color_spec_new (MetaColorSpecType type)
{
  MetaColorSpec *spec;
  MetaColorSpec dummy;
  int size;

  size = G_STRUCT_OFFSET (MetaColorSpec, data);

  switch (type)
    {
    case META_COLOR_SPEC_BASIC:
      size += sizeof (dummy.data.basic);
      break;

    case META_COLOR_SPEC_GTK:
      size += sizeof (dummy.data.gtk);
      break;

    case META_COLOR_SPEC_BLEND:
      size += sizeof (dummy.data.blend);
      break;

    case META_COLOR_SPEC_SHADE:
      size += sizeof (dummy.data.shade);
      break;
    }

  spec = g_malloc0 (size);

  spec->type = type;

  return spec;
}

void
meta_color_spec_free (MetaColorSpec *spec)
{
  g_return_if_fail (spec != NULL);

  switch (spec->type)
    {
    case META_COLOR_SPEC_BASIC:
      DEBUG_FILL_STRUCT (&spec->data.basic);
      break;

    case META_COLOR_SPEC_GTK:
      DEBUG_FILL_STRUCT (&spec->data.gtk);
      break;

    case META_COLOR_SPEC_BLEND:
      if (spec->data.blend.foreground)
        meta_color_spec_free (spec->data.blend.foreground);
      if (spec->data.blend.background)
        meta_color_spec_free (spec->data.blend.background);
      DEBUG_FILL_STRUCT (&spec->data.blend);
      break;

    case META_COLOR_SPEC_SHADE:
      if (spec->data.shade.base)
        meta_color_spec_free (spec->data.shade.base);
      DEBUG_FILL_STRUCT (&spec->data.shade);
      break;
    }

  g_free (spec);
}

MetaColorSpec*
meta_color_spec_new_from_string (const char *str,
                                 GError    **err)
{
  MetaColorSpec *spec;

  spec = NULL;
  
  if (str[0] == 'g' && str[1] == 't' && str[2] == 'k' && str[3] == ':')
    {
      /* GTK color */
      const char *bracket;
      const char *end_bracket;
      char *tmp;
      GtkStateType state;
      MetaGtkColorComponent component;
      
      bracket = str;
      while (*bracket && *bracket != '[')
        ++bracket;

      if (*bracket == '\0')
        {
          g_set_error (err, META_THEME_ERROR,
                       META_THEME_ERROR_FAILED,
                       _("GTK color specification must have the state in brackets, e.g. gtk:fg[NORMAL] where NORMAL is the state; could not parse \"%s\""),
                       str);
          return NULL;
        }

      end_bracket = bracket;
      ++end_bracket;
      while (*end_bracket && *end_bracket != ']')
        ++end_bracket;
      
      if (*end_bracket == '\0')
        {
          g_set_error (err, META_THEME_ERROR,
                       META_THEME_ERROR_FAILED,
                       _("GTK color specification must have a close bracket after the state, e.g. gtk:fg[NORMAL] where NORMAL is the state; could not parse \"%s\""),
                       str);
          return NULL;
        }

      tmp = g_strndup (bracket + 1, end_bracket - bracket - 1);
      state = meta_gtk_state_from_string (tmp);
      if (((int) state) == -1)
        {
          g_set_error (err, META_THEME_ERROR,
                       META_THEME_ERROR_FAILED,
                       _("Did not understand state \"%s\" in color specification"),
                       tmp);
          g_free (tmp);
          return NULL;
        }
      g_free (tmp);
      
      tmp = g_strndup (str + 4, bracket - str - 4);
      component = meta_color_component_from_string (tmp);
      if (component == META_GTK_COLOR_LAST)
        {
          g_set_error (err, META_THEME_ERROR,
                       META_THEME_ERROR_FAILED,
                       _("Did not understand color component \"%s\" in color specification"),
                       tmp);
          g_free (tmp);
          return NULL;
        }
      g_free (tmp);

      spec = meta_color_spec_new (META_COLOR_SPEC_GTK);
      spec->data.gtk.state = state;
      spec->data.gtk.component = component;
      g_assert (spec->data.gtk.state < N_GTK_STATES);
      g_assert (spec->data.gtk.component < META_GTK_COLOR_LAST);
    }
  else if (str[0] == 'b' && str[1] == 'l' && str[2] == 'e' && str[3] == 'n' &&
           str[4] == 'd' && str[5] == '/')
    {
      /* blend */
      char **split;
      double alpha;
      char *end;
      MetaColorSpec *fg;
      MetaColorSpec *bg;
      
      split = g_strsplit (str, "/", 4);
      
      if (split[0] == NULL || split[1] == NULL ||
          split[2] == NULL || split[3] == NULL)
        {
          g_set_error (err, META_THEME_ERROR,
                       META_THEME_ERROR_FAILED,
                       _("Blend format is \"blend/bg_color/fg_color/alpha\", \"%s\" does not fit the format"),
                       str);
          g_strfreev (split);
          return NULL;
        }

      alpha = g_ascii_strtod (split[3], &end);
      if (end == split[3])
        {
          g_set_error (err, META_THEME_ERROR,
                       META_THEME_ERROR_FAILED,
                       _("Could not parse alpha value \"%s\" in blended color"),
                       split[3]);
          g_strfreev (split);
          return NULL;
        }

      if (alpha < (0.0 - 1e6) || alpha > (1.0 + 1e6))
        {
          g_set_error (err, META_THEME_ERROR,
                       META_THEME_ERROR_FAILED,
                       _("Alpha value \"%s\" in blended color is not between 0.0 and 1.0"),
                       split[3]);
          g_strfreev (split);
          return NULL;
        }
      
      fg = NULL;
      bg = NULL;

      bg = meta_color_spec_new_from_string (split[1], err);
      if (bg == NULL)
        {
          g_strfreev (split);
          return NULL;
        }

      fg = meta_color_spec_new_from_string (split[2], err);
      if (fg == NULL)
        {
          meta_color_spec_free (bg);
          g_strfreev (split);
          return NULL;
        }

      g_strfreev (split);
      
      spec = meta_color_spec_new (META_COLOR_SPEC_BLEND);
      spec->data.blend.alpha = alpha;
      spec->data.blend.background = bg;
      spec->data.blend.foreground = fg;
    }
  else if (str[0] == 's' && str[1] == 'h' && str[2] == 'a' && str[3] == 'd' &&
           str[4] == 'e' && str[5] == '/')
    {
      /* shade */
      char **split;
      double factor;
      char *end;
      MetaColorSpec *base;
      
      split = g_strsplit (str, "/", 3);
      
      if (split[0] == NULL || split[1] == NULL ||
          split[2] == NULL)
        {
          g_set_error (err, META_THEME_ERROR,
                       META_THEME_ERROR_FAILED,
                       _("Shade format is \"shade/base_color/factor\", \"%s\" does not fit the format"),
                       str);
          g_strfreev (split);
          return NULL;
        }

      factor = g_ascii_strtod (split[2], &end);
      if (end == split[2])
        {
          g_set_error (err, META_THEME_ERROR,
                       META_THEME_ERROR_FAILED,
                       _("Could not parse shade factor \"%s\" in shaded color"),
                       split[2]);
          g_strfreev (split);
          return NULL;
        }

      if (factor < (0.0 - 1e6))
        {
          g_set_error (err, META_THEME_ERROR,
                       META_THEME_ERROR_FAILED,
                       _("Shade factor \"%s\" in shaded color is negative"),
                       split[2]);
          g_strfreev (split);
          return NULL;
        }
      
      base = NULL;

      base = meta_color_spec_new_from_string (split[1], err);
      if (base == NULL)
        {
          g_strfreev (split);
          return NULL;
        }

      g_strfreev (split);
      
      spec = meta_color_spec_new (META_COLOR_SPEC_SHADE);
      spec->data.shade.factor = factor;
      spec->data.shade.base = base;
    }
  else
    {
      spec = meta_color_spec_new (META_COLOR_SPEC_BASIC);
      
      if (!gdk_color_parse (str, &spec->data.basic.color))
        {
          g_set_error (err, META_THEME_ERROR,
                       META_THEME_ERROR_FAILED,
                       _("Could not parse color \"%s\""),
                       str);
          meta_color_spec_free (spec);
          return NULL;
        }
    }

  g_assert (spec);
  
  return spec;
}

MetaColorSpec*
meta_color_spec_new_gtk (MetaGtkColorComponent component,
                         GtkStateType          state)
{
  MetaColorSpec *spec;

  spec = meta_color_spec_new (META_COLOR_SPEC_GTK);

  spec->data.gtk.component = component;
  spec->data.gtk.state = state;

  return spec;
}

void
meta_color_spec_render (MetaColorSpec *spec,
                        GtkWidget     *widget,
                        GdkColor      *color)
{
  GtkStyle *style;

  style = gtk_widget_get_style (widget);

  g_return_if_fail (spec != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (style != NULL);

  switch (spec->type)
    {
    case META_COLOR_SPEC_BASIC:
      *color = spec->data.basic.color;
      break;

    case META_COLOR_SPEC_GTK:
      switch (spec->data.gtk.component)
        {
        case META_GTK_COLOR_BG:
          *color = style->bg[spec->data.gtk.state];
          break;
        case META_GTK_COLOR_FG:
          *color = style->fg[spec->data.gtk.state];
          break;
        case META_GTK_COLOR_BASE:
          *color = style->base[spec->data.gtk.state];
          break;
        case META_GTK_COLOR_TEXT:
          *color = style->text[spec->data.gtk.state];
          break;
        case META_GTK_COLOR_LIGHT:
          *color = style->light[spec->data.gtk.state];
          break;
        case META_GTK_COLOR_DARK:
          *color = style->dark[spec->data.gtk.state];
          break;
        case META_GTK_COLOR_MID:
          *color = style->mid[spec->data.gtk.state];
          break;
        case META_GTK_COLOR_TEXT_AA:
          *color = style->text_aa[spec->data.gtk.state];
          break;
        case META_GTK_COLOR_LAST:
          g_assert_not_reached ();
          break;
        }
      break;

    case META_COLOR_SPEC_BLEND:
      {
        GdkColor bg, fg;

        meta_color_spec_render (spec->data.blend.background, widget, &bg);
        meta_color_spec_render (spec->data.blend.foreground, widget, &fg);

        color_composite (&bg, &fg, spec->data.blend.alpha, 
                         &spec->data.blend.color);

        *color = spec->data.blend.color;
      }
      break;

    case META_COLOR_SPEC_SHADE:
      {
        meta_color_spec_render (spec->data.shade.base, widget, 
                                &spec->data.shade.color);
            
        gtk_style_shade (&spec->data.shade.color, 
                         &spec->data.shade.color, spec->data.shade.factor);

        *color = spec->data.shade.color;
      }
      break;
    }
}

/**
 * Represents an operation as a string.
 *
 * \param type  an operation, such as addition
 * \return  a string, such as "+"
 */
static const char*
op_name (PosOperatorType type)
{
  switch (type)
    {
    case POS_OP_ADD:
      return "+";
    case POS_OP_SUBTRACT:
      return "-";
    case POS_OP_MULTIPLY:
      return "*";
    case POS_OP_DIVIDE:
      return "/";
    case POS_OP_MOD:
      return "%";
    case POS_OP_MAX:
      return "`max`";
    case POS_OP_MIN:
      return "`min`";
    case POS_OP_NONE:
      break;
    }

  return "<unknown>";
}

/**
 * Parses a string and returns an operation.
 *
 * \param p  a pointer into a string representing an operation; part of an
 *           expression somewhere, so not null-terminated
 * \param len  set to the length of the string found. Set to 0 if none is.
 * \return  the operation found. If none was, returns POS_OP_NONE.
 */
static PosOperatorType
op_from_string (const char *p,
                int        *len)
{
  *len = 0;
  
  switch (*p)
    {
    case '+':
      *len = 1;
      return POS_OP_ADD;
    case '-':
      *len = 1;
      return POS_OP_SUBTRACT;
    case '*':
      *len = 1;
      return POS_OP_MULTIPLY;
    case '/':
      *len = 1;
      return POS_OP_DIVIDE;
    case '%':
      *len = 1;
      return POS_OP_MOD;

    case '`':
      if (p[0] == '`' &&
          p[1] == 'm' &&
          p[2] == 'a' &&
          p[3] == 'x' &&
          p[4] == '`')
        {
          *len = 5;
          return POS_OP_MAX;
        }
      else if (p[0] == '`' &&
               p[1] == 'm' &&
               p[2] == 'i' &&
               p[3] == 'n' &&
               p[4] == '`')
        {
          *len = 5;
          return POS_OP_MIN;
        }
    }

  return POS_OP_NONE;
}

/**
 * Frees an array of tokens. All the tokens and their associated memory
 * will be freed.
 *
 * \param tokens  an array of tokens to be freed
 * \param n_tokens  how many tokens are in the array.
 */
static void
free_tokens (PosToken *tokens,
             int       n_tokens)
{
  int i;

  /* n_tokens can be 0 since tokens may have been allocated more than
   * it was initialized
   */

  for (i = 0; i < n_tokens; i++)
    if (tokens[i].type == POS_TOKEN_VARIABLE)
      g_free (tokens[i].d.v.name);

  g_free (tokens);
}

/**
 * Tokenises a number in an expression.
 *
 * \param p  a pointer into a string representing an operation; part of an
 *           expression somewhere, so not null-terminated
 * \param end_return  set to a pointer to the end of the number found; but
 *                    not updated if no number was found at all
 * \param next  set to either an integer or a float token
 * \param[out] err  set to the problem if there was a problem
 * \return TRUE if a valid number was found, FALSE otherwise (and "err" will
 *         have been set)
 *
 * \bug The "while (*start)..." part: what's wrong with strchr-ish things?
 * \bug The name is wrong: it doesn't parse anything.
 * \ingroup tokenizer
 */
static gboolean
parse_number (const char  *p,
              const char **end_return,
              PosToken    *next,
              GError     **err)
{
  const char *start = p;
  char *end;
  gboolean is_float;
  char *num_str;

  while (*p && (*p == '.' || g_ascii_isdigit (*p)))
    ++p;

  if (p == start)
    {
      char buf[7] = { '\0' };
      buf[g_unichar_to_utf8 (g_utf8_get_char (p), buf)] = '\0';
      g_set_error (err, META_THEME_ERROR,
                   META_THEME_ERROR_BAD_CHARACTER,
                   _("Coordinate expression contains character '%s' which is not allowed"),
                   buf);
      return FALSE;
    }

  *end_return = p;

  /* we need this to exclude floats like "1e6" */
  num_str = g_strndup (start, p - start);
  start = num_str;
  is_float = FALSE;
  while (*start)
    {
      if (*start == '.')
        is_float = TRUE;
      ++start;
    }

  if (is_float)
    {
      next->type = POS_TOKEN_DOUBLE;
      next->d.d.val = g_ascii_strtod (num_str, &end);

      if (end == num_str)
        {
          g_set_error (err, META_THEME_ERROR,
                       META_THEME_ERROR_FAILED,
                       _("Coordinate expression contains floating point number '%s' which could not be parsed"),
                       num_str);
          g_free (num_str);
          return FALSE;
        }
    }
  else
    {
      next->type = POS_TOKEN_INT;
      next->d.i.val = strtol (num_str, &end, 10);
      if (end == num_str)
        {
          g_set_error (err, META_THEME_ERROR,
                       META_THEME_ERROR_FAILED,
                       _("Coordinate expression contains integer '%s' which could not be parsed"),
                       num_str);
          g_free (num_str);
          return FALSE;
        }
    }

  g_free (num_str);

  g_assert (next->type == POS_TOKEN_INT || next->type == POS_TOKEN_DOUBLE);

  return TRUE;
}

/**
 * Whether a variable can validly appear as part of the name of a variable.
 */
#define IS_VARIABLE_CHAR(c) (g_ascii_isalpha ((c)) || (c) == '_')

#if 0
static void
debug_print_tokens (PosToken *tokens,
                    int       n_tokens)
{
  int i;
  
  for (i = 0; i < n_tokens; i++)
    {
      PosToken *t = &tokens[i];

      g_print (" ");

      switch (t->type)
        {
        case POS_TOKEN_INT:
          g_print ("\"%d\"", t->d.i.val);
          break;
        case POS_TOKEN_DOUBLE:
          g_print ("\"%g\"", t->d.d.val);
          break;
        case POS_TOKEN_OPEN_PAREN:
          g_print ("\"(\"");
          break;
        case POS_TOKEN_CLOSE_PAREN:
          g_print ("\")\"");
          break;
        case POS_TOKEN_VARIABLE:
          g_print ("\"%s\"", t->d.v.name);
          break;
        case POS_TOKEN_OPERATOR:
          g_print ("\"%s\"", op_name (t->d.o.op));
          break;
        }
    }

  g_print ("\n");
}
#endif

/**
 * Tokenises an expression.
 *
 * \param      expr        The expression
 * \param[out] tokens_p    The resulting tokens
 * \param[out] n_tokens_p  The number of resulting tokens
 * \param[out] err  set to the problem if there was a problem
 *
 * \return  True if the expression was successfully tokenised; false otherwise.
 *
 * \ingroup tokenizer
 */
static gboolean
pos_tokenize (const char  *expr,
              PosToken   **tokens_p,
              int         *n_tokens_p,
              GError     **err)
{
  PosToken *tokens;
  int n_tokens;
  int allocated;
  const char *p;
  
  *tokens_p = NULL;
  *n_tokens_p = 0;

  allocated = 3;
  n_tokens = 0;
  tokens = g_new (PosToken, allocated);

  p = expr;
  while (*p)
    {
      PosToken *next;
      int len;
      
      if (n_tokens == allocated)
        {
          allocated *= 2;
          tokens = g_renew (PosToken, tokens, allocated);
        }

      next = &tokens[n_tokens];

      switch (*p)
        {
        case '*':
        case '/':
        case '+':
        case '-': /* negative numbers aren't allowed so this is easy */
        case '%':
        case '`':
          next->type = POS_TOKEN_OPERATOR;
          next->d.o.op = op_from_string (p, &len);
          if (next->d.o.op != POS_OP_NONE)
            {
              ++n_tokens;
              p = p + (len - 1); /* -1 since we ++p later */
            }
          else
            {
              g_set_error (err, META_THEME_ERROR,
                           META_THEME_ERROR_FAILED,
                           _("Coordinate expression contained unknown operator at the start of this text: \"%s\""),
                           p);
              
              goto error;
            }
          break;

        case '(':
          next->type = POS_TOKEN_OPEN_PAREN;
          ++n_tokens;
          break;

        case ')':
          next->type = POS_TOKEN_CLOSE_PAREN;
          ++n_tokens;
          break;

        case ' ':
        case '\t':
        case '\n':		
          break;

        default:
          if (IS_VARIABLE_CHAR (*p))
            {
              /* Assume variable */
              const char *start = p;
              while (*p && IS_VARIABLE_CHAR (*p))
                ++p;
              g_assert (p != start);
              next->type = POS_TOKEN_VARIABLE;
              next->d.v.name = g_strndup (start, p - start);
              ++n_tokens;
              --p; /* since we ++p again at the end of while loop */
            }
          else
            {
              /* Assume number */
              const char *end;

              if (!parse_number (p, &end, next, err))
                goto error;

              ++n_tokens;
              p = end - 1; /* -1 since we ++p again at the end of while loop */
            }

          break;
        }

      ++p;
    }

  if (n_tokens == 0)
    {
      g_set_error (err, META_THEME_ERROR,
                   META_THEME_ERROR_FAILED,
                   _("Coordinate expression was empty or not understood"));

      goto error;
    }

  *tokens_p = tokens;
  *n_tokens_p = n_tokens;

  return TRUE;

 error:
  g_assert (err == NULL || *err != NULL);

  free_tokens (tokens, n_tokens);
  return FALSE;
}

/**
 * The type of a PosExpr: either integer, double, or an operation.
 * \ingroup parser
 */
typedef enum
{
  POS_EXPR_INT,
  POS_EXPR_DOUBLE,
  POS_EXPR_OPERATOR
} PosExprType;

/**
 * Type and value of an expression in a parsed sequence. We don't
 * keep expressions in a tree; if this is of type POS_EXPR_OPERATOR,
 * the arguments of the operator will be in the array positions
 * immediately preceding and following this operator; they cannot
 * themselves be operators.
 *
 * \bug operator is char; it should really be of PosOperatorType.
 * \ingroup parser
 */
typedef struct
{
  PosExprType type;
  union
  {
    double double_val;
    int int_val;
    char operator;
  } d;
} PosExpr;

#if 0
static void
debug_print_exprs (PosExpr *exprs,
                   int      n_exprs)
{
  int i;

  for (i = 0; i < n_exprs; i++)
    {
      switch (exprs[i].type)
        {
        case POS_EXPR_INT:
          g_print (" %d", exprs[i].d.int_val);
          break;
        case POS_EXPR_DOUBLE:
          g_print (" %g", exprs[i].d.double_val);
          break;
        case POS_EXPR_OPERATOR:
          g_print (" %s", op_name (exprs[i].d.operator));
          break;
        }
    }
  g_print ("\n");
}
#endif

static gboolean
do_operation (PosExpr *a,
              PosExpr *b,
              PosOperatorType op,
              GError **err)
{
  /* Promote types to double if required */
  if (a->type == POS_EXPR_DOUBLE ||
      b->type == POS_EXPR_DOUBLE)
    {
      if (a->type != POS_EXPR_DOUBLE)
        {
          a->type = POS_EXPR_DOUBLE;
          a->d.double_val = a->d.int_val;
        }
      if (b->type != POS_EXPR_DOUBLE)
        {
          b->type = POS_EXPR_DOUBLE;
          b->d.double_val = b->d.int_val;
        }
    }

  g_assert (a->type == b->type);

  if (a->type == POS_EXPR_INT)
    {
      switch (op)
        {
        case POS_OP_MULTIPLY:
          a->d.int_val = a->d.int_val * b->d.int_val;
          break;
        case POS_OP_DIVIDE:
          if (b->d.int_val == 0)
            {
              g_set_error (err, META_THEME_ERROR,
                           META_THEME_ERROR_DIVIDE_BY_ZERO,
                           _("Coordinate expression results in division by zero"));
              return FALSE;
            }
          a->d.int_val = a->d.int_val / b->d.int_val;
          break;
        case POS_OP_MOD:
          if (b->d.int_val == 0)
            {
              g_set_error (err, META_THEME_ERROR,
                           META_THEME_ERROR_DIVIDE_BY_ZERO,
                           _("Coordinate expression results in division by zero"));
              return FALSE;
            }
          a->d.int_val = a->d.int_val % b->d.int_val;
          break;
        case POS_OP_ADD:
          a->d.int_val = a->d.int_val + b->d.int_val;
          break;
        case POS_OP_SUBTRACT:
          a->d.int_val = a->d.int_val - b->d.int_val;
          break;
        case POS_OP_MAX:
          a->d.int_val = MAX (a->d.int_val, b->d.int_val);
          break;
        case POS_OP_MIN:
          a->d.int_val = MIN (a->d.int_val, b->d.int_val);
          break;
        case POS_OP_NONE:
          g_assert_not_reached ();
          break;
        }
    }
  else if (a->type == POS_EXPR_DOUBLE)
    {
      switch (op)
        {
        case POS_OP_MULTIPLY:
          a->d.double_val = a->d.double_val * b->d.double_val;
          break;
        case POS_OP_DIVIDE:
          if (b->d.double_val == 0.0)
            {
              g_set_error (err, META_THEME_ERROR,
                           META_THEME_ERROR_DIVIDE_BY_ZERO,
                           _("Coordinate expression results in division by zero"));
              return FALSE;
            }
          a->d.double_val = a->d.double_val / b->d.double_val;
          break;
        case POS_OP_MOD:
          g_set_error (err, META_THEME_ERROR,
                       META_THEME_ERROR_MOD_ON_FLOAT,
                       _("Coordinate expression tries to use mod operator on a floating-point number"));
          return FALSE;
        case POS_OP_ADD:
          a->d.double_val = a->d.double_val + b->d.double_val;
          break;
        case POS_OP_SUBTRACT:
          a->d.double_val = a->d.double_val - b->d.double_val;
          break;
        case POS_OP_MAX:
          a->d.double_val = MAX (a->d.double_val, b->d.double_val);
          break;
        case POS_OP_MIN:
          a->d.double_val = MIN (a->d.double_val, b->d.double_val);
          break;
        case POS_OP_NONE:
          g_assert_not_reached ();
          break;
        }
    }
  else
    g_assert_not_reached ();

  return TRUE;
}

static gboolean
do_operations (PosExpr *exprs,
               int     *n_exprs,
               int      precedence,
               GError **err)
{
  int i;

#if 0
  g_print ("Doing prec %d ops on %d exprs\n", precedence, *n_exprs);
  debug_print_exprs (exprs, *n_exprs);
#endif

  i = 1;
  while (i < *n_exprs)
    {
      gboolean compress;

      /* exprs[i-1] first operand
       * exprs[i]   operator
       * exprs[i+1] second operand
       *
       * we replace first operand with result of mul/div/mod,
       * or skip over operator and second operand if we have
       * an add/subtract
       */

      if (exprs[i-1].type == POS_EXPR_OPERATOR)
        {
          g_set_error (err, META_THEME_ERROR,
                       META_THEME_ERROR_FAILED,
                       _("Coordinate expression has an operator \"%s\" where an operand was expected"),
                       op_name (exprs[i-1].d.operator));
          return FALSE;
        }

      if (exprs[i].type != POS_EXPR_OPERATOR)
        {
          g_set_error (err, META_THEME_ERROR,
                       META_THEME_ERROR_FAILED,
                       _("Coordinate expression had an operand where an operator was expected"));
          return FALSE;
        }

      if (i == (*n_exprs - 1))
        {
          g_set_error (err, META_THEME_ERROR,
                       META_THEME_ERROR_FAILED,
                       _("Coordinate expression ended with an operator instead of an operand"));
          return FALSE;
        }

      g_assert ((i+1) < *n_exprs);

      if (exprs[i+1].type == POS_EXPR_OPERATOR)
        {
          g_set_error (err, META_THEME_ERROR,
                       META_THEME_ERROR_FAILED,
                       _("Coordinate expression has operator \"%c\" following operator \"%c\" with no operand in between"),
                       exprs[i+1].d.operator,
                       exprs[i].d.operator);
          return FALSE;
        }

      compress = FALSE;

      switch (precedence)
        {
        case 2:
          switch (exprs[i].d.operator)
            {
            case POS_OP_DIVIDE:
            case POS_OP_MOD:
            case POS_OP_MULTIPLY:
              compress = TRUE;
              if (!do_operation (&exprs[i-1], &exprs[i+1],
                                 exprs[i].d.operator,
                                 err))
                return FALSE;
              break;
            }
          break;
        case 1:
          switch (exprs[i].d.operator)
            {
            case POS_OP_ADD:
            case POS_OP_SUBTRACT:
              compress = TRUE;
              if (!do_operation (&exprs[i-1], &exprs[i+1],
                                 exprs[i].d.operator,
                                 err))
                return FALSE;
              break;
            }
          break;
          /* I have no rationale at all for making these low-precedence */
        case 0:
          switch (exprs[i].d.operator)
            {
            case POS_OP_MAX:
            case POS_OP_MIN:
              compress = TRUE;
              if (!do_operation (&exprs[i-1], &exprs[i+1],
                                 exprs[i].d.operator,
                                 err))
                return FALSE;
              break;
            }
          break;
        }

      if (compress)
        {
          /* exprs[i-1] first operand (now result)
           * exprs[i]   operator
           * exprs[i+1] second operand
           * exprs[i+2] new operator
           *
           * we move new operator just after first operand
           */
          if ((i+2) < *n_exprs)
            {
              g_memmove (&exprs[i], &exprs[i+2],
                         sizeof (PosExpr) * (*n_exprs - i - 2));
            }

          *n_exprs -= 2;
        }
      else
        {
          /* Skip operator and next operand */
          i += 2;
        }
    }

  return TRUE;
}

/**
 * There is a predefined set of variables which can appear in an expression.
 * Here we take a token representing a variable, and return the current value
 * of that variable in a particular environment.
 * (The value is always an integer.)
 *
 * There are supposedly some circumstances in which this function can be
 * called from outside Metacity, in which case env->theme will be NULL, and
 * therefore we can't use it to find out quark values, so we do the comparison
 * using strcmp, which is slower.
 *
 * \param t  The token representing a variable
 * \param[out] result  The value of that variable; not set if the token did
 *                     not represent a known variable
 * \param env  The environment within which t should be evaluated
 * \param[out] err  set to the problem if there was a problem
 *
 * \return true if we found the variable asked for, false if we didn't
 *
 * \bug shouldn't t be const?
 * \bug we should perhaps consider some sort of lookup arrangement into an
 *      array; also, the duplication of code is unlovely; perhaps using glib
 *      string hashes instead of quarks would fix both problems?
 * \ingroup parser
 */
static gboolean
pos_eval_get_variable (PosToken                  *t,
                       int                       *result,
                       const MetaPositionExprEnv *env,
                       GError                   **err)
{
  if (env->theme)
    {
      if (t->d.v.name_quark == env->theme->quark_width)
        *result = env->rect.width;
      else if (t->d.v.name_quark == env->theme->quark_height)
        *result = env->rect.height;
      else if (env->object_width >= 0 &&
               t->d.v.name_quark == env->theme->quark_object_width)
        *result = env->object_width;
      else if (env->object_height >= 0 &&
               t->d.v.name_quark == env->theme->quark_object_height)
        *result = env->object_height;
      else if (t->d.v.name_quark == env->theme->quark_left_width)
        *result = env->left_width;
      else if (t->d.v.name_quark == env->theme->quark_right_width)
        *result = env->right_width;
      else if (t->d.v.name_quark == env->theme->quark_top_height)
        *result = env->top_height;
      else if (t->d.v.name_quark == env->theme->quark_bottom_height)
        *result = env->bottom_height;
      else if (t->d.v.name_quark == env->theme->quark_mini_icon_width)
        *result = env->mini_icon_width;
      else if (t->d.v.name_quark == env->theme->quark_mini_icon_height)
        *result = env->mini_icon_height;
      else if (t->d.v.name_quark == env->theme->quark_icon_width)
        *result = env->icon_width;
      else if (t->d.v.name_quark == env->theme->quark_icon_height)
        *result = env->icon_height;
      else if (t->d.v.name_quark == env->theme->quark_title_width)
        *result = env->title_width;
      else if (t->d.v.name_quark == env->theme->quark_title_height)
        *result = env->title_height;
      else if (t->d.v.name_quark == env->theme->quark_frame_x_center)
        *result = env->frame_x_center;
      else if (t->d.v.name_quark == env->theme->quark_frame_y_center)
        *result = env->frame_y_center;
      else
        {
          g_set_error (err, META_THEME_ERROR,
                       META_THEME_ERROR_UNKNOWN_VARIABLE,
                       _("Coordinate expression had unknown variable or constant \"%s\""),
                       t->d.v.name);
          return FALSE;
        }
    }
  else 
    {
      if (strcmp (t->d.v.name, "width") == 0)
        *result = env->rect.width;
      else if (strcmp (t->d.v.name, "height") == 0)
        *result = env->rect.height;
      else if (env->object_width >= 0 &&
               strcmp (t->d.v.name, "object_width") == 0)
        *result = env->object_width;
      else if (env->object_height >= 0 &&
               strcmp (t->d.v.name, "object_height") == 0)
        *result = env->object_height;
      else if (strcmp (t->d.v.name, "left_width") == 0)
        *result = env->left_width;
      else if (strcmp (t->d.v.name, "right_width") == 0)
        *result = env->right_width;
      else if (strcmp (t->d.v.name, "top_height") == 0)
        *result = env->top_height;
      else if (strcmp (t->d.v.name, "bottom_height") == 0)
        *result = env->bottom_height;
      else if (strcmp (t->d.v.name, "mini_icon_width") == 0)
        *result = env->mini_icon_width;
      else if (strcmp (t->d.v.name, "mini_icon_height") == 0)
        *result = env->mini_icon_height;
      else if (strcmp (t->d.v.name, "icon_width") == 0)
        *result = env->icon_width;
      else if (strcmp (t->d.v.name, "icon_height") == 0)
        *result = env->icon_height;
      else if (strcmp (t->d.v.name, "title_width") == 0)
        *result = env->title_width;
      else if (strcmp (t->d.v.name, "title_height") == 0)
        *result = env->title_height;
      else if (strcmp (t->d.v.name, "frame_x_center") == 0)
        *result = env->frame_x_center;
      else if (strcmp (t->d.v.name, "frame_y_center") == 0)
        *result = env->frame_y_center;
      else
        {
          g_set_error (err, META_THEME_ERROR,
                       META_THEME_ERROR_UNKNOWN_VARIABLE,
                       _("Coordinate expression had unknown variable or constant \"%s\""),
                       t->d.v.name);
          return FALSE;
        }
    }

  return TRUE;
}

/**
 * Evaluates a sequence of tokens within a particular environment context,
 * and returns the current value. May recur if parantheses are found.
 *
 * \param tokens  A list of tokens to evaluate.
 * \param n_tokens  How many tokens are in the list.
 * \param env  The environment context in which to evaluate the expression.
 * \param[out] result  The current value of the expression
 * 
 * \bug Yes, we really do reparse the expression every time it's evaluated.
 *      We should keep the parse tree around all the time and just
 *      run the new values through it.
 * \ingroup parser
 */
static gboolean
pos_eval_helper (PosToken                   *tokens,
                 int                         n_tokens,
                 const MetaPositionExprEnv  *env,
                 PosExpr                    *result,
                 GError                    **err)
{
  /* Lazy-ass hardcoded limit on number of terms in expression */
#define MAX_EXPRS 32
  int paren_level;
  int first_paren;
  int i;
  PosExpr exprs[MAX_EXPRS];
  int n_exprs;
  int precedence;
  
  /* Our first goal is to get a list of PosExpr, essentially
   * substituting variables and handling parentheses.
   */

  first_paren = 0;
  paren_level = 0;
  n_exprs = 0;
  for (i = 0; i < n_tokens; i++)
    {
      PosToken *t = &tokens[i];

      if (n_exprs >= MAX_EXPRS)
        {
          g_set_error (err, META_THEME_ERROR,
                       META_THEME_ERROR_FAILED,
                       _("Coordinate expression parser overflowed its buffer."));
          return FALSE;
        }

      if (paren_level == 0)
        {
          switch (t->type)
            {
            case POS_TOKEN_INT:
              exprs[n_exprs].type = POS_EXPR_INT;
              exprs[n_exprs].d.int_val = t->d.i.val;
              ++n_exprs;
              break;

            case POS_TOKEN_DOUBLE:
              exprs[n_exprs].type = POS_EXPR_DOUBLE;
              exprs[n_exprs].d.double_val = t->d.d.val;
              ++n_exprs;
              break;

            case POS_TOKEN_OPEN_PAREN:
              ++paren_level;
              if (paren_level == 1)
                first_paren = i;
              break;

            case POS_TOKEN_CLOSE_PAREN:
              g_set_error (err, META_THEME_ERROR,
                           META_THEME_ERROR_BAD_PARENS,
                           _("Coordinate expression had a close parenthesis with no open parenthesis"));
              return FALSE;

            case POS_TOKEN_VARIABLE:
              exprs[n_exprs].type = POS_EXPR_INT;

              /* FIXME we should just dump all this crap
               * in a hash, maybe keep width/height out
               * for optimization purposes
               */
              if (!pos_eval_get_variable (t, &exprs[n_exprs].d.int_val, env, err))
                return FALSE;
                  
              ++n_exprs;
              break;

            case POS_TOKEN_OPERATOR:
              exprs[n_exprs].type = POS_EXPR_OPERATOR;
              exprs[n_exprs].d.operator = t->d.o.op;
              ++n_exprs;
              break;
            }
        }
      else
        {
          g_assert (paren_level > 0);

          switch (t->type)
            {
            case POS_TOKEN_INT:
            case POS_TOKEN_DOUBLE:
            case POS_TOKEN_VARIABLE:
            case POS_TOKEN_OPERATOR:
              break;

            case POS_TOKEN_OPEN_PAREN:
              ++paren_level;
              break;

            case POS_TOKEN_CLOSE_PAREN:
              if (paren_level == 1)
                {
                  /* We closed a toplevel paren group, so recurse */
                  if (!pos_eval_helper (&tokens[first_paren+1],
                                        i - first_paren - 1,
                                        env,
                                        &exprs[n_exprs],
                                        err))
                    return FALSE;

                  ++n_exprs;
                }

              --paren_level;
              break;

            }
        }
    }

  if (paren_level > 0)
    {
      g_set_error (err, META_THEME_ERROR,
                   META_THEME_ERROR_BAD_PARENS,
                   _("Coordinate expression had an open parenthesis with no close parenthesis"));
      return FALSE;
    }

  /* Now we have no parens and no vars; so we just do all the multiplies
   * and divides, then all the add and subtract.
   */
  if (n_exprs == 0)
    {
      g_set_error (err, META_THEME_ERROR,
                   META_THEME_ERROR_FAILED,
                   _("Coordinate expression doesn't seem to have any operators or operands"));
      return FALSE;
    }

  /* precedence 1 ops */
  precedence = 2;
  while (precedence >= 0)
    {
      if (!do_operations (exprs, &n_exprs, precedence, err))
        return FALSE;
      --precedence;
    }

  g_assert (n_exprs == 1);

  *result = *exprs;

  return TRUE;
}

/*
 *   expr = int | double | expr * expr | expr / expr |
 *          expr + expr | expr - expr | (expr)
 *
 *   so very not worth fooling with bison, yet so very painful by hand.
 */
/**
 * Evaluates an expression.
 *
 * \param spec  The expression to evaluate.
 * \param env   The environment context to evaluate the expression in.
 * \param[out] val_p  The integer value of the expression; if the expression
 *                    is of type float, this will be rounded. If we return
 *                    FALSE because the expression is invalid, this will be
 *                    zero.
 * \param[out] err    The error, if anything went wrong.
 *
 * \return  True if we evaluated the expression successfully; false otherwise.
 *
 * \bug Shouldn't spec be const?
 * \ingroup parser
 */
static gboolean
pos_eval (MetaDrawSpec              *spec,
          const MetaPositionExprEnv *env,
          int                       *val_p,
          GError                   **err)
{
  PosExpr expr;

  *val_p = 0;

  if (pos_eval_helper (spec->tokens, spec->n_tokens, env, &expr, err))
    {
      switch (expr.type)
        {
        case POS_EXPR_INT:
          *val_p = expr.d.int_val;
          break;
        case POS_EXPR_DOUBLE:
          *val_p = expr.d.double_val;
          break;
        case POS_EXPR_OPERATOR:
          g_assert_not_reached ();
          break;
        }
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

/* We always return both X and Y, but only one will be meaningful in
 * most contexts.
 */

gboolean
meta_parse_position_expression (MetaDrawSpec              *spec,
                                const MetaPositionExprEnv *env,
                                int                       *x_return,
                                int                       *y_return,
                                GError                   **err)
{
  /* All positions are in a coordinate system with x, y at the origin.
   * The expression can have -, +, *, / as operators, floating point
   * or integer constants, and the variables "width" and "height" and
   * optionally "object_width" and object_height". Negative numbers
   * aren't allowed.
   */
  int val;

  if (spec->constant)
    val = spec->value;
  else
    {
      if (pos_eval (spec, env, &spec->value, err) == FALSE)
        {
          g_assert (err == NULL || *err != NULL);
          return FALSE;
        }

      val = spec->value;
    }

  if (x_return)
    *x_return = env->rect.x + val;
  if (y_return)
    *y_return = env->rect.y + val;

  return TRUE;
}


gboolean
meta_parse_size_expression (MetaDrawSpec              *spec,
                            const MetaPositionExprEnv *env,
                            int                       *val_return,
                            GError                   **err)
{
  int val;

  if (spec->constant)
    val = spec->value;
  else 
    {
      if (pos_eval (spec, env, &spec->value, err) == FALSE)
        {
          g_assert (err == NULL || *err != NULL);
          return FALSE;
        }

      val = spec->value;
    }

  if (val_return)
    *val_return = MAX (val, 1); /* require that sizes be at least 1x1 */

  return TRUE;
}

/* To do this we tokenize, replace variable tokens
 * that are constants, then reassemble. The purpose
 * here is to optimize expressions so we don't do hash
 * lookups to eval them. Obviously it's a tradeoff that
 * slows down theme load times.
 */
gboolean
meta_theme_replace_constants (MetaTheme   *theme,
                              PosToken    *tokens,
                              int          n_tokens,
                              GError     **err)
{
  int i;
  double dval;
  int ival;
  gboolean is_constant = TRUE;
  
  /* Loop through tokenized string looking for variables to replace */
  for (i = 0; i < n_tokens; i++)
    {
      PosToken *t = &tokens[i];      

      if (t->type == POS_TOKEN_VARIABLE)
        {
          if (meta_theme_lookup_int_constant (theme, t->d.v.name, &ival))
            {
              t->type = POS_TOKEN_INT;
              t->d.i.val = ival;
            }
          else if (meta_theme_lookup_float_constant (theme, t->d.v.name, &dval))
            {
              t->type = POS_TOKEN_DOUBLE;
              t->d.d.val = dval;
            }
          else 
            {
              /* If we've found a variable that cannot be replaced then the
                 expression is not a constant expression and we want to 
                 replace it with a GQuark */

              t->d.v.name_quark = g_quark_from_string (t->d.v.name);
              is_constant = FALSE;
            }
        }
    }  

  return is_constant;
}

static int
parse_x_position_unchecked (MetaDrawSpec              *spec,
                            const MetaPositionExprEnv *env)
{
  int retval;
  GError *error;

  retval = 0;
  error = NULL;
  if (!meta_parse_position_expression (spec, env, &retval, NULL, &error))
    {
      meta_warning (_("Theme contained an expression that resulted in an error: %s\n"),
                    error->message);
      
      g_error_free (error);
    }
  
  return retval;
}

static int
parse_y_position_unchecked (MetaDrawSpec              *spec,
                            const MetaPositionExprEnv *env)
{
  int retval;
  GError *error;

  retval = 0;
  error = NULL;
  if (!meta_parse_position_expression (spec, env, NULL, &retval, &error))
    {
      meta_warning (_("Theme contained an expression that resulted in an error: %s\n"),
                    error->message);

      g_error_free (error);
    }

  return retval;
}

static int
parse_size_unchecked (MetaDrawSpec        *spec,
                      MetaPositionExprEnv *env)
{
  int retval;
  GError *error;

  retval = 0;
  error = NULL;
  if (!meta_parse_size_expression (spec, env, &retval, &error))
    {
      meta_warning (_("Theme contained an expression that resulted in an error: %s\n"),
                    error->message);

      g_error_free (error);
    }

  return retval;
}

void
meta_draw_spec_free (MetaDrawSpec *spec)
{
  if (!spec) return;
  free_tokens (spec->tokens, spec->n_tokens);
  g_slice_free (MetaDrawSpec, spec);
}

MetaDrawSpec *
meta_draw_spec_new (MetaTheme  *theme,
                    const char *expr,
                    GError    **error)
{
  MetaDrawSpec *spec;

  spec = g_slice_new0 (MetaDrawSpec);

  pos_tokenize (expr, &spec->tokens, &spec->n_tokens, NULL);
  
  spec->constant = meta_theme_replace_constants (theme, spec->tokens, 
                                                 spec->n_tokens, NULL);
  if (spec->constant) 
    {
      gboolean result;

      result = pos_eval (spec, NULL, &spec->value, error);
      if (result == FALSE)
        {
          meta_draw_spec_free (spec);
          return NULL;
        }
    }
    
  return spec;
}

MetaDrawOp*
meta_draw_op_new (MetaDrawType type)
{
  MetaDrawOp *op;
  MetaDrawOp dummy;
  int size;

  size = G_STRUCT_OFFSET (MetaDrawOp, data);

  switch (type)
    {
    case META_DRAW_LINE:
      size += sizeof (dummy.data.line);
      break;

    case META_DRAW_RECTANGLE:
      size += sizeof (dummy.data.rectangle);
      break;

    case META_DRAW_ARC:
      size += sizeof (dummy.data.arc);
      break;

    case META_DRAW_CLIP:
      size += sizeof (dummy.data.clip);
      break;
      
    case META_DRAW_TINT:
      size += sizeof (dummy.data.tint);
      break;

    case META_DRAW_GRADIENT:
      size += sizeof (dummy.data.gradient);
      break;

    case META_DRAW_IMAGE:
      size += sizeof (dummy.data.image);
      break;

    case META_DRAW_GTK_ARROW:
      size += sizeof (dummy.data.gtk_arrow);
      break;

    case META_DRAW_GTK_BOX:
      size += sizeof (dummy.data.gtk_box);
      break;

    case META_DRAW_GTK_VLINE:
      size += sizeof (dummy.data.gtk_vline);
      break;

    case META_DRAW_ICON:
      size += sizeof (dummy.data.icon);
      break;

    case META_DRAW_TITLE:
      size += sizeof (dummy.data.title);
      break;
    case META_DRAW_OP_LIST:
      size += sizeof (dummy.data.op_list);
      break;
    case META_DRAW_TILE:
      size += sizeof (dummy.data.tile);
      break;
    }

  op = g_malloc0 (size);

  op->type = type;

  return op;
}

void
meta_draw_op_free (MetaDrawOp *op)
{
  g_return_if_fail (op != NULL);

  switch (op->type)
    {
    case META_DRAW_LINE:
      if (op->data.line.color_spec)
        meta_color_spec_free (op->data.line.color_spec);

      meta_draw_spec_free (op->data.line.x1);
      meta_draw_spec_free (op->data.line.y1);
      meta_draw_spec_free (op->data.line.x2);
      meta_draw_spec_free (op->data.line.y2);
      break;

    case META_DRAW_RECTANGLE:
      if (op->data.rectangle.color_spec)
        g_free (op->data.rectangle.color_spec);

      meta_draw_spec_free (op->data.rectangle.x);
      meta_draw_spec_free (op->data.rectangle.y);
      meta_draw_spec_free (op->data.rectangle.width);
      meta_draw_spec_free (op->data.rectangle.height);
      break;

    case META_DRAW_ARC:
      if (op->data.arc.color_spec)
        g_free (op->data.arc.color_spec);

      meta_draw_spec_free (op->data.arc.x);
      meta_draw_spec_free (op->data.arc.y);
      meta_draw_spec_free (op->data.arc.width);
      meta_draw_spec_free (op->data.arc.height);
      break;

    case META_DRAW_CLIP:
      meta_draw_spec_free (op->data.clip.x);
      meta_draw_spec_free (op->data.clip.y);
      meta_draw_spec_free (op->data.clip.width);
      meta_draw_spec_free (op->data.clip.height);
      break;
      
    case META_DRAW_TINT:
      if (op->data.tint.color_spec)
        meta_color_spec_free (op->data.tint.color_spec);

      if (op->data.tint.alpha_spec)
        meta_alpha_gradient_spec_free (op->data.tint.alpha_spec);

      meta_draw_spec_free (op->data.tint.x);
      meta_draw_spec_free (op->data.tint.y);
      meta_draw_spec_free (op->data.tint.width);
      meta_draw_spec_free (op->data.tint.height);
      break;

    case META_DRAW_GRADIENT:
      if (op->data.gradient.gradient_spec)
        meta_gradient_spec_free (op->data.gradient.gradient_spec);

      if (op->data.gradient.alpha_spec)
        meta_alpha_gradient_spec_free (op->data.gradient.alpha_spec);

      meta_draw_spec_free (op->data.gradient.x);
      meta_draw_spec_free (op->data.gradient.y);
      meta_draw_spec_free (op->data.gradient.width);
      meta_draw_spec_free (op->data.gradient.height);
      break;

    case META_DRAW_IMAGE:
      if (op->data.image.alpha_spec)
        meta_alpha_gradient_spec_free (op->data.image.alpha_spec);

      if (op->data.image.pixbuf)
        g_object_unref (G_OBJECT (op->data.image.pixbuf));

      if (op->data.image.colorize_spec)
	meta_color_spec_free (op->data.image.colorize_spec);

      if (op->data.image.colorize_cache_pixbuf)
        g_object_unref (G_OBJECT (op->data.image.colorize_cache_pixbuf));

      meta_draw_spec_free (op->data.image.x);
      meta_draw_spec_free (op->data.image.y);
      meta_draw_spec_free (op->data.image.width);
      meta_draw_spec_free (op->data.image.height);
      break;

    case META_DRAW_GTK_ARROW:
      meta_draw_spec_free (op->data.gtk_arrow.x);
      meta_draw_spec_free (op->data.gtk_arrow.y);
      meta_draw_spec_free (op->data.gtk_arrow.width);
      meta_draw_spec_free (op->data.gtk_arrow.height);
      break;

    case META_DRAW_GTK_BOX:
      meta_draw_spec_free (op->data.gtk_box.x);
      meta_draw_spec_free (op->data.gtk_box.y);
      meta_draw_spec_free (op->data.gtk_box.width);
      meta_draw_spec_free (op->data.gtk_box.height);
      break;

    case META_DRAW_GTK_VLINE:
      meta_draw_spec_free (op->data.gtk_vline.x);
      meta_draw_spec_free (op->data.gtk_vline.y1);
      meta_draw_spec_free (op->data.gtk_vline.y2);
      break;

    case META_DRAW_ICON:
      if (op->data.icon.alpha_spec)
        meta_alpha_gradient_spec_free (op->data.icon.alpha_spec);

      meta_draw_spec_free (op->data.icon.x);
      meta_draw_spec_free (op->data.icon.y);
      meta_draw_spec_free (op->data.icon.width);
      meta_draw_spec_free (op->data.icon.height);
      break;

    case META_DRAW_TITLE:
      if (op->data.title.color_spec)
        meta_color_spec_free (op->data.title.color_spec);

      meta_draw_spec_free (op->data.title.x);
      meta_draw_spec_free (op->data.title.y);
      if (op->data.title.ellipsize_width)
        meta_draw_spec_free (op->data.title.ellipsize_width);
      break;

    case META_DRAW_OP_LIST:
      if (op->data.op_list.op_list)
        meta_draw_op_list_unref (op->data.op_list.op_list);

      meta_draw_spec_free (op->data.op_list.x);
      meta_draw_spec_free (op->data.op_list.y);
      meta_draw_spec_free (op->data.op_list.width);
      meta_draw_spec_free (op->data.op_list.height);
      break;

    case META_DRAW_TILE:
      if (op->data.tile.op_list)
        meta_draw_op_list_unref (op->data.tile.op_list);

      meta_draw_spec_free (op->data.tile.x);
      meta_draw_spec_free (op->data.tile.y);
      meta_draw_spec_free (op->data.tile.width);
      meta_draw_spec_free (op->data.tile.height);
      meta_draw_spec_free (op->data.tile.tile_xoffset);
      meta_draw_spec_free (op->data.tile.tile_yoffset);
      meta_draw_spec_free (op->data.tile.tile_width);
      meta_draw_spec_free (op->data.tile.tile_height);
      break;
    }

  g_free (op);
}

static GdkGC*
get_gc_for_primitive (GtkWidget          *widget,
                      GdkDrawable        *drawable,
                      MetaColorSpec      *color_spec,
                      const GdkRectangle *clip,
                      int                 line_width)
{
  GdkGC *gc;
  GdkGCValues values;
  GdkColor color;

  meta_color_spec_render (color_spec, widget, &color);

  values.foreground = color;

  gdk_rgb_find_color (gdk_drawable_get_colormap (drawable),
                      &values.foreground);

  values.line_width = line_width;

  gc = gdk_gc_new_with_values (drawable, &values,
                               GDK_GC_FOREGROUND | GDK_GC_LINE_WIDTH);

  if (clip)
    gdk_gc_set_clip_rectangle (gc,
                               (GdkRectangle*) clip); /* const cast */

  return gc;
}

static GdkPixbuf*
apply_alpha (GdkPixbuf             *pixbuf,
             MetaAlphaGradientSpec *spec,
             gboolean               force_copy)
{
  GdkPixbuf *new_pixbuf;
  gboolean needs_alpha;
  
  g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);
  
  needs_alpha = spec && (spec->n_alphas > 1 ||
                         spec->alphas[0] != 0xff);

  if (!needs_alpha)
    return pixbuf;
  
  if (!gdk_pixbuf_get_has_alpha (pixbuf))
    {
      new_pixbuf = gdk_pixbuf_add_alpha (pixbuf, FALSE, 0, 0, 0);
      g_object_unref (G_OBJECT (pixbuf));
      pixbuf = new_pixbuf;
    }
  else if (force_copy)
    {
      new_pixbuf = gdk_pixbuf_copy (pixbuf);
      g_object_unref (G_OBJECT (pixbuf));
      pixbuf = new_pixbuf;
    }
  
  g_assert (gdk_pixbuf_get_has_alpha (pixbuf));

  meta_gradient_add_alpha (pixbuf, spec->alphas, spec->n_alphas, spec->type);
  
  return pixbuf;
}

static void
render_pixbuf (GdkDrawable        *drawable,
               const GdkRectangle *clip,
               GdkPixbuf          *pixbuf,
               int                 x,
               int                 y)
{
  /* grumble, render_to_drawable_alpha does not accept a clip
   * mask, so we have to go through some BS
   */
  /* FIXME once GTK 1.3.13 has been out a while we can use
   * render_to_drawable() which now does alpha with clip.
   *
   * Though the gdk_rectangle_intersect() check may be a useful
   * optimization anyway.
   */
  GdkRectangle pixbuf_rect;
  GdkRectangle draw_rect;

  pixbuf_rect.x = x;
  pixbuf_rect.y = y;
  pixbuf_rect.width = gdk_pixbuf_get_width (pixbuf);
  pixbuf_rect.height = gdk_pixbuf_get_height (pixbuf);

  if (clip)
    {
      if (!gdk_rectangle_intersect ((GdkRectangle*)clip,
                                    &pixbuf_rect, &draw_rect))
        return;
    }
  else
    {
      draw_rect = pixbuf_rect;
    }

  gdk_draw_pixbuf (drawable,
                   NULL,
                   pixbuf,
                   draw_rect.x - pixbuf_rect.x,
                   draw_rect.y - pixbuf_rect.y,
                   draw_rect.x, draw_rect.y,
                   draw_rect.width,
                   draw_rect.height,
                   GDK_RGB_DITHER_NORMAL,
                   draw_rect.x - pixbuf_rect.x,
                   draw_rect.y - pixbuf_rect.y);
}

static GdkPixbuf*
pixbuf_tile (GdkPixbuf *tile,
             int        width,
             int        height)
{
  GdkPixbuf *pixbuf;
  int tile_width;
  int tile_height;
  int i, j;
  
  tile_width = gdk_pixbuf_get_width (tile);
  tile_height = gdk_pixbuf_get_height (tile);
  
  pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                           gdk_pixbuf_get_has_alpha (tile),
                           8, width, height);

  i = 0;
  while (i < width)
    {
      j = 0;
      while (j < height)
        {
          int w, h;

          w = MIN (tile_width, width - i);
          h = MIN (tile_height, height - j);
          
          gdk_pixbuf_copy_area (tile,
                                0, 0,
                                w, h,
                                pixbuf,
                                i, j);

          j += tile_height;
        }
      
      i += tile_width;
    }
  
  return pixbuf;
}

static GdkPixbuf *
replicate_rows (GdkPixbuf  *src,
                int         src_x,
                int         src_y,
                int         width,
                int         height)
{
  unsigned int n_channels = gdk_pixbuf_get_n_channels (src);
  unsigned int src_rowstride = gdk_pixbuf_get_rowstride (src);
  unsigned char *pixels = (gdk_pixbuf_get_pixels (src) + src_y * src_rowstride + src_x
                           * n_channels);
  unsigned char *dest_pixels;
  GdkPixbuf *result;
  unsigned int dest_rowstride;
  int i;

  result = gdk_pixbuf_new (GDK_COLORSPACE_RGB, n_channels == 4, 8,
                           width, height);
  dest_rowstride = gdk_pixbuf_get_rowstride (result);
  dest_pixels = gdk_pixbuf_get_pixels (result);
  
  for (i = 0; i < height; i++)
    memcpy (dest_pixels + dest_rowstride * i, pixels, n_channels * width);

  return result;
}

static GdkPixbuf *
replicate_cols (GdkPixbuf  *src,
                int         src_x,
                int         src_y,
                int         width,
                int         height)
{
  unsigned int n_channels = gdk_pixbuf_get_n_channels (src);
  unsigned int src_rowstride = gdk_pixbuf_get_rowstride (src);
  unsigned char *pixels = (gdk_pixbuf_get_pixels (src) + src_y * src_rowstride + src_x
                           * n_channels);
  unsigned char *dest_pixels;
  GdkPixbuf *result;
  unsigned int dest_rowstride;
  int i, j;

  result = gdk_pixbuf_new (GDK_COLORSPACE_RGB, n_channels == 4, 8,
                           width, height);
  dest_rowstride = gdk_pixbuf_get_rowstride (result);
  dest_pixels = gdk_pixbuf_get_pixels (result);

  for (i = 0; i < height; i++)
    {
      unsigned char *p = dest_pixels + dest_rowstride * i;
      unsigned char *q = pixels + src_rowstride * i;

      unsigned char r = *(q++);
      unsigned char g = *(q++);
      unsigned char b = *(q++);
      
      if (n_channels == 4)
        {
          unsigned char a;
          
          a = *(q++);
          
          for (j = 0; j < width; j++)
            {
              *(p++) = r;
              *(p++) = g;
              *(p++) = b;                    
              *(p++) = a;
            }
        }
      else
        {
          for (j = 0; j < width; j++)
            {
              *(p++) = r;
              *(p++) = g;
              *(p++) = b;
            }
        }
    }

  return result;
}

static GdkPixbuf*
scale_and_alpha_pixbuf (GdkPixbuf             *src,
                        MetaAlphaGradientSpec *alpha_spec,
                        MetaImageFillType      fill_type,
                        int                    width,
                        int                    height,
                        gboolean               vertical_stripes,
                        gboolean               horizontal_stripes)
{
  GdkPixbuf *pixbuf;
  GdkPixbuf *temp_pixbuf;

  pixbuf = NULL;

  pixbuf = src;

  if (gdk_pixbuf_get_width (pixbuf) == width &&
      gdk_pixbuf_get_height (pixbuf) == height)
    {
      g_object_ref (G_OBJECT (pixbuf));
    }
  else
    {
      if (fill_type == META_IMAGE_FILL_TILE)
        {
          pixbuf = pixbuf_tile (pixbuf, width, height);
        }
      else
        {
    	  int src_h, src_w, dest_h, dest_w;
          src_h = gdk_pixbuf_get_height (src);
          src_w = gdk_pixbuf_get_width (src);

          /* prefer to replicate_cols if possible, as that
           * is faster (no memory reads)
           */
          if (horizontal_stripes)
            {
              dest_w = gdk_pixbuf_get_width (src);
              dest_h = height;
            }
          else if (vertical_stripes)
            {
              dest_w = width;
              dest_h = gdk_pixbuf_get_height (src);
            }

          else
            {
              dest_w = width;
              dest_h = height;
            }

          if (dest_w == src_w && dest_h == src_h)
            {
              temp_pixbuf = src;
              g_object_ref (G_OBJECT (temp_pixbuf));
            }
          else
            {
              temp_pixbuf = gdk_pixbuf_scale_simple (src,
                                                     dest_w, dest_h,
                                                     GDK_INTERP_BILINEAR);
            }

          /* prefer to replicate_cols if possible, as that
           * is faster (no memory reads)
           */
          if (horizontal_stripes)
            {
              pixbuf = replicate_cols (temp_pixbuf, 0, 0, width, height);
              g_object_unref (G_OBJECT (temp_pixbuf));
            }
          else if (vertical_stripes)
            {
              pixbuf = replicate_rows (temp_pixbuf, 0, 0, width, height);
              g_object_unref (G_OBJECT (temp_pixbuf));
            }
          else 
            {
              pixbuf = temp_pixbuf;
            }
        }
    }

  if (pixbuf)
    pixbuf = apply_alpha (pixbuf, alpha_spec, pixbuf == src);
  
  return pixbuf;
}

static GdkPixbuf*
draw_op_as_pixbuf (const MetaDrawOp    *op,
                   GtkWidget           *widget,
                   const MetaDrawInfo  *info,
                   int                  width,
                   int                  height)
{
  /* Try to get the op as a pixbuf, assuming w/h in the op
   * matches the width/height passed in. return NULL
   * if the op can't be converted to an equivalent pixbuf.
   */
  GdkPixbuf *pixbuf;

  pixbuf = NULL;

  switch (op->type)
    {
    case META_DRAW_LINE:
      break;

    case META_DRAW_RECTANGLE:
      if (op->data.rectangle.filled)
        {
          GdkColor color;

          meta_color_spec_render (op->data.rectangle.color_spec,
                                  widget,
                                  &color);

          pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                                   FALSE,
                                   8, width, height);

          gdk_pixbuf_fill (pixbuf, GDK_COLOR_RGBA (color));
        }
      break;

    case META_DRAW_ARC:
      break;

    case META_DRAW_CLIP:
      break;
      
    case META_DRAW_TINT:
      {
        GdkColor color;
        guint32 rgba;
        gboolean has_alpha;

        meta_color_spec_render (op->data.rectangle.color_spec,
                                widget,
                                &color);

        has_alpha =
          op->data.tint.alpha_spec &&
          (op->data.tint.alpha_spec->n_alphas > 1 ||
           op->data.tint.alpha_spec->alphas[0] != 0xff);
        
        pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                                 has_alpha,
                                 8, width, height);

        if (!has_alpha)
          {
            rgba = GDK_COLOR_RGBA (color);
            
            gdk_pixbuf_fill (pixbuf, rgba);
          }
        else if (op->data.tint.alpha_spec->n_alphas == 1)
          {
            rgba = GDK_COLOR_RGBA (color);
            rgba &= ~0xff;
            rgba |= op->data.tint.alpha_spec->alphas[0];
            
            gdk_pixbuf_fill (pixbuf, rgba);
          }
        else
          {
            rgba = GDK_COLOR_RGBA (color);
            
            gdk_pixbuf_fill (pixbuf, rgba);

            meta_gradient_add_alpha (pixbuf,
                                     op->data.tint.alpha_spec->alphas,
                                     op->data.tint.alpha_spec->n_alphas,
                                     op->data.tint.alpha_spec->type);
          }
      }
      break;

    case META_DRAW_GRADIENT:
      {
        pixbuf = meta_gradient_spec_render (op->data.gradient.gradient_spec,
                                            widget, width, height);

        pixbuf = apply_alpha (pixbuf,
                              op->data.gradient.alpha_spec,
                              FALSE);
      }
      break;

      
    case META_DRAW_IMAGE:
      {
	if (op->data.image.colorize_spec)
	  {
	    GdkColor color;

            meta_color_spec_render (op->data.image.colorize_spec,
                                    widget, &color);
            
            if (op->data.image.colorize_cache_pixbuf == NULL ||
                op->data.image.colorize_cache_pixel != GDK_COLOR_RGB (color))
              {
                if (op->data.image.colorize_cache_pixbuf)
                  g_object_unref (G_OBJECT (op->data.image.colorize_cache_pixbuf));
                
                /* const cast here */
                ((MetaDrawOp*)op)->data.image.colorize_cache_pixbuf =
                  colorize_pixbuf (op->data.image.pixbuf,
                                   &color);
                ((MetaDrawOp*)op)->data.image.colorize_cache_pixel =
                  GDK_COLOR_RGB (color);
              }
            
            if (op->data.image.colorize_cache_pixbuf)
              {
                pixbuf = scale_and_alpha_pixbuf (op->data.image.colorize_cache_pixbuf,
                                                 op->data.image.alpha_spec,
                                                 op->data.image.fill_type,
                                                 width, height,
                                                 op->data.image.vertical_stripes,
                                                 op->data.image.horizontal_stripes);
              }
	  }
	else
	  {
	    pixbuf = scale_and_alpha_pixbuf (op->data.image.pixbuf,
                                             op->data.image.alpha_spec,
                                             op->data.image.fill_type,
                                             width, height,
                                             op->data.image.vertical_stripes,
                                             op->data.image.horizontal_stripes);
	  }
        break;
      }
      
    case META_DRAW_GTK_ARROW:
    case META_DRAW_GTK_BOX:
    case META_DRAW_GTK_VLINE:
      break;

    case META_DRAW_ICON:
      if (info->mini_icon &&
          width <= gdk_pixbuf_get_width (info->mini_icon) &&
          height <= gdk_pixbuf_get_height (info->mini_icon))
        pixbuf = scale_and_alpha_pixbuf (info->mini_icon,
                                         op->data.icon.alpha_spec,
                                         op->data.icon.fill_type,
                                         width, height,
                                         FALSE, FALSE);
      else if (info->icon)
        pixbuf = scale_and_alpha_pixbuf (info->icon,
                                         op->data.icon.alpha_spec,
                                         op->data.icon.fill_type,
                                         width, height,
                                         FALSE, FALSE);
      break;

    case META_DRAW_TITLE:
      break;

    case META_DRAW_OP_LIST:
      break;

    case META_DRAW_TILE:
      break;
    }

  return pixbuf;
}

static void
fill_env (MetaPositionExprEnv *env,
          const MetaDrawInfo  *info,
          MetaRectangle        logical_region)
{
  /* FIXME this stuff could be raised into draw_op_list_draw() probably
   */
  env->rect = logical_region;
  env->object_width = -1;
  env->object_height = -1;
  if (info->fgeom)
    {
      env->left_width = info->fgeom->left_width;
      env->right_width = info->fgeom->right_width;
      env->top_height = info->fgeom->top_height;
      env->bottom_height = info->fgeom->bottom_height;
      env->frame_x_center = info->fgeom->width / 2 - logical_region.x;
      env->frame_y_center = info->fgeom->height / 2 - logical_region.y;
    }
  else
    {
      env->left_width = 0;
      env->right_width = 0;
      env->top_height = 0;
      env->bottom_height = 0;
      env->frame_x_center = 0;
      env->frame_y_center = 0;
    }
  
  env->mini_icon_width = info->mini_icon ? gdk_pixbuf_get_width (info->mini_icon) : 0;
  env->mini_icon_height = info->mini_icon ? gdk_pixbuf_get_height (info->mini_icon) : 0;
  env->icon_width = info->icon ? gdk_pixbuf_get_width (info->icon) : 0;
  env->icon_height = info->icon ? gdk_pixbuf_get_height (info->icon) : 0;

  env->title_width = info->title_layout_width;
  env->title_height = info->title_layout_height;
  env->theme = meta_current_theme;
}

static void
meta_draw_op_draw_with_env (const MetaDrawOp    *op,
                            GtkStyle            *style_gtk,
                            GtkWidget           *widget,
                            GdkDrawable         *drawable,
                            const GdkRectangle  *clip,
                            const MetaDrawInfo  *info,
                            MetaRectangle        rect,
                            MetaPositionExprEnv *env)
{
  GdkGC *gc;
  
  switch (op->type)
    {
    case META_DRAW_LINE:
      {
        int x1, x2, y1, y2;

        gc = get_gc_for_primitive (widget, drawable,
                                   op->data.line.color_spec,
                                   clip,
                                   op->data.line.width);

        if (op->data.line.dash_on_length > 0 &&
            op->data.line.dash_off_length > 0)
          {
            gint8 dash_list[2];
            dash_list[0] = op->data.line.dash_on_length;
            dash_list[1] = op->data.line.dash_off_length;
            gdk_gc_set_dashes (gc, 0, dash_list, 2);
          }

        x1 = parse_x_position_unchecked (op->data.line.x1, env);
        y1 = parse_y_position_unchecked (op->data.line.y1, env); 

        if (!op->data.line.x2 &&
            !op->data.line.y2 &&
            op->data.line.width==0)
          gdk_draw_point (drawable, gc, x1, y1);
        else
          {
            if (op->data.line.x2)
              x2 = parse_x_position_unchecked (op->data.line.x2, env);
            else
              x2 = x1;

            if (op->data.line.y2)
              y2 = parse_y_position_unchecked (op->data.line.y2, env);
            else
              y2 = y1;

            gdk_draw_line (drawable, gc, x1, y1, x2, y2);
          }

        g_object_unref (G_OBJECT (gc));
      }
      break;

    case META_DRAW_RECTANGLE:
      {
        int rx, ry, rwidth, rheight;

        gc = get_gc_for_primitive (widget, drawable,
                                   op->data.rectangle.color_spec,
                                   clip, 0);

        rx = parse_x_position_unchecked (op->data.rectangle.x, env);
        ry = parse_y_position_unchecked (op->data.rectangle.y, env);
        rwidth = parse_size_unchecked (op->data.rectangle.width, env);
        rheight = parse_size_unchecked (op->data.rectangle.height, env);

        gdk_draw_rectangle (drawable, gc,
                            op->data.rectangle.filled,
                            rx, ry, rwidth, rheight);

        g_object_unref (G_OBJECT (gc));
      }
      break;

    case META_DRAW_ARC:
      {
        int rx, ry, rwidth, rheight;

        gc = get_gc_for_primitive (widget, drawable,
                                   op->data.arc.color_spec,
                                   clip, 0);

        rx = parse_x_position_unchecked (op->data.arc.x, env);
        ry = parse_y_position_unchecked (op->data.arc.y, env);
        rwidth = parse_size_unchecked (op->data.arc.width, env);
        rheight = parse_size_unchecked (op->data.arc.height, env);

        gdk_draw_arc (drawable,
                      gc,
                      op->data.arc.filled,
                      rx, ry, rwidth, rheight,
                      op->data.arc.start_angle * (360.0 * 64.0) -
                      (90.0 * 64.0), /* start at 12 instead of 3 oclock */
                      op->data.arc.extent_angle * (360.0 * 64.0));

        g_object_unref (G_OBJECT (gc));
      }
      break;

    case META_DRAW_CLIP:
      break;
      
    case META_DRAW_TINT:
      {
        int rx, ry, rwidth, rheight;
        gboolean needs_alpha;
        
        needs_alpha = op->data.tint.alpha_spec &&
          (op->data.tint.alpha_spec->n_alphas > 1 ||
           op->data.tint.alpha_spec->alphas[0] != 0xff);
        
        rx = parse_x_position_unchecked (op->data.tint.x, env);
        ry = parse_y_position_unchecked (op->data.tint.y, env);
        rwidth = parse_size_unchecked (op->data.tint.width, env);
        rheight = parse_size_unchecked (op->data.tint.height, env);

        if (!needs_alpha)
          {
            gc = get_gc_for_primitive (widget, drawable,
                                       op->data.tint.color_spec,
                                       clip, 0);

            gdk_draw_rectangle (drawable, gc,
                                TRUE,
                                rx, ry, rwidth, rheight);

            g_object_unref (G_OBJECT (gc));
          }
        else
          {
            GdkPixbuf *pixbuf;

            pixbuf = draw_op_as_pixbuf (op, widget, info,
                                        rwidth, rheight);

            if (pixbuf)
              {
                render_pixbuf (drawable, clip, pixbuf, rx, ry);

                g_object_unref (G_OBJECT (pixbuf));
              }
          }
      }
      break;

    case META_DRAW_GRADIENT:
      {
        int rx, ry, rwidth, rheight;
        GdkPixbuf *pixbuf;

        rx = parse_x_position_unchecked (op->data.gradient.x, env);
        ry = parse_y_position_unchecked (op->data.gradient.y, env);
        rwidth = parse_size_unchecked (op->data.gradient.width, env);
        rheight = parse_size_unchecked (op->data.gradient.height, env);

        pixbuf = draw_op_as_pixbuf (op, widget, info,
                                    rwidth, rheight);

        if (pixbuf)
          {
            render_pixbuf (drawable, clip, pixbuf, rx, ry);

            g_object_unref (G_OBJECT (pixbuf));
          }
      }
      break;

    case META_DRAW_IMAGE:
      {
        int rx, ry, rwidth, rheight;
        GdkPixbuf *pixbuf;

        if (op->data.image.pixbuf)
          {
            env->object_width = gdk_pixbuf_get_width (op->data.image.pixbuf);
            env->object_height = gdk_pixbuf_get_height (op->data.image.pixbuf);
          }

        rwidth = parse_size_unchecked (op->data.image.width, env);
        rheight = parse_size_unchecked (op->data.image.height, env);
        
        pixbuf = draw_op_as_pixbuf (op, widget, info,
                                    rwidth, rheight);

        if (pixbuf)
          {
            rx = parse_x_position_unchecked (op->data.image.x, env);
            ry = parse_y_position_unchecked (op->data.image.y, env);

            render_pixbuf (drawable, clip, pixbuf, rx, ry);

            g_object_unref (G_OBJECT (pixbuf));
          }
      }
      break;

    case META_DRAW_GTK_ARROW:
      {
        int rx, ry, rwidth, rheight;

        rx = parse_x_position_unchecked (op->data.gtk_arrow.x, env);
        ry = parse_y_position_unchecked (op->data.gtk_arrow.y, env);
        rwidth = parse_size_unchecked (op->data.gtk_arrow.width, env);
        rheight = parse_size_unchecked (op->data.gtk_arrow.height, env);

        gtk_paint_arrow (style_gtk,
                         drawable,
                         op->data.gtk_arrow.state,
                         op->data.gtk_arrow.shadow,
                         (GdkRectangle*) clip,
                         widget,
                         "metacity",
                         op->data.gtk_arrow.arrow,
                         op->data.gtk_arrow.filled,
                         rx, ry, rwidth, rheight);
      }
      break;

    case META_DRAW_GTK_BOX:
      {
        int rx, ry, rwidth, rheight;

        rx = parse_x_position_unchecked (op->data.gtk_box.x, env);
        ry = parse_y_position_unchecked (op->data.gtk_box.y, env);
        rwidth = parse_size_unchecked (op->data.gtk_box.width, env);
        rheight = parse_size_unchecked (op->data.gtk_box.height, env);

        gtk_paint_box (style_gtk,
                       drawable,
                       op->data.gtk_box.state,
                       op->data.gtk_box.shadow,
                       (GdkRectangle*) clip,
                       widget,
                       "metacity",
                       rx, ry, rwidth, rheight);
      }
      break;

    case META_DRAW_GTK_VLINE:
      {
        int rx, ry1, ry2;

        rx = parse_x_position_unchecked (op->data.gtk_vline.x, env);
        ry1 = parse_y_position_unchecked (op->data.gtk_vline.y1, env);
        ry2 = parse_y_position_unchecked (op->data.gtk_vline.y2, env);
        
        gtk_paint_vline (style_gtk,
                         drawable,
                         op->data.gtk_vline.state,
                         (GdkRectangle*) clip,
                         widget,
                         "metacity",
                         ry1, ry2, rx);
      }
      break;

    case META_DRAW_ICON:
      {
        int rx, ry, rwidth, rheight;
        GdkPixbuf *pixbuf;

        rwidth = parse_size_unchecked (op->data.icon.width, env);
        rheight = parse_size_unchecked (op->data.icon.height, env);
        
        pixbuf = draw_op_as_pixbuf (op, widget, info,
                                    rwidth, rheight);

        if (pixbuf)
          {
            rx = parse_x_position_unchecked (op->data.icon.x, env);
            ry = parse_y_position_unchecked (op->data.icon.y, env);

            render_pixbuf (drawable, clip, pixbuf, rx, ry);

            g_object_unref (G_OBJECT (pixbuf));
          }
      }
      break;

    case META_DRAW_TITLE:
      if (info->title_layout)
        {
          int rx, ry;
          PangoRectangle ink_rect, logical_rect;

          gc = get_gc_for_primitive (widget, drawable,
                                     op->data.title.color_spec,
                                     clip, 0);

          rx = parse_x_position_unchecked (op->data.title.x, env);
          ry = parse_y_position_unchecked (op->data.title.y, env);

          if (op->data.title.ellipsize_width)
            {
              int ellipsize_width;
              int right_bearing;

              ellipsize_width = parse_x_position_unchecked (op->data.title.ellipsize_width, env);
              /* HACK: parse_x_position_unchecked adds in env->rect.x, subtract out again */
              ellipsize_width -= env->rect.x;

              pango_layout_set_width (info->title_layout, -1);
              pango_layout_get_pixel_extents (info->title_layout,
                                              &ink_rect, &logical_rect);

              /* Pango's idea of ellipsization is with respect to the logical rect.
               * correct for this, by reducing the ellipsization width by the overflow
               * of the un-ellipsized text on the right... it's always the visual
               * right we want regardless of bidi, since since the X we pass in to
               * gdk_draw_layout() is always the left edge of the line.
               */
              right_bearing = (ink_rect.x + ink_rect.width) - (logical_rect.x + logical_rect.width);
              right_bearing = MAX (right_bearing, 0);

              ellipsize_width -= right_bearing;
              ellipsize_width = MAX (ellipsize_width, 0);

              /* Only ellipsizing when necessary is a performance optimization -
               * pango_layout_set_width() will force a relayout if it isn't the
               * same as the current width of -1.
               */
              if (ellipsize_width < logical_rect.width)
                pango_layout_set_width (info->title_layout, PANGO_SCALE * ellipsize_width);
            }

          gdk_draw_layout (drawable, gc,
                           rx, ry,
                           info->title_layout);

          /* Remove any ellipsization we might have set; will short-circuit
           * if the width is already -1 */
          pango_layout_set_width (info->title_layout, -1);

          g_object_unref (G_OBJECT (gc));
        }
      break;

    case META_DRAW_OP_LIST:
      {
        MetaRectangle d_rect;

        d_rect.x = parse_x_position_unchecked (op->data.op_list.x, env);
        d_rect.y = parse_y_position_unchecked (op->data.op_list.y, env);
        d_rect.width = parse_size_unchecked (op->data.op_list.width, env);
        d_rect.height = parse_size_unchecked (op->data.op_list.height, env);

        meta_draw_op_list_draw_with_style (op->data.op_list.op_list,
                                           style_gtk, widget, drawable, clip, info,
                                d_rect);
      }
      break;

    case META_DRAW_TILE:
      {
        int rx, ry, rwidth, rheight;
        int tile_xoffset, tile_yoffset; 
        GdkRectangle new_clip;
        MetaRectangle tile;
        
        rx = parse_x_position_unchecked (op->data.tile.x, env);
        ry = parse_y_position_unchecked (op->data.tile.y, env);
        rwidth = parse_size_unchecked (op->data.tile.width, env);
        rheight = parse_size_unchecked (op->data.tile.height, env);

        new_clip.x = rx;
        new_clip.y = ry;
        new_clip.width = rwidth;
        new_clip.height = rheight;

        if (clip == NULL || gdk_rectangle_intersect ((GdkRectangle*)clip, &new_clip,
                                                     &new_clip))
          {
            tile_xoffset = parse_x_position_unchecked (op->data.tile.tile_xoffset, env);
            tile_yoffset = parse_y_position_unchecked (op->data.tile.tile_yoffset, env);
            /* tile offset should not include x/y */
            tile_xoffset -= rect.x;
            tile_yoffset -= rect.y;
            
            tile.width = parse_size_unchecked (op->data.tile.tile_width, env);
            tile.height = parse_size_unchecked (op->data.tile.tile_height, env);

            tile.x = rx - tile_xoffset;
        
            while (tile.x < (rx + rwidth))
              {
                tile.y = ry - tile_yoffset;
                while (tile.y < (ry + rheight))
                  {
                    meta_draw_op_list_draw_with_style (op->data.tile.op_list,
                                                       style_gtk, widget, drawable, &new_clip, info,
                                            tile);

                    tile.y += tile.height;
                  }

                tile.x += tile.width;
              }
          }
      }
      break;
    }
}

void
meta_draw_op_draw_with_style (const MetaDrawOp    *op,
                              GtkStyle            *style_gtk,
                   GtkWidget           *widget,
                   GdkDrawable         *drawable,
                   const GdkRectangle  *clip,
                   const MetaDrawInfo  *info,
                   MetaRectangle        logical_region)
{
  MetaPositionExprEnv env;

  g_return_if_fail (style_gtk->colormap == gdk_drawable_get_colormap (drawable));

  fill_env (&env, info, logical_region);

  meta_draw_op_draw_with_env (op, style_gtk, widget, drawable, clip,
                              info, logical_region,
                              &env);

}

void
meta_draw_op_draw (const MetaDrawOp    *op,
                   GtkWidget           *widget,
                   GdkDrawable         *drawable,
                   const GdkRectangle  *clip,
                   const MetaDrawInfo  *info,
                   MetaRectangle        logical_region)
{
  meta_draw_op_draw_with_style (op, gtk_widget_get_style (widget), widget,
                                drawable, clip, info, logical_region);
}

MetaDrawOpList*
meta_draw_op_list_new (int n_preallocs)
{
  MetaDrawOpList *op_list;

  g_return_val_if_fail (n_preallocs >= 0, NULL);

  op_list = g_new (MetaDrawOpList, 1);

  op_list->refcount = 1;
  op_list->n_allocated = n_preallocs;
  op_list->ops = g_new (MetaDrawOp*, op_list->n_allocated);
  op_list->n_ops = 0;

  return op_list;
}

void
meta_draw_op_list_ref (MetaDrawOpList *op_list)
{
  g_return_if_fail (op_list != NULL);

  op_list->refcount += 1;
}

void
meta_draw_op_list_unref (MetaDrawOpList *op_list)
{
  g_return_if_fail (op_list != NULL);
  g_return_if_fail (op_list->refcount > 0);

  op_list->refcount -= 1;

  if (op_list->refcount == 0)
    {
      int i;

      for (i = 0; i < op_list->n_ops; i++)
        meta_draw_op_free (op_list->ops[i]);

      g_free (op_list->ops);

      DEBUG_FILL_STRUCT (op_list);
      g_free (op_list);
    }
}

void
meta_draw_op_list_draw_with_style  (const MetaDrawOpList *op_list,
                                    GtkStyle             *style_gtk,
                         GtkWidget            *widget,
                         GdkDrawable          *drawable,
                         const GdkRectangle   *clip,
                         const MetaDrawInfo   *info,
                         MetaRectangle         rect)
{
  int i;
  GdkRectangle active_clip;
  GdkRectangle orig_clip;
  MetaPositionExprEnv env;

  g_return_if_fail (style_gtk->colormap == gdk_drawable_get_colormap (drawable));

  if (op_list->n_ops == 0)
    return;
  
  fill_env (&env, info, rect);
  
  /* FIXME this can be optimized, potentially a lot, by
   * compressing multiple ops when possible. For example,
   * anything convertible to a pixbuf can be composited
   * client-side, and putting a color tint over a pixbuf
   * can be done without creating the solid-color pixbuf.
   *
   * To implement this my plan is to have the idea of a
   * compiled draw op (with the string expressions already
   * evaluated), we make an array of those, and then fold
   * adjacent items when possible.
   */
  if (clip)
    {
      orig_clip = *clip;
    }
  else
    {
      orig_clip.x = rect.x;
      orig_clip.y = rect.y;
      orig_clip.width = rect.width;
      orig_clip.height = rect.height;
    }

  active_clip = orig_clip;

  for (i = 0; i < op_list->n_ops; i++)
    {
      MetaDrawOp *op = op_list->ops[i];
      
      if (op->type == META_DRAW_CLIP)
        {
          active_clip.x = parse_x_position_unchecked (op->data.clip.x, &env);
          active_clip.y = parse_y_position_unchecked (op->data.clip.y, &env);
          active_clip.width = parse_size_unchecked (op->data.clip.width, &env);
          active_clip.height = parse_size_unchecked (op->data.clip.height, &env);
          
          gdk_rectangle_intersect (&orig_clip, &active_clip, &active_clip);
        }
      else if (active_clip.width > 0 &&
               active_clip.height > 0)
        {
          meta_draw_op_draw_with_env (op,
                                      style_gtk, widget, drawable, &active_clip, info,
                                      rect,
                                      &env);
        }
    }
}

void
meta_draw_op_list_draw  (const MetaDrawOpList *op_list,
                         GtkWidget            *widget,
                         GdkDrawable          *drawable,
                         const GdkRectangle   *clip,
                         const MetaDrawInfo   *info,
                         MetaRectangle         rect)

{
  meta_draw_op_list_draw_with_style (op_list, gtk_widget_get_style (widget), widget,
                                     drawable, clip, info, rect);
}

void
meta_draw_op_list_append (MetaDrawOpList       *op_list,
                          MetaDrawOp           *op)
{
  if (op_list->n_ops == op_list->n_allocated)
    {
      op_list->n_allocated *= 2;
      op_list->ops = g_renew (MetaDrawOp*, op_list->ops, op_list->n_allocated);
    }

  op_list->ops[op_list->n_ops] = op;
  op_list->n_ops += 1;
}

gboolean
meta_draw_op_list_validate (MetaDrawOpList    *op_list,
                            GError           **error)
{
  g_return_val_if_fail (op_list != NULL, FALSE);

  /* empty lists are OK, nothing else to check really */

  return TRUE;
}

/* This is not done in validate, since we wouldn't know the name
 * of the list to report the error. It might be nice to
 * store names inside the list sometime.
 */
gboolean
meta_draw_op_list_contains (MetaDrawOpList    *op_list,
                            MetaDrawOpList    *child)
{
  int i;

  /* mmm, huge tree recursion */

  for (i = 0; i < op_list->n_ops; i++)
    {
      if (op_list->ops[i]->type == META_DRAW_OP_LIST)
        {
          if (op_list->ops[i]->data.op_list.op_list == child)
            return TRUE;
          
          if (meta_draw_op_list_contains (op_list->ops[i]->data.op_list.op_list,
                                          child))
            return TRUE;
        }
      else if (op_list->ops[i]->type == META_DRAW_TILE)
        {
          if (op_list->ops[i]->data.tile.op_list == child)
            return TRUE;
          
          if (meta_draw_op_list_contains (op_list->ops[i]->data.tile.op_list,
                                          child))
            return TRUE;
        }
    }

  return FALSE;
}

/**
 * Constructor for a MetaFrameStyle.
 *
 * \param parent  The parent style. Data not filled in here will be
 *                looked for in the parent style, and in its parent
 *                style, and so on.
 *
 * \return The newly-constructed style.
 */
MetaFrameStyle*
meta_frame_style_new (MetaFrameStyle *parent)
{
  MetaFrameStyle *style;

  style = g_new0 (MetaFrameStyle, 1);

  style->refcount = 1;

  /* Default alpha is fully opaque */
  style->window_background_alpha = 255;

  style->parent = parent;
  if (parent)
    meta_frame_style_ref (parent);

  return style;
}

/**
 * Increases the reference count of a frame style.
 * If the style is NULL, this is a no-op.
 *
 * \param style  The style.
 */
void
meta_frame_style_ref (MetaFrameStyle *style)
{
  g_return_if_fail (style != NULL);

  style->refcount += 1;
}

static void
free_button_ops (MetaDrawOpList *op_lists[META_BUTTON_TYPE_LAST][META_BUTTON_STATE_LAST])
{
  int i, j;

  for (i = 0; i < META_BUTTON_TYPE_LAST; i++)
    for (j = 0; j < META_BUTTON_STATE_LAST; j++)
      if (op_lists[i][j])
        meta_draw_op_list_unref (op_lists[i][j]);
}

void
meta_frame_style_unref (MetaFrameStyle *style)
{
  g_return_if_fail (style != NULL);
  g_return_if_fail (style->refcount > 0);

  style->refcount -= 1;

  if (style->refcount == 0)
    {
      int i;

      free_button_ops (style->buttons);

      for (i = 0; i < META_FRAME_PIECE_LAST; i++)
        if (style->pieces[i])
          meta_draw_op_list_unref (style->pieces[i]);

      if (style->layout)
        meta_frame_layout_unref (style->layout);

      if (style->window_background_color)
        meta_color_spec_free (style->window_background_color);

      /* we hold a reference to any parent style */
      if (style->parent)
        meta_frame_style_unref (style->parent);

      DEBUG_FILL_STRUCT (style);
      g_free (style);
    }
}

static MetaDrawOpList*
get_button (MetaFrameStyle *style,
            MetaButtonType  type,
            MetaButtonState state)
{
  MetaDrawOpList *op_list;
  MetaFrameStyle *parent;
  
  parent = style;
  op_list = NULL;
  while (parent && op_list == NULL)
    {
      op_list = parent->buttons[type][state];
      parent = parent->parent;
    }

  /* We fall back to middle button backgrounds if we don't
   * have the ones on the sides
   */

  if (op_list == NULL &&
      (type == META_BUTTON_TYPE_LEFT_LEFT_BACKGROUND ||
       type == META_BUTTON_TYPE_LEFT_RIGHT_BACKGROUND))
    return get_button (style, META_BUTTON_TYPE_LEFT_MIDDLE_BACKGROUND,
                       state);

  if (op_list == NULL &&
      (type == META_BUTTON_TYPE_RIGHT_LEFT_BACKGROUND ||
       type == META_BUTTON_TYPE_RIGHT_RIGHT_BACKGROUND))
    return get_button (style, META_BUTTON_TYPE_RIGHT_MIDDLE_BACKGROUND,
                       state);
  
  /* We fall back to normal if no prelight */
  if (op_list == NULL &&
      state == META_BUTTON_STATE_PRELIGHT)
    return get_button (style, type, META_BUTTON_STATE_NORMAL);

  return op_list;
}

gboolean
meta_frame_style_validate (MetaFrameStyle    *style,
                           guint              current_theme_version,
                           GError           **error)
{
  int i, j;
  
  g_return_val_if_fail (style != NULL, FALSE);
  g_return_val_if_fail (style->layout != NULL, FALSE);

  for (i = 0; i < META_BUTTON_TYPE_LAST; i++)
    {
      /* for now the "positional" buttons are optional */
      if (i >= META_BUTTON_TYPE_CLOSE)
        {
          for (j = 0; j < META_BUTTON_STATE_LAST; j++)
            {
              if (get_button (style, i, j) == NULL &&
                  meta_theme_earliest_version_with_button (i) <= current_theme_version
                  )
                {
                  g_set_error (error, META_THEME_ERROR,
                               META_THEME_ERROR_FAILED,
                               _("<button function=\"%s\" state=\"%s\" draw_ops=\"whatever\"/> must be specified for this frame style"),
                               meta_button_type_to_string (i),
                               meta_button_state_to_string (j));
                  return FALSE;
                }
            }
        }
    }
  
  return TRUE;
}

static void
button_rect (MetaButtonType           type,
             const MetaFrameGeometry *fgeom,
             int                      middle_background_offset,
             GdkRectangle            *rect)
{
  switch (type)
    {
    case META_BUTTON_TYPE_LEFT_LEFT_BACKGROUND:
      *rect = fgeom->left_left_background;
      break;

    case META_BUTTON_TYPE_LEFT_MIDDLE_BACKGROUND:
      *rect = fgeom->left_middle_backgrounds[middle_background_offset];
      break;
      
    case META_BUTTON_TYPE_LEFT_RIGHT_BACKGROUND:
      *rect = fgeom->left_right_background;
      break;
      
    case META_BUTTON_TYPE_RIGHT_LEFT_BACKGROUND:
      *rect = fgeom->right_left_background;
      break;
      
    case META_BUTTON_TYPE_RIGHT_MIDDLE_BACKGROUND:
      *rect = fgeom->right_middle_backgrounds[middle_background_offset];
      break;
      
    case META_BUTTON_TYPE_RIGHT_RIGHT_BACKGROUND:
      *rect = fgeom->right_right_background;
      break;
      
    case META_BUTTON_TYPE_CLOSE:
      *rect = fgeom->close_rect.visible;
      break;

    case META_BUTTON_TYPE_SHADE:
      *rect = fgeom->shade_rect.visible;
      break;

    case META_BUTTON_TYPE_UNSHADE:
      *rect = fgeom->unshade_rect.visible;
      break;

    case META_BUTTON_TYPE_ABOVE:
      *rect = fgeom->above_rect.visible;
      break;

    case META_BUTTON_TYPE_UNABOVE:
      *rect = fgeom->unabove_rect.visible;
      break;

    case META_BUTTON_TYPE_STICK:
      *rect = fgeom->stick_rect.visible;
      break;

    case META_BUTTON_TYPE_UNSTICK:
      *rect = fgeom->unstick_rect.visible;
      break;

    case META_BUTTON_TYPE_MAXIMIZE:
      *rect = fgeom->max_rect.visible;
      break;

    case META_BUTTON_TYPE_MINIMIZE:
      *rect = fgeom->min_rect.visible;
      break;

    case META_BUTTON_TYPE_MENU:
      *rect = fgeom->menu_rect.visible;
      break;
      
    case META_BUTTON_TYPE_LAST:
      g_assert_not_reached ();
      break;
    }
}

void
meta_frame_style_draw_with_style (MetaFrameStyle          *style,
                                  GtkStyle                *style_gtk,
                       GtkWidget               *widget,
                       GdkDrawable             *drawable,
                       int                      x_offset,
                       int                      y_offset,
                       const GdkRectangle      *clip,
                       const MetaFrameGeometry *fgeom,
                       int                      client_width,
                       int                      client_height,
                       PangoLayout             *title_layout,
                       int                      text_height,
                       MetaButtonState          button_states[META_BUTTON_TYPE_LAST],
                       GdkPixbuf               *mini_icon,
                       GdkPixbuf               *icon)
{
  int i, j;
  GdkRectangle titlebar_rect;
  GdkRectangle left_titlebar_edge;
  GdkRectangle right_titlebar_edge;
  GdkRectangle bottom_titlebar_edge;
  GdkRectangle top_titlebar_edge;
  GdkRectangle left_edge, right_edge, bottom_edge;
  PangoRectangle logical_rect;
  MetaDrawInfo draw_info;
  
  g_return_if_fail (style_gtk->colormap == gdk_drawable_get_colormap (drawable));

  titlebar_rect.x = 0;
  titlebar_rect.y = 0;
  titlebar_rect.width = fgeom->width;
  titlebar_rect.height = fgeom->top_height;

  left_titlebar_edge.x = titlebar_rect.x;
  left_titlebar_edge.y = titlebar_rect.y + fgeom->top_titlebar_edge;
  left_titlebar_edge.width = fgeom->left_titlebar_edge;
  left_titlebar_edge.height = titlebar_rect.height - fgeom->top_titlebar_edge - fgeom->bottom_titlebar_edge;

  right_titlebar_edge.y = left_titlebar_edge.y;
  right_titlebar_edge.height = left_titlebar_edge.height;
  right_titlebar_edge.width = fgeom->right_titlebar_edge;
  right_titlebar_edge.x = titlebar_rect.x + titlebar_rect.width - right_titlebar_edge.width;

  top_titlebar_edge.x = titlebar_rect.x;
  top_titlebar_edge.y = titlebar_rect.y;
  top_titlebar_edge.width = titlebar_rect.width;
  top_titlebar_edge.height = fgeom->top_titlebar_edge;

  bottom_titlebar_edge.x = titlebar_rect.x;
  bottom_titlebar_edge.width = titlebar_rect.width;
  bottom_titlebar_edge.height = fgeom->bottom_titlebar_edge;
  bottom_titlebar_edge.y = titlebar_rect.y + titlebar_rect.height - bottom_titlebar_edge.height;

  left_edge.x = 0;
  left_edge.y = fgeom->top_height;
  left_edge.width = fgeom->left_width;
  left_edge.height = fgeom->height - fgeom->top_height - fgeom->bottom_height;

  right_edge.x = fgeom->width - fgeom->right_width;
  right_edge.y = fgeom->top_height;
  right_edge.width = fgeom->right_width;
  right_edge.height = fgeom->height - fgeom->top_height - fgeom->bottom_height;

  bottom_edge.x = 0;
  bottom_edge.y = fgeom->height - fgeom->bottom_height;
  bottom_edge.width = fgeom->width;
  bottom_edge.height = fgeom->bottom_height;

  if (title_layout)
    pango_layout_get_pixel_extents (title_layout,
                                    NULL, &logical_rect);

  draw_info.mini_icon = mini_icon;
  draw_info.icon = icon;
  draw_info.title_layout = title_layout;
  draw_info.title_layout_width = title_layout ? logical_rect.width : 0;
  draw_info.title_layout_height = title_layout ? logical_rect.height : 0;
  draw_info.fgeom = fgeom;
  
  /* The enum is in the order the pieces should be rendered. */
  i = 0;
  while (i < META_FRAME_PIECE_LAST)
    {
      GdkRectangle rect;
      GdkRectangle combined_clip;
      
      switch ((MetaFramePiece) i)
        {
        case META_FRAME_PIECE_ENTIRE_BACKGROUND:
          rect.x = 0;
          rect.y = 0;
          rect.width = fgeom->width;
          rect.height = fgeom->height;
          break;

        case META_FRAME_PIECE_TITLEBAR:
          rect = titlebar_rect;
          break;

        case META_FRAME_PIECE_LEFT_TITLEBAR_EDGE:
          rect = left_titlebar_edge;
          break;

        case META_FRAME_PIECE_RIGHT_TITLEBAR_EDGE:
          rect = right_titlebar_edge;
          break;

        case META_FRAME_PIECE_TOP_TITLEBAR_EDGE:
          rect = top_titlebar_edge;
          break;

        case META_FRAME_PIECE_BOTTOM_TITLEBAR_EDGE:
          rect = bottom_titlebar_edge;
          break;

        case META_FRAME_PIECE_TITLEBAR_MIDDLE:
          rect.x = left_titlebar_edge.x + left_titlebar_edge.width;
          rect.y = top_titlebar_edge.y + top_titlebar_edge.height;
          rect.width = titlebar_rect.width - left_titlebar_edge.width -
            right_titlebar_edge.width;
          rect.height = titlebar_rect.height - top_titlebar_edge.height - bottom_titlebar_edge.height;
          break;

        case META_FRAME_PIECE_TITLE:
          rect = fgeom->title_rect;
          break;

        case META_FRAME_PIECE_LEFT_EDGE:
          rect = left_edge;
          break;

        case META_FRAME_PIECE_RIGHT_EDGE:
          rect = right_edge;
          break;

        case META_FRAME_PIECE_BOTTOM_EDGE:
          rect = bottom_edge;
          break;

        case META_FRAME_PIECE_OVERLAY:
          rect.x = 0;
          rect.y = 0;
          rect.width = fgeom->width;
          rect.height = fgeom->height;
          break;

        case META_FRAME_PIECE_LAST:
          g_assert_not_reached ();
          break;
        }

      rect.x += x_offset;
      rect.y += y_offset;

      if (clip == NULL)
        combined_clip = rect;
      else
        gdk_rectangle_intersect ((GdkRectangle*) clip, /* const cast */
                                 &rect,
                                 &combined_clip);

      if (combined_clip.width > 0 && combined_clip.height > 0)
        {
          MetaDrawOpList *op_list;
          MetaFrameStyle *parent;

          parent = style;
          op_list = NULL;
          while (parent && op_list == NULL)
            {
              op_list = parent->pieces[i];
              parent = parent->parent;
            }

          if (op_list)
            {
              MetaRectangle m_rect;
              m_rect = meta_rect (rect.x, rect.y, rect.width, rect.height);
              meta_draw_op_list_draw_with_style (op_list,
                                                 style_gtk,
                                      widget,
                                      drawable,
                                      &combined_clip,
                                      &draw_info,
                                      m_rect);
            }
        }


      /* Draw buttons just before overlay */
      if ((i + 1) == META_FRAME_PIECE_OVERLAY)
        {
          int middle_bg_offset;

          middle_bg_offset = 0;
          j = 0;
          while (j < META_BUTTON_TYPE_LAST)
            {
              button_rect (j, fgeom, middle_bg_offset, &rect);
              
              rect.x += x_offset;
              rect.y += y_offset;
              
              if (clip == NULL)
                combined_clip = rect;
              else
                gdk_rectangle_intersect ((GdkRectangle*) clip, /* const cast */
                                         &rect,
                                         &combined_clip);
              
              if (combined_clip.width > 0 && combined_clip.height > 0)
                {
                  MetaDrawOpList *op_list;
                  
                  op_list = get_button (style, j, button_states[j]);
                  
                  if (op_list)
                    {
                      MetaRectangle m_rect;
                      m_rect = meta_rect (rect.x, rect.y,
                                          rect.width, rect.height);
                      meta_draw_op_list_draw_with_style (op_list,
                                                         style_gtk,
                                              widget,
                                              drawable,
                                              &combined_clip,
                                              &draw_info,
                                              m_rect);
                    }
                }

              /* MIDDLE_BACKGROUND type may get drawn more than once */
              if ((j == META_BUTTON_TYPE_RIGHT_MIDDLE_BACKGROUND ||
                   j == META_BUTTON_TYPE_LEFT_MIDDLE_BACKGROUND) &&
                  middle_bg_offset < MAX_MIDDLE_BACKGROUNDS)
                {
                  ++middle_bg_offset;
                }
              else
                {
                  middle_bg_offset = 0;
                  ++j;
                }
            }
        }
      
      ++i;
    }
}

void
meta_frame_style_draw (MetaFrameStyle          *style,
                       GtkWidget               *widget,
                       GdkDrawable             *drawable,
                       int                      x_offset,
                       int                      y_offset,
                       const GdkRectangle      *clip,
                       const MetaFrameGeometry *fgeom,
                       int                      client_width,
                       int                      client_height,
                       PangoLayout             *title_layout,
                       int                      text_height,
                       MetaButtonState          button_states[META_BUTTON_TYPE_LAST],
                       GdkPixbuf               *mini_icon,
                       GdkPixbuf               *icon)
{
  meta_frame_style_draw_with_style (style, gtk_widget_get_style (widget), widget,
                                    drawable, x_offset, y_offset,
                                    clip, fgeom, client_width, client_height,
                                    title_layout, text_height,
                                    button_states, mini_icon, icon);
}

MetaFrameStyleSet*
meta_frame_style_set_new (MetaFrameStyleSet *parent)
{
  MetaFrameStyleSet *style_set;

  style_set = g_new0 (MetaFrameStyleSet, 1);

  style_set->parent = parent;
  if (parent)
    meta_frame_style_set_ref (parent);

  style_set->refcount = 1;
  
  return style_set;
}

static void
free_focus_styles (MetaFrameStyle *focus_styles[META_FRAME_FOCUS_LAST])
{
  int i;

  for (i = 0; i < META_FRAME_FOCUS_LAST; i++)
    if (focus_styles[i])
      meta_frame_style_unref (focus_styles[i]);
}

void
meta_frame_style_set_ref (MetaFrameStyleSet *style_set)
{
  g_return_if_fail (style_set != NULL);

  style_set->refcount += 1;
}

void
meta_frame_style_set_unref (MetaFrameStyleSet *style_set)
{
  g_return_if_fail (style_set != NULL);
  g_return_if_fail (style_set->refcount > 0);

  style_set->refcount -= 1;

  if (style_set->refcount == 0)
    {
      int i;

      for (i = 0; i < META_FRAME_RESIZE_LAST; i++)
        {
          free_focus_styles (style_set->normal_styles[i]);
          free_focus_styles (style_set->shaded_styles[i]);
        }

      free_focus_styles (style_set->maximized_styles);
      free_focus_styles (style_set->maximized_and_shaded_styles);

      if (style_set->parent)
        meta_frame_style_set_unref (style_set->parent);

      DEBUG_FILL_STRUCT (style_set);
      g_free (style_set);
    }
}


static MetaFrameStyle*
get_style (MetaFrameStyleSet *style_set,
           MetaFrameState     state,
           MetaFrameResize    resize,
           MetaFrameFocus     focus)
{
  MetaFrameStyle *style;  
  
  style = NULL;

  switch (state)
    {
    case META_FRAME_STATE_NORMAL:
    case META_FRAME_STATE_SHADED:
      {
        if (state == META_FRAME_STATE_SHADED)
          style = style_set->shaded_styles[resize][focus];
        else
          style = style_set->normal_styles[resize][focus];

        /* Try parent if we failed here */
        if (style == NULL && style_set->parent)
          style = get_style (style_set->parent, state, resize, focus);
      
        /* Allow people to omit the vert/horz/none resize modes */
        if (style == NULL &&
            resize != META_FRAME_RESIZE_BOTH)
          style = get_style (style_set, state, META_FRAME_RESIZE_BOTH, focus);
      }
      break;
    default:
      {
        MetaFrameStyle **styles;

        styles = NULL;
      
        switch (state)
          {
          case META_FRAME_STATE_MAXIMIZED:
            styles = style_set->maximized_styles;
            break;
          case META_FRAME_STATE_MAXIMIZED_AND_SHADED:
            styles = style_set->maximized_and_shaded_styles;
            break;
          case META_FRAME_STATE_NORMAL:
          case META_FRAME_STATE_SHADED:
          case META_FRAME_STATE_LAST:
            g_assert_not_reached ();
            break;
          }

        style = styles[focus];

        /* Try parent if we failed here */
        if (style == NULL && style_set->parent)
          style = get_style (style_set->parent, state, resize, focus);      
      }
    }

  return style;
}

static gboolean
check_state  (MetaFrameStyleSet *style_set,
              MetaFrameState     state,
              GError           **error)
{
  int i;

  for (i = 0; i < META_FRAME_FOCUS_LAST; i++)
    {
      if (get_style (style_set, state,
                     META_FRAME_RESIZE_NONE, i) == NULL)
        {
          /* Translators: This error occurs when a <frame> tag is missing
           * in theme XML.  The "<frame ...>" is intended as a noun phrase,
           * and the "missing" qualifies it.  You should translate "whatever".
           */
          g_set_error (error, META_THEME_ERROR,
                       META_THEME_ERROR_FAILED,
                       _("Missing <frame state=\"%s\" resize=\"%s\" focus=\"%s\" style=\"whatever\"/>"),
                       meta_frame_state_to_string (state),
                       meta_frame_resize_to_string (META_FRAME_RESIZE_NONE),
                       meta_frame_focus_to_string (i));
          return FALSE;
        }
    }

  return TRUE;
}

gboolean
meta_frame_style_set_validate  (MetaFrameStyleSet *style_set,
                                GError           **error)
{
  int i, j;
  
  g_return_val_if_fail (style_set != NULL, FALSE);

  for (i = 0; i < META_FRAME_RESIZE_LAST; i++)
    for (j = 0; j < META_FRAME_FOCUS_LAST; j++)
      if (get_style (style_set, META_FRAME_STATE_NORMAL, i, j) == NULL)
        {
          g_set_error (error, META_THEME_ERROR,
                       META_THEME_ERROR_FAILED,
                       _("Missing <frame state=\"%s\" resize=\"%s\" focus=\"%s\" style=\"whatever\"/>"),
                       meta_frame_state_to_string (META_FRAME_STATE_NORMAL),
                       meta_frame_resize_to_string (i),
                       meta_frame_focus_to_string (j));
          return FALSE;
        }

  if (!check_state (style_set, META_FRAME_STATE_SHADED, error))
    return FALSE;
  
  if (!check_state (style_set, META_FRAME_STATE_MAXIMIZED, error))
    return FALSE;

  if (!check_state (style_set, META_FRAME_STATE_MAXIMIZED_AND_SHADED, error))
    return FALSE;
  
  return TRUE;
}

MetaTheme*
meta_theme_get_current (void)
{
  return meta_current_theme;
}

void
meta_theme_set_current (const char *name,
                        gboolean    force_reload)
{
  MetaTheme *new_theme;
  GError *err;

  meta_topic (META_DEBUG_THEMES, "Setting current theme to \"%s\"\n", name);
  
  if (!force_reload &&
      meta_current_theme &&
      strcmp (name, meta_current_theme->name) == 0)
    return;
  
  err = NULL;
  new_theme = meta_theme_load (name, &err);

  if (new_theme == NULL)
    {
      meta_warning (_("Failed to load theme \"%s\": %s\n"),
                    name, err->message);
      g_error_free (err);
    }
  else
    {
      if (meta_current_theme)
        meta_theme_free (meta_current_theme);

      meta_current_theme = new_theme;

      meta_topic (META_DEBUG_THEMES, "New theme is \"%s\"\n", meta_current_theme->name);
    }
}

MetaTheme*
meta_theme_new (void)
{
  MetaTheme *theme;

  theme = g_new0 (MetaTheme, 1);

  theme->images_by_filename =
    g_hash_table_new_full (g_str_hash,
                           g_str_equal,
                           g_free,
                           (GDestroyNotify) g_object_unref);
  
  theme->layouts_by_name =
    g_hash_table_new_full (g_str_hash,
                           g_str_equal,
                           g_free,
                           (GDestroyNotify) meta_frame_layout_unref);

  theme->draw_op_lists_by_name =
    g_hash_table_new_full (g_str_hash,
                           g_str_equal,
                           g_free,
                           (GDestroyNotify) meta_draw_op_list_unref);

  theme->styles_by_name =
    g_hash_table_new_full (g_str_hash,
                           g_str_equal,
                           g_free,
                           (GDestroyNotify) meta_frame_style_unref);

  theme->style_sets_by_name =
    g_hash_table_new_full (g_str_hash,
                           g_str_equal,
                           g_free,
                           (GDestroyNotify) meta_frame_style_set_unref);
  
  /* Create our variable quarks so we can look up variables without
     having to strcmp for the names */
  theme->quark_width = g_quark_from_static_string ("width");
  theme->quark_height = g_quark_from_static_string ("height");
  theme->quark_object_width = g_quark_from_static_string ("object_width");
  theme->quark_object_height = g_quark_from_static_string ("object_height");
  theme->quark_left_width = g_quark_from_static_string ("left_width");
  theme->quark_right_width = g_quark_from_static_string ("right_width");
  theme->quark_top_height = g_quark_from_static_string ("top_height");
  theme->quark_bottom_height = g_quark_from_static_string ("bottom_height");
  theme->quark_mini_icon_width = g_quark_from_static_string ("mini_icon_width");
  theme->quark_mini_icon_height = g_quark_from_static_string ("mini_icon_height");
  theme->quark_icon_width = g_quark_from_static_string ("icon_width");
  theme->quark_icon_height = g_quark_from_static_string ("icon_height");
  theme->quark_title_width = g_quark_from_static_string ("title_width");
  theme->quark_title_height = g_quark_from_static_string ("title_height");
  theme->quark_frame_x_center = g_quark_from_static_string ("frame_x_center");
  theme->quark_frame_y_center = g_quark_from_static_string ("frame_y_center");
  return theme;
}


void
meta_theme_free (MetaTheme *theme)
{
  int i;

  g_return_if_fail (theme != NULL);

  g_free (theme->name);
  g_free (theme->dirname);
  g_free (theme->filename);
  g_free (theme->readable_name);
  g_free (theme->date);
  g_free (theme->description);
  g_free (theme->author);
  g_free (theme->copyright);

  /* be more careful when destroying the theme hash tables,
     since they are only constructed as needed, and may be NULL. */
  if (theme->integer_constants)
    g_hash_table_destroy (theme->integer_constants);
  if (theme->images_by_filename)
    g_hash_table_destroy (theme->images_by_filename);
  if (theme->layouts_by_name)
    g_hash_table_destroy (theme->layouts_by_name);
  if (theme->draw_op_lists_by_name)  
    g_hash_table_destroy (theme->draw_op_lists_by_name);
  if (theme->styles_by_name)  
    g_hash_table_destroy (theme->styles_by_name);
  if (theme->style_sets_by_name)  
    g_hash_table_destroy (theme->style_sets_by_name);

  for (i = 0; i < META_FRAME_TYPE_LAST; i++)
    if (theme->style_sets_by_type[i])
      meta_frame_style_set_unref (theme->style_sets_by_type[i]);

  DEBUG_FILL_STRUCT (theme);
  g_free (theme);
}

gboolean
meta_theme_validate (MetaTheme *theme,
                     GError   **error)
{
  int i;
  
  g_return_val_if_fail (theme != NULL, FALSE);

  /* FIXME what else should be checked? */

  g_assert (theme->name);
  
  if (theme->readable_name == NULL)
    {
      /* Translators: This error means that a necessary XML tag (whose name
       * is given in angle brackets) was not found in a given theme (whose
       * name is given second, in quotation marks).
       */
      g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                   _("No <%s> set for theme \"%s\""), "name", theme->name);
      return FALSE;
    }

  if (theme->author == NULL)
    {
      g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                   _("No <%s> set for theme \"%s\""), "author", theme->name);
      return FALSE;
    }

  if (theme->date == NULL)
    {
      g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                   _("No <%s> set for theme \"%s\""), "date", theme->name);
      return FALSE;
    }

  if (theme->description == NULL)
    {
      g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                   _("No <%s> set for theme \"%s\""), "description", theme->name);
      return FALSE;
    }

  if (theme->copyright == NULL)
    {
      g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                   _("No <%s> set for theme \"%s\""), "copyright", theme->name);
      return FALSE;
    }

  for (i = 0; i < (int)META_FRAME_TYPE_LAST; i++)
    if (theme->style_sets_by_type[i] == NULL)
      {
        g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                     _("No frame style set for window type \"%s\" in theme \"%s\", add a <window type=\"%s\" style_set=\"whatever\"/> element"),
                     meta_frame_type_to_string (i),
                     theme->name,
                     meta_frame_type_to_string (i));
        
        return FALSE;          
      }

  return TRUE;
}

GdkPixbuf*
meta_theme_load_image (MetaTheme  *theme,
                       const char *filename,
                       guint size_of_theme_icons,
                       GError    **error)
{
  GdkPixbuf *pixbuf;

  pixbuf = g_hash_table_lookup (theme->images_by_filename,
                                filename);

  if (pixbuf == NULL)
    {
       
      if (g_str_has_prefix (filename, "theme:") &&
          META_THEME_ALLOWS (theme, META_THEME_IMAGES_FROM_ICON_THEMES))
        {
          pixbuf = gtk_icon_theme_load_icon (
              gtk_icon_theme_get_default (),
              filename+6,
              size_of_theme_icons,
              0,
              error);
          if (pixbuf == NULL) return NULL;
         }
      else
        {
          char *full_path;
          full_path = g_build_filename (theme->dirname, filename, NULL);
      
          pixbuf = gdk_pixbuf_new_from_file (full_path, error);
          if (pixbuf == NULL)
            {
              g_free (full_path);
              return NULL;
            }
      
          g_free (full_path);
        }      
      g_hash_table_replace (theme->images_by_filename,
                            g_strdup (filename),
                            pixbuf);
    }

  g_assert (pixbuf);
  
  g_object_ref (G_OBJECT (pixbuf));

  return pixbuf;
}

static MetaFrameStyle*
theme_get_style (MetaTheme     *theme,
                 MetaFrameType  type,
                 MetaFrameFlags flags)
{
  MetaFrameState state;
  MetaFrameResize resize;
  MetaFrameFocus focus;
  MetaFrameStyle *style;
  MetaFrameStyleSet *style_set;

  style_set = theme->style_sets_by_type[type];

  /* Right now the parser forces a style set for all types,
   * but this fallback code is here in case I take that out.
   */
  if (style_set == NULL)
    style_set = theme->style_sets_by_type[META_FRAME_TYPE_NORMAL];
  if (style_set == NULL)
    return NULL;
  
  switch (flags & (META_FRAME_MAXIMIZED | META_FRAME_SHADED))
    {
    case 0:
      state = META_FRAME_STATE_NORMAL;
      break;
    case META_FRAME_MAXIMIZED:
      state = META_FRAME_STATE_MAXIMIZED;
      break;
    case META_FRAME_SHADED:
      state = META_FRAME_STATE_SHADED;
      break;
    case (META_FRAME_MAXIMIZED | META_FRAME_SHADED):
      state = META_FRAME_STATE_MAXIMIZED_AND_SHADED;
      break;
    default:
      g_assert_not_reached ();
      state = META_FRAME_STATE_LAST; /* compiler */
      break;
    }

  switch (flags & (META_FRAME_ALLOWS_VERTICAL_RESIZE | META_FRAME_ALLOWS_HORIZONTAL_RESIZE))
    {
    case 0:
      resize = META_FRAME_RESIZE_NONE;
      break;
    case META_FRAME_ALLOWS_VERTICAL_RESIZE:
      resize = META_FRAME_RESIZE_VERTICAL;
      break;
    case META_FRAME_ALLOWS_HORIZONTAL_RESIZE:
      resize = META_FRAME_RESIZE_HORIZONTAL;
      break;
    case (META_FRAME_ALLOWS_VERTICAL_RESIZE | META_FRAME_ALLOWS_HORIZONTAL_RESIZE):
      resize = META_FRAME_RESIZE_BOTH;
      break;
    default:
      g_assert_not_reached ();
      resize = META_FRAME_RESIZE_LAST; /* compiler */
      break;
    }
  
  /* re invert the styles used for focus/unfocussed while flashing a frame */
  if (((flags & META_FRAME_HAS_FOCUS) && !(flags & META_FRAME_IS_FLASHING))
      || (!(flags & META_FRAME_HAS_FOCUS) && (flags & META_FRAME_IS_FLASHING)))
    focus = META_FRAME_FOCUS_YES;
  else
    focus = META_FRAME_FOCUS_NO;

  style = get_style (style_set, state, resize, focus);

  return style;
}

MetaFrameStyle*
meta_theme_get_frame_style (MetaTheme     *theme,
                            MetaFrameType  type,
                            MetaFrameFlags flags)
{
  MetaFrameStyle *style;

  g_return_val_if_fail (type < META_FRAME_TYPE_LAST, NULL);
  
  style = theme_get_style (theme, type, flags);

  return style;
}

double
meta_theme_get_title_scale (MetaTheme     *theme,
                            MetaFrameType  type,
                            MetaFrameFlags flags)
{
  MetaFrameStyle *style;

  g_return_val_if_fail (type < META_FRAME_TYPE_LAST, 1.0);
  
  style = theme_get_style (theme, type, flags);
  
  /* Parser is not supposed to allow this currently */
  if (style == NULL)
    return 1.0;

  return style->layout->title_scale;
}

void
meta_theme_draw_frame_with_style (MetaTheme              *theme,
                                  GtkStyle               *style_gtk,
                       GtkWidget              *widget,
                       GdkDrawable            *drawable,
                       const GdkRectangle     *clip,
                       int                     x_offset,
                       int                     y_offset,
                       MetaFrameType           type,
                       MetaFrameFlags          flags,
                       int                     client_width,
                       int                     client_height,
                       PangoLayout            *title_layout,
                       int                     text_height,
                       const MetaButtonLayout *button_layout,
                       MetaButtonState         button_states[META_BUTTON_TYPE_LAST],
                       GdkPixbuf              *mini_icon,
                       GdkPixbuf              *icon)
{
  MetaFrameGeometry fgeom;
  MetaFrameStyle *style;

  g_return_if_fail (type < META_FRAME_TYPE_LAST);
  
  style = theme_get_style (theme, type, flags);
  
  /* Parser is not supposed to allow this currently */
  if (style == NULL)
    return;
  
  meta_frame_layout_calc_geometry (style->layout,
                                   text_height,
                                   flags,
                                   client_width, client_height,
                                   button_layout,
                                   &fgeom,
                                   theme);  

  meta_frame_style_draw_with_style (style,
                                    style_gtk,
                                    widget,
                                    drawable,
                                    x_offset, y_offset,
                                    clip,
                                    &fgeom,
                                    client_width, client_height,
                                    title_layout,
                                    text_height,
                                    button_states,
                                    mini_icon, icon);
}

void
meta_theme_draw_frame (MetaTheme              *theme,
                       GtkWidget              *widget,
                       GdkDrawable            *drawable,
                       const GdkRectangle     *clip,
                       int                     x_offset,
                       int                     y_offset,
                       MetaFrameType           type,
                       MetaFrameFlags          flags,
                       int                     client_width,
                       int                     client_height,
                       PangoLayout            *title_layout,
                       int                     text_height,
                       const MetaButtonLayout *button_layout,
                       MetaButtonState         button_states[META_BUTTON_TYPE_LAST],
                       GdkPixbuf              *mini_icon,
                       GdkPixbuf              *icon)
{
  meta_theme_draw_frame_with_style (theme, gtk_widget_get_style (widget), widget,
                                    drawable, clip, x_offset, y_offset, type,flags,
                                    client_width, client_height,
                                    title_layout, text_height,
                                    button_layout, button_states,
                                    mini_icon, icon);
}

void
meta_theme_draw_frame_by_name (MetaTheme              *theme,
                               GtkWidget              *widget,
                               GdkDrawable            *drawable,
                               const GdkRectangle     *clip,
                               int                     x_offset,
                               int                     y_offset,
                               const gchar             *style_name,
                               MetaFrameFlags          flags,
                               int                     client_width,
                               int                     client_height,
                               PangoLayout            *title_layout,
                               int                     text_height,
                               const MetaButtonLayout *button_layout,
                               MetaButtonState         button_states[META_BUTTON_TYPE_LAST],
                               GdkPixbuf              *mini_icon,
                               GdkPixbuf              *icon)
{
  MetaFrameGeometry fgeom;
  MetaFrameStyle *style;

  style = meta_theme_lookup_style (theme, style_name);
  
  /* Parser is not supposed to allow this currently */
  if (style == NULL)
    return;
  
  meta_frame_layout_calc_geometry (style->layout,
                                   text_height,
                                   flags,
                                   client_width, client_height,
                                   button_layout,
                                   &fgeom,
                                   theme);  

  meta_frame_style_draw (style,
                         widget,
                         drawable,
                         x_offset, y_offset,
                         clip,
                         &fgeom,
                         client_width, client_height,
                         title_layout,
                         text_height,
                         button_states,
                         mini_icon, icon);
}

void
meta_theme_get_frame_borders (MetaTheme      *theme,
                              MetaFrameType   type,
                              int             text_height,
                              MetaFrameFlags  flags,
                              int            *top_height,
                              int            *bottom_height,
                              int            *left_width,
                              int            *right_width)
{
  MetaFrameStyle *style;

  g_return_if_fail (type < META_FRAME_TYPE_LAST);
  
  if (top_height)
    *top_height = 0;
  if (bottom_height)
    *bottom_height = 0;
  if (left_width)
    *left_width = 0;
  if (right_width)
    *right_width = 0;
  
  style = theme_get_style (theme, type, flags);
  
  /* Parser is not supposed to allow this currently */
  if (style == NULL)
    return;

  meta_frame_layout_get_borders (style->layout,
                                 text_height,
                                 flags,
                                 top_height, bottom_height,
                                 left_width, right_width);
}

void
meta_theme_calc_geometry (MetaTheme              *theme,
                          MetaFrameType           type,
                          int                     text_height,
                          MetaFrameFlags          flags,
                          int                     client_width,
                          int                     client_height,
                          const MetaButtonLayout *button_layout,
                          MetaFrameGeometry      *fgeom)
{
  MetaFrameStyle *style;

  g_return_if_fail (type < META_FRAME_TYPE_LAST);
  
  style = theme_get_style (theme, type, flags);
  
  /* Parser is not supposed to allow this currently */
  if (style == NULL)
    return;

  meta_frame_layout_calc_geometry (style->layout,
                                   text_height,
                                   flags,
                                   client_width, client_height,
                                   button_layout,
                                   fgeom,
                                   theme);
}

MetaFrameLayout*
meta_theme_lookup_layout (MetaTheme         *theme,
                          const char        *name)
{
  return g_hash_table_lookup (theme->layouts_by_name, name);
}

void
meta_theme_insert_layout (MetaTheme         *theme,
                          const char        *name,
                          MetaFrameLayout   *layout)
{
  meta_frame_layout_ref (layout);
  g_hash_table_replace (theme->layouts_by_name, g_strdup (name), layout);
}

MetaDrawOpList*
meta_theme_lookup_draw_op_list (MetaTheme         *theme,
                                const char        *name)
{
  return g_hash_table_lookup (theme->draw_op_lists_by_name, name);
}

void
meta_theme_insert_draw_op_list (MetaTheme         *theme,
                                const char        *name,
                                MetaDrawOpList    *op_list)
{
  meta_draw_op_list_ref (op_list);
  g_hash_table_replace (theme->draw_op_lists_by_name, g_strdup (name), op_list);
}

MetaFrameStyle*
meta_theme_lookup_style (MetaTheme         *theme,
                         const char        *name)
{
  return g_hash_table_lookup (theme->styles_by_name, name);
}

void
meta_theme_insert_style (MetaTheme         *theme,
                         const char        *name,
                         MetaFrameStyle    *style)
{
  meta_frame_style_ref (style);
  g_hash_table_replace (theme->styles_by_name, g_strdup (name), style);
}

MetaFrameStyleSet*
meta_theme_lookup_style_set (MetaTheme         *theme,
                             const char        *name)
{
  return g_hash_table_lookup (theme->style_sets_by_name, name);
}

void
meta_theme_insert_style_set    (MetaTheme         *theme,
                                const char        *name,
                                MetaFrameStyleSet *style_set)
{
  meta_frame_style_set_ref (style_set);
  g_hash_table_replace (theme->style_sets_by_name, g_strdup (name), style_set);
}

static gboolean
first_uppercase (const char *str)
{  
  return g_ascii_isupper (*str);
}

gboolean
meta_theme_define_int_constant (MetaTheme   *theme,
                                const char  *name,
                                int          value,
                                GError     **error)
{
  if (theme->integer_constants == NULL)
    theme->integer_constants = g_hash_table_new_full (g_str_hash,
                                                      g_str_equal,
                                                      g_free,
                                                      NULL);

  if (!first_uppercase (name))
    {
      g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                   _("User-defined constants must begin with a capital letter; \"%s\" does not"),
                   name);
      return FALSE;
    }
  
  if (g_hash_table_lookup_extended (theme->integer_constants, name, NULL, NULL))
    {
      g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                   _("Constant \"%s\" has already been defined"),
                   name);
      
      return FALSE;
    }

  g_hash_table_insert (theme->integer_constants,
                       g_strdup (name),
                       GINT_TO_POINTER (value));

  return TRUE;
}

gboolean
meta_theme_lookup_int_constant (MetaTheme   *theme,
                                const char  *name,
                                int         *value)
{
  gpointer old_value;

  *value = 0;
  
  if (theme->integer_constants == NULL)
    return FALSE;
  
  if (g_hash_table_lookup_extended (theme->integer_constants,
                                    name, NULL, &old_value))
    {
      *value = GPOINTER_TO_INT (old_value);
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

gboolean
meta_theme_define_float_constant (MetaTheme   *theme,
                                  const char  *name,
                                  double       value,
                                  GError     **error)
{
  double *d;
  
  if (theme->float_constants == NULL)
    theme->float_constants = g_hash_table_new_full (g_str_hash,
                                                    g_str_equal,
                                                    g_free,
                                                    g_free);

  if (!first_uppercase (name))
    {
      g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                   _("User-defined constants must begin with a capital letter; \"%s\" does not"),
                   name);
      return FALSE;
    }
  
  if (g_hash_table_lookup_extended (theme->float_constants, name, NULL, NULL))
    {
      g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                   _("Constant \"%s\" has already been defined"),
                   name);
      
      return FALSE;
    }

  d = g_new (double, 1);
  *d = value;
  
  g_hash_table_insert (theme->float_constants,
                       g_strdup (name), d);

  return TRUE;
}

gboolean
meta_theme_lookup_float_constant (MetaTheme   *theme,
                                  const char  *name,
                                  double      *value)
{
  double *d;

  *value = 0.0;
  
  if (theme->float_constants == NULL)
    return FALSE;

  d = g_hash_table_lookup (theme->float_constants, name);

  if (d)
    {
      *value = *d;
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

gboolean
meta_theme_define_color_constant (MetaTheme   *theme,
                                  const char  *name,
                                  const char  *value,
                                  GError     **error)
{
  if (theme->color_constants == NULL)
    theme->color_constants = g_hash_table_new_full (g_str_hash,
                                                    g_str_equal,
                                                    g_free,
                                                    NULL);

  if (!first_uppercase (name))
    {
      g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                   _("User-defined constants must begin with a capital letter; \"%s\" does not"),
                   name);
      return FALSE;
    }
  
  if (g_hash_table_lookup_extended (theme->color_constants, name, NULL, NULL))
    {
      g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                   _("Constant \"%s\" has already been defined"),
                   name);
      
      return FALSE;
    }

  g_hash_table_insert (theme->color_constants,
                       g_strdup (name),
                       g_strdup (value));

  return TRUE;
}

/**
 * Looks up a colour constant.
 *
 * \param theme  the theme containing the constant
 * \param name  the name of the constant
 * \param value  [out] the string representation of the colour, or NULL if it
 *               doesn't exist
 * \return  TRUE if it exists, FALSE otherwise
 */
gboolean
meta_theme_lookup_color_constant (MetaTheme   *theme,
                                  const char  *name,
                                  char       **value)
{
  char *result;

  *value = NULL;
  
  if (theme->color_constants == NULL)
    return FALSE;

  result = g_hash_table_lookup (theme->color_constants, name);

  if (result)
    {
      *value = result;
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}


PangoFontDescription*
meta_gtk_widget_get_font_desc (GtkWidget *widget,
                               double     scale,
			       const PangoFontDescription *override)
{
  PangoFontDescription *font_desc;
  
  g_return_val_if_fail (gtk_widget_get_realized (widget), NULL);

  font_desc = pango_font_description_copy (gtk_widget_get_style (widget)->font_desc);

  if (override)
    pango_font_description_merge (font_desc, override, TRUE);

  pango_font_description_set_size (font_desc,
                                   MAX (pango_font_description_get_size (font_desc) * scale, 1));

  return font_desc;
}

/**
 * Returns the height of the letters in a particular font.
 *
 * \param font_desc  the font
 * \param context  the context of the font
 * \return  the height of the letters
 */
int
meta_pango_font_desc_get_text_height (const PangoFontDescription *font_desc,
                                      PangoContext         *context)
{
  PangoFontMetrics *metrics;
  PangoLanguage *lang;
  int retval;

  lang = pango_context_get_language (context);
  metrics = pango_context_get_metrics (context, font_desc, lang);

  retval = PANGO_PIXELS (pango_font_metrics_get_ascent (metrics) + 
                         pango_font_metrics_get_descent (metrics));
  
  pango_font_metrics_unref (metrics);
  
  return retval;
}

MetaGtkColorComponent
meta_color_component_from_string (const char *str)
{
  if (strcmp ("fg", str) == 0)
    return META_GTK_COLOR_FG;
  else if (strcmp ("bg", str) == 0)
    return META_GTK_COLOR_BG;
  else if (strcmp ("light", str) == 0)
    return META_GTK_COLOR_LIGHT;
  else if (strcmp ("dark", str) == 0)
    return META_GTK_COLOR_DARK;
  else if (strcmp ("mid", str) == 0)
    return META_GTK_COLOR_MID;
  else if (strcmp ("text", str) == 0)
    return META_GTK_COLOR_TEXT;
  else if (strcmp ("base", str) == 0)
    return META_GTK_COLOR_BASE;
  else if (strcmp ("text_aa", str) == 0)
    return META_GTK_COLOR_TEXT_AA;
  else
    return META_GTK_COLOR_LAST;
}

const char*
meta_color_component_to_string (MetaGtkColorComponent component)
{
  switch (component)
    {
    case META_GTK_COLOR_FG:
      return "fg";
    case META_GTK_COLOR_BG:
      return "bg";
    case META_GTK_COLOR_LIGHT:
      return "light";
    case META_GTK_COLOR_DARK:
      return "dark";
    case META_GTK_COLOR_MID:
      return "mid";
    case META_GTK_COLOR_TEXT:
      return "text";
    case META_GTK_COLOR_BASE:
      return "base";
    case META_GTK_COLOR_TEXT_AA:
      return "text_aa";
    case META_GTK_COLOR_LAST:
      break;
    }

  return "<unknown>";
}

MetaButtonState
meta_button_state_from_string (const char *str)
{
  if (strcmp ("normal", str) == 0)
    return META_BUTTON_STATE_NORMAL;
  else if (strcmp ("pressed", str) == 0)
    return META_BUTTON_STATE_PRESSED;
  else if (strcmp ("prelight", str) == 0)
    return META_BUTTON_STATE_PRELIGHT;
  else
    return META_BUTTON_STATE_LAST;
}

const char*
meta_button_state_to_string (MetaButtonState state)
{
  switch (state)
    {
    case META_BUTTON_STATE_NORMAL:
      return "normal";
    case META_BUTTON_STATE_PRESSED:
      return "pressed";
    case META_BUTTON_STATE_PRELIGHT:
      return "prelight";
    case META_BUTTON_STATE_LAST:
      break;
    }

  return "<unknown>";
}

MetaButtonType
meta_button_type_from_string (const char *str, MetaTheme *theme)
{
  if (META_THEME_ALLOWS(theme, META_THEME_SHADE_STICK_ABOVE_BUTTONS))
    {
      if (strcmp ("shade", str) == 0)
        return META_BUTTON_TYPE_SHADE;
      else if (strcmp ("above", str) == 0)
        return META_BUTTON_TYPE_ABOVE;
      else if (strcmp ("stick", str) == 0)
        return META_BUTTON_TYPE_STICK;
      else if (strcmp ("unshade", str) == 0)
        return META_BUTTON_TYPE_UNSHADE;
      else if (strcmp ("unabove", str) == 0)
        return META_BUTTON_TYPE_UNABOVE;
      else if (strcmp ("unstick", str) == 0)
        return META_BUTTON_TYPE_UNSTICK;
     }

  if (strcmp ("close", str) == 0)
    return META_BUTTON_TYPE_CLOSE;
  else if (strcmp ("maximize", str) == 0)
    return META_BUTTON_TYPE_MAXIMIZE;
  else if (strcmp ("minimize", str) == 0)
    return META_BUTTON_TYPE_MINIMIZE;
  else if (strcmp ("menu", str) == 0)
    return META_BUTTON_TYPE_MENU;
  else if (strcmp ("left_left_background", str) == 0)
    return META_BUTTON_TYPE_LEFT_LEFT_BACKGROUND;
  else if (strcmp ("left_middle_background", str) == 0)
    return META_BUTTON_TYPE_LEFT_MIDDLE_BACKGROUND;
  else if (strcmp ("left_right_background", str) == 0)
    return META_BUTTON_TYPE_LEFT_RIGHT_BACKGROUND;
  else if (strcmp ("right_left_background", str) == 0)
    return META_BUTTON_TYPE_RIGHT_LEFT_BACKGROUND;
  else if (strcmp ("right_middle_background", str) == 0)
    return META_BUTTON_TYPE_RIGHT_MIDDLE_BACKGROUND;
  else if (strcmp ("right_right_background", str) == 0)
    return META_BUTTON_TYPE_RIGHT_RIGHT_BACKGROUND;
  else
    return META_BUTTON_TYPE_LAST;
}

const char*
meta_button_type_to_string (MetaButtonType type)
{
  switch (type)
    {
    case META_BUTTON_TYPE_CLOSE:
      return "close";
    case META_BUTTON_TYPE_MAXIMIZE:
      return "maximize";
    case META_BUTTON_TYPE_MINIMIZE:
      return "minimize";
    case META_BUTTON_TYPE_SHADE:
     return "shade";
    case META_BUTTON_TYPE_ABOVE:
      return "above";
    case META_BUTTON_TYPE_STICK:
      return "stick";
    case META_BUTTON_TYPE_UNSHADE:
      return "unshade";
    case META_BUTTON_TYPE_UNABOVE:
      return "unabove";
    case META_BUTTON_TYPE_UNSTICK:
      return "unstick";
     case META_BUTTON_TYPE_MENU:
      return "menu";
    case META_BUTTON_TYPE_LEFT_LEFT_BACKGROUND:
      return "left_left_background";
    case META_BUTTON_TYPE_LEFT_MIDDLE_BACKGROUND:
      return "left_middle_background";
    case META_BUTTON_TYPE_LEFT_RIGHT_BACKGROUND:
      return "left_right_background";
    case META_BUTTON_TYPE_RIGHT_LEFT_BACKGROUND:
      return "right_left_background";
    case META_BUTTON_TYPE_RIGHT_MIDDLE_BACKGROUND:
      return "right_middle_background";
    case META_BUTTON_TYPE_RIGHT_RIGHT_BACKGROUND:
      return "right_right_background";      
    case META_BUTTON_TYPE_LAST:
      break;
    }

  return "<unknown>";
}

MetaFramePiece
meta_frame_piece_from_string (const char *str)
{
  if (strcmp ("entire_background", str) == 0)
    return META_FRAME_PIECE_ENTIRE_BACKGROUND;
  else if (strcmp ("titlebar", str) == 0)
    return META_FRAME_PIECE_TITLEBAR;
  else if (strcmp ("titlebar_middle", str) == 0)
    return META_FRAME_PIECE_TITLEBAR_MIDDLE;
  else if (strcmp ("left_titlebar_edge", str) == 0)
    return META_FRAME_PIECE_LEFT_TITLEBAR_EDGE;
  else if (strcmp ("right_titlebar_edge", str) == 0)
    return META_FRAME_PIECE_RIGHT_TITLEBAR_EDGE;
  else if (strcmp ("top_titlebar_edge", str) == 0)
    return META_FRAME_PIECE_TOP_TITLEBAR_EDGE;
  else if (strcmp ("bottom_titlebar_edge", str) == 0)
    return META_FRAME_PIECE_BOTTOM_TITLEBAR_EDGE;
  else if (strcmp ("title", str) == 0)
    return META_FRAME_PIECE_TITLE;
  else if (strcmp ("left_edge", str) == 0)
    return META_FRAME_PIECE_LEFT_EDGE;
  else if (strcmp ("right_edge", str) == 0)
    return META_FRAME_PIECE_RIGHT_EDGE;
  else if (strcmp ("bottom_edge", str) == 0)
    return META_FRAME_PIECE_BOTTOM_EDGE;
  else if (strcmp ("overlay", str) == 0)
    return META_FRAME_PIECE_OVERLAY;
  else
    return META_FRAME_PIECE_LAST;
}

const char*
meta_frame_piece_to_string (MetaFramePiece piece)
{
  switch (piece)
    {
    case META_FRAME_PIECE_ENTIRE_BACKGROUND:
      return "entire_background";
    case META_FRAME_PIECE_TITLEBAR:
      return "titlebar";
    case META_FRAME_PIECE_TITLEBAR_MIDDLE:
      return "titlebar_middle";
    case META_FRAME_PIECE_LEFT_TITLEBAR_EDGE:
      return "left_titlebar_edge";
    case META_FRAME_PIECE_RIGHT_TITLEBAR_EDGE:
      return "right_titlebar_edge";
    case META_FRAME_PIECE_TOP_TITLEBAR_EDGE:
      return "top_titlebar_edge";
    case META_FRAME_PIECE_BOTTOM_TITLEBAR_EDGE:
      return "bottom_titlebar_edge";
    case META_FRAME_PIECE_TITLE:
      return "title";
    case META_FRAME_PIECE_LEFT_EDGE:
      return "left_edge";
    case META_FRAME_PIECE_RIGHT_EDGE:
      return "right_edge";
    case META_FRAME_PIECE_BOTTOM_EDGE:
      return "bottom_edge";
    case META_FRAME_PIECE_OVERLAY:
      return "overlay";
    case META_FRAME_PIECE_LAST:
      break;
    }

  return "<unknown>";
}

MetaFrameState
meta_frame_state_from_string (const char *str)
{
  if (strcmp ("normal", str) == 0)
    return META_FRAME_STATE_NORMAL;
  else if (strcmp ("maximized", str) == 0)
    return META_FRAME_STATE_MAXIMIZED;
  else if (strcmp ("shaded", str) == 0)
    return META_FRAME_STATE_SHADED;
  else if (strcmp ("maximized_and_shaded", str) == 0)
    return META_FRAME_STATE_MAXIMIZED_AND_SHADED;
  else
    return META_FRAME_STATE_LAST;
}

const char*
meta_frame_state_to_string (MetaFrameState state)
{
  switch (state)
    {
    case META_FRAME_STATE_NORMAL:
      return "normal";
    case META_FRAME_STATE_MAXIMIZED:
      return "maximized";
    case META_FRAME_STATE_SHADED:
      return "shaded";
    case META_FRAME_STATE_MAXIMIZED_AND_SHADED:
      return "maximized_and_shaded";
    case META_FRAME_STATE_LAST:
      break;
    }

  return "<unknown>";
}

MetaFrameResize
meta_frame_resize_from_string (const char *str)
{
  if (strcmp ("none", str) == 0)
    return META_FRAME_RESIZE_NONE;
  else if (strcmp ("vertical", str) == 0)
    return META_FRAME_RESIZE_VERTICAL;
  else if (strcmp ("horizontal", str) == 0)
    return META_FRAME_RESIZE_HORIZONTAL;
  else if (strcmp ("both", str) == 0)
    return META_FRAME_RESIZE_BOTH;
  else
    return META_FRAME_RESIZE_LAST;
}

const char*
meta_frame_resize_to_string (MetaFrameResize resize)
{
  switch (resize)
    {
    case META_FRAME_RESIZE_NONE:
      return "none";
    case META_FRAME_RESIZE_VERTICAL:
      return "vertical";
    case META_FRAME_RESIZE_HORIZONTAL:
      return "horizontal";
    case META_FRAME_RESIZE_BOTH:
      return "both";
    case META_FRAME_RESIZE_LAST:
      break;
    }

  return "<unknown>";
}

MetaFrameFocus
meta_frame_focus_from_string (const char *str)
{
  if (strcmp ("no", str) == 0)
    return META_FRAME_FOCUS_NO;
  else if (strcmp ("yes", str) == 0)
    return META_FRAME_FOCUS_YES;
  else
    return META_FRAME_FOCUS_LAST;
}

const char*
meta_frame_focus_to_string (MetaFrameFocus focus)
{
  switch (focus)
    {
    case META_FRAME_FOCUS_NO:
      return "no";
    case META_FRAME_FOCUS_YES:
      return "yes";
    case META_FRAME_FOCUS_LAST:
      break;
    }

  return "<unknown>";
}

MetaFrameType
meta_frame_type_from_string (const char *str)
{
  if (strcmp ("normal", str) == 0)
    return META_FRAME_TYPE_NORMAL;
  else if (strcmp ("dialog", str) == 0)
    return META_FRAME_TYPE_DIALOG;
  else if (strcmp ("modal_dialog", str) == 0)
    return META_FRAME_TYPE_MODAL_DIALOG;
  else if (strcmp ("utility", str) == 0)
    return META_FRAME_TYPE_UTILITY;
  else if (strcmp ("menu", str) == 0)
    return META_FRAME_TYPE_MENU;
  else if (strcmp ("border", str) == 0)
    return META_FRAME_TYPE_BORDER;
#if 0
  else if (strcmp ("toolbar", str) == 0)
    return META_FRAME_TYPE_TOOLBAR;
#endif
  else
    return META_FRAME_TYPE_LAST;
}

const char*
meta_frame_type_to_string (MetaFrameType type)
{
  switch (type)
    {
    case META_FRAME_TYPE_NORMAL:
      return "normal";
    case META_FRAME_TYPE_DIALOG:
      return "dialog";
    case META_FRAME_TYPE_MODAL_DIALOG:
      return "modal_dialog";
    case META_FRAME_TYPE_UTILITY:
      return "utility";
    case META_FRAME_TYPE_MENU:
      return "menu";
    case META_FRAME_TYPE_BORDER:
      return "border";
#if 0
    case META_FRAME_TYPE_TOOLBAR:
      return "toolbar";
#endif
    case  META_FRAME_TYPE_LAST:
      break;
    }

  return "<unknown>";
}

MetaGradientType
meta_gradient_type_from_string (const char *str)
{
  if (strcmp ("vertical", str) == 0)
    return META_GRADIENT_VERTICAL;
  else if (strcmp ("horizontal", str) == 0)
    return META_GRADIENT_HORIZONTAL;
  else if (strcmp ("diagonal", str) == 0)
    return META_GRADIENT_DIAGONAL;
  else
    return META_GRADIENT_LAST;
}

const char*
meta_gradient_type_to_string (MetaGradientType type)
{
  switch (type)
    {
    case META_GRADIENT_VERTICAL:
      return "vertical";
    case META_GRADIENT_HORIZONTAL:
      return "horizontal";
    case META_GRADIENT_DIAGONAL:
      return "diagonal";
    case META_GRADIENT_LAST:
      break;
    }

  return "<unknown>";
}

GtkStateType
meta_gtk_state_from_string (const char *str)
{
  if (strcmp ("normal", str) == 0 || strcmp ("NORMAL", str) == 0)
    return GTK_STATE_NORMAL;
  else if (strcmp ("prelight", str) == 0 || strcmp ("PRELIGHT", str) == 0)
    return GTK_STATE_PRELIGHT;
  else if (strcmp ("active", str) == 0 || strcmp ("ACTIVE", str) == 0)
    return GTK_STATE_ACTIVE;
  else if (strcmp ("selected", str) == 0 || strcmp ("SELECTED", str) == 0)
    return GTK_STATE_SELECTED;
  else if (strcmp ("insensitive", str) == 0 || strcmp ("INSENSITIVE", str) == 0)
    return GTK_STATE_INSENSITIVE;
  else
    return -1; /* hack */
}

const char*
meta_gtk_state_to_string (GtkStateType state)
{
  switch (state)
    {
    case GTK_STATE_NORMAL:
      return "NORMAL";
    case GTK_STATE_PRELIGHT:
      return "PRELIGHT";
    case GTK_STATE_ACTIVE:
      return "ACTIVE";
    case GTK_STATE_SELECTED:
      return "SELECTED";
    case GTK_STATE_INSENSITIVE:
      return "INSENSITIVE";
    }

  return "<unknown>";
}

GtkShadowType
meta_gtk_shadow_from_string (const char *str)
{
  if (strcmp ("none", str) == 0)
    return GTK_SHADOW_NONE;
  else if (strcmp ("in", str) == 0)
    return GTK_SHADOW_IN;
  else if (strcmp ("out", str) == 0)
    return GTK_SHADOW_OUT;
  else if (strcmp ("etched_in", str) == 0)
    return GTK_SHADOW_ETCHED_IN;
  else if (strcmp ("etched_out", str) == 0)
    return GTK_SHADOW_ETCHED_OUT;
  else
    return -1;
}

const char*
meta_gtk_shadow_to_string (GtkShadowType shadow)
{
  switch (shadow)
    {
    case GTK_SHADOW_NONE:
      return "none";
    case GTK_SHADOW_IN:
      return "in";
    case GTK_SHADOW_OUT:
      return "out";
    case GTK_SHADOW_ETCHED_IN:
      return "etched_in";
    case GTK_SHADOW_ETCHED_OUT:
      return "etched_out";
    }

  return "<unknown>";
}

GtkArrowType
meta_gtk_arrow_from_string (const char *str)
{
  if (strcmp ("up", str) == 0)
    return GTK_ARROW_UP;
  else if (strcmp ("down", str) == 0)
    return GTK_ARROW_DOWN;
  else if (strcmp ("left", str) == 0)
    return GTK_ARROW_LEFT;
  else if (strcmp ("right", str) == 0)
    return GTK_ARROW_RIGHT;
  else if (strcmp ("none", str) == 0)
    return GTK_ARROW_NONE;
  else
    return -1;
}

const char*
meta_gtk_arrow_to_string (GtkArrowType arrow)
{
  switch (arrow)
    {
    case GTK_ARROW_UP:
      return "up";
    case GTK_ARROW_DOWN:
      return "down";
    case GTK_ARROW_LEFT:
      return "left";
    case GTK_ARROW_RIGHT:
      return "right";
    case GTK_ARROW_NONE:
      return "none";
    }

  return "<unknown>";
}

/**
 * Returns a fill_type from a string.  The inverse of
 * meta_image_fill_type_to_string().
 *
 * \param str  a string representing a fill_type
 * \result  the fill_type, or -1 if it represents no fill_type.
 */
MetaImageFillType
meta_image_fill_type_from_string (const char *str)
{
  if (strcmp ("tile", str) == 0)
    return META_IMAGE_FILL_TILE;
  else if (strcmp ("scale", str) == 0)
    return META_IMAGE_FILL_SCALE;
  else
    return -1;
}

/**
 * Returns a string representation of a fill_type.  The inverse of
 * meta_image_fill_type_from_string().
 *
 * \param fill_type  the fill type
 * \result  a string representing that type
 */
const char*
meta_image_fill_type_to_string (MetaImageFillType fill_type)
{
  switch (fill_type)
    {
    case META_IMAGE_FILL_TILE:
      return "tile";
    case META_IMAGE_FILL_SCALE:
      return "scale";
    }
  
  return "<unknown>";
}

/**
 * Takes a colour "a", scales the lightness and saturation by a certain amount,
 * and sets "b" to the resulting colour.
 * gtkstyle.c cut-and-pastage.
 *
 * \param a  the starting colour
 * \param b  [out] the resulting colour
 * \param k  amount to scale lightness and saturation by
 */ 
static void
gtk_style_shade (GdkColor *a,
                 GdkColor *b,
                 gdouble   k)
{
  gdouble red;
  gdouble green;
  gdouble blue;
  
  red = (gdouble) a->red / 65535.0;
  green = (gdouble) a->green / 65535.0;
  blue = (gdouble) a->blue / 65535.0;
  
  rgb_to_hls (&red, &green, &blue);
  
  green *= k;
  if (green > 1.0)
    green = 1.0;
  else if (green < 0.0)
    green = 0.0;
  
  blue *= k;
  if (blue > 1.0)
    blue = 1.0;
  else if (blue < 0.0)
    blue = 0.0;
  
  hls_to_rgb (&red, &green, &blue);
  
  b->red = red * 65535.0;
  b->green = green * 65535.0;
  b->blue = blue * 65535.0;
}

/**
 * Converts a red/green/blue triplet to a hue/lightness/saturation triplet.
 *
 * \param r  on input, red; on output, hue
 * \param g  on input, green; on output, lightness
 * \param b  on input, blue; on output, saturation
 */
static void
rgb_to_hls (gdouble *r,
            gdouble *g,
            gdouble *b)
{
  gdouble min;
  gdouble max;
  gdouble red;
  gdouble green;
  gdouble blue;
  gdouble h, l, s;
  gdouble delta;
  
  red = *r;
  green = *g;
  blue = *b;
  
  if (red > green)
    {
      if (red > blue)
        max = red;
      else
        max = blue;
      
      if (green < blue)
        min = green;
      else
        min = blue;
    }
  else
    {
      if (green > blue)
        max = green;
      else
        max = blue;
      
      if (red < blue)
        min = red;
      else
        min = blue;
    }
  
  l = (max + min) / 2;
  s = 0;
  h = 0;
  
  if (max != min)
    {
      if (l <= 0.5)
        s = (max - min) / (max + min);
      else
        s = (max - min) / (2 - max - min);
      
      delta = max -min;
      if (red == max)
        h = (green - blue) / delta;
      else if (green == max)
        h = 2 + (blue - red) / delta;
      else if (blue == max)
        h = 4 + (red - green) / delta;
      
      h *= 60;
      if (h < 0.0)
        h += 360;
    }
  
  *r = h;
  *g = l;
  *b = s;
}

/**
 * Converts a hue/lightness/saturation triplet to a red/green/blue triplet.
 *
 * \param h  on input, hue; on output, red
 * \param l  on input, lightness; on output, green
 * \param s  on input, saturation; on output, blue
 */
static void
hls_to_rgb (gdouble *h,
            gdouble *l,
            gdouble *s)
{
  gdouble hue;
  gdouble lightness;
  gdouble saturation;
  gdouble m1, m2;
  gdouble r, g, b;
  
  lightness = *l;
  saturation = *s;
  
  if (lightness <= 0.5)
    m2 = lightness * (1 + saturation);
  else
    m2 = lightness + saturation - lightness * saturation;
  m1 = 2 * lightness - m2;
  
  if (saturation == 0)
    {
      *h = lightness;
      *l = lightness;
      *s = lightness;
    }
  else
    {
      hue = *h + 120;
      while (hue > 360)
        hue -= 360;
      while (hue < 0)
        hue += 360;
      
      if (hue < 60)
        r = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
        r = m2;
      else if (hue < 240)
        r = m1 + (m2 - m1) * (240 - hue) / 60;
      else
        r = m1;
      
      hue = *h;
      while (hue > 360)
        hue -= 360;
      while (hue < 0)
        hue += 360;
      
      if (hue < 60)
        g = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
        g = m2;
      else if (hue < 240)
        g = m1 + (m2 - m1) * (240 - hue) / 60;
      else
        g = m1;
      
      hue = *h - 120;
      while (hue > 360)
        hue -= 360;
      while (hue < 0)
        hue += 360;
      
      if (hue < 60)
        b = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
        b = m2;
      else if (hue < 240)
        b = m1 + (m2 - m1) * (240 - hue) / 60;
      else
        b = m1;
      
      *h = r;
      *l = g;
      *s = b;
    }
}

#if 0
/* These are some functions I'm saving to use in optimizing
 * MetaDrawOpList, namely to pre-composite pixbufs on client side
 * prior to rendering to the server
 */
static void
draw_bg_solid_composite (const MetaTextureSpec *bg,
                         const MetaTextureSpec *fg,
                         double                 alpha,
                         GtkWidget             *widget,
                         GdkDrawable           *drawable,
                         const GdkRectangle    *clip,
                         MetaTextureDrawMode    mode,
                         double                 xalign,
                         double                 yalign,
                         int                    x,
                         int                    y,
                         int                    width,
                         int                    height)
{
  GdkColor bg_color;

  g_assert (bg->type == META_TEXTURE_SOLID);
  g_assert (fg->type != META_TEXTURE_COMPOSITE);
  g_assert (fg->type != META_TEXTURE_SHAPE_LIST);

  meta_color_spec_render (bg->data.solid.color_spec,
                          widget,
                          &bg_color);

  switch (fg->type)
    {
    case META_TEXTURE_SOLID:
      {
        GdkColor fg_color;

        meta_color_spec_render (fg->data.solid.color_spec,
                                widget,
                                &fg_color);

        color_composite (&bg_color, &fg_color,
                         alpha, &fg_color);

        draw_color_rectangle (widget, drawable, &fg_color, clip,
                              x, y, width, height);
      }
      break;

    case META_TEXTURE_GRADIENT:
      /* FIXME I think we could just composite all the colors in
       * the gradient prior to generating the gradient?
       */
      /* FALL THRU */
    case META_TEXTURE_IMAGE:
      {
        GdkPixbuf *pixbuf;
        GdkPixbuf *composited;

        pixbuf = meta_texture_spec_render (fg, widget, mode, 255,
                                           width, height);

        if (pixbuf == NULL)
          return;

        composited = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                                     gdk_pixbuf_get_has_alpha (pixbuf), 8,
                                     gdk_pixbuf_get_width (pixbuf),
                                     gdk_pixbuf_get_height (pixbuf));

        if (composited == NULL)
          {
            g_object_unref (G_OBJECT (pixbuf));
            return;
          }

        gdk_pixbuf_composite_color (pixbuf,
                                    composited,
                                    0, 0,
                                    gdk_pixbuf_get_width (pixbuf),
                                    gdk_pixbuf_get_height (pixbuf),
                                    0.0, 0.0, /* offsets */
                                    1.0, 1.0, /* scale */
                                    GDK_INTERP_BILINEAR,
                                    255 * alpha,
                                    0, 0,     /* check offsets */
                                    0,        /* check size */
                                    GDK_COLOR_RGB (bg_color),
                                    GDK_COLOR_RGB (bg_color));

        /* Need to draw background since pixbuf is not
         * necessarily covering the whole thing
         */
        draw_color_rectangle (widget, drawable, &bg_color, clip,
                              x, y, width, height);

        render_pixbuf_aligned (drawable, clip, composited,
                               xalign, yalign,
                               x, y, width, height);

        g_object_unref (G_OBJECT (pixbuf));
        g_object_unref (G_OBJECT (composited));
      }
      break;

    case META_TEXTURE_BLANK:
    case META_TEXTURE_COMPOSITE:
    case META_TEXTURE_SHAPE_LIST:
      g_assert_not_reached ();
      break;
    }
}

static void
draw_bg_gradient_composite (const MetaTextureSpec *bg,
                            const MetaTextureSpec *fg,
                            double                 alpha,
                            GtkWidget             *widget,
                            GdkDrawable           *drawable,
                            const GdkRectangle    *clip,
                            MetaTextureDrawMode    mode,
                            double                 xalign,
                            double                 yalign,
                            int                    x,
                            int                    y,
                            int                    width,
                            int                    height)
{
  g_assert (bg->type == META_TEXTURE_GRADIENT);
  g_assert (fg->type != META_TEXTURE_COMPOSITE);
  g_assert (fg->type != META_TEXTURE_SHAPE_LIST);

  switch (fg->type)
    {
    case META_TEXTURE_SOLID:
    case META_TEXTURE_GRADIENT:
    case META_TEXTURE_IMAGE:
      {
        GdkPixbuf *bg_pixbuf;
        GdkPixbuf *fg_pixbuf;
        GdkPixbuf *composited;
        int fg_width, fg_height;

        bg_pixbuf = meta_texture_spec_render (bg, widget, mode, 255,
                                              width, height);

        if (bg_pixbuf == NULL)
          return;

        fg_pixbuf = meta_texture_spec_render (fg, widget, mode, 255,
                                              width, height);

        if (fg_pixbuf == NULL)
          {
            g_object_unref (G_OBJECT (bg_pixbuf));
            return;
          }

        /* gradients always fill the entire target area */
        g_assert (gdk_pixbuf_get_width (bg_pixbuf) == width);
        g_assert (gdk_pixbuf_get_height (bg_pixbuf) == height);

        composited = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                                     gdk_pixbuf_get_has_alpha (bg_pixbuf), 8,
                                     gdk_pixbuf_get_width (bg_pixbuf),
                                     gdk_pixbuf_get_height (bg_pixbuf));

        if (composited == NULL)
          {
            g_object_unref (G_OBJECT (bg_pixbuf));
            g_object_unref (G_OBJECT (fg_pixbuf));
            return;
          }

        fg_width = gdk_pixbuf_get_width (fg_pixbuf);
        fg_height = gdk_pixbuf_get_height (fg_pixbuf);

        /* If we wanted to be all cool we could deal with the
         * offsets and try to composite only in the clip rectangle,
         * but I just don't care enough to figure it out.
         */

        gdk_pixbuf_composite (fg_pixbuf,
                              composited,
                              x + (width - fg_width) * xalign,
                              y + (height - fg_height) * yalign,
                              gdk_pixbuf_get_width (fg_pixbuf),
                              gdk_pixbuf_get_height (fg_pixbuf),
                              0.0, 0.0, /* offsets */
                              1.0, 1.0, /* scale */
                              GDK_INTERP_BILINEAR,
                              255 * alpha);

        render_pixbuf (drawable, clip, composited, x, y);

        g_object_unref (G_OBJECT (bg_pixbuf));
        g_object_unref (G_OBJECT (fg_pixbuf));
        g_object_unref (G_OBJECT (composited));
      }
      break;

    case META_TEXTURE_BLANK:
    case META_TEXTURE_SHAPE_LIST:
    case META_TEXTURE_COMPOSITE:
      g_assert_not_reached ();
      break;
    }
}
#endif

/**
 * Returns the earliest version of the theme format which required support
 * for a particular button.  (For example, "shade" first appeared in v2, and
 * "close" in v1.)
 *
 * \param type  the button type
 * \return  the number of the theme format
 */
guint
meta_theme_earliest_version_with_button (MetaButtonType type)
{
  switch (type)
    {
    case META_BUTTON_TYPE_CLOSE:
    case META_BUTTON_TYPE_MAXIMIZE:
    case META_BUTTON_TYPE_MINIMIZE:
    case META_BUTTON_TYPE_MENU:
    case META_BUTTON_TYPE_LEFT_LEFT_BACKGROUND:
    case META_BUTTON_TYPE_LEFT_MIDDLE_BACKGROUND:
    case META_BUTTON_TYPE_LEFT_RIGHT_BACKGROUND:
    case META_BUTTON_TYPE_RIGHT_LEFT_BACKGROUND:
    case META_BUTTON_TYPE_RIGHT_MIDDLE_BACKGROUND:
    case META_BUTTON_TYPE_RIGHT_RIGHT_BACKGROUND:
      return 1000;
      
    case META_BUTTON_TYPE_SHADE:
    case META_BUTTON_TYPE_ABOVE:
    case META_BUTTON_TYPE_STICK:
    case META_BUTTON_TYPE_UNSHADE:
    case META_BUTTON_TYPE_UNABOVE:
    case META_BUTTON_TYPE_UNSTICK:
      return 2000;

    default:
      meta_warning("Unknown button %d\n", type);
      return 1000;
    }
}
