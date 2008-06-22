/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2004 Sun Microsystems Inc.
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
#include "gailscale.h"
#include <libgail-util/gailmisc.h>

static void	    gail_scale_class_init        (GailScaleClass *klass);

static void         gail_scale_init              (GailScale      *scale);

static void         gail_scale_real_initialize   (AtkObject      *obj,
                                                  gpointer      data);
static void         gail_scale_notify            (GObject       *obj,
                                                  GParamSpec    *pspec);
static void         gail_scale_finalize          (GObject        *object);

/* atktext.h */ 
static void	    atk_text_interface_init        (AtkTextIface      *iface);

static gchar*	    gail_scale_get_text	           (AtkText	      *text,
                                                    gint	      start_pos,
						    gint	      end_pos);
static gunichar	    gail_scale_get_character_at_offset
                                                   (AtkText	      *text,
						    gint	      offset);
static gchar*       gail_scale_get_text_before_offset
                                                   (AtkText	      *text,
 						    gint	      offset,
						    AtkTextBoundary   boundary_type,
						    gint	      *start_offset,
						    gint	      *end_offset);
static gchar*       gail_scale_get_text_at_offset  (AtkText	      *text,
 						    gint	      offset,
						    AtkTextBoundary   boundary_type,
						    gint	      *start_offset,
						    gint	      *end_offset);
static gchar*      gail_scale_get_text_after_offset
                                                   (AtkText	      *text,
 						    gint	      offset,
						    AtkTextBoundary   boundary_type,
						    gint	      *start_offset,
						    gint	      *end_offset);
static gint	   gail_scale_get_character_count  (AtkText	      *text);
static void        gail_scale_get_character_extents
                                                   (AtkText	      *text,
						    gint 	      offset,
		                                    gint 	      *x,
                    		   	            gint 	      *y,
                                		    gint 	      *width,
                                     		    gint 	      *height,
			        		    AtkCoordType      coords);
static gint        gail_scale_get_offset_at_point  (AtkText           *text,
                                                    gint              x,
                                                    gint              y,
			                            AtkCoordType      coords);
static AtkAttributeSet* gail_scale_get_run_attributes 
                                                   (AtkText           *text,
              					    gint 	      offset,
                                                    gint 	      *start_offset,
					            gint	      *end_offset);
static AtkAttributeSet* gail_scale_get_default_attributes
                                                   (AtkText           *text);

G_DEFINE_TYPE_WITH_CODE (GailScale, gail_scale, GAIL_TYPE_RANGE,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_TEXT, atk_text_interface_init))

static void	 
gail_scale_class_init (GailScaleClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  class->initialize = gail_scale_real_initialize;

  gobject_class->finalize = gail_scale_finalize;
  gobject_class->notify = gail_scale_notify;
}

static void
gail_scale_init (GailScale      *scale)
{
}

static void
gail_scale_real_initialize (AtkObject *obj,
                            gpointer  data)
{
  GailScale *gail_scale;
  const gchar *txt;
  PangoLayout *layout;

  ATK_OBJECT_CLASS (gail_scale_parent_class)->initialize (obj, data);

  gail_scale = GAIL_SCALE (obj);
  gail_scale->textutil = gail_text_util_new ();

  layout = gtk_scale_get_layout (GTK_SCALE (data));
  if (layout)
    {
      txt = pango_layout_get_text (layout);
      if (txt)
        {
          gail_text_util_text_setup (gail_scale->textutil, txt);
        }
    }
}

static void
gail_scale_finalize (GObject *object)
{
  GailScale *scale = GAIL_SCALE (object);

  g_object_unref (scale->textutil);
  G_OBJECT_CLASS (gail_scale_parent_class)->finalize (object);

}

static void
gail_scale_notify (GObject    *obj,
                   GParamSpec *pspec)
{
  GailScale *scale = GAIL_SCALE (obj);

  if (strcmp (pspec->name, "accessible-value") == 0)
    {
      GtkWidget *widget;

      widget = GTK_ACCESSIBLE (obj)->widget;
      if (widget)
        {
          GtkScale *gtk_scale;
          PangoLayout *layout;
          const gchar *txt;

          gtk_scale = GTK_SCALE (widget);
          layout = gtk_scale_get_layout (gtk_scale);
          if (layout)
            {
              txt = pango_layout_get_text (layout);
              if (txt)
                {
	          g_signal_emit_by_name (obj, "text_changed::delete", 0,
                                         gtk_text_buffer_get_char_count (scale->textutil->buffer));
                  gail_text_util_text_setup (scale->textutil, txt);
	          g_signal_emit_by_name (obj, "text_changed::insert", 0,
                                         g_utf8_strlen (txt, -1));
                }
            }
        }
    }
  G_OBJECT_CLASS (gail_scale_parent_class)->notify (obj, pspec);
}

/* atktext.h */

static void
atk_text_interface_init (AtkTextIface *iface)
{
  iface->get_text = gail_scale_get_text;
  iface->get_character_at_offset = gail_scale_get_character_at_offset;
  iface->get_text_before_offset = gail_scale_get_text_before_offset;
  iface->get_text_at_offset = gail_scale_get_text_at_offset;
  iface->get_text_after_offset = gail_scale_get_text_after_offset;
  iface->get_character_count = gail_scale_get_character_count;
  iface->get_character_extents = gail_scale_get_character_extents;
  iface->get_offset_at_point = gail_scale_get_offset_at_point;
  iface->get_run_attributes = gail_scale_get_run_attributes;
  iface->get_default_attributes = gail_scale_get_default_attributes;
}

static gchar*
gail_scale_get_text (AtkText *text,
                     gint    start_pos,
                     gint    end_pos)
{
  GtkWidget *widget;
  GailScale *scale;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  scale = GAIL_SCALE (text);
  return gail_text_util_get_substring (scale->textutil, 
                                       start_pos, end_pos);
}

static gchar*
gail_scale_get_text_before_offset (AtkText         *text,
     				   gint            offset,
				   AtkTextBoundary boundary_type,
				   gint            *start_offset,
				   gint            *end_offset)
{
  GtkWidget *widget;
  GailScale *scale;
  PangoLayout *layout;
  gchar *txt;
  
  widget = GTK_ACCESSIBLE (text)->widget;
  
  if (widget == NULL)
    /* State is defunct */
    return NULL;
  
  scale = GAIL_SCALE (text);
  layout = gtk_scale_get_layout (GTK_SCALE (widget));
  if (layout)
    {
      txt =  gail_text_util_get_text (scale->textutil,
                           layout, GAIL_BEFORE_OFFSET, 
                           boundary_type, offset, start_offset, end_offset); 
    }
  else
    txt = NULL;

  return txt;
}

static gchar*
gail_scale_get_text_at_offset (AtkText         *text,
	   		       gint            offset,
			       AtkTextBoundary boundary_type,
 			       gint            *start_offset,
			       gint            *end_offset)
{
  GtkWidget *widget;
  GailScale *scale;
  PangoLayout *layout;
  gchar *txt;
 
  widget = GTK_ACCESSIBLE (text)->widget;
  
  if (widget == NULL)
    /* State is defunct */
    return NULL;
  
  scale = GAIL_SCALE (text);
  layout = gtk_scale_get_layout (GTK_SCALE (widget));
  if (layout)
    {
      txt =  gail_text_util_get_text (scale->textutil,
                              layout, GAIL_AT_OFFSET, 
                              boundary_type, offset, start_offset, end_offset);
    }
  else
    txt = NULL;

  return txt;
}

static gchar*
gail_scale_get_text_after_offset (AtkText         *text,
				  gint            offset,
				  AtkTextBoundary boundary_type,
				  gint            *start_offset,
				  gint            *end_offset)
{
  GtkWidget *widget;
  GailScale *scale;
  PangoLayout *layout;
  gchar *txt;

  widget = GTK_ACCESSIBLE (text)->widget;
  
  if (widget == NULL)
    /* State is defunct */
    return NULL;
  
  scale = GAIL_SCALE (text);
  layout = gtk_scale_get_layout (GTK_SCALE (widget));
  if (layout)
    {
      txt =  gail_text_util_get_text (scale->textutil,
                              layout, GAIL_AFTER_OFFSET, 
                              boundary_type, offset, start_offset, end_offset);
    }
  else
    txt = NULL;

  return txt;
}

static gint
gail_scale_get_character_count (AtkText *text)
{
  GtkWidget *widget;
  GailScale *scale;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return 0;

  scale = GAIL_SCALE (text);
  if (scale->textutil->buffer)
    return gtk_text_buffer_get_char_count (scale->textutil->buffer);
  else
    return 0;

}

static void
gail_scale_get_character_extents (AtkText      *text,
				  gint         offset,
		                  gint         *x,
                    		  gint 	       *y,
                                  gint 	       *width,
                                  gint 	       *height,
			          AtkCoordType coords)
{
  GtkWidget *widget;
  GtkScale *scale;
  PangoRectangle char_rect;
  PangoLayout *layout;
  gint index, x_layout, y_layout;
  const gchar *scale_text;
 
  widget = GTK_ACCESSIBLE (text)->widget;

  if (widget == NULL)
    /* State is defunct */
    return;

  scale = GTK_SCALE (widget);
  layout = gtk_scale_get_layout (scale);
  if (!layout)
    return;
  scale_text = pango_layout_get_text (layout);
  if (!scale_text)
    return;
  index = g_utf8_offset_to_pointer (scale_text, offset) - scale_text;
  gtk_scale_get_layout_offsets (scale, &x_layout, &y_layout);
  pango_layout_index_to_pos (layout, index, &char_rect);
  gail_misc_get_extents_from_pango_rectangle (widget, &char_rect, 
                    x_layout, y_layout, x, y, width, height, coords);
} 

static gint 
gail_scale_get_offset_at_point (AtkText      *text,
                                gint         x,
                                gint         y,
	          		AtkCoordType coords)
{ 
  GtkWidget *widget;
  GtkScale *scale;
  PangoLayout *layout;
  gint index, x_layout, y_layout;
  const gchar *scale_text;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return -1;

  scale = GTK_SCALE (widget);
  layout = gtk_scale_get_layout (scale);
  if (!layout)
    return -1;
  scale_text = pango_layout_get_text (layout);
  if (!scale_text)
    return -1;
  
  gtk_scale_get_layout_offsets (scale, &x_layout, &y_layout);
  index = gail_misc_get_index_at_point_in_layout (widget, 
                                              layout, 
                                              x_layout, y_layout, x, y, coords);
  if (index == -1)
    {
      if (coords == ATK_XY_WINDOW || coords == ATK_XY_SCREEN)
        index = g_utf8_strlen (scale_text, -1);
    }
  else
    index = g_utf8_pointer_to_offset (scale_text, scale_text + index);  

  return index;
}

static AtkAttributeSet*
gail_scale_get_run_attributes (AtkText *text,
                               gint    offset,
                               gint    *start_offset,
	                       gint    *end_offset)
{
  GtkWidget *widget;
  GtkScale *scale;
  AtkAttributeSet *at_set = NULL;
  GtkTextDirection dir;
  PangoLayout *layout;
  const gchar *scale_text;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  scale = GTK_SCALE (widget);

  layout = gtk_scale_get_layout (scale);
  if (!layout)
    return at_set;
  scale_text = pango_layout_get_text (layout);
  if (!scale_text)
    return at_set;

  dir = gtk_widget_get_direction (widget);
  if (dir == GTK_TEXT_DIR_RTL)
    {
      at_set = gail_misc_add_attribute (at_set, 
                                        ATK_TEXT_ATTR_DIRECTION,
     g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_DIRECTION, dir)));
    }

  at_set = gail_misc_layout_get_run_attributes (at_set,
                                                layout,
                                                (gchar *)scale_text,
                                                offset,
                                                start_offset,
                                                end_offset);
  return at_set;
}

static AtkAttributeSet*
gail_scale_get_default_attributes (AtkText *text)
{
  GtkWidget *widget;
  AtkAttributeSet *at_set = NULL;
  PangoLayout *layout;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  layout = gtk_scale_get_layout (GTK_SCALE (widget));
  if (layout)
    {
      at_set = gail_misc_get_default_attributes (at_set,
                                                 layout,
                                                 widget);
    }
  return at_set;
}

static gunichar 
gail_scale_get_character_at_offset (AtkText *text,
                                    gint    offset)
{
  GtkWidget *widget;
  GtkScale *scale;
  PangoLayout *layout;
  const gchar *string;
  gchar *index;
  gunichar c;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return '\0';

  scale = GTK_SCALE (widget);

  layout = gtk_scale_get_layout (scale);
  if (!layout)
    return '\0';
  string = pango_layout_get_text (layout);
  if (offset >= g_utf8_strlen (string, -1))
    c = '\0';
  else
    {
      index = g_utf8_offset_to_pointer (string, offset);

      c = g_utf8_get_char (index);
    }

  return c;
}
