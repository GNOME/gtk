/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001 Sun Microsystems Inc.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "gailhtmlboxtext.h"

static void           gail_html_box_text_class_init          (GailHtmlBoxTextClass *klass);
static void           gail_html_box_text_text_interface_init (AtkTextIface        *iface);
static gchar*         gail_html_box_text_get_text            (AtkText             *text,
                                                              gint                start_offset,
                                                              gint                end_offset);
static gchar*         gail_html_box_text_get_text_after_offset 
                                                             (AtkText             *text,
                                                              gint                offset,
                                                              AtkTextBoundary     boundary_type,
                                                              gint                *start_offset,
                                                              gint                *end_offset);
static gchar*         gail_html_box_text_get_text_at_offset  (AtkText             *text,
                                                              gint                offset,
                                                              AtkTextBoundary     boundary_type,
                                                              gint                *start_offset,
                                                              gint                *end_offset);
static gchar*         gail_html_box_text_get_text_before_offset 
                                                             (AtkText             *text,
                                                              gint                offset,
                                                              AtkTextBoundary     boundary_type,
                                                              gint                *start_offset,
                                                              gint                *end_offset);
static gunichar       gail_html_box_text_get_character_at_offset 
                                                              (AtkText            *text,
                                                               gint               offset);
static gint           gail_html_box_text_get_character_count  (AtkText            *text);
static gint           gail_html_box_text_get_caret_offset     (AtkText            *text);
static gboolean       gail_html_box_text_set_caret_offset     (AtkText            *text,
                                                               gint               offset);
static gint           gail_html_box_text_get_offset_at_point  (AtkText            *text,
                                                               gint               x,
                                                               gint               y,
                                                               AtkCoordType       coords);
static void           gail_html_box_text_get_character_extents (AtkText           *text,
                                                                gint              offset,
                                                                gint              *x,
                                                                gint              *y,
                                                                gint              *width,
                                                                gint              *height,
                                                                AtkCoordType      coords);
static AtkAttributeSet* 
                      gail_html_box_text_get_run_attributes    (AtkText           *text,
                                                                gint              offset,
                                                                gint              *start_offset,
                                                                gint              *end_offset);
static AtkAttributeSet* 
                      gail_html_box_text_get_default_attributes (AtkText          *text);
static gint           gail_html_box_text_get_n_selections      (AtkText           *text);
static gchar*         gail_html_box_text_get_selection         (AtkText           *text,
                                                                gint              selection_num,
                                                                gint              *start_pos,
                                                                gint              *end_pos);
static gboolean       gail_html_box_text_add_selection         (AtkText           *text,
                                                                gint              start_pos,
                                                                gint              end_pos);
static gboolean       gail_html_box_text_remove_selection      (AtkText           *text,
                                                                gint              selection_num);
static gboolean       gail_html_box_text_set_selection         (AtkText           *text,
                                                                gint              selection_num,
                                                                gint              start_pos,
                                                                gint              end_pos);
static AtkAttributeSet*
                      add_to_attr_set                          (AtkAttributeSet   *attrib_set,
                                                                GtkTextAttributes *attrs,
                                                                AtkTextAttribute  attr);
static gchar*         get_text_near_offset                     (AtkText           *text,
                                                                GailOffsetType    function,
                                                                AtkTextBoundary   boundary_type,
                                                                gint              offset,
                                                                gint              *start_offset,
                                                                gint              *end_offset);

G_DEFINE_TYPE_WITH_CODE (GailHtmlBoxText, gail_html_box_text, GAIL_TYPE_HTML_BOX,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_TEXT, gail_html_box_text_text_interface_init))

AtkObject*
gail_html_box_text_new (GObject *obj)
{
  gpointer object;
  AtkObject *atk_object;
  GailHtmlBoxText *gail_text;

  g_return_val_if_fail (HTML_IS_BOX_TEXT (obj), NULL);
  object = g_object_new (GAIL_TYPE_HTML_BOX_TEXT, NULL);
  atk_object = ATK_OBJECT (object);
  gail_text = GAIL_HTML_BOX_TEXT (object);

  atk_object_initialize (atk_object, obj);
  gail_text->texthelper = gail_text_helper_new ();
#if 0
  gail_text_helper_text_setup (gail_text->texthelper,
                               HTML_BOX_TEXT (obj)->master->text);
#endif

  atk_object->role =  ATK_ROLE_TEXT;
  return atk_object;
}

static void
gail_html_box_text_class_init (GailHtmlBoxTextClass *klass)
{
}

static void
gail_html_box_text_text_interface_init (AtkTextIface *iface)
{
  iface->get_text = gail_html_box_text_get_text;
  iface->get_text_after_offset = gail_html_box_text_get_text_after_offset;
  iface->get_text_at_offset = gail_html_box_text_get_text_at_offset;
  iface->get_text_before_offset = gail_html_box_text_get_text_before_offset;
  iface->get_character_at_offset = gail_html_box_text_get_character_at_offset;
  iface->get_character_count = gail_html_box_text_get_character_count;
  iface->get_caret_offset = gail_html_box_text_get_caret_offset;
  iface->set_caret_offset = gail_html_box_text_set_caret_offset;
  iface->get_offset_at_point = gail_html_box_text_get_offset_at_point;
  iface->get_character_extents = gail_html_box_text_get_character_extents;
  iface->get_n_selections = gail_html_box_text_get_n_selections;
  iface->get_selection = gail_html_box_text_get_selection;
  iface->add_selection = gail_html_box_text_add_selection;
  iface->remove_selection = gail_html_box_text_remove_selection;
  iface->set_selection = gail_html_box_text_set_selection;
  iface->get_run_attributes = gail_html_box_text_get_run_attributes;
  iface->get_default_attributes = gail_html_box_text_get_default_attributes;
}

static gchar*
gail_html_box_text_get_text (AtkText *text,
                             gint    start_offset,
                             gint    end_offset)
{
  GailHtmlBoxText *gail_text;
  GtkTextBuffer *buffer;
  GtkTextIter start, end;

  g_return_val_if_fail (GAIL_IS_HTML_BOX_TEXT (text), NULL);
  gail_text = GAIL_HTML_BOX_TEXT (text);
  g_return_val_if_fail (gail_text->texthelper, NULL);

  buffer = gail_text->texthelper->buffer;
  gtk_text_buffer_get_iter_at_offset (buffer, &start, start_offset);
  gtk_text_buffer_get_iter_at_offset (buffer, &end, end_offset);

  return gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
}

static gchar*
gail_html_box_text_get_text_after_offset (AtkText         *text,
                                          gint            offset,
                                          AtkTextBoundary boundary_type,
                                          gint            *start_offset,
                                          gint            *end_offset)
{
  return get_text_near_offset (text, GAIL_AFTER_OFFSET,
                               boundary_type, offset, 
                               start_offset, end_offset);
}

static gchar*
gail_html_box_text_get_text_at_offset (AtkText         *text,
                                       gint            offset,
                                       AtkTextBoundary boundary_type,
                                       gint            *start_offset,
                                       gint            *end_offset)
{
  return get_text_near_offset (text, GAIL_AT_OFFSET,
                               boundary_type, offset, 
                               start_offset, end_offset);
}

static gchar*
gail_html_box_text_get_text_before_offset (AtkText         *text,
                                           gint            offset,
                                           AtkTextBoundary boundary_type,
                                           gint            *start_offset,
                                           gint            *end_offset)
{
  return get_text_near_offset (text, GAIL_BEFORE_OFFSET,
                               boundary_type, offset, 
                               start_offset, end_offset);
}

static gunichar
gail_html_box_text_get_character_at_offset (AtkText *text,
                                            gint    offset)
{
  GailHtmlBoxText *gail_item;
  GtkTextIter start, end;
  GtkTextBuffer *buffer;
  gchar *string;
  gchar *index;

  g_return_val_if_fail (GAIL_IS_HTML_BOX_TEXT (text), 0);
  gail_item = GAIL_HTML_BOX_TEXT (text);
  buffer = gail_item->texthelper->buffer;
  gtk_text_buffer_get_start_iter (buffer, &start);
  gtk_text_buffer_get_end_iter (buffer, &end);
  string = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
  index = g_utf8_offset_to_pointer (string, offset);
  g_free (string);

  return g_utf8_get_char (index);
}

static gint
gail_html_box_text_get_character_count (AtkText *text)
{
  GtkTextBuffer *buffer;
  GailHtmlBoxText *gail_text;

  g_return_val_if_fail (GAIL_IS_HTML_BOX_TEXT (text), 0);
  gail_text = GAIL_HTML_BOX_TEXT (text);
  g_return_val_if_fail (gail_text->texthelper, 0);
  buffer = gail_text->texthelper->buffer;
  return gtk_text_buffer_get_char_count (buffer);
}

static gint
gail_html_box_text_get_caret_offset (AtkText *text)
{
  GailHtmlBoxText *gail_text;
  GtkTextBuffer *buffer;
  GtkTextMark *cursor_mark;
  GtkTextIter cursor_itr;

  g_return_val_if_fail (GAIL_IS_HTML_BOX_TEXT (text), 0);
  gail_text = GAIL_HTML_BOX_TEXT (text);
  g_return_val_if_fail (gail_text->texthelper, 0);
  buffer = gail_text->texthelper->buffer;
  cursor_mark = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &cursor_itr, cursor_mark);
  return gtk_text_iter_get_offset (&cursor_itr);
}

static gboolean
gail_html_box_text_set_caret_offset (AtkText *text,
                                     gint    offset)
{
  GailHtmlBoxText *gail_text;
  GtkTextBuffer *buffer;
  GtkTextIter pos_itr;

  g_return_val_if_fail (GAIL_IS_HTML_BOX_TEXT (text), FALSE);
  gail_text = GAIL_HTML_BOX_TEXT (text);
  g_return_val_if_fail (gail_text->texthelper, FALSE);
  buffer = gail_text->texthelper->buffer;
  gtk_text_buffer_get_iter_at_offset (buffer,  &pos_itr, offset);
  gtk_text_buffer_move_mark_by_name (buffer, "insert", &pos_itr);
  return TRUE;
}

static gint
gail_html_box_text_get_offset_at_point (AtkText      *text,
                                        gint         x,
                                        gint         y,
                                        AtkCoordType coords)
{
  return -1;
}

static void
gail_html_box_text_get_character_extents (AtkText      *text,
                                          gint         offset,
                                          gint         *x,
                                          gint         *y,
                                          gint         *width,
                                          gint         *height,
                                          AtkCoordType coords)
{
  return;
}

static AtkAttributeSet*
gail_html_box_text_get_run_attributes (AtkText *text,
                                       gint    offset,
                                       gint    *start_offset,
                                       gint    *end_offset)
{
  return NULL;
}

static AtkAttributeSet*
gail_html_box_text_get_default_attributes (AtkText *text)
{
  return NULL;
}

static gint
gail_html_box_text_get_n_selections (AtkText *text)
{
  return 0;
}

static gchar*
gail_html_box_text_get_selection (AtkText *text,
                                  gint    selection_num,
                                  gint    *start_pos,
                                  gint    *end_pos)
{
  return NULL;
}

static gboolean
gail_html_box_text_add_selection (AtkText *text,
                                gint    start_pos,
                                gint    end_pos)
{
  return FALSE;
}

static gboolean
gail_html_box_text_remove_selection (AtkText *text,
                                 gint    selection_num)
{
  return FALSE;
}

static gboolean
gail_html_box_text_set_selection (AtkText *text,
                                  gint    selection_num,
                                  gint    start_pos,
                                  gint    end_pos)
{
  return FALSE;
}

static AtkAttributeSet*
add_to_attr_set (AtkAttributeSet   *attrib_set,
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
                              pango_font_description_get_size (attrs->font));
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
  return gail_text_helper_add_attribute (attrib_set, 
                                         attr,
                                         value);
}

static gchar*
get_text_near_offset (AtkText          *text,
                      GailOffsetType   function,
                      AtkTextBoundary  boundary_type,
                      gint             offset,
                      gint             *start_offset,
                      gint             *end_offset)
{
  return gail_text_helper_get_text (GAIL_HTML_BOX_TEXT (text)->texthelper, NULL,
                                    function, boundary_type, offset, 
                                    start_offset, end_offset);
}
