/* GTK+ - accessibility implementations
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gtk/gtk.h>
#include "../gtkpango.h"
#include "gtktextcellaccessible.h"
#include "gtkcontainercellaccessible.h"
#include "gtkcellaccessibleparent.h"
#include "gtkstylecontextprivate.h"

struct _GtkTextCellAccessiblePrivate
{
  gchar *cell_text;
  gint caret_pos;
  gint cell_length;
  PangoLayout *layout;
};

static const gchar* gtk_text_cell_accessible_get_name    (AtkObject      *atk_obj);


/* atktext.h */

static gchar*    gtk_text_cell_accessible_get_text                (AtkText        *text,
                                                        gint            start_pos,
                                                        gint            end_pos);
static gunichar gtk_text_cell_accessible_get_character_at_offset  (AtkText        *text,
                                                         gint           offset);
static gchar*   gtk_text_cell_accessible_get_text_before_offset   (AtkText        *text,
                                                         gint           offset,
                                                         AtkTextBoundary boundary_type,
                                                         gint           *start_offset,
                                                         gint           *end_offset);
static gchar*   gtk_text_cell_accessible_get_text_at_offset       (AtkText        *text,
                                                         gint           offset,
                                                         AtkTextBoundary boundary_type,
                                                         gint           *start_offset,
                                                         gint           *end_offset);
static gchar*   gtk_text_cell_accessible_get_text_after_offset    (AtkText        *text,
                                                         gint           offset,
                                                         AtkTextBoundary boundary_type,
                                                         gint           *start_offset,
                                                         gint           *end_offset);
static gint      gtk_text_cell_accessible_get_character_count     (AtkText        *text);
static gint      gtk_text_cell_accessible_get_caret_offset        (AtkText        *text);
static gboolean  gtk_text_cell_accessible_set_caret_offset        (AtkText        *text,
                                                         gint           offset);
static void      gtk_text_cell_accessible_get_character_extents   (AtkText        *text,
                                                         gint           offset,
                                                         gint           *x,
                                                         gint           *y,
                                                         gint           *width,
                                                         gint           *height,
                                                         AtkCoordType   coords);
static gint      gtk_text_cell_accessible_get_offset_at_point     (AtkText        *text,
                                                         gint           x,
                                                         gint           y,
                                                         AtkCoordType   coords);
static AtkAttributeSet* gtk_text_cell_accessible_get_run_attributes 
                                                        (AtkText        *text,
                                                         gint           offset,
                                                         gint           *start_offset,      
                                                         gint           *end_offset); 
static AtkAttributeSet* gtk_text_cell_accessible_get_default_attributes 
                                                        (AtkText        *text);

static GtkWidget*       get_widget                      (GtkTextCellAccessible *cell);
static PangoLayout*     create_pango_layout             (GtkTextCellAccessible *cell);
static void             add_attr                        (PangoAttrList  *attr_list,
                                                         PangoAttribute *attr);

/* Misc */

static void gtk_text_cell_accessible_update_cache       (GtkCellAccessible *cell);

static void atk_text_interface_init (AtkTextIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkTextCellAccessible, gtk_text_cell_accessible, GTK_TYPE_RENDERER_CELL_ACCESSIBLE,
                         G_ADD_PRIVATE (GtkTextCellAccessible)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_TEXT, atk_text_interface_init))

static AtkStateSet *
gtk_text_cell_accessible_ref_state_set (AtkObject *accessible)
{
  AtkStateSet *state_set;

  state_set = ATK_OBJECT_CLASS (gtk_text_cell_accessible_parent_class)->ref_state_set (accessible);

  atk_state_set_add_state (state_set, ATK_STATE_SINGLE_LINE);

  return state_set;
}

static void
gtk_text_cell_accessible_finalize (GObject *object)
{
  GtkTextCellAccessible *text_cell = GTK_TEXT_CELL_ACCESSIBLE (object);

  g_free (text_cell->priv->cell_text);

  if (text_cell->priv->layout)
    g_object_unref (text_cell->priv->layout);

  G_OBJECT_CLASS (gtk_text_cell_accessible_parent_class)->finalize (object);
}

static const gchar *
gtk_text_cell_accessible_get_name (AtkObject *atk_obj)
{
  GtkTextCellAccessible *text_cell = GTK_TEXT_CELL_ACCESSIBLE (atk_obj);

  if (atk_obj->name)
    return atk_obj->name;

  return text_cell->priv->cell_text;
}

static void
gtk_text_cell_accessible_update_cache (GtkCellAccessible *cell)
{
  GtkTextCellAccessible *text_cell = GTK_TEXT_CELL_ACCESSIBLE (cell);
  AtkObject *obj = ATK_OBJECT (cell);
  gboolean rv = FALSE;
  gint temp_length;
  gchar *text;
  GtkCellRenderer *renderer;

  if (text_cell->priv->layout)
    g_object_unref (text_cell->priv->layout);
  text_cell->priv->layout = create_pango_layout (text_cell);

  g_object_get (cell, "renderer", &renderer, NULL);
  g_object_get (renderer, "text", &text, NULL);
  g_object_unref (renderer);

  if (text_cell->priv->cell_text)
    {
      if (text == NULL || g_strcmp0 (text_cell->priv->cell_text, text) != 0)
        {
          g_free (text_cell->priv->cell_text);
          temp_length = text_cell->priv->cell_length;
          text_cell->priv->cell_text = NULL;
          text_cell->priv->cell_length = 0;
          g_signal_emit_by_name (cell, "text-changed::delete", 0, temp_length);
          if (obj->name == NULL)
            g_object_notify (G_OBJECT (obj), "accessible-name");
          if (text)
            rv = TRUE;
        }
    }
  else
    rv = TRUE;

  if (rv)
    {
      if (text == NULL)
        {
          text_cell->priv->cell_text = g_strdup ("");
          text_cell->priv->cell_length = 0;
        }
      else
        {
          text_cell->priv->cell_text = g_strdup (text);
          text_cell->priv->cell_length = g_utf8_strlen (text, -1);
        }
    }

  g_free (text);

  if (rv)
    {
      g_signal_emit_by_name (cell, "text-changed::insert",
                             0, text_cell->priv->cell_length);

      if (obj->name == NULL)
        g_object_notify (G_OBJECT (obj), "accessible-name");
    }
}

static void
gtk_text_cell_accessible_class_init (GtkTextCellAccessibleClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass *atk_object_class = ATK_OBJECT_CLASS (klass);
  GtkCellAccessibleClass *cell_class = GTK_CELL_ACCESSIBLE_CLASS (klass);

  cell_class->update_cache = gtk_text_cell_accessible_update_cache;

  atk_object_class->get_name = gtk_text_cell_accessible_get_name;
  atk_object_class->ref_state_set = gtk_text_cell_accessible_ref_state_set;

  gobject_class->finalize = gtk_text_cell_accessible_finalize;
}

static void
gtk_text_cell_accessible_init (GtkTextCellAccessible *text_cell)
{
  text_cell->priv = gtk_text_cell_accessible_get_instance_private (text_cell);
}

static gchar *
gtk_text_cell_accessible_get_text (AtkText *atk_text,
                                   gint     start_pos,
                                   gint     end_pos)
{
  gchar *text;

  text = GTK_TEXT_CELL_ACCESSIBLE (atk_text)->priv->cell_text;
  if (text)
    return g_utf8_substring (text, start_pos, end_pos > -1 ? end_pos : g_utf8_strlen (text, -1));
  else
    return g_strdup ("");
}

static gchar *
gtk_text_cell_accessible_get_text_before_offset (AtkText         *atk_text,
                                                 gint             offset,
                                                 AtkTextBoundary  boundary_type,
                                                 gint            *start_offset,
                                                 gint            *end_offset)
{
  GtkTextCellAccessible *text_cell = GTK_TEXT_CELL_ACCESSIBLE (atk_text);
  gchar *text;

  text = _gtk_pango_get_text_before (text_cell->priv->layout, boundary_type, offset, start_offset, end_offset);

  return text;
}

static gchar *
gtk_text_cell_accessible_get_text_at_offset (AtkText         *atk_text,
                                             gint             offset,
                                             AtkTextBoundary  boundary_type,
                                             gint            *start_offset,
                                             gint            *end_offset)
{
  GtkTextCellAccessible *text_cell = GTK_TEXT_CELL_ACCESSIBLE (atk_text);
  gchar *text;

  text = _gtk_pango_get_text_at (text_cell->priv->layout, boundary_type, offset, start_offset, end_offset);

  return text;
}

static gchar *
gtk_text_cell_accessible_get_text_after_offset (AtkText         *atk_text,
                                                gint             offset,
                                                AtkTextBoundary  boundary_type,
                                                gint            *start_offset,
                                                gint            *end_offset)
{
  GtkTextCellAccessible *text_cell = GTK_TEXT_CELL_ACCESSIBLE (atk_text);
  gchar *text;

  text = _gtk_pango_get_text_after (text_cell->priv->layout, boundary_type, offset, start_offset, end_offset);

  return text;
}

static gint
gtk_text_cell_accessible_get_character_count (AtkText *text)
{
  if (GTK_TEXT_CELL_ACCESSIBLE (text)->priv->cell_text != NULL)
    return GTK_TEXT_CELL_ACCESSIBLE (text)->priv->cell_length;
  else
    return 0;
}

static gint
gtk_text_cell_accessible_get_caret_offset (AtkText *text)
{
  return GTK_TEXT_CELL_ACCESSIBLE (text)->priv->caret_pos;
}

static gboolean
gtk_text_cell_accessible_set_caret_offset (AtkText *text,
                                           gint     offset)
{
  GtkTextCellAccessible *text_cell = GTK_TEXT_CELL_ACCESSIBLE (text);

  if (text_cell->priv->cell_text == NULL)
    return FALSE;
  else
    {
      /* Only set the caret within the bounds and if it is to a new position. */
      if (offset >= 0 &&
          offset <= text_cell->priv->cell_length &&
          offset != text_cell->priv->caret_pos)
        {
          text_cell->priv->caret_pos = offset;

          /* emit the signal */
          g_signal_emit_by_name (text, "text-caret-moved", offset);
          return TRUE;
        }
      else
        return FALSE;
    }
}

static AtkAttributeSet *
gtk_text_cell_accessible_get_run_attributes (AtkText *text,
                                             gint     offset,
                                             gint    *start_offset,
                                             gint    *end_offset)
{
  AtkAttributeSet *attrib_set = NULL;
  GtkTextCellAccessible *text_cell = GTK_TEXT_CELL_ACCESSIBLE (text);

  attrib_set = _gtk_pango_get_run_attributes  (NULL, text_cell->priv->layout, offset, start_offset, end_offset);

  return attrib_set;
}

static AtkAttributeSet *
add_attribute (AtkAttributeSet  *attributes,
               AtkTextAttribute  attr,
               const gchar      *value)
{
  AtkAttribute *at;

  at = g_new (AtkAttribute, 1);
  at->name = g_strdup (atk_text_attribute_get_name (attr));
  at->value = g_strdup (value);

  return g_slist_prepend (attributes, at);
}

static AtkAttributeSet *
gtk_text_cell_accessible_get_default_attributes (AtkText *text)
{
  AtkAttributeSet *attrib_set = NULL;
  GtkTextCellAccessible *text_cell = GTK_TEXT_CELL_ACCESSIBLE (text);
  GtkWidget *widget;

  widget = get_widget (GTK_TEXT_CELL_ACCESSIBLE (text));

  attrib_set = add_attribute (attrib_set, ATK_TEXT_ATTR_DIRECTION,
                   atk_text_attribute_get_value (ATK_TEXT_ATTR_DIRECTION,
                                                 gtk_widget_get_direction (widget)));
  attrib_set = _gtk_pango_get_default_attributes (NULL, text_cell->priv->layout);

  attrib_set = _gtk_style_context_get_attributes (attrib_set,
                                                  gtk_widget_get_style_context (widget),
                                                  gtk_widget_get_state_flags (widget));

  return attrib_set;
}

GtkWidget *
get_widget (GtkTextCellAccessible *text)
{
  AtkObject *parent;

  parent = atk_object_get_parent (ATK_OBJECT (text));
  if (GTK_IS_CONTAINER_CELL_ACCESSIBLE (parent))
    parent = atk_object_get_parent (parent);

  return gtk_accessible_get_widget (GTK_ACCESSIBLE (parent));
}

/* This function is used by gtk_text_cell_accessible_get_offset_at_point()
 * and gtk_text_cell_accessible_get_character_extents(). There is no
 * cached PangoLayout so we must create a temporary one using this function.
 */
static PangoLayout *
create_pango_layout (GtkTextCellAccessible *text)
{
  GdkRGBA *foreground_rgba;
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
  GtkRendererCellAccessible *gail_renderer;
  GtkCellRendererText *gtk_renderer;

  gail_renderer = GTK_RENDERER_CELL_ACCESSIBLE (text);
  g_object_get (gail_renderer, "renderer", &gtk_renderer, NULL);

  g_object_get (gtk_renderer,
                "text", &renderer_text,
                "attributes", &attributes,
                "foreground-set", &foreground_set,
                "foreground-rgba", &foreground_rgba,
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
  g_object_unref (gtk_renderer);

  layout = gtk_widget_create_pango_layout (get_widget (text), renderer_text);

  if (attributes)
    attr_list = pango_attr_list_copy (attributes);
  else
    attr_list = pango_attr_list_new ();

  if (foreground_set)
    {
      add_attr (attr_list, pango_attr_foreground_new (foreground_rgba->red * 65535,
                                                      foreground_rgba->green * 65535,
                                                      foreground_rgba->blue * 65535));
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
  pango_attr_list_unref (attributes);
  g_free (renderer_text);
  gdk_rgba_free (foreground_rgba);

  return layout;
}

static void
add_attr (PangoAttrList *attr_list,
         PangoAttribute *attr)
{
  attr->start_index = 0;
  attr->end_index = G_MAXINT;
  pango_attr_list_insert (attr_list, attr);
}


static void
get_origins (GtkWidget *widget,
             gint      *x_window,
             gint      *y_window,
             gint      *x_toplevel,
             gint      *y_toplevel)
{
  GdkWindow *window;

  if (GTK_IS_TREE_VIEW (widget))
    window = gtk_tree_view_get_bin_window (GTK_TREE_VIEW (widget));
  else
    window = gtk_widget_get_window (widget);

  gdk_window_get_origin (window, x_window, y_window);
  window = gdk_window_get_toplevel (gtk_widget_get_window (widget));
  gdk_window_get_origin (window, x_toplevel, y_toplevel);
}

static void
gtk_text_cell_accessible_get_character_extents (AtkText      *text,
                                                gint          offset,
                                                gint         *x,
                                                gint         *y,
                                                gint         *width,
                                                gint         *height,
                                                AtkCoordType  coords)
{
  GtkRendererCellAccessible *gail_renderer;
  GtkRequisition min_size;
  GtkCellRendererText *gtk_renderer;
  GdkRectangle rendered_rect;
  GtkWidget *widget;
  AtkObject *parent;
  PangoRectangle char_rect;
  GtkTextCellAccessible *text_cell = GTK_TEXT_CELL_ACCESSIBLE (text);
  gchar *renderer_text;
  gfloat xalign, yalign;
  gint x_offset, y_offset, index;
  gint xpad, ypad;
  gint x_window, y_window, x_toplevel, y_toplevel;

  if (!GTK_TEXT_CELL_ACCESSIBLE (text)->priv->cell_text)
    {
      *x = *y = *height = *width = 0;
      return;
    }
  if (offset < 0 || offset >= GTK_TEXT_CELL_ACCESSIBLE (text)->priv->cell_length)
    {
      *x = *y = *height = *width = 0;
      return;
    }
  gail_renderer = GTK_RENDERER_CELL_ACCESSIBLE (text);
  g_object_get (gail_renderer, "renderer", &gtk_renderer, NULL);
  g_object_get (gtk_renderer, "text", &renderer_text, NULL);
  if (renderer_text == NULL)
    {
      g_object_unref (gtk_renderer);
      return;
    }

  parent = atk_object_get_parent (ATK_OBJECT (text));
  if (GTK_IS_CONTAINER_CELL_ACCESSIBLE (parent))
    parent = atk_object_get_parent (parent);
  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (parent));
  g_return_if_fail (GTK_IS_CELL_ACCESSIBLE_PARENT (parent));
  gtk_cell_accessible_parent_get_cell_area (GTK_CELL_ACCESSIBLE_PARENT (parent),
                                            GTK_CELL_ACCESSIBLE (text),
                                            &rendered_rect);

  gtk_cell_renderer_get_preferred_size (GTK_CELL_RENDERER (gtk_renderer),
                                        widget,
                                        &min_size, NULL);

  gtk_cell_renderer_get_alignment (GTK_CELL_RENDERER (gtk_renderer), &xalign, &yalign);
  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    xalign = 1.0 - xalign;
  x_offset = MAX (0, xalign * (rendered_rect.width - min_size.width));
  y_offset = MAX (0, yalign * (rendered_rect.height - min_size.height));

  index = g_utf8_offset_to_pointer (renderer_text, offset) - renderer_text;
  pango_layout_index_to_pos (text_cell->priv->layout, index, &char_rect);

  gtk_cell_renderer_get_padding (GTK_CELL_RENDERER (gtk_renderer), &xpad, &ypad);

  get_origins (widget, &x_window, &y_window, &x_toplevel, &y_toplevel);

  *x = (char_rect.x / PANGO_SCALE) + x_offset + rendered_rect.x + xpad + x_window;
  *y = (char_rect.y / PANGO_SCALE) + y_offset + rendered_rect.y + ypad + y_window;
  *height = char_rect.height / PANGO_SCALE;
  *width = char_rect.width / PANGO_SCALE;

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
    }

  g_free (renderer_text);
  g_object_unref (gtk_renderer);
}

static gint
gtk_text_cell_accessible_get_offset_at_point (AtkText      *text,
                                              gint          x,
                                              gint          y,
                                              AtkCoordType  coords)
{
  AtkObject *parent;
  GtkRendererCellAccessible *gail_renderer;
  GtkCellRendererText *gtk_renderer;
  GtkRequisition min_size;
  GtkWidget *widget;
  GdkRectangle rendered_rect;
  GtkTextCellAccessible *text_cell = GTK_TEXT_CELL_ACCESSIBLE (text);
  gchar *renderer_text;
  gfloat xalign, yalign;
  gint x_offset, y_offset, index;
  gint xpad, ypad;
  gint x_window, y_window, x_toplevel, y_toplevel;
  gint x_temp, y_temp;
  gboolean ret;

  if (!GTK_TEXT_CELL_ACCESSIBLE (text)->priv->cell_text)
    return -1;

  gail_renderer = GTK_RENDERER_CELL_ACCESSIBLE (text);
  g_object_get (gail_renderer, "renderer", &gtk_renderer, NULL);
  parent = atk_object_get_parent (ATK_OBJECT (text));

  g_object_get (gtk_renderer, "text", &renderer_text, NULL);
  if (text == NULL)
    {
      g_object_unref (gtk_renderer);
      g_free (renderer_text);
      return -1;
    }

  if (GTK_IS_CONTAINER_CELL_ACCESSIBLE (parent))
    parent = atk_object_get_parent (parent);

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (parent));

  g_return_val_if_fail (GTK_IS_CELL_ACCESSIBLE_PARENT (parent), -1);
  gtk_cell_accessible_parent_get_cell_area (GTK_CELL_ACCESSIBLE_PARENT (parent),
                                            GTK_CELL_ACCESSIBLE (text),
                                            &rendered_rect);

  gtk_cell_renderer_get_preferred_size (GTK_CELL_RENDERER (gtk_renderer),
                                        widget,
                                        &min_size, NULL);
  gtk_cell_renderer_get_alignment (GTK_CELL_RENDERER (gtk_renderer), &xalign, &yalign);
  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    xalign = 1.0 - xalign;
  x_offset = MAX (0, xalign * (rendered_rect.width - min_size.width));
  y_offset = MAX (0, yalign * (rendered_rect.height - min_size.height));

  gtk_cell_renderer_get_padding (GTK_CELL_RENDERER (gtk_renderer), &xpad, &ypad);

  get_origins (widget, &x_window, &y_window, &x_toplevel, &y_toplevel);

  x_temp =  x - (x_offset + rendered_rect.x + xpad) - x_window;
  y_temp =  y - (y_offset + rendered_rect.y + ypad) - y_window;
  if (coords == ATK_XY_WINDOW)
    {
      x_temp += x_toplevel;
      y_temp += y_toplevel;
    }
  else if (coords != ATK_XY_SCREEN)
    index = -1;

  ret = pango_layout_xy_to_index (text_cell->priv->layout,
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

  g_object_unref (gtk_renderer);

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
gtk_text_cell_accessible_get_character_at_offset (AtkText *text,
                                                  gint     offset)
{
  gchar *index;
  gchar *string;

  string = GTK_TEXT_CELL_ACCESSIBLE(text)->priv->cell_text;

  if (!string)
    return '\0';

  if (offset >= g_utf8_strlen (string, -1))
    return '\0';

  index = g_utf8_offset_to_pointer (string, offset);

  return g_utf8_get_char (index);
}

static void
atk_text_interface_init (AtkTextIface *iface)
{
  iface->get_text = gtk_text_cell_accessible_get_text;
  iface->get_character_at_offset = gtk_text_cell_accessible_get_character_at_offset;
  iface->get_text_before_offset = gtk_text_cell_accessible_get_text_before_offset;
  iface->get_text_at_offset = gtk_text_cell_accessible_get_text_at_offset;
  iface->get_text_after_offset = gtk_text_cell_accessible_get_text_after_offset;
  iface->get_character_count = gtk_text_cell_accessible_get_character_count;
  iface->get_caret_offset = gtk_text_cell_accessible_get_caret_offset;
  iface->set_caret_offset = gtk_text_cell_accessible_set_caret_offset;
  iface->get_run_attributes = gtk_text_cell_accessible_get_run_attributes;
  iface->get_default_attributes = gtk_text_cell_accessible_get_default_attributes;
  iface->get_character_extents = gtk_text_cell_accessible_get_character_extents;
  iface->get_offset_at_point = gtk_text_cell_accessible_get_offset_at_point;
}
