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
#include "gailnotebookpage.h"
#include <libgail-util/gailmisc.h>
#include "gail-private-macros.h"

static void      gail_notebook_page_class_init      (GailNotebookPageClass     *klass);
static void                  gail_notebook_page_init           (GailNotebookPage *page);
static void                  gail_notebook_page_finalize       (GObject   *object);
static void                  gail_notebook_page_label_map_gtk  (GtkWidget *widget,
                                                                gpointer  data);

static const gchar*          gail_notebook_page_get_name       (AtkObject *accessible);
static AtkObject*            gail_notebook_page_get_parent     (AtkObject *accessible);
static gint                  gail_notebook_page_get_n_children (AtkObject *accessible);
static AtkObject*            gail_notebook_page_ref_child      (AtkObject *accessible,
                                                                gint      i); 
static gint                  gail_notebook_page_get_index_in_parent
                                                               (AtkObject *accessible);
static AtkStateSet*          gail_notebook_page_ref_state_set  (AtkObject *accessible);

static gint                  gail_notebook_page_notify          (GObject   *obj,
                                                                 GParamSpec *pspec,
                                                                 gpointer   user_data);
static void                  gail_notebook_page_init_textutil   (GailNotebookPage      *notebook_page,
                                                                 GtkWidget             *label);

static void                  atk_component_interface_init      (AtkComponentIface *iface);

static AtkObject*            gail_notebook_page_ref_accessible_at_point 
                                                               (AtkComponent *component,
                                                                gint         x,
                                                                gint         y,
                                                                AtkCoordType coord_type);

static void                  gail_notebook_page_get_extents    (AtkComponent *component,
                                                                gint         *x,
                                                                gint         *y,
                                                                gint         *width,
                                                                gint         *height,
                                                                AtkCoordType coord_type);

static AtkObject*            _gail_notebook_page_get_tab_label (GailNotebookPage *page);

/* atktext.h */ 
static void	  atk_text_interface_init	   (AtkTextIface	*iface);

static gchar*	  gail_notebook_page_get_text	   (AtkText	      *text,
                                                    gint	      start_pos,
						    gint	      end_pos);
static gunichar	  gail_notebook_page_get_character_at_offset
                                                   (AtkText	      *text,
						    gint	      offset);
static gchar*     gail_notebook_page_get_text_before_offset
                                                   (AtkText	      *text,
 						    gint	      offset,
						    AtkTextBoundary   boundary_type,
						    gint	      *start_offset,
						    gint	      *end_offset);
static gchar*     gail_notebook_page_get_text_at_offset
                                                   (AtkText	      *text,
 						    gint	      offset,
						    AtkTextBoundary   boundary_type,
						    gint	      *start_offset,
						    gint	      *end_offset);
static gchar*     gail_notebook_page_get_text_after_offset
                                                   (AtkText	      *text,
 						    gint	      offset,
						    AtkTextBoundary   boundary_type,
						    gint	      *start_offset,
						    gint	      *end_offset);
static gint	  gail_notebook_page_get_character_count (AtkText	      *text);
static void       gail_notebook_page_get_character_extents
                                                   (AtkText	      *text,
						    gint 	      offset,
		                                    gint 	      *x,
                    		   	            gint 	      *y,
                                		    gint 	      *width,
                                     		    gint 	      *height,
			        		    AtkCoordType      coords);
static gint      gail_notebook_page_get_offset_at_point
                                                   (AtkText           *text,
                                                    gint              x,
                                                    gint              y,
			                            AtkCoordType      coords);
static AtkAttributeSet* gail_notebook_page_get_run_attributes 
                                                   (AtkText           *text,
              					    gint 	      offset,
                                                    gint 	      *start_offset,
					            gint	      *end_offset);
static AtkAttributeSet* gail_notebook_page_get_default_attributes
                                                   (AtkText           *text);
static GtkWidget* get_label_from_notebook_page     (GailNotebookPage  *page);
static GtkWidget* find_label_child (GtkContainer *container);

/* FIXME: not GAIL_TYPE_OBJECT? */
G_DEFINE_TYPE_WITH_CODE (GailNotebookPage, gail_notebook_page, ATK_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_COMPONENT, atk_component_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_TEXT, atk_text_interface_init))

static void
gail_notebook_page_class_init (GailNotebookPageClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);
  
  class->get_name = gail_notebook_page_get_name;
  class->get_parent = gail_notebook_page_get_parent;
  class->get_n_children = gail_notebook_page_get_n_children;
  class->ref_child = gail_notebook_page_ref_child;
  class->ref_state_set = gail_notebook_page_ref_state_set;
  class->get_index_in_parent = gail_notebook_page_get_index_in_parent;

  gobject_class->finalize = gail_notebook_page_finalize;
}

static void
gail_notebook_page_init (GailNotebookPage *page)
{
}

static gint
notify_child_added (gpointer data)
{
  GailNotebookPage *page;
  AtkObject *atk_object, *atk_parent;

  g_return_val_if_fail (GAIL_IS_NOTEBOOK_PAGE (data), FALSE);
  page = GAIL_NOTEBOOK_PAGE (data);
  atk_object = ATK_OBJECT (data);

  page->notify_child_added_id = 0;

  /* The widget page->notebook may be deleted before this handler is called */
  if (page->notebook != NULL)
    {
      atk_parent = gtk_widget_get_accessible (GTK_WIDGET (page->notebook));
      atk_object_set_parent (atk_object, atk_parent);
      g_signal_emit_by_name (atk_parent, "children_changed::add", page->index, atk_object, NULL);
    }
  
  return FALSE;
}

AtkObject*
gail_notebook_page_new (GtkNotebook *notebook, 
                        gint        pagenum)
{
  GObject *object;
  AtkObject *atk_object;
  GailNotebookPage *page;
  GtkWidget *child;
  GtkWidget *label;
  GList *list;
  
  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), NULL);

  child = gtk_notebook_get_nth_page (notebook, pagenum);

  if (!child)
    return NULL;

  object = g_object_new (GAIL_TYPE_NOTEBOOK_PAGE, NULL);
  g_return_val_if_fail (object != NULL, NULL);

  page = GAIL_NOTEBOOK_PAGE (object);
  page->notebook = notebook;
  g_object_add_weak_pointer (G_OBJECT (page->notebook), (gpointer *)&page->notebook);
  page->index = pagenum;
  list = g_list_nth (notebook->children, pagenum);
  page->page = list->data;
  page->textutil = NULL;
  
  atk_object = ATK_OBJECT (page);
  atk_object->role = ATK_ROLE_PAGE_TAB;
  atk_object->layer = ATK_LAYER_WIDGET;

  page->notify_child_added_id = gdk_threads_add_idle (notify_child_added, atk_object);
  /*
   * We get notified of changes to the label
   */
  label = get_label_from_notebook_page (page);
  if (GTK_IS_LABEL (label))
    {
      if (gtk_widget_get_mapped (label))
        gail_notebook_page_init_textutil (page, label);
      else
        g_signal_connect (label,
                          "map",
                          G_CALLBACK (gail_notebook_page_label_map_gtk),
                          page);
    }

  return atk_object;
}

static void
gail_notebook_page_label_map_gtk (GtkWidget *widget,
                                  gpointer data)
{
  GailNotebookPage *page;

  page = GAIL_NOTEBOOK_PAGE (data);
  gail_notebook_page_init_textutil (page, widget);
}

static void
gail_notebook_page_init_textutil (GailNotebookPage *page,
                                  GtkWidget        *label)
{
  const gchar *label_text;

  if (page->textutil == NULL)
    {
      page->textutil = gail_text_util_new ();
      g_signal_connect (label,
                        "notify",
                        (GCallback) gail_notebook_page_notify,
                        page);     
    }
  label_text = gtk_label_get_text (GTK_LABEL (label));
  gail_text_util_text_setup (page->textutil, label_text);
}

static gint
gail_notebook_page_notify (GObject    *obj, 
                           GParamSpec *pspec,
                           gpointer   user_data)
{
  AtkObject *atk_obj = ATK_OBJECT (user_data);
  GtkLabel *label;
  GailNotebookPage *page;

  if (strcmp (pspec->name, "label") == 0)
    {
      const gchar* label_text;

      label = GTK_LABEL (obj);

      label_text = gtk_label_get_text (label);

      page = GAIL_NOTEBOOK_PAGE (atk_obj);
      gail_text_util_text_setup (page->textutil, label_text);

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
gail_notebook_page_finalize (GObject *object)
{
  GailNotebookPage *page = GAIL_NOTEBOOK_PAGE (object);

  if (page->notebook)
    g_object_remove_weak_pointer (G_OBJECT (page->notebook), (gpointer *)&page->notebook);

  if (page->textutil)
    g_object_unref (page->textutil);

  if (page->notify_child_added_id)
    g_source_remove (page->notify_child_added_id);

  G_OBJECT_CLASS (gail_notebook_page_parent_class)->finalize (object);
}

static const gchar*
gail_notebook_page_get_name (AtkObject *accessible)
{
  g_return_val_if_fail (GAIL_IS_NOTEBOOK_PAGE (accessible), NULL);
  
  if (accessible->name != NULL)
    return accessible->name;
  else
    {
      GtkWidget *label;

      label = get_label_from_notebook_page (GAIL_NOTEBOOK_PAGE (accessible));
      if (GTK_IS_LABEL (label))
        return gtk_label_get_text (GTK_LABEL (label));
      else
        return NULL;
    }
}

static AtkObject*
gail_notebook_page_get_parent (AtkObject *accessible)
{
  GailNotebookPage *page;
  
  g_return_val_if_fail (GAIL_IS_NOTEBOOK_PAGE (accessible), NULL);
  
  page = GAIL_NOTEBOOK_PAGE (accessible);

  if (!page->notebook)
    return NULL;

  return gtk_widget_get_accessible (GTK_WIDGET (page->notebook));
}

static gint
gail_notebook_page_get_n_children (AtkObject *accessible)
{
  /* Notebook page has only one child */
  g_return_val_if_fail (GAIL_IS_NOTEBOOK_PAGE (accessible), 0);

  return 1;
}

static AtkObject*
gail_notebook_page_ref_child (AtkObject *accessible,
                              gint i)
{
  GtkWidget *child;
  AtkObject *child_obj;
  GailNotebookPage *page = NULL;
   
  g_return_val_if_fail (GAIL_IS_NOTEBOOK_PAGE (accessible), NULL);
  if (i != 0)
    return NULL;
   
  page = GAIL_NOTEBOOK_PAGE (accessible);
  if (!page->notebook)
    return NULL;

  child = gtk_notebook_get_nth_page (page->notebook, page->index);
  gail_return_val_if_fail (GTK_IS_WIDGET (child), NULL);
   
  child_obj = gtk_widget_get_accessible (child);
  g_object_ref (child_obj);
  return child_obj;
}

static gint
gail_notebook_page_get_index_in_parent (AtkObject *accessible)
{
  GailNotebookPage *page;

  g_return_val_if_fail (GAIL_IS_NOTEBOOK_PAGE (accessible), -1);
  page = GAIL_NOTEBOOK_PAGE (accessible);

  return page->index;
}

static AtkStateSet*
gail_notebook_page_ref_state_set (AtkObject *accessible)
{
  AtkStateSet *state_set, *label_state_set, *merged_state_set;
  AtkObject *atk_label;

  g_return_val_if_fail (GAIL_NOTEBOOK_PAGE (accessible), NULL);

  state_set = ATK_OBJECT_CLASS (gail_notebook_page_parent_class)->ref_state_set (accessible);

  atk_label = _gail_notebook_page_get_tab_label (GAIL_NOTEBOOK_PAGE (accessible));
  if (atk_label)
    {
      label_state_set = atk_object_ref_state_set (atk_label);
      merged_state_set = atk_state_set_or_sets (state_set, label_state_set);
      g_object_unref (label_state_set);
      g_object_unref (state_set);
    }
  else
    {
      AtkObject *child;

      child = atk_object_ref_accessible_child (accessible, 0);
      gail_return_val_if_fail (child, state_set);

      merged_state_set = state_set;
      state_set = atk_object_ref_state_set (child);
      if (atk_state_set_contains_state (state_set, ATK_STATE_VISIBLE))
        {
          atk_state_set_add_state (merged_state_set, ATK_STATE_VISIBLE);
          if (atk_state_set_contains_state (state_set, ATK_STATE_ENABLED))
              atk_state_set_add_state (merged_state_set, ATK_STATE_ENABLED);
          if (atk_state_set_contains_state (state_set, ATK_STATE_SHOWING))
              atk_state_set_add_state (merged_state_set, ATK_STATE_SHOWING);

        } 
      g_object_unref (state_set);
      g_object_unref (child);
    }
  return merged_state_set;
}


static void
atk_component_interface_init (AtkComponentIface *iface)
{
  /*
   * We use the default implementations for contains, get_position, get_size
   */
  iface->ref_accessible_at_point = gail_notebook_page_ref_accessible_at_point;
  iface->get_extents = gail_notebook_page_get_extents;
}

static AtkObject*
gail_notebook_page_ref_accessible_at_point (AtkComponent *component,
                                            gint         x,
                                            gint         y,
                                            AtkCoordType coord_type)
{
  /*
   * There is only one child so we return it.
   */
  AtkObject* child;

  g_return_val_if_fail (ATK_IS_OBJECT (component), NULL);

  child = atk_object_ref_accessible_child (ATK_OBJECT (component), 0);
  return child;
}

static void
gail_notebook_page_get_extents (AtkComponent *component,
                                gint         *x,
                                gint         *y,
                                gint         *width,
                                gint         *height,
                                AtkCoordType coord_type)
{
  AtkObject *atk_label;

  g_return_if_fail (GAIL_IS_NOTEBOOK_PAGE (component));

  atk_label = _gail_notebook_page_get_tab_label (GAIL_NOTEBOOK_PAGE (component));

  if (!atk_label)
    {
      AtkObject *child;

      *width = 0;
      *height = 0;

      child = atk_object_ref_accessible_child (ATK_OBJECT (component), 0);
      gail_return_if_fail (child);

      atk_component_get_position (ATK_COMPONENT (child), x, y, coord_type);
      g_object_unref (child);
    }
  else
    {
      atk_component_get_extents (ATK_COMPONENT (atk_label), 
                                 x, y, width, height, coord_type);
    }
  return; 
}

static AtkObject*
_gail_notebook_page_get_tab_label (GailNotebookPage *page)
{
  GtkWidget *label;

  label = get_label_from_notebook_page (page);
  if (label)
    return gtk_widget_get_accessible (label);
  else
    return NULL;
}

/* atktext.h */

static void
atk_text_interface_init (AtkTextIface *iface)
{
  iface->get_text = gail_notebook_page_get_text;
  iface->get_character_at_offset = gail_notebook_page_get_character_at_offset;
  iface->get_text_before_offset = gail_notebook_page_get_text_before_offset;
  iface->get_text_at_offset = gail_notebook_page_get_text_at_offset;
  iface->get_text_after_offset = gail_notebook_page_get_text_after_offset;
  iface->get_character_count = gail_notebook_page_get_character_count;
  iface->get_character_extents = gail_notebook_page_get_character_extents;
  iface->get_offset_at_point = gail_notebook_page_get_offset_at_point;
  iface->get_run_attributes = gail_notebook_page_get_run_attributes;
  iface->get_default_attributes = gail_notebook_page_get_default_attributes;
}

static gchar*
gail_notebook_page_get_text (AtkText *text,
                             gint    start_pos,
                             gint    end_pos)
{
  GtkWidget *label;
  GailNotebookPage *notebook_page;
  const gchar *label_text;

  notebook_page = GAIL_NOTEBOOK_PAGE (text);
  label = get_label_from_notebook_page (notebook_page);

  if (!GTK_IS_LABEL (label))
    return NULL;

  if (!notebook_page->textutil) 
    gail_notebook_page_init_textutil (notebook_page, label);

  label_text = gtk_label_get_text (GTK_LABEL (label));

  if (label_text == NULL)
    return NULL;
  else
  {
    return gail_text_util_get_substring (notebook_page->textutil, 
                                         start_pos, end_pos);
  }
}

static gchar*
gail_notebook_page_get_text_before_offset (AtkText         *text,
     				           gint            offset,
				           AtkTextBoundary boundary_type,
				           gint            *start_offset,
				           gint            *end_offset)
{
  GtkWidget *label;
  GailNotebookPage *notebook_page;
  
  notebook_page = GAIL_NOTEBOOK_PAGE (text);
  label = get_label_from_notebook_page (notebook_page);

  if (!GTK_IS_LABEL(label))
    return NULL;

  if (!notebook_page->textutil)
    gail_notebook_page_init_textutil (notebook_page, label);

  return gail_text_util_get_text (notebook_page->textutil,
                           gtk_label_get_layout (GTK_LABEL (label)), GAIL_BEFORE_OFFSET, 
                           boundary_type, offset, start_offset, end_offset); 
}

static gchar*
gail_notebook_page_get_text_at_offset (AtkText         *text,
			               gint            offset,
			               AtkTextBoundary boundary_type,
 			               gint            *start_offset,
			               gint            *end_offset)
{
  GtkWidget *label;
  GailNotebookPage *notebook_page;
 
  notebook_page = GAIL_NOTEBOOK_PAGE (text);
  label = get_label_from_notebook_page (notebook_page);

  if (!GTK_IS_LABEL(label))
    return NULL;

  if (!notebook_page->textutil)
    gail_notebook_page_init_textutil (notebook_page, label);

  return gail_text_util_get_text (notebook_page->textutil,
                              gtk_label_get_layout (GTK_LABEL (label)), GAIL_AT_OFFSET, 
                              boundary_type, offset, start_offset, end_offset);
}

static gchar*
gail_notebook_page_get_text_after_offset (AtkText         *text,
				          gint            offset,
				          AtkTextBoundary boundary_type,
				          gint            *start_offset,
				          gint            *end_offset)
{
  GtkWidget *label;
  GailNotebookPage *notebook_page;

  notebook_page = GAIL_NOTEBOOK_PAGE (text);
  label = get_label_from_notebook_page (notebook_page);

  if (!GTK_IS_LABEL(label))
    return NULL;

  if (!notebook_page->textutil)
    gail_notebook_page_init_textutil (notebook_page, label);

  return gail_text_util_get_text (notebook_page->textutil,
                           gtk_label_get_layout (GTK_LABEL (label)), GAIL_AFTER_OFFSET, 
                           boundary_type, offset, start_offset, end_offset);
}

static gint
gail_notebook_page_get_character_count (AtkText *text)
{
  GtkWidget *label;
  GailNotebookPage *notebook_page;

  notebook_page = GAIL_NOTEBOOK_PAGE (text);
  label = get_label_from_notebook_page (notebook_page);

  if (!GTK_IS_LABEL(label))
    return 0;

  return g_utf8_strlen (gtk_label_get_text (GTK_LABEL (label)), -1);
}

static void
gail_notebook_page_get_character_extents (AtkText      *text,
				          gint         offset,
		                          gint         *x,
                    		          gint         *y,
                                          gint 	       *width,
                                          gint 	       *height,
			                  AtkCoordType coords)
{
  GtkWidget *label;
  GailNotebookPage *notebook_page;
  PangoRectangle char_rect;
  gint index, x_layout, y_layout;
  const gchar *label_text;
 
  notebook_page = GAIL_NOTEBOOK_PAGE (text);
  label = get_label_from_notebook_page (notebook_page);

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
gail_notebook_page_get_offset_at_point (AtkText      *text,
                                        gint         x,
                                        gint         y,
	          		        AtkCoordType coords)
{ 
  GtkWidget *label;
  GailNotebookPage *notebook_page;
  gint index, x_layout, y_layout;
  const gchar *label_text;

  notebook_page = GAIL_NOTEBOOK_PAGE (text);
  label = get_label_from_notebook_page (notebook_page);

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
gail_notebook_page_get_run_attributes (AtkText *text,
                                       gint    offset,
                                       gint    *start_offset,
	                               gint    *end_offset)
{
  GtkWidget *label;
  GailNotebookPage *notebook_page;
  AtkAttributeSet *at_set = NULL;
  GtkJustification justify;
  GtkTextDirection dir;

  notebook_page = GAIL_NOTEBOOK_PAGE (text);
  label = get_label_from_notebook_page (notebook_page);

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
gail_notebook_page_get_default_attributes (AtkText *text)
{
  GtkWidget *label;
  GailNotebookPage *notebook_page;
  AtkAttributeSet *at_set = NULL;

  notebook_page = GAIL_NOTEBOOK_PAGE (text);
  label = get_label_from_notebook_page (notebook_page);

  if (!GTK_IS_LABEL(label))
    return NULL;

  at_set = gail_misc_get_default_attributes (at_set,
                                             gtk_label_get_layout (GTK_LABEL (label)),
                                             label);
  return at_set;
}

static gunichar 
gail_notebook_page_get_character_at_offset (AtkText *text,
                                            gint    offset)
{
  GtkWidget *label;
  GailNotebookPage *notebook_page;
  const gchar *string;
  gchar *index;

  notebook_page = GAIL_NOTEBOOK_PAGE (text);
  label = get_label_from_notebook_page (notebook_page);

  if (!GTK_IS_LABEL(label))
    return '\0';
  string = gtk_label_get_text (GTK_LABEL (label));
  if (offset >= g_utf8_strlen (string, -1))
    return '\0';
  index = g_utf8_offset_to_pointer (string, offset);

  return g_utf8_get_char (index);
}

static GtkWidget*
get_label_from_notebook_page (GailNotebookPage *page)
{
  GtkWidget *child;
  GtkNotebook *notebook;

  notebook = page->notebook;
  if (!notebook)
    return NULL;

  if (!gtk_notebook_get_show_tabs (notebook))
    return NULL;

  child = gtk_notebook_get_nth_page (notebook, page->index);
  if (child == NULL) return NULL;
  g_return_val_if_fail (GTK_IS_WIDGET (child), NULL);

  child = gtk_notebook_get_tab_label (notebook, child);

  if (GTK_IS_LABEL (child))
    return child;

  if (GTK_IS_CONTAINER (child))
    child = find_label_child (GTK_CONTAINER (child));

  return child;
}

static GtkWidget*
find_label_child (GtkContainer *container)
{
  GList *children, *tmp_list;
  GtkWidget *child;

  children = gtk_container_get_children (container);

  child = NULL;
  for (tmp_list = children; tmp_list != NULL; tmp_list = tmp_list->next)
    {
      if (GTK_IS_LABEL (tmp_list->data))
        {
          child = GTK_WIDGET (tmp_list->data);
          break;
        }
      else if (GTK_IS_CONTAINER (tmp_list->data))
        {
          child = find_label_child (GTK_CONTAINER (tmp_list->data));
          if (child)
            break;
        }
    }
  g_list_free (children);
  return child;
}
