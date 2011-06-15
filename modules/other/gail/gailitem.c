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

#undef GTK_DISABLE_DEPRECATED

#include <gtk/gtk.h>
#include "gailitem.h"
#include <libgail-util/gailmisc.h>

static void                  gail_item_class_init      (GailItemClass *klass);
static void                  gail_item_init            (GailItem      *item);
static const gchar*          gail_item_get_name        (AtkObject     *obj);
static gint                  gail_item_get_n_children  (AtkObject     *obj);
static AtkObject*            gail_item_ref_child       (AtkObject     *obj,
                                                        gint          i);
static void                  gail_item_real_initialize (AtkObject     *obj,
                                                        gpointer      data);
static void                  gail_item_label_map_gtk   (GtkWidget     *widget,
                                                        gpointer      data);
static void                  gail_item_finalize        (GObject       *object);
static void                  gail_item_init_textutil   (GailItem      *item,
                                                        GtkWidget     *label);
static void                  gail_item_notify_label_gtk(GObject       *obj,
                                                        GParamSpec    *pspec,
                                                        gpointer      data);

/* atktext.h */ 
static void	  atk_text_interface_init	   (AtkTextIface	*iface);

static gchar*	  gail_item_get_text		   (AtkText	      *text,
                                                    gint	      start_pos,
						    gint	      end_pos);
static gunichar	  gail_item_get_character_at_offset(AtkText	      *text,
						    gint	      offset);
static gchar*     gail_item_get_text_before_offset (AtkText	      *text,
 						    gint	      offset,
						    AtkTextBoundary   boundary_type,
						    gint	      *start_offset,
						    gint	      *end_offset);
static gchar*     gail_item_get_text_at_offset     (AtkText	      *text,
 						    gint	      offset,
						    AtkTextBoundary   boundary_type,
						    gint	      *start_offset,
						    gint	      *end_offset);
static gchar*     gail_item_get_text_after_offset  (AtkText	      *text,
 						    gint	      offset,
						    AtkTextBoundary   boundary_type,
						    gint	      *start_offset,
						    gint	      *end_offset);
static gint	  gail_item_get_character_count    (AtkText	      *text);
static void       gail_item_get_character_extents  (AtkText	      *text,
						    gint 	      offset,
		                                    gint 	      *x,
                    		   	            gint 	      *y,
                                		    gint 	      *width,
                                     		    gint 	      *height,
			        		    AtkCoordType      coords);
static gint      gail_item_get_offset_at_point     (AtkText           *text,
                                                    gint              x,
                                                    gint              y,
			                            AtkCoordType      coords);
static AtkAttributeSet* gail_item_get_run_attributes 
                                                   (AtkText           *text,
              					    gint 	      offset,
                                                    gint 	      *start_offset,
					            gint	      *end_offset);
static AtkAttributeSet* gail_item_get_default_attributes
                                                   (AtkText           *text);
static GtkWidget*            get_label_from_container   (GtkWidget    *container);

G_DEFINE_TYPE_WITH_CODE (GailItem, gail_item, GAIL_TYPE_CONTAINER,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_TEXT, atk_text_interface_init))

static void
gail_item_class_init (GailItemClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);
  GailContainerClass *container_class;

  container_class = (GailContainerClass *)klass;

  gobject_class->finalize = gail_item_finalize;

  class->get_name = gail_item_get_name;
  class->get_n_children = gail_item_get_n_children;
  class->ref_child = gail_item_ref_child;
  class->initialize = gail_item_real_initialize;
  /*
   * As we report the item as having no children we are not interested
   * in add and remove signals
   */
  container_class->add_gtk = NULL;
  container_class->remove_gtk = NULL;
}

static void
gail_item_init (GailItem      *item)
{
}

static void
gail_item_real_initialize (AtkObject *obj,
                           gpointer	data)
{
  GailItem *item = GAIL_ITEM (obj);
  GtkWidget  *label;

  ATK_OBJECT_CLASS (gail_item_parent_class)->initialize (obj, data);

  item->textutil = NULL;
  item->text = NULL;

  label = get_label_from_container (GTK_WIDGET (data));
  if (GTK_IS_LABEL (label))
    {
      if (gtk_widget_get_mapped (label))
        gail_item_init_textutil (item, label);
      else
        g_signal_connect (label,
                          "map",
                          G_CALLBACK (gail_item_label_map_gtk),
                          item);
    }

  obj->role = ATK_ROLE_LIST_ITEM;
}

static void
gail_item_label_map_gtk (GtkWidget *widget,
                         gpointer data)
{
  GailItem *item;

  item = GAIL_ITEM (data);
  gail_item_init_textutil (item, widget);
}

static void
gail_item_init_textutil (GailItem  *item,
                         GtkWidget *label)
{
  const gchar *label_text;

  if (item->textutil == NULL)
    {
      item->textutil = gail_text_util_new ();
      g_signal_connect (label,
                        "notify",
                        (GCallback) gail_item_notify_label_gtk,
                        item);     
    }
  label_text = gtk_label_get_text (GTK_LABEL (label));
  gail_text_util_text_setup (item->textutil, label_text);
}

static void
gail_item_finalize (GObject *object)
{
  GailItem *item = GAIL_ITEM (object);

  if (item->textutil)
    {
      g_object_unref (item->textutil);
    }
  if (item->text)
    {
      g_free (item->text);
      item->text = NULL;
    }
  G_OBJECT_CLASS (gail_item_parent_class)->finalize (object);
}

static const gchar*
gail_item_get_name (AtkObject *obj)
{
  const gchar* name;

  g_return_val_if_fail (GAIL_IS_ITEM (obj), NULL);

  name = ATK_OBJECT_CLASS (gail_item_parent_class)->get_name (obj);
  if (name == NULL)
    {
      /*
       * Get the label child
       */
      GtkWidget *widget;
      GtkWidget *label;

      widget = GTK_ACCESSIBLE (obj)->widget;
      if (widget == NULL)
        /*
         * State is defunct
         */
        return NULL;

      label = get_label_from_container (widget);
      if (GTK_IS_LABEL (label))
	return gtk_label_get_text (GTK_LABEL(label));
      /*
       * If we have a menu item in a menu attached to a GtkOptionMenu
       * the label of the selected item is detached from the menu item
       */
      else if (GTK_IS_MENU_ITEM (widget))
        {
          GtkWidget *parent;
          GtkWidget *attach;
          GList *list;
          AtkObject *parent_obj;
          gint index;

          parent = gtk_widget_get_parent (widget);
          if (GTK_IS_MENU (parent))
            {
              attach = gtk_menu_get_attach_widget (GTK_MENU (parent)); 

              if (GTK_IS_OPTION_MENU (attach))
                {
                  label = get_label_from_container (attach);
                  if (GTK_IS_LABEL (label))
	            return gtk_label_get_text (GTK_LABEL(label));
                }
              list = gtk_container_get_children (GTK_CONTAINER (parent));
              index = g_list_index (list, widget);

              if (index < 0 || index > g_list_length (list))
                {
                  g_list_free (list);
                  return NULL;
                }
              g_list_free (list);

              parent_obj = atk_object_get_parent (gtk_widget_get_accessible (parent));
              if (GTK_IS_ACCESSIBLE (parent_obj))
                {
                  parent = GTK_ACCESSIBLE (parent_obj)->widget;
                  if (GTK_IS_COMBO_BOX (parent))
                    {
                      GtkTreeModel *model;
                      GtkTreeIter iter;
                      GailItem *item;
                      gint n_columns, i;

                      model = gtk_combo_box_get_model (GTK_COMBO_BOX (parent));                       
                      item = GAIL_ITEM (obj);
                      if (gtk_tree_model_iter_nth_child (model, &iter, NULL, index))
                        {
                          n_columns = gtk_tree_model_get_n_columns (model);
                          for (i = 0; i < n_columns; i++)
                            {
                              GValue value = { 0, };

                               gtk_tree_model_get_value (model, &iter, i, &value);
                               if (G_VALUE_HOLDS_STRING (&value))
                                 {
				   g_free (item->text);
                                   item->text =  (gchar *) g_value_dup_string (&value);
                                   g_value_unset (&value);
                                   break;
                                 }

                               g_value_unset (&value);
                            }
                        }
                      name = item->text;
                    }
                }
            }
        }
    }
  return name;
}

/*
 * We report that this object has no children
 */

static gint
gail_item_get_n_children (AtkObject* obj)
{
  return 0;
}

static AtkObject*
gail_item_ref_child (AtkObject *obj,
                     gint      i)
{
  return NULL;
}

static void
gail_item_notify_label_gtk (GObject           *obj,
                            GParamSpec        *pspec,
                            gpointer           data)
{
  AtkObject* atk_obj = ATK_OBJECT (data);
  GtkLabel *label;
  GailItem *gail_item;

  if (strcmp (pspec->name, "label") == 0)
    {
      const gchar* label_text;

      label = GTK_LABEL (obj);

      label_text = gtk_label_get_text (label);

      gail_item = GAIL_ITEM (atk_obj);
      gail_text_util_text_setup (gail_item->textutil, label_text);

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
}

/* atktext.h */

static void
atk_text_interface_init (AtkTextIface *iface)
{
  iface->get_text = gail_item_get_text;
  iface->get_character_at_offset = gail_item_get_character_at_offset;
  iface->get_text_before_offset = gail_item_get_text_before_offset;
  iface->get_text_at_offset = gail_item_get_text_at_offset;
  iface->get_text_after_offset = gail_item_get_text_after_offset;
  iface->get_character_count = gail_item_get_character_count;
  iface->get_character_extents = gail_item_get_character_extents;
  iface->get_offset_at_point = gail_item_get_offset_at_point;
  iface->get_run_attributes = gail_item_get_run_attributes;
  iface->get_default_attributes = gail_item_get_default_attributes;
}

static gchar*
gail_item_get_text (AtkText *text,
                    gint    start_pos,
                    gint    end_pos)
{
  GtkWidget *widget;
  GtkWidget *label;
  GailItem *item;
  const gchar *label_text;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  label = get_label_from_container (widget);

  if (!GTK_IS_LABEL (label))
    return NULL;

  item = GAIL_ITEM (text);
  if (!item->textutil) 
    gail_item_init_textutil (item, label);

  label_text = gtk_label_get_text (GTK_LABEL (label));

  if (label_text == NULL)
    return NULL;
  else
  {
    return gail_text_util_get_substring (item->textutil, 
                                         start_pos, end_pos);
  }
}

static gchar*
gail_item_get_text_before_offset (AtkText         *text,
				  gint            offset,
				  AtkTextBoundary boundary_type,
				  gint            *start_offset,
				  gint            *end_offset)
{
  GtkWidget *widget;
  GtkWidget *label;
  GailItem *item;
  
  widget = GTK_ACCESSIBLE (text)->widget;
  
  if (widget == NULL)
    /* State is defunct */
    return NULL;
  
  /* Get label */
  label = get_label_from_container (widget);

  if (!GTK_IS_LABEL(label))
    return NULL;

  item = GAIL_ITEM (text);
  if (!item->textutil)
    gail_item_init_textutil (item, label);

  return gail_text_util_get_text (item->textutil,
                           gtk_label_get_layout (GTK_LABEL (label)), GAIL_BEFORE_OFFSET, 
                           boundary_type, offset, start_offset, end_offset); 
}

static gchar*
gail_item_get_text_at_offset (AtkText         *text,
			      gint            offset,
			      AtkTextBoundary boundary_type,
 			      gint            *start_offset,
			      gint            *end_offset)
{
  GtkWidget *widget;
  GtkWidget *label;
  GailItem *item;
 
  widget = GTK_ACCESSIBLE (text)->widget;
  
  if (widget == NULL)
    /* State is defunct */
    return NULL;
  
  /* Get label */
  label = get_label_from_container (widget);

  if (!GTK_IS_LABEL(label))
    return NULL;

  item = GAIL_ITEM (text);
  if (!item->textutil)
    gail_item_init_textutil (item, label);

  return gail_text_util_get_text (item->textutil,
                              gtk_label_get_layout (GTK_LABEL (label)), GAIL_AT_OFFSET, 
                              boundary_type, offset, start_offset, end_offset);
}

static gchar*
gail_item_get_text_after_offset (AtkText         *text,
				 gint            offset,
				 AtkTextBoundary boundary_type,
				 gint            *start_offset,
				 gint            *end_offset)
{
  GtkWidget *widget;
  GtkWidget *label;
  GailItem *item;

  widget = GTK_ACCESSIBLE (text)->widget;
  
  if (widget == NULL)
  {
    /* State is defunct */
    return NULL;
  }
  
  /* Get label */
  label = get_label_from_container (widget);

  if (!GTK_IS_LABEL(label))
    return NULL;

  item = GAIL_ITEM (text);
  if (!item->textutil)
    gail_item_init_textutil (item, label);

  return gail_text_util_get_text (item->textutil,
                           gtk_label_get_layout (GTK_LABEL (label)), GAIL_AFTER_OFFSET, 
                           boundary_type, offset, start_offset, end_offset);
}

static gint
gail_item_get_character_count (AtkText *text)
{
  GtkWidget *widget;
  GtkWidget *label;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return 0;

  label = get_label_from_container (widget);

  if (!GTK_IS_LABEL(label))
    return 0;

  return g_utf8_strlen (gtk_label_get_text (GTK_LABEL (label)), -1);
}

static void
gail_item_get_character_extents (AtkText      *text,
				 gint         offset,
		                 gint         *x,
                    		 gint 	      *y,
                                 gint 	      *width,
                                 gint 	      *height,
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

  label = get_label_from_container (widget);

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
gail_item_get_offset_at_point (AtkText      *text,
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

  label = get_label_from_container (widget);

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
gail_item_get_run_attributes (AtkText *text,
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

  label = get_label_from_container (widget);

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
gail_item_get_default_attributes (AtkText *text)
{
  GtkWidget *widget;
  GtkWidget *label;
  AtkAttributeSet *at_set = NULL;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  label = get_label_from_container (widget);

  if (!GTK_IS_LABEL(label))
    return NULL;

  at_set = gail_misc_get_default_attributes (at_set,
                                             gtk_label_get_layout (GTK_LABEL (label)),
                                             widget);
  return at_set;
}

static gunichar 
gail_item_get_character_at_offset (AtkText *text,
                                   gint	   offset)
{
  GtkWidget *widget;
  GtkWidget *label;
  const gchar *string;
  gchar *index;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return '\0';

  label = get_label_from_container (widget);

  if (!GTK_IS_LABEL(label))
    return '\0';
  string = gtk_label_get_text (GTK_LABEL (label));
  if (offset >= g_utf8_strlen (string, -1))
    return '\0';
  index = g_utf8_offset_to_pointer (string, offset);

  return g_utf8_get_char (index);
}

static GtkWidget*
get_label_from_container (GtkWidget *container)
{
  GtkWidget *label;
  GList *children, *tmp_list;

  if (!GTK_IS_CONTAINER (container))
    return NULL;
 
  children = gtk_container_get_children (GTK_CONTAINER (container));
  label = NULL;

  for (tmp_list = children; tmp_list != NULL; tmp_list = tmp_list->next) 
    {
      if (GTK_IS_LABEL (tmp_list->data))
	{
           label = tmp_list->data;
           break;
	}
      /*
       * Get label from menu item in desktop background preferences
       * option menu. See bug #144084.
       */
      else if (GTK_IS_BOX (tmp_list->data))
        {
           label = get_label_from_container (GTK_WIDGET (tmp_list->data));
           if (label)
             break;
        }
    }
  g_list_free (children);

  return label;
}
