/* GAIL - The GNOME Accessibility Enabling Library
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
#include <string.h>
#include <gtk/gtk.h>
#include "gailtextcell.h"
#include "gailcontainercell.h"
#include "gailcellparent.h"
#include <libgail-util/gailmisc.h>
#include "gail-private-macros.h"

static void      gail_text_cell_class_init		(GailTextCellClass *klass);
static void      gail_text_cell_init			(GailTextCell	*text_cell);
static void      gail_text_cell_finalize		(GObject	*object);

static G_CONST_RETURN gchar* gail_text_cell_get_name    (AtkObject      *atk_obj);

static void      atk_text_interface_init		(AtkTextIface	*iface);

/* atktext.h */

static gchar*    gail_text_cell_get_text		(AtkText	*text,
							gint		start_pos,
							gint		end_pos);
static gunichar gail_text_cell_get_character_at_offset	(AtkText	*text,
							 gint		offset);
static gchar*	gail_text_cell_get_text_before_offset	(AtkText	*text,
							 gint		offset,
							 AtkTextBoundary boundary_type,
							 gint		*start_offset,
							 gint		*end_offset);
static gchar*	gail_text_cell_get_text_at_offset	(AtkText	*text,
							 gint		offset,
							 AtkTextBoundary boundary_type,
							 gint		*start_offset,
							 gint		*end_offset);
static gchar*	gail_text_cell_get_text_after_offset	(AtkText	*text,
							 gint		offset,
							 AtkTextBoundary boundary_type,
							 gint		*start_offset,
							 gint		*end_offset);
static gint      gail_text_cell_get_character_count	(AtkText	*text);
static gint      gail_text_cell_get_caret_offset	(AtkText	*text);
static gboolean  gail_text_cell_set_caret_offset	(AtkText	*text,
							 gint		offset);
static void      gail_text_cell_get_character_extents	(AtkText	*text,
							 gint		offset,
							 gint		*x,
							 gint		*y,
							 gint		*width,
							 gint		*height,
							 AtkCoordType	coords);
static gint      gail_text_cell_get_offset_at_point	(AtkText	*text,
							 gint		x,
							 gint		y,
							 AtkCoordType	coords);
static AtkAttributeSet* gail_text_cell_get_run_attributes 
                                                        (AtkText	*text,
							 gint		offset,
							 gint		*start_offset,      
							 gint		*end_offset); 
static AtkAttributeSet* gail_text_cell_get_default_attributes 
                                                        (AtkText        *text);

static PangoLayout*     create_pango_layout             (GtkCellRendererText *gtk_renderer,
                                                         GtkWidget           *widget);
static void             add_attr                        (PangoAttrList  *attr_list,
                                                         PangoAttribute *attr);

/* Misc */

static gboolean gail_text_cell_update_cache		(GailRendererCell *cell,
							 gboolean	emit_change_signal);

gchar *gail_text_cell_property_list[] = {
  /* Set font_desc first since it resets other values if it is NULL */
  "font_desc",

  "attributes",
  "background_gdk",
  "editable",
  "family",
  "foreground_gdk",
  "rise",
  "scale",
  "size",
  "size_points",
  "stretch",
  "strikethrough",
  "style",
  "text",
  "underline",
  "variant",
  "weight",

  /* Also need the sets */
  "background_set",
  "editable_set",
  "family_set",
  "foreground_set",
  "rise_set",
  "scale_set",
  "size_set",
  "stretch_set",
  "strikethrough_set",
  "style_set",
  "underline_set",
  "variant_set",
  "weight_set",
  NULL
};

G_DEFINE_TYPE_WITH_CODE (GailTextCell, gail_text_cell, GAIL_TYPE_RENDERER_CELL,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_TEXT, atk_text_interface_init))

static void 
gail_text_cell_class_init (GailTextCellClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass *atk_object_class = ATK_OBJECT_CLASS (klass);
  GailRendererCellClass *renderer_cell_class = GAIL_RENDERER_CELL_CLASS (klass);

  renderer_cell_class->update_cache = gail_text_cell_update_cache;
  renderer_cell_class->property_list = gail_text_cell_property_list;

  atk_object_class->get_name = gail_text_cell_get_name;

  gobject_class->finalize = gail_text_cell_finalize;
}

/* atktext.h */

static void
gail_text_cell_init (GailTextCell *text_cell)
{
  text_cell->cell_text = NULL;
  text_cell->caret_pos = 0;
  text_cell->cell_length = 0;
  text_cell->textutil = gail_text_util_new ();
  atk_state_set_add_state (GAIL_CELL (text_cell)->state_set,
                           ATK_STATE_SINGLE_LINE);
}

AtkObject* 
gail_text_cell_new (void)
{
  GObject *object;
  AtkObject *atk_object;
  GailRendererCell *cell;

  object = g_object_new (GAIL_TYPE_TEXT_CELL, NULL);

  g_return_val_if_fail (object != NULL, NULL);

  atk_object = ATK_OBJECT (object);
  atk_object->role = ATK_ROLE_TABLE_CELL;

  cell = GAIL_RENDERER_CELL(object);

  cell->renderer = gtk_cell_renderer_text_new ();
  g_object_ref_sink (cell->renderer);
  return atk_object;
}

static void
gail_text_cell_finalize (GObject            *object)
{
  GailTextCell *text_cell = GAIL_TEXT_CELL (object);

  g_object_unref (text_cell->textutil);
  g_free (text_cell->cell_text);

  G_OBJECT_CLASS (gail_text_cell_parent_class)->finalize (object);
}

static G_CONST_RETURN gchar*
gail_text_cell_get_name (AtkObject *atk_obj)
{
  if (atk_obj->name)
    return atk_obj->name;
  else
    {
      GailTextCell *text_cell = GAIL_TEXT_CELL (atk_obj);

      return text_cell->cell_text;
    }
}

static gboolean
gail_text_cell_update_cache (GailRendererCell *cell,
                             gboolean         emit_change_signal)
{
  GailTextCell *text_cell = GAIL_TEXT_CELL (cell);
  AtkObject *obj = ATK_OBJECT (cell);
  gboolean rv = FALSE;
  gint temp_length;
  gchar *new_cache;

  g_object_get (G_OBJECT (cell->renderer), "text", &new_cache, NULL);

  if (text_cell->cell_text)
    {
     /*
      * If the new value is NULL and the old value isn't NULL, then the
      * value has changed.
      */
      if (new_cache == NULL ||
          strcmp (text_cell->cell_text, new_cache))
        {
          g_free (text_cell->cell_text);
          temp_length = text_cell->cell_length;
          text_cell->cell_text = NULL;
          text_cell->cell_length = 0;
          if (emit_change_signal)
            {
              g_signal_emit_by_name (cell, "text_changed::delete", 0, temp_length);
              if (obj->name == NULL)
                g_object_notify (G_OBJECT (obj), "accessible-name");
            }
          if (new_cache)
            rv = TRUE;
        }
    }
  else
    rv = TRUE;

  if (rv)
    {
      if (new_cache == NULL)
        {
          text_cell->cell_text = g_strdup ("");
          text_cell->cell_length = 0;
        }
      else
        {
          text_cell->cell_text = g_strdup (new_cache);
          text_cell->cell_length = g_utf8_strlen (new_cache, -1);
        }
    }

  g_free (new_cache);
  gail_text_util_text_setup (text_cell->textutil, text_cell->cell_text);
  
  if (rv)
    {
      if (emit_change_signal)
        {
          g_signal_emit_by_name (cell, "text_changed::insert",
  			         0, text_cell->cell_length);

          if (obj->name == NULL)
            g_object_notify (G_OBJECT (obj), "accessible-name");
        }
    }
  return rv;
}

static void
atk_text_interface_init (AtkTextIface *iface)
{
  iface->get_text = gail_text_cell_get_text;
  iface->get_character_at_offset = gail_text_cell_get_character_at_offset;
  iface->get_text_before_offset = gail_text_cell_get_text_before_offset;
  iface->get_text_at_offset = gail_text_cell_get_text_at_offset;
  iface->get_text_after_offset = gail_text_cell_get_text_after_offset;
  iface->get_character_count = gail_text_cell_get_character_count;
  iface->get_caret_offset = gail_text_cell_get_caret_offset;
  iface->set_caret_offset = gail_text_cell_set_caret_offset;
  iface->get_run_attributes = gail_text_cell_get_run_attributes;
  iface->get_default_attributes = gail_text_cell_get_default_attributes;
  iface->get_character_extents = gail_text_cell_get_character_extents;
  iface->get_offset_at_point = gail_text_cell_get_offset_at_point;
}

static gchar* 
gail_text_cell_get_text (AtkText *text, 
                         gint    start_pos,
                         gint    end_pos)
{
  if (GAIL_TEXT_CELL (text)->cell_text)
    return gail_text_util_get_substring (GAIL_TEXT_CELL (text)->textutil,
              start_pos, end_pos);
  else
    return g_strdup ("");
}

static gchar* 
gail_text_cell_get_text_before_offset (AtkText         *text,
                                       gint            offset,
                                       AtkTextBoundary boundary_type,
                                       gint            *start_offset,
                                       gint            *end_offset)
{
  return gail_text_util_get_text (GAIL_TEXT_CELL (text)->textutil,
        NULL, GAIL_BEFORE_OFFSET, boundary_type, offset, start_offset,
        end_offset);
}

static gchar* 
gail_text_cell_get_text_at_offset (AtkText         *text,
                                   gint            offset,
                                   AtkTextBoundary boundary_type,
                                   gint            *start_offset,
                                   gint            *end_offset)
{
  return gail_text_util_get_text (GAIL_TEXT_CELL (text)->textutil,
        NULL, GAIL_AT_OFFSET, boundary_type, offset, start_offset, end_offset);
}

static gchar* 
gail_text_cell_get_text_after_offset (AtkText         *text,
                                      gint            offset,
                                      AtkTextBoundary boundary_type,
                                      gint            *start_offset,
                                      gint            *end_offset)
{
  return gail_text_util_get_text (GAIL_TEXT_CELL (text)->textutil,
        NULL, GAIL_AFTER_OFFSET, boundary_type, offset, start_offset,
        end_offset);
}

static gint 
gail_text_cell_get_character_count (AtkText *text)
{
  if (GAIL_TEXT_CELL (text)->cell_text != NULL)
    return GAIL_TEXT_CELL (text)->cell_length;
  else
    return 0;
}

static gint 
gail_text_cell_get_caret_offset (AtkText *text)
{
  return GAIL_TEXT_CELL (text)->caret_pos;
}

static gboolean 
gail_text_cell_set_caret_offset (AtkText *text,
                                 gint    offset)
{
  GailTextCell *text_cell = GAIL_TEXT_CELL (text);

  if (text_cell->cell_text == NULL)
    return FALSE;
  else
    {

      /* Only set the caret within the bounds and if it is to a new position. */
      if (offset >= 0 && 
          offset <= text_cell->cell_length &&
          offset != text_cell->caret_pos)
        {
          text_cell->caret_pos = offset;

          /* emit the signal */
          g_signal_emit_by_name (text, "text_caret_moved", offset);
          return TRUE;
        }
      else
        return FALSE;
    }
}

static AtkAttributeSet*
gail_text_cell_get_run_attributes (AtkText *text,
                                  gint     offset,
                                  gint     *start_offset,
                                  gint     *end_offset) 
{
  GailRendererCell *gail_renderer; 
  GtkCellRendererText *gtk_renderer;
  AtkAttributeSet *attrib_set = NULL;
  PangoLayout *layout;
  AtkObject *parent;
  GtkWidget *widget;
  gchar *renderer_text;

  gail_renderer = GAIL_RENDERER_CELL (text);
  gtk_renderer = GTK_CELL_RENDERER_TEXT (gail_renderer->renderer);

  parent = atk_object_get_parent (ATK_OBJECT (text));
  if (GAIL_IS_CONTAINER_CELL (parent))
    parent = atk_object_get_parent (parent);
  g_return_val_if_fail (GAIL_IS_CELL_PARENT (parent), NULL);
  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (parent));
  layout = create_pango_layout (gtk_renderer, widget),
  g_object_get (gtk_renderer, "text", &renderer_text, NULL);
  attrib_set = gail_misc_layout_get_run_attributes (attrib_set, 
                                                    layout,
                                                    renderer_text,
                                                    offset,
                                                    start_offset,
                                                    end_offset);
  g_free (renderer_text);
  g_object_unref (G_OBJECT (layout));
  
  return attrib_set;
}

static AtkAttributeSet*
gail_text_cell_get_default_attributes (AtkText	*text)
{
  GailRendererCell *gail_renderer; 
  GtkCellRendererText *gtk_renderer;
  AtkAttributeSet *attrib_set = NULL;
  PangoLayout *layout;
  AtkObject *parent;
  GtkWidget *widget;

  gail_renderer = GAIL_RENDERER_CELL (text);
  gtk_renderer = GTK_CELL_RENDERER_TEXT (gail_renderer->renderer);

  parent = atk_object_get_parent (ATK_OBJECT (text));
  if (GAIL_IS_CONTAINER_CELL (parent))
    parent = atk_object_get_parent (parent);
  g_return_val_if_fail (GAIL_IS_CELL_PARENT (parent), NULL);
  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (parent));
  layout = create_pango_layout (gtk_renderer, widget),

  attrib_set = gail_misc_get_default_attributes (attrib_set, 
                                                 layout,
                                                 widget);
  g_object_unref (G_OBJECT (layout));
  return attrib_set;
}

/* 
 * This function is used by gail_text_cell_get_offset_at_point()
 * and gail_text_cell_get_character_extents(). There is no 
 * cached PangoLayout for gailtextcell so we must create a temporary
 * one using this function.
 */ 
static PangoLayout*
create_pango_layout(GtkCellRendererText *gtk_renderer,
                    GtkWidget           *widget)
{
  GdkColor *foreground_gdk;
  PangoAttrList *attr_list, *attributes;
  PangoLayout *layout;
  PangoUnderline uline, underline;
  PangoFontMask mask;
  PangoFontDescription *font_desc;
  gboolean foreground_set, strikethrough_set, strikethrough;
  gboolean scale_set, underline_set, rise_set;
  gchar *renderer_text;
  gdouble scale;
  gint rise;

  g_object_get (gtk_renderer,
                "text", &renderer_text,
                "attributes", &attributes,
                "foreground-set", &foreground_set,
                "foreground-gdk", &foreground_gdk,
                "strikethrough-set", &strikethrough_set,
                "strikethrough", &strikethrough,
                "font-desc", &font_desc,
                "scale-set", &scale_set,
                "scale", &scale,
                "underline-set", &underline_set,
                "underline", &underline,
                "rise-set", &rise_set,
                "rise", &rise,
                NULL);

  layout = gtk_widget_create_pango_layout (widget, renderer_text);

  if (attributes)
    attr_list = pango_attr_list_copy (attributes);
  else
    attr_list = pango_attr_list_new ();

  if (foreground_set)
    {
      add_attr (attr_list, pango_attr_foreground_new (foreground_gdk->red,
                                                      foreground_gdk->green,
                                                      foreground_gdk->blue));
    }

  if (strikethrough_set)
    add_attr (attr_list,
              pango_attr_strikethrough_new (strikethrough));

  mask = pango_font_description_get_set_fields (font_desc);

  if (mask & PANGO_FONT_MASK_FAMILY)
    add_attr (attr_list,
      pango_attr_family_new (pango_font_description_get_family (font_desc)));

  if (mask & PANGO_FONT_MASK_STYLE)
    add_attr (attr_list, pango_attr_style_new (pango_font_description_get_style (font_desc)));

  if (mask & PANGO_FONT_MASK_VARIANT)
    add_attr (attr_list, pango_attr_variant_new (pango_font_description_get_variant (font_desc)));

  if (mask & PANGO_FONT_MASK_WEIGHT)
    add_attr (attr_list, pango_attr_weight_new (pango_font_description_get_weight (font_desc)));

  if (mask & PANGO_FONT_MASK_STRETCH)
    add_attr (attr_list, pango_attr_stretch_new (pango_font_description_get_stretch (font_desc)));

  if (mask & PANGO_FONT_MASK_SIZE)
    add_attr (attr_list, pango_attr_size_new (pango_font_description_get_size (font_desc)));

  if (scale_set && scale != 1.0)
    add_attr (attr_list, pango_attr_scale_new (scale));

  if (underline_set)
    uline = underline;
  else
    uline = PANGO_UNDERLINE_NONE;

  if (uline != PANGO_UNDERLINE_NONE)
    add_attr (attr_list,
              pango_attr_underline_new (underline));

  if (rise_set)
    add_attr (attr_list, pango_attr_rise_new (rise));

  pango_layout_set_attributes (layout, attr_list);
  pango_layout_set_width (layout, -1);
  pango_attr_list_unref (attr_list);

  pango_font_description_free (font_desc);
  g_free (foreground_gdk);
  pango_attr_list_unref (attributes);
  g_free (renderer_text);

  return layout;
}

static void 
add_attr (PangoAttrList  *attr_list,
         PangoAttribute *attr)
{
  attr->start_index = 0;
  attr->end_index = G_MAXINT;
  pango_attr_list_insert (attr_list, attr);
}

static void      
gail_text_cell_get_character_extents (AtkText          *text,
                                      gint             offset,
                                      gint             *x,
                                      gint             *y,
                                      gint             *width,
                                      gint             *height,
                                      AtkCoordType     coords)
{
  GailRendererCell *gail_renderer; 
  GtkRequisition min_size;
  GtkCellRendererText *gtk_renderer;
  GdkRectangle rendered_rect;
  GtkWidget *widget;
  AtkObject *parent;
  PangoRectangle char_rect;
  PangoLayout *layout;
  gchar *renderer_text;
  gfloat xalign, yalign;
  gint x_offset, y_offset, index;
  gint xpad, ypad;

  if (!GAIL_TEXT_CELL (text)->cell_text)
    {
      *x = *y = *height = *width = 0;
      return;
    }
  if (offset < 0 || offset >= GAIL_TEXT_CELL (text)->cell_length)
    {
      *x = *y = *height = *width = 0;
      return;
    }
  gail_renderer = GAIL_RENDERER_CELL (text);
  gtk_renderer = GTK_CELL_RENDERER_TEXT (gail_renderer->renderer);
  /*
   * Thus would be inconsistent with the cache
   */
  g_object_get (gtk_renderer, "text", &renderer_text, NULL);
  if (text == NULL)
    {
      g_free (renderer_text);
      return;
    }

  parent = atk_object_get_parent (ATK_OBJECT (text));
  if (GAIL_IS_CONTAINER_CELL (parent))
    parent = atk_object_get_parent (parent);
  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (parent));
  g_return_if_fail (GAIL_IS_CELL_PARENT (parent));
  gail_cell_parent_get_cell_area (GAIL_CELL_PARENT (parent), GAIL_CELL (text),
                                  &rendered_rect);

  gtk_cell_renderer_get_preferred_size (GTK_CELL_RENDERER (gtk_renderer),
                                        widget,
                                        &min_size, NULL);

  gtk_cell_renderer_get_alignment (GTK_CELL_RENDERER (gtk_renderer), &xalign, &yalign);
  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    xalign = 1.0 - xalign;
  x_offset = MAX (0, xalign * (rendered_rect.width - min_size.width));
  y_offset = MAX (0, yalign * (rendered_rect.height - min_size.height));

  layout = create_pango_layout (gtk_renderer, widget);

  index = g_utf8_offset_to_pointer (renderer_text, offset) - renderer_text;
  pango_layout_index_to_pos (layout, index, &char_rect);

  gtk_cell_renderer_get_padding (gail_renderer->renderer, &xpad, &ypad);
  gail_misc_get_extents_from_pango_rectangle (widget,
      &char_rect,
      x_offset + rendered_rect.x + xpad,
      y_offset + rendered_rect.y + ypad,
      x, y, width, height, coords);

  g_free (renderer_text);
  g_object_unref (layout);

  return;
} 

static gint      
gail_text_cell_get_offset_at_point (AtkText          *text,
                                    gint             x,
                                    gint             y,
                                    AtkCoordType     coords)
{
  AtkObject *parent;
  GailRendererCell *gail_renderer; 
  GtkCellRendererText *gtk_renderer;
  GtkRequisition min_size;
  GtkWidget *widget;
  GdkRectangle rendered_rect;
  PangoLayout *layout;
  gchar *renderer_text;
  gfloat xalign, yalign;
  gint x_offset, y_offset, index;
  gint xpad, ypad;
 
  if (!GAIL_TEXT_CELL (text)->cell_text)
    return -1;

  gail_renderer = GAIL_RENDERER_CELL (text);
  gtk_renderer = GTK_CELL_RENDERER_TEXT (gail_renderer->renderer);
  parent = atk_object_get_parent (ATK_OBJECT (text));

  g_object_get (gtk_renderer, "text", &renderer_text, NULL);
  if (text == NULL)
    {
      g_free (renderer_text);
      return -1;
    }

  if (GAIL_IS_CONTAINER_CELL (parent))
    parent = atk_object_get_parent (parent);

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (parent));

  g_return_val_if_fail (GAIL_IS_CELL_PARENT (parent), -1);
  gail_cell_parent_get_cell_area (GAIL_CELL_PARENT (parent), GAIL_CELL (text),
                                  &rendered_rect);

  gtk_cell_renderer_get_preferred_size (GTK_CELL_RENDERER (gtk_renderer),
                                        widget,
                                        &min_size, NULL);
  gtk_cell_renderer_get_alignment (GTK_CELL_RENDERER (gtk_renderer), &xalign, &yalign);
  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    xalign = 1.0 - xalign;
  x_offset = MAX (0, xalign * (rendered_rect.width - min_size.width));
  y_offset = MAX (0, yalign * (rendered_rect.height - min_size.height));

  layout = create_pango_layout (gtk_renderer, widget);

  gtk_cell_renderer_get_padding (gail_renderer->renderer, &xpad, &ypad);
  index = gail_misc_get_index_at_point_in_layout (widget, layout,
        x_offset + rendered_rect.x + xpad,
        y_offset + rendered_rect.y + ypad,
        x, y, coords);
  g_object_unref (layout);
  if (index == -1)
    {
      if (coords == ATK_XY_WINDOW || coords == ATK_XY_SCREEN)
        {
          glong length;

          length = g_utf8_strlen (renderer_text, -1);
          g_free (renderer_text);

          return length;
        }

      g_free (renderer_text);

      return index;  
    }
  else
    {
      glong offset;

      offset = g_utf8_pointer_to_offset (renderer_text,
                                         renderer_text + index);
      g_free (renderer_text);

      return offset;
    }
}

static gunichar 
gail_text_cell_get_character_at_offset (AtkText       *text,
                                        gint          offset)
{
  gchar *index;
  gchar *string;

  string = GAIL_TEXT_CELL(text)->cell_text;

  if (!string)
    return '\0';

  if (offset >= g_utf8_strlen (string, -1))
    return '\0';

  index = g_utf8_offset_to_pointer (string, offset);

  return g_utf8_get_char (index);
}

