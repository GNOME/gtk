/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001, 2002, 2003 Sun Microsystems Inc.
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
#include "gailstatusbar.h"
#include <libgail-util/gailmisc.h>

static void         gail_statusbar_class_init          (GailStatusbarClass *klass);
static void         gail_statusbar_init                (GailStatusbar      *bar);
static const gchar* gail_statusbar_get_name            (AtkObject          *obj);
static gint         gail_statusbar_get_n_children      (AtkObject          *obj);
static AtkObject*   gail_statusbar_ref_child           (AtkObject          *obj,
                                                        gint               i);
static void         gail_statusbar_real_initialize     (AtkObject          *obj,
                                                        gpointer           data);

static gint         gail_statusbar_notify              (GObject            *obj,
                                                        GParamSpec         *pspec,
                                                        gpointer           user_data);
static void         gail_statusbar_finalize            (GObject            *object);
static void         gail_statusbar_init_textutil       (GailStatusbar      *statusbar,
                                                        GtkWidget          *label);

/* atktext.h */ 
static void	  atk_text_interface_init	   (AtkTextIface	*iface);

static gchar*	  gail_statusbar_get_text	   (AtkText	      *text,
                                                    gint	      start_pos,
						    gint	      end_pos);
static gunichar	  gail_statusbar_get_character_at_offset
                                                   (AtkText	      *text,
						    gint	      offset);
static gchar*     gail_statusbar_get_text_before_offset
                                                   (AtkText	      *text,
 						    gint	      offset,
						    AtkTextBoundary   boundary_type,
						    gint	      *start_offset,
						    gint	      *end_offset);
static gchar*     gail_statusbar_get_text_at_offset(AtkText	      *text,
 						    gint	      offset,
						    AtkTextBoundary   boundary_type,
						    gint	      *start_offset,
						    gint	      *end_offset);
static gchar*     gail_statusbar_get_text_after_offset
                                                   (AtkText	      *text,
 						    gint	      offset,
						    AtkTextBoundary   boundary_type,
						    gint	      *start_offset,
						    gint	      *end_offset);
static gint	  gail_statusbar_get_character_count (AtkText	      *text);
static void       gail_statusbar_get_character_extents
                                                   (AtkText	      *text,
						    gint 	      offset,
		                                    gint 	      *x,
                    		   	            gint 	      *y,
                                		    gint 	      *width,
                                     		    gint 	      *height,
			        		    AtkCoordType      coords);
static gint      gail_statusbar_get_offset_at_point(AtkText           *text,
                                                    gint              x,
                                                    gint              y,
			                            AtkCoordType      coords);
static AtkAttributeSet* gail_statusbar_get_run_attributes 
                                                   (AtkText           *text,
              					    gint 	      offset,
                                                    gint 	      *start_offset,
					            gint	      *end_offset);
static AtkAttributeSet* gail_statusbar_get_default_attributes
                                                   (AtkText           *text);
static GtkWidget* get_label_from_statusbar         (GtkWidget         *statusbar);

G_DEFINE_TYPE_WITH_CODE (GailStatusbar, gail_statusbar, GAIL_TYPE_CONTAINER,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_TEXT, atk_text_interface_init))

static void
gail_statusbar_class_init (GailStatusbarClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass  *class = ATK_OBJECT_CLASS (klass);
  GailContainerClass *container_class;

  container_class = (GailContainerClass*)klass;

  gobject_class->finalize = gail_statusbar_finalize;

  class->get_name = gail_statusbar_get_name;
  class->get_n_children = gail_statusbar_get_n_children;
  class->ref_child = gail_statusbar_ref_child;
  class->initialize = gail_statusbar_real_initialize;
  /*
   * As we report the statusbar as having no children we are not interested
   * in add and remove signals
   */
  container_class->add_gtk = NULL;
  container_class->remove_gtk = NULL;
}

static void
gail_statusbar_init (GailStatusbar *bar)
{
}

static const gchar*
gail_statusbar_get_name (AtkObject *obj)
{
  const gchar* name;

  g_return_val_if_fail (GAIL_IS_STATUSBAR (obj), NULL);

  name = ATK_OBJECT_CLASS (gail_statusbar_parent_class)->get_name (obj);
  if (name != NULL)
    return name;
  else
    {
      /*
       * Get the text on the label
       */
      GtkWidget *widget;
      GtkWidget *label;

      widget = GTK_ACCESSIBLE (obj)->widget;
      if (widget == NULL)
        /*
         * State is defunct
         */
        return NULL;

     g_return_val_if_fail (GTK_IS_STATUSBAR (widget), NULL);
     label = get_label_from_statusbar (widget);
     if (GTK_IS_LABEL (label))
       return gtk_label_get_label (GTK_LABEL (label));
     else 
       return NULL;
   }
}

static gint
gail_statusbar_get_n_children (AtkObject *obj)
{
  GtkWidget *widget;
  GList *children;
  gint count = 0;

  widget = GTK_ACCESSIBLE (obj)->widget;
  if (widget == NULL)
    return 0;

  children = gtk_container_get_children (GTK_CONTAINER (widget));
  if (children != NULL)
    {
      count = g_list_length (children);
      g_list_free (children);
    }

  return count;
}

static AtkObject*
gail_statusbar_ref_child (AtkObject *obj,
                          gint      i)
{
  GList *children, *tmp_list;
  AtkObject  *accessible;
  GtkWidget *widget;

  g_return_val_if_fail ((i >= 0), NULL);
  widget = GTK_ACCESSIBLE (obj)->widget;
  if (widget == NULL)
    return NULL;

  children = gtk_container_get_children (GTK_CONTAINER (widget));
  if (children == NULL)
    return NULL;

  tmp_list = g_list_nth (children, i);
  if (!tmp_list)
    {
      g_list_free (children);
      return NULL;
    }
  accessible = gtk_widget_get_accessible (GTK_WIDGET (tmp_list->data));

  g_list_free (children);
  g_object_ref (accessible);
  return accessible;
}

static void
gail_statusbar_real_initialize (AtkObject *obj,
                                gpointer  data)
{
  GailStatusbar *statusbar = GAIL_STATUSBAR (obj);
  GtkWidget *label;

  ATK_OBJECT_CLASS (gail_statusbar_parent_class)->initialize (obj, data);

  /*
   * We get notified of changes to the label
   */
  label = get_label_from_statusbar (GTK_WIDGET (data));
  if (GTK_IS_LABEL (label))
    {
      gail_statusbar_init_textutil (statusbar, label);
    }

  obj->role = ATK_ROLE_STATUSBAR;

}

static gint
gail_statusbar_notify (GObject    *obj, 
                       GParamSpec *pspec,
                       gpointer   user_data)
{
  AtkObject *atk_obj = ATK_OBJECT (user_data);
  GtkLabel *label;
  GailStatusbar *statusbar;

  if (strcmp (pspec->name, "label") == 0)
    {
      const gchar* label_text;

      label = GTK_LABEL (obj);

      label_text = gtk_label_get_text (label);

      statusbar = GAIL_STATUSBAR (atk_obj);
      gail_text_util_text_setup (statusbar->textutil, label_text);

      if (atk_obj->name == NULL)
      {
        /*
         * The label has changed so notify a change in accessible-name
         */
        g_object_notify (G_OBJECT (atk_obj), "accessible-name");
      }
      /*
       * The label is the only property which can be changed
       */
      g_signal_emit_by_name (atk_obj, "visible_data_changed");
    }
  return 1;
}

static void
gail_statusbar_init_textutil (GailStatusbar *statusbar,
                              GtkWidget     *label)
{
  const gchar *label_text;

  statusbar->textutil = gail_text_util_new ();
  label_text = gtk_label_get_text (GTK_LABEL (label));
  gail_text_util_text_setup (statusbar->textutil, label_text);
  g_signal_connect (label,
                    "notify",
                    (GCallback) gail_statusbar_notify,
                    statusbar);     
}

static void
gail_statusbar_finalize (GObject *object)
{
  GailStatusbar *statusbar = GAIL_STATUSBAR (object);

  if (statusbar->textutil)
    {
      g_object_unref (statusbar->textutil);
    }
  G_OBJECT_CLASS (gail_statusbar_parent_class)->finalize (object);
}

/* atktext.h */

static void
atk_text_interface_init (AtkTextIface *iface)
{
  iface->get_text = gail_statusbar_get_text;
  iface->get_character_at_offset = gail_statusbar_get_character_at_offset;
  iface->get_text_before_offset = gail_statusbar_get_text_before_offset;
  iface->get_text_at_offset = gail_statusbar_get_text_at_offset;
  iface->get_text_after_offset = gail_statusbar_get_text_after_offset;
  iface->get_character_count = gail_statusbar_get_character_count;
  iface->get_character_extents = gail_statusbar_get_character_extents;
  iface->get_offset_at_point = gail_statusbar_get_offset_at_point;
  iface->get_run_attributes = gail_statusbar_get_run_attributes;
  iface->get_default_attributes = gail_statusbar_get_default_attributes;
}

static gchar*
gail_statusbar_get_text (AtkText *text,
                         gint    start_pos,
                         gint    end_pos)
{
  GtkWidget *widget;
  GtkWidget *label;
  GailStatusbar *statusbar;
  const gchar *label_text;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  label = get_label_from_statusbar (widget);

  if (!GTK_IS_LABEL (label))
    return NULL;

  statusbar = GAIL_STATUSBAR (text);
  if (!statusbar->textutil) 
    gail_statusbar_init_textutil (statusbar, label);

  label_text = gtk_label_get_text (GTK_LABEL (label));

  if (label_text == NULL)
    return NULL;
  else
  {
    return gail_text_util_get_substring (statusbar->textutil, 
                                         start_pos, end_pos);
  }
}

static gchar*
gail_statusbar_get_text_before_offset (AtkText         *text,
     				       gint            offset,
				       AtkTextBoundary boundary_type,
				       gint            *start_offset,
				       gint            *end_offset)
{
  GtkWidget *widget;
  GtkWidget *label;
  GailStatusbar *statusbar;
  
  widget = GTK_ACCESSIBLE (text)->widget;
  
  if (widget == NULL)
    /* State is defunct */
    return NULL;
  
  /* Get label */
  label = get_label_from_statusbar (widget);

  if (!GTK_IS_LABEL(label))
    return NULL;

  statusbar = GAIL_STATUSBAR (text);
  if (!statusbar->textutil)
    gail_statusbar_init_textutil (statusbar, label);

  return gail_text_util_get_text (statusbar->textutil,
                           gtk_label_get_layout (GTK_LABEL (label)), GAIL_BEFORE_OFFSET, 
                           boundary_type, offset, start_offset, end_offset); 
}

static gchar*
gail_statusbar_get_text_at_offset (AtkText         *text,
			           gint            offset,
			           AtkTextBoundary boundary_type,
 			           gint            *start_offset,
			           gint            *end_offset)
{
  GtkWidget *widget;
  GtkWidget *label;
  GailStatusbar *statusbar;
 
  widget = GTK_ACCESSIBLE (text)->widget;
  
  if (widget == NULL)
    /* State is defunct */
    return NULL;
  
  /* Get label */
  label = get_label_from_statusbar (widget);

  if (!GTK_IS_LABEL(label))
    return NULL;

  statusbar = GAIL_STATUSBAR (text);
  if (!statusbar->textutil)
    gail_statusbar_init_textutil (statusbar, label);

  return gail_text_util_get_text (statusbar->textutil,
                              gtk_label_get_layout (GTK_LABEL (label)), GAIL_AT_OFFSET, 
                              boundary_type, offset, start_offset, end_offset);
}

static gchar*
gail_statusbar_get_text_after_offset (AtkText         *text,
				      gint            offset,
				      AtkTextBoundary boundary_type,
				      gint            *start_offset,
				      gint            *end_offset)
{
  GtkWidget *widget;
  GtkWidget *label;
  GailStatusbar *statusbar;

  widget = GTK_ACCESSIBLE (text)->widget;
  
  if (widget == NULL)
  {
    /* State is defunct */
    return NULL;
  }
  
  /* Get label */
  label = get_label_from_statusbar (widget);

  if (!GTK_IS_LABEL(label))
    return NULL;

  statusbar = GAIL_STATUSBAR (text);
  if (!statusbar->textutil)
    gail_statusbar_init_textutil (statusbar, label);

  return gail_text_util_get_text (statusbar->textutil,
                           gtk_label_get_layout (GTK_LABEL (label)), GAIL_AFTER_OFFSET, 
                           boundary_type, offset, start_offset, end_offset);
}

static gint
gail_statusbar_get_character_count (AtkText *text)
{
  GtkWidget *widget;
  GtkWidget *label;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return 0;

  label = get_label_from_statusbar (widget);

  if (!GTK_IS_LABEL(label))
    return 0;

  return g_utf8_strlen (gtk_label_get_text (GTK_LABEL (label)), -1);
}

static void
gail_statusbar_get_character_extents (AtkText      *text,
				      gint         offset,
		                      gint         *x,
                    		      gint 	   *y,
                                      gint 	   *width,
                                      gint 	   *height,
			              AtkCoordType coords)
{
  GtkWidget *widget;
  GtkWidget *label;
  PangoRectangle char_rect;
  gint index, x_layout, y_layout;
  const gchar *label_text;
 
  widget = GTK_ACCESSIBLE (text)->widget;

  if (widget == NULL)
    /* State is defunct */
    return;

  label = get_label_from_statusbar (widget);

  if (!GTK_IS_LABEL(label))
    return;
  
  gtk_label_get_layout_offsets (GTK_LABEL (label), &x_layout, &y_layout);
  label_text = gtk_label_get_text (GTK_LABEL (label));
  index = g_utf8_offset_to_pointer (label_text, offset) - label_text;
  pango_layout_index_to_pos (gtk_label_get_layout (GTK_LABEL (label)), index, &char_rect);
  
  gail_misc_get_extents_from_pango_rectangle (label, &char_rect, 
                    x_layout, y_layout, x, y, width, height, coords);
} 

static gint 
gail_statusbar_get_offset_at_point (AtkText      *text,
                                    gint         x,
                                    gint         y,
	          		    AtkCoordType coords)
{ 
  GtkWidget *widget;
  GtkWidget *label;
  gint index, x_layout, y_layout;
  const gchar *label_text;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return -1;

  label = get_label_from_statusbar (widget);

  if (!GTK_IS_LABEL(label))
    return -1;
  
  gtk_label_get_layout_offsets (GTK_LABEL (label), &x_layout, &y_layout);
  
  index = gail_misc_get_index_at_point_in_layout (label, 
                                              gtk_label_get_layout (GTK_LABEL (label)), 
                                              x_layout, y_layout, x, y, coords);
  label_text = gtk_label_get_text (GTK_LABEL (label));
  if (index == -1)
    {
      if (coords == ATK_XY_WINDOW || coords == ATK_XY_SCREEN)
        return g_utf8_strlen (label_text, -1);

      return index;  
    }
  else
    return g_utf8_pointer_to_offset (label_text, label_text + index);  
}

static AtkAttributeSet*
gail_statusbar_get_run_attributes (AtkText *text,
                                   gint    offset,
                                   gint    *start_offset,
	                           gint    *end_offset)
{
  GtkWidget *widget;
  GtkWidget *label;
  AtkAttributeSet *at_set = NULL;
  GtkJustification justify;
  GtkTextDirection dir;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  label = get_label_from_statusbar (widget);

  if (!GTK_IS_LABEL(label))
    return NULL;
  
  /* Get values set for entire label, if any */
  justify = gtk_label_get_justify (GTK_LABEL (label));
  if (justify != GTK_JUSTIFY_CENTER)
    {
      at_set = gail_misc_add_attribute (at_set, 
                                        ATK_TEXT_ATTR_JUSTIFICATION,
     g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_JUSTIFICATION, justify)));
    }
  dir = gtk_widget_get_direction (label);
  if (dir == GTK_TEXT_DIR_RTL)
    {
      at_set = gail_misc_add_attribute (at_set, 
                                        ATK_TEXT_ATTR_DIRECTION,
     g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_DIRECTION, dir)));
    }

  at_set = gail_misc_layout_get_run_attributes (at_set,
                                                gtk_label_get_layout (GTK_LABEL (label)),
                                                (gchar *) gtk_label_get_text (GTK_LABEL (label)),
                                                offset,
                                                start_offset,
                                                end_offset);
  return at_set;
}

static AtkAttributeSet*
gail_statusbar_get_default_attributes (AtkText *text)
{
  GtkWidget *widget;
  GtkWidget *label;
  AtkAttributeSet *at_set = NULL;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  label = get_label_from_statusbar (widget);

  if (!GTK_IS_LABEL(label))
    return NULL;

  at_set = gail_misc_get_default_attributes (at_set,
                                             gtk_label_get_layout (GTK_LABEL (label)),
                                             widget);
  return at_set;
}

static gunichar 
gail_statusbar_get_character_at_offset (AtkText *text,
                                        gint	offset)
{
  GtkWidget *widget;
  GtkWidget *label;
  const gchar *string;
  gchar *index;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return '\0';

  label = get_label_from_statusbar (widget);

  if (!GTK_IS_LABEL(label))
    return '\0';
  string = gtk_label_get_text (GTK_LABEL (label));
  if (offset >= g_utf8_strlen (string, -1))
    return '\0';
  index = g_utf8_offset_to_pointer (string, offset);

  return g_utf8_get_char (index);
}

static GtkWidget*
get_label_from_statusbar (GtkWidget *statusbar)
{
  return GTK_STATUSBAR (statusbar)->label;
}
