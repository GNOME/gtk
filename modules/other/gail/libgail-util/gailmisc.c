/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001 Sun Microsystems Inc.
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

#include "config.h"

#include <stdlib.h>
#include <gtk/gtk.h>
#include "gailmisc.h"

/* IMPORTANT!!! This source file does NOT contain the implementation
 * code for AtkUtil - for that code, please see gail/gail.c.
 */

/**
 * SECTION:gailmisc
 * @Short_description: GailMisc is a set of utility functions which may be
 *   useful to implementors of Atk interfaces for custom widgets.
 * @Title: GailMisc
 *
 * GailMisc is a set of utility function which are used in the implemementation
 * of Atk interfaces for GTK+ widgets. They may be useful to implementors of
 * Atk interfaces for custom widgets.
 */


/**
 * gail_misc_get_extents_from_pango_rectangle:
 * @widget: The widget that contains the PangoLayout, that contains
 *   the PangoRectangle
 * @char_rect: The #PangoRectangle from which to calculate extents
 * @x_layout: The x-offset at which the widget displays the
 *   PangoLayout that contains the PangoRectangle, relative to @widget
 * @y_layout: The y-offset at which the widget displays the
 *   PangoLayout that contains the PangoRectangle, relative to @widget
 * @x: The x-position of the #PangoRectangle relative to @coords
 * @y: The y-position of the #PangoRectangle relative to @coords
 * @width: The width of the #PangoRectangle
 * @height: The  height of the #PangoRectangle
 * @coords: An #AtkCoordType enumeration
 *
 * Gets the extents of @char_rect in device coordinates,
 * relative to either top-level window or screen coordinates as
 * specified by @coords.
 **/
void
gail_misc_get_extents_from_pango_rectangle (GtkWidget      *widget,
                                            PangoRectangle *char_rect,
                                            gint           x_layout,
                                            gint           y_layout,
                                            gint           *x,
                                            gint           *y,
                                            gint           *width,
                                            gint           *height,
                                            AtkCoordType   coords)
{
  gint x_window, y_window, x_toplevel, y_toplevel;

  gail_misc_get_origins (widget, &x_window, &y_window, 
                         &x_toplevel, &y_toplevel);

  *x = (char_rect->x / PANGO_SCALE) + x_layout + x_window;
  *y = (char_rect->y / PANGO_SCALE) + y_layout + y_window;
  if (coords == ATK_XY_WINDOW)
    {
      *x -= x_toplevel;
      *y -= y_toplevel;
    }
  else if (coords != ATK_XY_SCREEN)
    {
      *x = 0;
      *y = 0;
      *height = 0;
      *width = 0;
      return;
    }
  *height = char_rect->height / PANGO_SCALE;
  *width = char_rect->width / PANGO_SCALE;

  return;
}

/**
 * gail_misc_get_index_at_point_in_layout:
 * @widget: A #GtkWidget
 * @layout: The #PangoLayout from which to get the index at the
 *   specified point.
 * @x_layout: The x-offset at which the widget displays the
 *   #PangoLayout, relative to @widget
 * @y_layout: The y-offset at which the widget displays the
 *   #PangoLayout, relative to @widget
 * @x: The x-coordinate relative to @coords at which to
 *   calculate the index
 * @y: The y-coordinate relative to @coords at which to
 *   calculate the index
 * @coords: An #AtkCoordType enumeration
 *
 * Gets the byte offset at the specified @x and @y in a #PangoLayout.
 *
 * Returns: the byte offset at the specified @x and @y in a
 *   #PangoLayout
 **/
gint
gail_misc_get_index_at_point_in_layout (GtkWidget   *widget,
                                        PangoLayout *layout,
                                        gint        x_layout,
                                        gint        y_layout,
                                        gint        x,
                                        gint        y,
                                        AtkCoordType coords)
{
  gint index, x_window, y_window, x_toplevel, y_toplevel;
  gint x_temp, y_temp;
  gboolean ret;

  gail_misc_get_origins (widget, &x_window, &y_window, 
                         &x_toplevel, &y_toplevel);
  x_temp =  x - x_layout - x_window;
  y_temp =  y - y_layout - y_window;
  if (coords == ATK_XY_WINDOW)
    {
      x_temp += x_toplevel;  
      y_temp += y_toplevel;
    }
  else if (coords != ATK_XY_SCREEN)
    return -1;

  ret = pango_layout_xy_to_index (layout, 
                                  x_temp * PANGO_SCALE,
                                  y_temp * PANGO_SCALE,
                                  &index, NULL);
  if (!ret)
    {
      if (x_temp < 0 || y_temp < 0)
        index = 0;
      else
        index = -1; 
    }
  return index;
}

/**
 * gail_misc_add_attribute:
 * @attrib_set: The #AtkAttributeSet to add the attribute to
 * @attr: The AtkTextAttrribute which identifies the attribute to be added
 * @value: The attribute value
 *
 * Creates an #AtkAttribute from @attr and @value, and adds it
 * to @attrib_set. 
 *
 * Returns: A pointer to the new #AtkAttributeSet.
 **/
AtkAttributeSet*
gail_misc_add_attribute (AtkAttributeSet *attrib_set,
                         AtkTextAttribute attr,
                         gchar           *value)
{
  AtkAttributeSet *return_set;
  AtkAttribute *at = g_malloc (sizeof (AtkAttribute));
  at->name = g_strdup (atk_text_attribute_get_name (attr));
  at->value = value;
  return_set = g_slist_prepend(attrib_set, at);
  return return_set;
}

/**
 * gail_misc_layout_get_run_attributes:
 * @attrib_set: The #AtkAttributeSet to add the attribute to
 * @layout: The PangoLayout from which the attributes will be obtained
 * @text: The text 
 * @offset: The offset at which the attributes are required
 * @start_offset: The start offset of the current run
 * @end_offset: The end offset of the current run
 *
 * Adds the attributes for the run starting at offset to the specified
 * attribute set.
 *
 * Returns: A pointer to the #AtkAttributeSet.
 **/
AtkAttributeSet* 
gail_misc_layout_get_run_attributes (AtkAttributeSet *attrib_set,
                                     PangoLayout     *layout,
                                     gchar           *text,
                                     gint            offset,
                                     gint            *start_offset,
                                     gint            *end_offset)
{
  PangoAttrIterator *iter;
  PangoAttrList *attr;  
  PangoAttrString *pango_string;
  PangoAttrInt *pango_int;
  PangoAttrColor *pango_color;
  PangoAttrLanguage *pango_lang;
  PangoAttrFloat *pango_float;
  gint index, start_index, end_index;
  gboolean is_next = TRUE;
  gchar *value = NULL;
  glong len;

  len = g_utf8_strlen (text, -1);
  /* Grab the attributes of the PangoLayout, if any */
  if ((attr = pango_layout_get_attributes (layout)) == NULL)
    {
      *start_offset = 0;
      *end_offset = len;
      return attrib_set;
    }
  iter = pango_attr_list_get_iterator (attr);
  /* Get invariant range offsets */
  /* If offset out of range, set offset in range */
  if (offset > len)
    offset = len;
  else if (offset < 0)
    offset = 0;

  index = g_utf8_offset_to_pointer (text, offset) - text;
  pango_attr_iterator_range (iter, &start_index, &end_index);
  while (is_next)
    {
      if (index >= start_index && index < end_index)
        {
          *start_offset = g_utf8_pointer_to_offset (text, 
                                                    text + start_index);  
          if (end_index == G_MAXINT)
          /* Last iterator */
            end_index = len;
      
          *end_offset = g_utf8_pointer_to_offset (text, 
                                                  text + end_index);  
          break;
        }  
      is_next = pango_attr_iterator_next (iter);
      pango_attr_iterator_range (iter, &start_index, &end_index);
    }
  /* Get attributes */
  if ((pango_string = (PangoAttrString*) pango_attr_iterator_get (iter, 
                                   PANGO_ATTR_FAMILY)) != NULL)
    {
      value = g_strdup_printf("%s", pango_string->value);
      attrib_set = gail_misc_add_attribute (attrib_set, 
                                            ATK_TEXT_ATTR_FAMILY_NAME, 
                                            value);
    } 
  if ((pango_int = (PangoAttrInt*) pango_attr_iterator_get (iter, 
                                   PANGO_ATTR_STYLE)) != NULL)
    {
      attrib_set = gail_misc_add_attribute (attrib_set, 
                                            ATK_TEXT_ATTR_STYLE, 
      g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_STYLE, pango_int->value)));
    } 
  if ((pango_int = (PangoAttrInt*) pango_attr_iterator_get (iter, 
                                   PANGO_ATTR_WEIGHT)) != NULL)
    {
      value = g_strdup_printf("%i", pango_int->value);
      attrib_set = gail_misc_add_attribute (attrib_set, 
                                            ATK_TEXT_ATTR_WEIGHT, 
                                            value);
    } 
  if ((pango_int = (PangoAttrInt*) pango_attr_iterator_get (iter, 
                                   PANGO_ATTR_VARIANT)) != NULL)
    {
      attrib_set = gail_misc_add_attribute (attrib_set, 
                                            ATK_TEXT_ATTR_VARIANT, 
       g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_VARIANT, pango_int->value)));
    } 
  if ((pango_int = (PangoAttrInt*) pango_attr_iterator_get (iter, 
                                   PANGO_ATTR_STRETCH)) != NULL)
    {
      attrib_set = gail_misc_add_attribute (attrib_set, 
                                            ATK_TEXT_ATTR_STRETCH, 
       g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_STRETCH, pango_int->value)));
    } 
  if ((pango_int = (PangoAttrInt*) pango_attr_iterator_get (iter, 
                                   PANGO_ATTR_SIZE)) != NULL)
    {
      value = g_strdup_printf("%i", pango_int->value / PANGO_SCALE);
      attrib_set = gail_misc_add_attribute (attrib_set, 
                                            ATK_TEXT_ATTR_SIZE,
                                            value);
    } 
  if ((pango_int = (PangoAttrInt*) pango_attr_iterator_get (iter, 
                                   PANGO_ATTR_UNDERLINE)) != NULL)
    {
      attrib_set = gail_misc_add_attribute (attrib_set, 
                                            ATK_TEXT_ATTR_UNDERLINE, 
       g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_UNDERLINE, pango_int->value)));
    } 
  if ((pango_int = (PangoAttrInt*) pango_attr_iterator_get (iter, 
                                   PANGO_ATTR_STRIKETHROUGH)) != NULL)
    {
      attrib_set = gail_misc_add_attribute (attrib_set, 
                                            ATK_TEXT_ATTR_STRIKETHROUGH, 
       g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_STRIKETHROUGH, pango_int->value)));
    } 
  if ((pango_int = (PangoAttrInt*) pango_attr_iterator_get (iter, 
                                   PANGO_ATTR_RISE)) != NULL)
    {
      value = g_strdup_printf("%i", pango_int->value);
      attrib_set = gail_misc_add_attribute (attrib_set, 
                                            ATK_TEXT_ATTR_RISE,
                                            value);
    } 
  if ((pango_lang = (PangoAttrLanguage*) pango_attr_iterator_get (iter, 
                                   PANGO_ATTR_LANGUAGE)) != NULL)
    {
      value = g_strdup( pango_language_to_string( pango_lang->value));
      attrib_set = gail_misc_add_attribute (attrib_set, 
                                            ATK_TEXT_ATTR_LANGUAGE, 
                                            value);
    } 
  if ((pango_float = (PangoAttrFloat*) pango_attr_iterator_get (iter, 
                                   PANGO_ATTR_SCALE)) != NULL)
    {
      value = g_strdup_printf("%g", pango_float->value);
      attrib_set = gail_misc_add_attribute (attrib_set, 
                                            ATK_TEXT_ATTR_SCALE, 
                                            value);
    } 
  if ((pango_color = (PangoAttrColor*) pango_attr_iterator_get (iter, 
                                    PANGO_ATTR_FOREGROUND)) != NULL)
    {
      value = g_strdup_printf ("%u,%u,%u", 
                               pango_color->color.red, 
                               pango_color->color.green, 
                               pango_color->color.blue);
      attrib_set = gail_misc_add_attribute (attrib_set, 
                                            ATK_TEXT_ATTR_FG_COLOR, 
                                            value);
    } 
  if ((pango_color = (PangoAttrColor*) pango_attr_iterator_get (iter, 
                                     PANGO_ATTR_BACKGROUND)) != NULL)
    {
      value = g_strdup_printf ("%u,%u,%u", 
                               pango_color->color.red, 
                               pango_color->color.green, 
                               pango_color->color.blue);
      attrib_set = gail_misc_add_attribute (attrib_set, 
                                            ATK_TEXT_ATTR_BG_COLOR, 
                                            value);
    } 
  pango_attr_iterator_destroy (iter);
  return attrib_set;
}

/**
 * gail_misc_get_default_attributes:
 * @attrib_set: The #AtkAttributeSet to add the attribute to
 * @layout: The PangoLayout from which the attributes will be obtained
 * @widget: The GtkWidget for which the default attributes are required.
 *
 * Adds the default attributes to the specified attribute set.
 *
 * Returns: A pointer to the #AtkAttributeSet.
 **/
AtkAttributeSet* 
gail_misc_get_default_attributes (AtkAttributeSet *attrib_set,
                                  PangoLayout     *layout,
                                  GtkWidget       *widget)
{
  PangoContext *context;
  GtkStyle *style_value;
  gint int_value;
  PangoWrapMode mode;

  attrib_set = gail_misc_add_attribute (attrib_set, 
                                        ATK_TEXT_ATTR_DIRECTION,
     g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_DIRECTION, 
                                        gtk_widget_get_direction (widget))));

  context = pango_layout_get_context (layout);
  if (context)
    {
      PangoLanguage* language;
      PangoFontDescription* font;

      language = pango_context_get_language (context);
      if (language)
        {
          attrib_set = gail_misc_add_attribute (attrib_set,
                                                ATK_TEXT_ATTR_LANGUAGE,
                      g_strdup (pango_language_to_string (language)));
        }
      font = pango_context_get_font_description (context);
      if (font)
        {
          attrib_set = gail_misc_add_attribute (attrib_set,
                                                ATK_TEXT_ATTR_STYLE,
              g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_STYLE,
                                   pango_font_description_get_style (font))));
          attrib_set = gail_misc_add_attribute (attrib_set,
                                                ATK_TEXT_ATTR_VARIANT,
              g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_VARIANT,
                                   pango_font_description_get_variant (font))));
          attrib_set = gail_misc_add_attribute (attrib_set,
                                                ATK_TEXT_ATTR_STRETCH,
              g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_STRETCH,
                                   pango_font_description_get_stretch (font))));
          attrib_set = gail_misc_add_attribute (attrib_set,
                                                ATK_TEXT_ATTR_FAMILY_NAME,
              g_strdup (pango_font_description_get_family (font)));
          attrib_set = gail_misc_add_attribute (attrib_set,
                                                ATK_TEXT_ATTR_WEIGHT,
                    g_strdup_printf ("%d",
                                   pango_font_description_get_weight (font)));
          attrib_set = gail_misc_add_attribute (attrib_set,
                                                ATK_TEXT_ATTR_SIZE,
                    g_strdup_printf ("%i",
                                   pango_font_description_get_size (font) / PANGO_SCALE));
        }
    }
  if (pango_layout_get_justify (layout))
    {
      int_value = 3;
    }
  else
    {
      PangoAlignment align;

      align = pango_layout_get_alignment (layout);
      if (align == PANGO_ALIGN_LEFT)
        int_value = 0;
      else if (align == PANGO_ALIGN_CENTER)
        int_value = 2;
      else /* if (align == PANGO_ALIGN_RIGHT) */
        int_value = 1;
    }
  attrib_set = gail_misc_add_attribute (attrib_set,
                                        ATK_TEXT_ATTR_JUSTIFICATION,
              g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_JUSTIFICATION, 
                                                      int_value))); 
  mode = pango_layout_get_wrap (layout);
  if (mode == PANGO_WRAP_WORD)
    int_value = 2;
  else /* if (mode == PANGO_WRAP_CHAR) */
    int_value = 1;
  attrib_set = gail_misc_add_attribute (attrib_set,
                                        ATK_TEXT_ATTR_WRAP_MODE,
              g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_WRAP_MODE, 
                                                      int_value))); 

  style_value = gtk_widget_get_style (widget);
  if (style_value)
    {
      GdkColor color;
      gchar *value;

      color = style_value->base[GTK_STATE_NORMAL];
      value = g_strdup_printf ("%u,%u,%u",
                               color.red, color.green, color.blue);
      attrib_set = gail_misc_add_attribute (attrib_set,
                                            ATK_TEXT_ATTR_BG_COLOR,
                                            value); 
      color = style_value->text[GTK_STATE_NORMAL];
      value = g_strdup_printf ("%u,%u,%u",
                               color.red, color.green, color.blue);
      attrib_set = gail_misc_add_attribute (attrib_set,
                                            ATK_TEXT_ATTR_FG_COLOR,
                                            value); 
    }
  attrib_set = gail_misc_add_attribute (attrib_set,
                                        ATK_TEXT_ATTR_FG_STIPPLE,
              g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_FG_STIPPLE, 
                                                      0))); 
  attrib_set = gail_misc_add_attribute (attrib_set,
                                        ATK_TEXT_ATTR_BG_STIPPLE,
              g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_BG_STIPPLE, 
                                                      0))); 
  attrib_set = gail_misc_add_attribute (attrib_set,
                                        ATK_TEXT_ATTR_STRIKETHROUGH,
              g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_STRIKETHROUGH, 
                                                      0))); 
  attrib_set = gail_misc_add_attribute (attrib_set,
                                        ATK_TEXT_ATTR_UNDERLINE,
              g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_UNDERLINE, 
                                                      0))); 
  attrib_set = gail_misc_add_attribute (attrib_set,
                                        ATK_TEXT_ATTR_RISE,
                                               g_strdup_printf ("%i", 0));
  attrib_set = gail_misc_add_attribute (attrib_set,
                                        ATK_TEXT_ATTR_SCALE,
                                               g_strdup_printf ("%g", 1.0));
  attrib_set = gail_misc_add_attribute (attrib_set,
                                        ATK_TEXT_ATTR_BG_FULL_HEIGHT,
                                               g_strdup_printf ("%i", 0));
  attrib_set = gail_misc_add_attribute (attrib_set,
                                        ATK_TEXT_ATTR_PIXELS_INSIDE_WRAP,
                                               g_strdup_printf ("%i", 0));
  attrib_set = gail_misc_add_attribute (attrib_set,
                                        ATK_TEXT_ATTR_PIXELS_BELOW_LINES,
                                        g_strdup_printf ("%i", 0));
  attrib_set = gail_misc_add_attribute (attrib_set,
                                        ATK_TEXT_ATTR_PIXELS_ABOVE_LINES,
                                        g_strdup_printf ("%i", 0));
  attrib_set = gail_misc_add_attribute (attrib_set,
                                        ATK_TEXT_ATTR_EDITABLE,
              g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_EDITABLE, 
                                                      0))); 
  attrib_set = gail_misc_add_attribute (attrib_set,
                                        ATK_TEXT_ATTR_INVISIBLE,
              g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_INVISIBLE, 
                                                      0))); 
  attrib_set = gail_misc_add_attribute (attrib_set,
                                        ATK_TEXT_ATTR_INDENT,
                                        g_strdup_printf ("%i", 0));
  attrib_set = gail_misc_add_attribute (attrib_set,
                                        ATK_TEXT_ATTR_RIGHT_MARGIN,
                                        g_strdup_printf ("%i", 0));
  attrib_set = gail_misc_add_attribute (attrib_set,
                                        ATK_TEXT_ATTR_LEFT_MARGIN,
                                        g_strdup_printf ("%i", 0));
  return attrib_set;
}

/**
 * gail_misc_get_origins:
 * @widget: a #GtkWidget
 * @x_window: the x-origin of the widget->window
 * @y_window: the y-origin of the widget->window
 * @x_toplevel: the x-origin of the toplevel window for widget->window
 * @y_toplevel: the y-origin of the toplevel window for widget->window
 *
 * Gets the origin of the widget window, and the origin of the
 * widgets top-level window.
 **/
void
gail_misc_get_origins (GtkWidget *widget,
                       gint      *x_window,
                       gint      *y_window,
                       gint      *x_toplevel,
                       gint      *y_toplevel)
{
  GdkWindow *window;

  if (GTK_IS_TREE_VIEW (widget))
    window = gtk_tree_view_get_bin_window (GTK_TREE_VIEW (widget));
  else
    window = widget->window;
  gdk_window_get_origin (window, x_window, y_window);
  window = gdk_window_get_toplevel (widget->window);
  gdk_window_get_origin (window, x_toplevel, y_toplevel);
}

/**
 * gail_misc_add_to_attr_set:
 * @attrib_set: An #AtkAttributeSet
 * @attrs: The #GtkTextAttributes containing the attribute value
 * @attr: The #AtkTextAttribute to be added
 *
 * Gets the value for the AtkTextAttribute from the GtkTextAttributes
 * and adds it to the AttributeSet.
 *
 * Returns: A pointer to the updated #AtkAttributeSet.
 **/
AtkAttributeSet*
gail_misc_add_to_attr_set (AtkAttributeSet   *attrib_set,
                           GtkTextAttributes *attrs,
                           AtkTextAttribute  attr)
{
  gchar *value;

  switch (attr)
    {
    case ATK_TEXT_ATTR_LEFT_MARGIN:
      value = g_strdup_printf ("%i", attrs->left_margin);
      break;
    case ATK_TEXT_ATTR_RIGHT_MARGIN:
      value = g_strdup_printf ("%i", attrs->right_margin);
      break;
    case ATK_TEXT_ATTR_INDENT:
      value = g_strdup_printf ("%i", attrs->indent);
      break;
    case ATK_TEXT_ATTR_INVISIBLE:
      value = g_strdup (atk_text_attribute_get_value (attr, attrs->invisible));
      break;
    case ATK_TEXT_ATTR_EDITABLE:
      value = g_strdup (atk_text_attribute_get_value (attr, attrs->editable));
      break;
    case ATK_TEXT_ATTR_PIXELS_ABOVE_LINES:
      value = g_strdup_printf ("%i", attrs->pixels_above_lines);
      break;
    case ATK_TEXT_ATTR_PIXELS_BELOW_LINES:
      value = g_strdup_printf ("%i", attrs->pixels_below_lines);
      break;
    case ATK_TEXT_ATTR_PIXELS_INSIDE_WRAP:
      value = g_strdup_printf ("%i", attrs->pixels_inside_wrap);
      break;
    case ATK_TEXT_ATTR_BG_FULL_HEIGHT:
      value = g_strdup (atk_text_attribute_get_value (attr, attrs->bg_full_height));
      break;
    case ATK_TEXT_ATTR_RISE:
      value = g_strdup_printf ("%i", attrs->appearance.rise);
      break;
    case ATK_TEXT_ATTR_UNDERLINE:
      value = g_strdup (atk_text_attribute_get_value (attr, attrs->appearance.underline));
      break;
    case ATK_TEXT_ATTR_STRIKETHROUGH:
      value = g_strdup (atk_text_attribute_get_value (attr, attrs->appearance.strikethrough));
      break;
    case ATK_TEXT_ATTR_SIZE:
      value = g_strdup_printf ("%i", 
                              pango_font_description_get_size (attrs->font) / PANGO_SCALE);
      break;
    case ATK_TEXT_ATTR_SCALE:
      value = g_strdup_printf ("%g", attrs->font_scale);
      break;
    case ATK_TEXT_ATTR_WEIGHT:
      value = g_strdup_printf ("%d", 
                              pango_font_description_get_weight (attrs->font));
      break;
    case ATK_TEXT_ATTR_LANGUAGE:
      value = g_strdup ((gchar *)(attrs->language));
      break;
    case ATK_TEXT_ATTR_FAMILY_NAME:
      value = g_strdup (pango_font_description_get_family (attrs->font));
      break;
    case ATK_TEXT_ATTR_BG_COLOR:
      value = g_strdup_printf ("%u,%u,%u",
                               attrs->appearance.bg_color.red,
                               attrs->appearance.bg_color.green,
                               attrs->appearance.bg_color.blue);
      break;
    case ATK_TEXT_ATTR_FG_COLOR:
      value = g_strdup_printf ("%u,%u,%u",
                               attrs->appearance.fg_color.red,
                               attrs->appearance.fg_color.green,
                               attrs->appearance.fg_color.blue);
      break;
    case ATK_TEXT_ATTR_BG_STIPPLE:
      value = g_strdup (atk_text_attribute_get_value (attr, attrs->appearance.bg_stipple ? 1 : 0));
      break;
    case ATK_TEXT_ATTR_FG_STIPPLE:
      value = g_strdup (atk_text_attribute_get_value (attr, attrs->appearance.fg_stipple ? 1 : 0));
      break;
    case ATK_TEXT_ATTR_WRAP_MODE:
      value = g_strdup (atk_text_attribute_get_value (attr, attrs->wrap_mode));
      break;
    case ATK_TEXT_ATTR_DIRECTION:
      value = g_strdup (atk_text_attribute_get_value (attr, attrs->direction));
      break;
    case ATK_TEXT_ATTR_JUSTIFICATION:
      value = g_strdup (atk_text_attribute_get_value (attr, attrs->justification));
      break;
    case ATK_TEXT_ATTR_STRETCH:
      value = g_strdup (atk_text_attribute_get_value (attr, 
                        pango_font_description_get_stretch (attrs->font)));
      break;
    case ATK_TEXT_ATTR_VARIANT:
      value = g_strdup (atk_text_attribute_get_value (attr, 
                        pango_font_description_get_variant (attrs->font)));
      break;
    case ATK_TEXT_ATTR_STYLE:
      value = g_strdup (atk_text_attribute_get_value (attr, 
                        pango_font_description_get_style (attrs->font)));
      break;
    default:
      value = NULL;
      break;
    }
  return gail_misc_add_attribute (attrib_set, attr, value);
}

/**
 * gail_misc_buffer_get_run_attributes:
 * @buffer: The #GtkTextBuffer for which the attributes will be obtained
 * @offset: The offset at which the attributes are required
 * @start_offset: The start offset of the current run
 * @end_offset: The end offset of the current run
 *
 * Creates an AtkAttributeSet which contains the attributes for the 
 * run starting at offset.
 *
 * Returns: A pointer to the #AtkAttributeSet.
 **/
AtkAttributeSet*
gail_misc_buffer_get_run_attributes (GtkTextBuffer *buffer,
                                     gint          offset,
                                     gint	    *start_offset,
                                     gint          *end_offset)
{
  GtkTextIter iter;
  AtkAttributeSet *attrib_set = NULL;
  AtkAttribute *at;
  GSList *tags, *temp_tags;
  gdouble scale = 1;
  gboolean val_set = FALSE;
  PangoFontMask mask;

  gtk_text_buffer_get_iter_at_offset (buffer, &iter, offset);

  gtk_text_iter_forward_to_tag_toggle (&iter, NULL);
  *end_offset = gtk_text_iter_get_offset (&iter);

  gtk_text_iter_backward_to_tag_toggle (&iter, NULL);
  *start_offset = gtk_text_iter_get_offset (&iter);

  gtk_text_buffer_get_iter_at_offset (buffer, &iter, offset);

  tags = gtk_text_iter_get_tags (&iter);
  tags = g_slist_reverse (tags);

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);
      PangoFontDescription *font;

      font = tag->values->font;

      if (font)
        {
          mask = pango_font_description_get_set_fields (font);
          val_set = mask & PANGO_FONT_MASK_STYLE;
          if (val_set)
            attrib_set = gail_misc_add_to_attr_set (attrib_set, tag->values, 
                                                    ATK_TEXT_ATTR_STYLE);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);
      PangoFontDescription *font;

      font = tag->values->font;

      if (font)
        {
          mask = pango_font_description_get_set_fields (font);
          val_set = mask & PANGO_FONT_MASK_VARIANT;
          if (val_set)
            attrib_set = gail_misc_add_to_attr_set (attrib_set, tag->values, 
                                                    ATK_TEXT_ATTR_VARIANT);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);
      PangoFontDescription *font;

      font = tag->values->font;

      if (font)
        {
          mask = pango_font_description_get_set_fields (font);
          val_set = mask & PANGO_FONT_MASK_STRETCH;
          if (val_set)
            attrib_set = gail_misc_add_to_attr_set (attrib_set, tag->values, 
                                                    ATK_TEXT_ATTR_STRETCH);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      val_set = tag->justification_set;
      if (val_set)
        attrib_set = gail_misc_add_to_attr_set (attrib_set, tag->values, 
                                                ATK_TEXT_ATTR_JUSTIFICATION);
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      if (tag->values->direction != GTK_TEXT_DIR_NONE)
        {
          val_set = TRUE;
          attrib_set = gail_misc_add_to_attr_set (attrib_set, tag->values, 
                                                  ATK_TEXT_ATTR_DIRECTION);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      val_set = tag->wrap_mode_set;
      if (val_set)
        attrib_set = gail_misc_add_to_attr_set (attrib_set, tag->values, 
                                                ATK_TEXT_ATTR_WRAP_MODE);
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      val_set = tag->fg_stipple_set;
      if (val_set)
        attrib_set = gail_misc_add_to_attr_set (attrib_set, tag->values, 
                                                ATK_TEXT_ATTR_FG_STIPPLE);
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      val_set = tag->bg_stipple_set;
      if (val_set)
        attrib_set = gail_misc_add_to_attr_set (attrib_set, tag->values, 
                                                ATK_TEXT_ATTR_BG_STIPPLE);
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      val_set = tag->fg_color_set;
      if (val_set)
        attrib_set = gail_misc_add_to_attr_set (attrib_set, tag->values, 
                                                ATK_TEXT_ATTR_FG_COLOR);
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);
  
      val_set = tag->bg_color_set;
      if (val_set)
        attrib_set = gail_misc_add_to_attr_set (attrib_set, tag->values, 
                                                ATK_TEXT_ATTR_BG_COLOR);
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);
      PangoFontDescription *font;

      font = tag->values->font;

      if (font)
        {
          mask = pango_font_description_get_set_fields (font);
          val_set = mask & PANGO_FONT_MASK_FAMILY;
          if (val_set)
            attrib_set = gail_misc_add_to_attr_set (attrib_set, tag->values, 
                                                    ATK_TEXT_ATTR_FAMILY_NAME);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      val_set = tag->language_set;
      if (val_set)
        attrib_set = gail_misc_add_to_attr_set (attrib_set, tag->values, 
                                                ATK_TEXT_ATTR_LANGUAGE);
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);
      PangoFontDescription *font;

      font = tag->values->font;

      if (font)
        {
          mask = pango_font_description_get_set_fields (font);
          val_set = mask & PANGO_FONT_MASK_WEIGHT;
          if (val_set)
            attrib_set = gail_misc_add_to_attr_set (attrib_set, tag->values, 
                                                    ATK_TEXT_ATTR_WEIGHT);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;


  /*
   * scale is special as the scale is the product of all scale values
   * specified.
   */
  temp_tags = tags;
  while (temp_tags)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      if (tag->scale_set)
        {
          val_set = TRUE;
          scale *= tag->values->font_scale;
        }
      temp_tags = temp_tags->next;
    }
  if (val_set)
    {
      at = g_malloc(sizeof(AtkAttribute));
      at->name = g_strdup(atk_text_attribute_get_name (ATK_TEXT_ATTR_SCALE));
      at->value = g_strdup_printf("%g", scale);
      attrib_set = g_slist_prepend(attrib_set, at);
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);
      PangoFontDescription *font;

      font = tag->values->font;

      if (font)
        {
          mask = pango_font_description_get_set_fields (font);
          val_set = mask & PANGO_FONT_MASK_SIZE;
          if (val_set)
            attrib_set = gail_misc_add_to_attr_set (attrib_set, tag->values, 
                                                    ATK_TEXT_ATTR_SIZE);
        }
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      val_set = tag->strikethrough_set;
      if (val_set)
        attrib_set = gail_misc_add_to_attr_set (attrib_set, tag->values, 
                                                ATK_TEXT_ATTR_STRIKETHROUGH);
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      val_set = tag->underline_set;
      if (val_set)
        attrib_set = gail_misc_add_to_attr_set (attrib_set, tag->values, 
                                                ATK_TEXT_ATTR_UNDERLINE);
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      val_set = tag->rise_set;
      if (val_set)
        attrib_set = gail_misc_add_to_attr_set (attrib_set, tag->values, 
                                                ATK_TEXT_ATTR_RISE);
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      val_set = tag->bg_full_height_set;
      if (val_set)
        attrib_set = gail_misc_add_to_attr_set (attrib_set, tag->values, 
                                                ATK_TEXT_ATTR_BG_FULL_HEIGHT);
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      val_set = tag->pixels_inside_wrap_set;
      if (val_set)
        attrib_set = gail_misc_add_to_attr_set (attrib_set, tag->values, 
                                                ATK_TEXT_ATTR_PIXELS_INSIDE_WRAP);
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      val_set = tag->pixels_below_lines_set;
      if (val_set)
        attrib_set = gail_misc_add_to_attr_set (attrib_set, tag->values, 
                                                ATK_TEXT_ATTR_PIXELS_BELOW_LINES);
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      val_set = tag->pixels_above_lines_set;
      if (val_set)
        attrib_set = gail_misc_add_to_attr_set (attrib_set, tag->values, 
                                                ATK_TEXT_ATTR_PIXELS_ABOVE_LINES);
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      val_set = tag->editable_set;
      if (val_set)
        attrib_set = gail_misc_add_to_attr_set (attrib_set, tag->values, 
                                                ATK_TEXT_ATTR_EDITABLE);
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      val_set = tag->invisible_set;
      if (val_set)
        attrib_set = gail_misc_add_to_attr_set (attrib_set, tag->values, 
                                                ATK_TEXT_ATTR_INVISIBLE);
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      val_set = tag->indent_set;
      if (val_set)
        attrib_set = gail_misc_add_to_attr_set (attrib_set, tag->values, 
                                                ATK_TEXT_ATTR_INDENT);
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      val_set = tag->right_margin_set;
      if (val_set)
        attrib_set = gail_misc_add_to_attr_set (attrib_set, tag->values, 
                                                ATK_TEXT_ATTR_RIGHT_MARGIN);
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  temp_tags = tags;
  while (temp_tags && !val_set)
    {
      GtkTextTag *tag = GTK_TEXT_TAG (temp_tags->data);

      val_set = tag->left_margin_set;
      if (val_set)
        attrib_set = gail_misc_add_to_attr_set (attrib_set, tag->values, 
                                                ATK_TEXT_ATTR_LEFT_MARGIN);
      temp_tags = temp_tags->next;
    }
  val_set = FALSE;

  g_slist_free (tags);
  return attrib_set;
}
