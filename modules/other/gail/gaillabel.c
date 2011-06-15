/* GAIL - The GNOME Accessibility Enabling Library
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
#include "gaillabel.h"
#include "gailwindow.h"
#include <libgail-util/gailmisc.h>

static void       gail_label_class_init            (GailLabelClass    *klass);
static void       gail_label_init                  (GailLabel         *label);
static void	  gail_label_real_initialize	   (AtkObject 	      *obj,
                                                    gpointer	      data);
static void	  gail_label_real_notify_gtk	   (GObject	      *obj,
                                                    GParamSpec	      *pspec);
static void       gail_label_map_gtk               (GtkWidget         *widget,
                                                    gpointer          data);
static void       gail_label_init_text_util        (GailLabel         *gail_label,
                                                    GtkWidget         *widget);
static void       gail_label_finalize              (GObject           *object);

static void       atk_text_interface_init          (AtkTextIface      *iface);

/* atkobject.h */

static const gchar*          gail_label_get_name         (AtkObject         *accessible);
static AtkStateSet*          gail_label_ref_state_set	 (AtkObject	    *accessible);
static AtkRelationSet*       gail_label_ref_relation_set (AtkObject         *accessible);

/* atktext.h */

static gchar*	  gail_label_get_text		   (AtkText	      *text,
                                                    gint	      start_pos,
						    gint	      end_pos);
static gunichar	  gail_label_get_character_at_offset(AtkText	      *text,
						    gint	      offset);
static gchar*     gail_label_get_text_before_offset(AtkText	      *text,
 						    gint	      offset,
						    AtkTextBoundary   boundary_type,
						    gint	      *start_offset,
						    gint	      *end_offset);
static gchar*     gail_label_get_text_at_offset    (AtkText	      *text,
 						    gint	      offset,
						    AtkTextBoundary   boundary_type,
						    gint	      *start_offset,
						    gint	      *end_offset);
static gchar*     gail_label_get_text_after_offset    (AtkText	      *text,
 						    gint	      offset,
						    AtkTextBoundary   boundary_type,
						    gint	      *start_offset,
						    gint	      *end_offset);
static gint	  gail_label_get_character_count   (AtkText	      *text);
static gint	  gail_label_get_caret_offset	   (AtkText	      *text);
static gboolean	  gail_label_set_caret_offset	   (AtkText	      *text,
                                                    gint	      offset);
static gint	  gail_label_get_n_selections	   (AtkText	      *text);
static gchar*	  gail_label_get_selection	   (AtkText	      *text,
                                                    gint	      selection_num,
                                                    gint	      *start_offset,
                                                    gint	      *end_offset);
static gboolean	  gail_label_add_selection	   (AtkText	      *text,
                                                    gint	      start_offset,
                                                    gint	      end_offset);
static gboolean	  gail_label_remove_selection	   (AtkText	      *text,
                                                    gint	      selection_num);
static gboolean	  gail_label_set_selection	   (AtkText	      *text,
                                                    gint	      selection_num,
                                                    gint	      start_offset,
						    gint	      end_offset);
static void gail_label_get_character_extents       (AtkText	      *text,
						    gint 	      offset,
		                                    gint 	      *x,
                    		   	            gint 	      *y,
                                		    gint 	      *width,
                                     		    gint 	      *height,
			        		    AtkCoordType      coords);
static gint gail_label_get_offset_at_point         (AtkText           *text,
                                                    gint              x,
                                                    gint              y,
			                            AtkCoordType      coords);
static AtkAttributeSet* gail_label_get_run_attributes 
                                                   (AtkText           *text,
              					    gint 	      offset,
                                                    gint 	      *start_offset,
					            gint	      *end_offset);
static AtkAttributeSet* gail_label_get_default_attributes
                                                   (AtkText           *text);

G_DEFINE_TYPE_WITH_CODE (GailLabel, gail_label, GAIL_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_TEXT, atk_text_interface_init))

static void
gail_label_class_init (GailLabelClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass  *class = ATK_OBJECT_CLASS (klass);
  GailWidgetClass *widget_class;

  gobject_class->finalize = gail_label_finalize;

  widget_class = (GailWidgetClass*)klass;
  widget_class->notify_gtk = gail_label_real_notify_gtk;

  class->get_name = gail_label_get_name;
  class->ref_state_set = gail_label_ref_state_set;
  class->ref_relation_set = gail_label_ref_relation_set;
  class->initialize = gail_label_real_initialize;
}

static void
gail_label_init (GailLabel *label)
{
}

static void
gail_label_real_initialize (AtkObject *obj,
                            gpointer  data)
{
  GtkWidget  *widget;
  GailLabel *gail_label;

  ATK_OBJECT_CLASS (gail_label_parent_class)->initialize (obj, data);
  
  gail_label = GAIL_LABEL (obj);

  gail_label->window_create_handler = 0;
  gail_label->has_top_level = FALSE;
  gail_label->cursor_position = 0;
  gail_label->selection_bound = 0;
  gail_label->textutil = NULL;
  gail_label->label_length = 0;
  
  widget = GTK_WIDGET (data);

  if (gtk_widget_get_mapped (widget))
    gail_label_init_text_util (gail_label, widget);
  else
    g_signal_connect (widget,
                      "map",
                      G_CALLBACK (gail_label_map_gtk),
                      gail_label);

  /* 
   * Check whether ancestor of GtkLabel is a GtkButton and if so
   * set accessible parent for GailLabel
   */
  while (widget != NULL)
    {
      widget = gtk_widget_get_parent (widget);
      if (GTK_IS_BUTTON (widget))
        {
          atk_object_set_parent (obj, gtk_widget_get_accessible (widget));
          break;
        }
    }

  if (GTK_IS_ACCEL_LABEL (widget))
    obj->role = ATK_ROLE_ACCEL_LABEL;
  else
    obj->role = ATK_ROLE_LABEL;
}

static void
gail_label_map_gtk (GtkWidget *widget,
                    gpointer data)
{
  GailLabel *gail_label;

  gail_label = GAIL_LABEL (data);
  gail_label_init_text_util (gail_label, widget);
}

static void
gail_label_init_text_util (GailLabel *gail_label,
                           GtkWidget *widget)
{
  GtkLabel *label;
  const gchar *label_text;

  if (gail_label->textutil == NULL)
    gail_label->textutil = gail_text_util_new ();

  label = GTK_LABEL (widget);
  label_text = gtk_label_get_text (label);
  gail_text_util_text_setup (gail_label->textutil, label_text);
  
  if (label_text == NULL)
    gail_label->label_length = 0;
  else
    gail_label->label_length = g_utf8_strlen (label_text, -1);
}

static void
notify_name_change (AtkObject *atk_obj)
{
  GtkLabel *label;
  GailLabel *gail_label;
  GtkWidget *widget;
  GObject *gail_obj;

  widget = GTK_ACCESSIBLE (atk_obj)->widget;
  if (widget == NULL)
    /*
     * State is defunct
     */
    return;

  gail_obj = G_OBJECT (atk_obj);
  label = GTK_LABEL (widget);
  gail_label = GAIL_LABEL (atk_obj);

  if (gail_label->textutil == NULL)
    return;

  /*
   * Check whether the label has actually changed before emitting
   * notification.
   */
  if (gail_label->textutil->buffer) 
    {
      GtkTextIter start, end;
      const char *new_label;
      char *old_label;
      int same;   

      gtk_text_buffer_get_start_iter (gail_label->textutil->buffer, &start);
      gtk_text_buffer_get_end_iter (gail_label->textutil->buffer, &end);
      old_label = gtk_text_buffer_get_text (gail_label->textutil->buffer, &start, &end, FALSE);
      new_label = gtk_label_get_text (label);
      same = strcmp (new_label, old_label);
      g_free (old_label);
      if (same == 0)
        return;
    }
   
  /* Create a delete text and an insert text signal */
 
  g_signal_emit_by_name (gail_obj, "text_changed::delete", 0, 
                         gail_label->label_length);

  gail_label_init_text_util (gail_label, widget);

  g_signal_emit_by_name (gail_obj, "text_changed::insert", 0, 
                         gail_label->label_length);

  if (atk_obj->name == NULL)
    /*
     * The label has changed so notify a change in accessible-name
     */
    g_object_notify (gail_obj, "accessible-name");

  g_signal_emit_by_name (gail_obj, "visible_data_changed");
}

static void
window_created (GObject *obj,
                gpointer data)
{
  g_return_if_fail (GAIL_LABEL (data));

  notify_name_change (ATK_OBJECT (data)); 
}

static void
gail_label_real_notify_gtk (GObject           *obj,
                            GParamSpec        *pspec)
{
  GtkWidget *widget = GTK_WIDGET (obj);
  AtkObject* atk_obj = gtk_widget_get_accessible (widget);
  GtkLabel *label;
  GailLabel *gail_label;
  GObject *gail_obj;
  AtkObject *top_level;
  AtkObject *temp_obj;

  gail_label = GAIL_LABEL (atk_obj);

  if (strcmp (pspec->name, "label") == 0)
    {
     /* 
      * We may get a label change for a label which is not attached to an
      * application. We wait until the toplevel window is created before
      * emitting the notification.
      *
      * This happens when [Ctrl+]Alt+Tab is pressed in metacity
      */
      if (!gail_label->has_top_level)
        {
          temp_obj = atk_obj;
          top_level = NULL;
          while (temp_obj)
            {
              top_level = temp_obj;
              temp_obj = atk_object_get_parent (top_level);
            }
          if (atk_object_get_role (top_level) != ATK_ROLE_APPLICATION)
            {
              if (gail_label->window_create_handler == 0 && 
                  GAIL_IS_WINDOW (top_level))
                gail_label->window_create_handler = g_signal_connect_after (top_level, "create", G_CALLBACK (window_created), atk_obj);
            }
          else
            gail_label->has_top_level = TRUE;
        }
      if (gail_label->has_top_level)
        notify_name_change (atk_obj);
    }
  else if (strcmp (pspec->name, "cursor-position") == 0)
    {
      gint start, end, tmp;
      gboolean text_caret_moved = FALSE;
      gboolean selection_changed = FALSE;

      gail_obj = G_OBJECT (atk_obj);
      label = GTK_LABEL (widget);

      if (gail_label->selection_bound != -1 && gail_label->selection_bound < gail_label->cursor_position)
        {
          tmp = gail_label->selection_bound;
          gail_label->selection_bound = gail_label->cursor_position;
          gail_label->cursor_position = tmp;
        }

      if (gtk_label_get_selection_bounds (label, &start, &end))
        {
          if (start != gail_label->cursor_position ||
              end != gail_label->selection_bound)
            {
              if (end != gail_label->selection_bound)
                {
                  gail_label->selection_bound = start;
                  gail_label->cursor_position = end;
                }
              else
                {
                  gail_label->selection_bound = end;
                  gail_label->cursor_position = start;
                }
              text_caret_moved = TRUE;
              if (start != end)
                selection_changed = TRUE;
            }
        }
      else 
        {
          if (gail_label->cursor_position != gail_label->selection_bound)
            selection_changed = TRUE;
          if (gtk_label_get_selectable (label))
            {
              if (gail_label->cursor_position != -1 && start != gail_label->cursor_position)
                text_caret_moved = TRUE;
              if (gail_label->selection_bound != -1 && end != gail_label->selection_bound)
                {
                  text_caret_moved = TRUE;
                  gail_label->cursor_position = end;
                  gail_label->selection_bound = start;
                }
              else
                {
                  gail_label->cursor_position = start;
                  gail_label->selection_bound = end;
                }
            }
          else
            {
              /* GtkLabel has become non selectable */

              gail_label->cursor_position = 0;
              gail_label->selection_bound = 0;
              text_caret_moved = TRUE;
            }

        }
        if (text_caret_moved)
          g_signal_emit_by_name (gail_obj, "text_caret_moved", 
                                 gail_label->cursor_position);
        if (selection_changed)
          g_signal_emit_by_name (gail_obj, "text_selection_changed");

    }
  else
    GAIL_WIDGET_CLASS (gail_label_parent_class)->notify_gtk (obj, pspec);
}

static void
gail_label_finalize (GObject            *object)
{
  GailLabel *label = GAIL_LABEL (object);

  if (label->textutil)
    g_object_unref (label->textutil);
  G_OBJECT_CLASS (gail_label_parent_class)->finalize (object);
}


/* atkobject.h */

static AtkStateSet*
gail_label_ref_state_set (AtkObject *accessible)
{
  AtkStateSet *state_set;
  GtkWidget *widget;

  state_set = ATK_OBJECT_CLASS (gail_label_parent_class)->ref_state_set (accessible);
  widget = GTK_ACCESSIBLE (accessible)->widget;

  if (widget == NULL)
    return state_set;

  atk_state_set_add_state (state_set, ATK_STATE_MULTI_LINE);

  return state_set;
}

AtkRelationSet*
gail_label_ref_relation_set (AtkObject *obj)
{
  GtkWidget *widget;
  AtkRelationSet *relation_set;

  g_return_val_if_fail (GAIL_IS_LABEL (obj), NULL);

  widget = GTK_ACCESSIBLE (obj)->widget;
  if (widget == NULL)
    /*
     * State is defunct
     */
    return NULL;

  relation_set = ATK_OBJECT_CLASS (gail_label_parent_class)->ref_relation_set (obj);

  if (!atk_relation_set_contains (relation_set, ATK_RELATION_LABEL_FOR))
    {
      /*
       * Get the mnemonic widget
       *
       * The relation set is not updated if the mnemonic widget is changed
       */
      GtkWidget *mnemonic_widget = GTK_LABEL (widget)->mnemonic_widget;

      if (mnemonic_widget)
        {
          AtkObject *accessible_array[1];
          AtkRelation* relation;

          if (!gtk_widget_get_can_focus (mnemonic_widget))
            {
            /*
             * Handle the case where a GtkFileChooserButton is specified as the 
             * mnemonic widget. use the combobox which is a child of the
             * GtkFileChooserButton as the mnemonic widget. See bug #359843.
             */
             if (GTK_IS_BOX (mnemonic_widget))
               {
                  GList *list, *tmpl;

                  list = gtk_container_get_children (GTK_CONTAINER (mnemonic_widget));
                  if (g_list_length (list) == 2)
                    {
                      tmpl = g_list_last (list);
                      if (GTK_IS_COMBO_BOX(tmpl->data))
                        {
                          mnemonic_widget = GTK_WIDGET(tmpl->data);
                        }
                    }
                  g_list_free (list);
                }
            /*
             * Handle the case where a GnomeIconEntry is specified as the 
             * mnemonic widget. use the button which is a grandchild of the
             * GnomeIconEntry as the mnemonic widget. See bug #133967.
             */
              else if (GTK_IS_BOX (mnemonic_widget))
                {
                  GList *list;

                  list = gtk_container_get_children (GTK_CONTAINER (mnemonic_widget));
                  if (g_list_length (list) == 1)
                    {
                      if (GTK_IS_ALIGNMENT (list->data))
                        {
                          GtkWidget *temp_widget;

                          temp_widget = GTK_BIN (list->data)->child;
                          if (GTK_IS_BUTTON (temp_widget))
                            mnemonic_widget = temp_widget;
                        }
                      else if (GTK_IS_HBOX (list->data))
                        {
                          GtkWidget *temp_widget;

                          temp_widget = GTK_WIDGET (list->data);
                          g_list_free (list);
                          list = gtk_container_get_children (GTK_CONTAINER (temp_widget));
                          if (GTK_IS_COMBO (list->data))
                            {
                              mnemonic_widget = GTK_WIDGET (list->data);
                            }
                        }
                    }
                  g_list_free (list);
                }
            }
          accessible_array[0] = gtk_widget_get_accessible (mnemonic_widget);
          relation = atk_relation_new (accessible_array, 1,
                                       ATK_RELATION_LABEL_FOR);
          atk_relation_set_add (relation_set, relation);
          /*
           * Unref the relation so that it is not leaked.
           */
          g_object_unref (relation);
        }
    }
  return relation_set;
}

static const gchar*
gail_label_get_name (AtkObject *accessible)
{
  const gchar *name;

  g_return_val_if_fail (GAIL_IS_LABEL (accessible), NULL);

  name = ATK_OBJECT_CLASS (gail_label_parent_class)->get_name (accessible);
  if (name != NULL)
    return name;
  else
    {
      /*
       * Get the text on the label
       */
      GtkWidget *widget;

      widget = GTK_ACCESSIBLE (accessible)->widget;
      if (widget == NULL)
        /*
         * State is defunct
         */
        return NULL;

      g_return_val_if_fail (GTK_IS_LABEL (widget), NULL);

      return gtk_label_get_text (GTK_LABEL (widget));
    }
}

/* atktext.h */

static void
atk_text_interface_init (AtkTextIface *iface)
{
  iface->get_text = gail_label_get_text;
  iface->get_character_at_offset = gail_label_get_character_at_offset;
  iface->get_text_before_offset = gail_label_get_text_before_offset;
  iface->get_text_at_offset = gail_label_get_text_at_offset;
  iface->get_text_after_offset = gail_label_get_text_after_offset;
  iface->get_character_count = gail_label_get_character_count;
  iface->get_caret_offset = gail_label_get_caret_offset;
  iface->set_caret_offset = gail_label_set_caret_offset;
  iface->get_n_selections = gail_label_get_n_selections;
  iface->get_selection = gail_label_get_selection;
  iface->add_selection = gail_label_add_selection;
  iface->remove_selection = gail_label_remove_selection;
  iface->set_selection = gail_label_set_selection;
  iface->get_character_extents = gail_label_get_character_extents;
  iface->get_offset_at_point = gail_label_get_offset_at_point;
  iface->get_run_attributes = gail_label_get_run_attributes;
  iface->get_default_attributes = gail_label_get_default_attributes;
}

static gchar*
gail_label_get_text (AtkText *text,
                     gint    start_pos,
                     gint    end_pos)
{
  GtkWidget *widget;
  GtkLabel  *label;

  const gchar *label_text;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  label = GTK_LABEL (widget);

  label_text = gtk_label_get_text (label);
 
  if (label_text == NULL)
    return NULL;
  else
  {
    if (GAIL_LABEL (text)->textutil == NULL) 
      gail_label_init_text_util (GAIL_LABEL (text), widget);
    return gail_text_util_get_substring (GAIL_LABEL(text)->textutil, 
                                         start_pos, end_pos);
  }
}

static gchar*
gail_label_get_text_before_offset (AtkText         *text,
				   gint            offset,
				   AtkTextBoundary boundary_type,
				   gint            *start_offset,
				   gint            *end_offset)
{
  GtkWidget *widget;
  GtkLabel *label;
  
  widget = GTK_ACCESSIBLE (text)->widget;
  
  if (widget == NULL)
    /* State is defunct */
    return NULL;
  
  /* Get label */
  label = GTK_LABEL (widget);

  return gail_text_util_get_text (GAIL_LABEL (text)->textutil,
                           gtk_label_get_layout (label), GAIL_BEFORE_OFFSET, 
                           boundary_type, offset, start_offset, end_offset); 
}

static gchar*
gail_label_get_text_at_offset (AtkText         *text,
			       gint            offset,
			       AtkTextBoundary boundary_type,
 			       gint            *start_offset,
			       gint            *end_offset)
{
  GtkWidget *widget;
  GtkLabel *label;
 
  widget = GTK_ACCESSIBLE (text)->widget;
  
  if (widget == NULL)
    /* State is defunct */
    return NULL;
  
  /* Get label */
  label = GTK_LABEL (widget);

  return gail_text_util_get_text (GAIL_LABEL (text)->textutil,
                              gtk_label_get_layout (label), GAIL_AT_OFFSET, 
                              boundary_type, offset, start_offset, end_offset);
}

static gchar*
gail_label_get_text_after_offset (AtkText         *text,
				  gint            offset,
				  AtkTextBoundary boundary_type,
				  gint            *start_offset,
				  gint            *end_offset)
{
  GtkWidget *widget;
  GtkLabel *label;

  widget = GTK_ACCESSIBLE (text)->widget;
  
  if (widget == NULL)
  {
    /* State is defunct */
    return NULL;
  }
  
  /* Get label */
  label = GTK_LABEL (widget);

  return gail_text_util_get_text (GAIL_LABEL (text)->textutil,
                           gtk_label_get_layout (label), GAIL_AFTER_OFFSET, 
                           boundary_type, offset, start_offset, end_offset);
}

static gint
gail_label_get_character_count (AtkText *text)
{
  GtkWidget *widget;
  GtkLabel  *label;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return 0;

  label = GTK_LABEL (widget);
  return g_utf8_strlen (gtk_label_get_text (label), -1);
}

static gint
gail_label_get_caret_offset (AtkText *text)
{
   return GAIL_LABEL (text)->cursor_position;
}

static gboolean
gail_label_set_caret_offset (AtkText *text, 
                             gint    offset)
{
  GtkWidget *widget;
  GtkLabel  *label;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return 0;

  label = GTK_LABEL (widget);

  if (gtk_label_get_selectable (label) &&
      offset >= 0 &&
      offset <= g_utf8_strlen (label->text, -1))
    {
      gtk_label_select_region (label, offset, offset);
      return TRUE;
    }
  else
    return FALSE;
}

static gint
gail_label_get_n_selections (AtkText *text)
{
  GtkWidget *widget;
  GtkLabel  *label;
  gint start, end;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return 0;

  label = GTK_LABEL (widget);

  if (!gtk_label_get_selectable (label))
     return 0;

  if (gtk_label_get_selection_bounds (label, &start, &end))
     return 1;
  else 
     return 0;
}

static gchar*
gail_label_get_selection (AtkText *text,
			  gint    selection_num,
                          gint    *start_pos,
                          gint    *end_pos)
{
  GtkWidget *widget;
  GtkLabel  *label;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  label = GTK_LABEL (widget);

 /* Only let the user get the selection if one is set, and if the
  * selection_num is 0.
  */
  if (!gtk_label_get_selectable( label) || selection_num != 0)
     return NULL;

  if (gtk_label_get_selection_bounds (label, start_pos, end_pos))
    {
      const gchar* label_text = gtk_label_get_text (label);
    
      if (label_text == NULL)
        return 0;
      else
        return gail_text_util_get_substring (GAIL_LABEL (text)->textutil, 
                                             *start_pos, *end_pos);
    }
  else 
    return NULL;
}

static gboolean
gail_label_add_selection (AtkText *text,
                          gint    start_pos,
                          gint    end_pos)
{
  GtkWidget *widget;
  GtkLabel  *label;
  gint start, end;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return FALSE;

  label = GTK_LABEL (widget);

  if (!gtk_label_get_selectable (label))
     return FALSE;

  if (! gtk_label_get_selection_bounds (label, &start, &end))
    {
      gtk_label_select_region (label, start_pos, end_pos);
      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
gail_label_remove_selection (AtkText *text,
                             gint    selection_num)
{
  GtkWidget *widget;
  GtkLabel  *label;
  gint start, end;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return FALSE;

  if (selection_num != 0)
     return FALSE;

  label = GTK_LABEL (widget);

  if (!gtk_label_get_selectable (label))
     return FALSE;

  if (gtk_label_get_selection_bounds (label, &start, &end))
    {
      gtk_label_select_region (label, 0, 0);
      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
gail_label_set_selection (AtkText *text,
			  gint	  selection_num,
                          gint    start_pos,
                          gint    end_pos)
{
  GtkWidget *widget;
  GtkLabel  *label;
  gint start, end;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return FALSE;

  if (selection_num != 0)
     return FALSE;

  label = GTK_LABEL (widget);

  if (!gtk_label_get_selectable (label))
     return FALSE;

  if (gtk_label_get_selection_bounds (label, &start, &end))
    {
      gtk_label_select_region (label, start_pos, end_pos);
      return TRUE;
    }
  else
    return FALSE;
}

static void
gail_label_get_character_extents (AtkText      *text,
				  gint         offset,
		                  gint         *x,
                    		  gint 	       *y,
                                  gint 	       *width,
                                  gint 	       *height,
			          AtkCoordType coords)
{
  GtkWidget *widget;
  GtkLabel *label;
  PangoRectangle char_rect;
  gint index, x_layout, y_layout;
 
  widget = GTK_ACCESSIBLE (text)->widget;

  if (widget == NULL)
    /* State is defunct */
    return;

  label = GTK_LABEL (widget);
  
  gtk_label_get_layout_offsets (label, &x_layout, &y_layout);
  index = g_utf8_offset_to_pointer (label->text, offset) - label->text;
  pango_layout_index_to_pos (gtk_label_get_layout (label), index, &char_rect);
  
  gail_misc_get_extents_from_pango_rectangle (widget, &char_rect, 
                    x_layout, y_layout, x, y, width, height, coords);
} 

static gint 
gail_label_get_offset_at_point (AtkText      *text,
                                gint         x,
                                gint         y,
			        AtkCoordType coords)
{ 
  GtkWidget *widget;
  GtkLabel *label;
  gint index, x_layout, y_layout;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return -1;
  label = GTK_LABEL (widget);
  
  gtk_label_get_layout_offsets (label, &x_layout, &y_layout);
  
  index = gail_misc_get_index_at_point_in_layout (widget, 
                                              gtk_label_get_layout (label), 
                                              x_layout, y_layout, x, y, coords);
  if (index == -1)
    {
      if (coords == ATK_XY_WINDOW || coords == ATK_XY_SCREEN)
        return g_utf8_strlen (label->text, -1);

      return index;  
    }
  else
    return g_utf8_pointer_to_offset (label->text, label->text + index);  
}

static AtkAttributeSet*
gail_label_get_run_attributes (AtkText        *text,
                               gint 	      offset,
                               gint 	      *start_offset,
	                       gint	      *end_offset)
{
  GtkWidget *widget;
  GtkLabel *label;
  AtkAttributeSet *at_set = NULL;
  GtkJustification justify;
  GtkTextDirection dir;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  label = GTK_LABEL (widget);
  
  /* Get values set for entire label, if any */
  justify = gtk_label_get_justify (label);
  if (justify != GTK_JUSTIFY_CENTER)
    {
      at_set = gail_misc_add_attribute (at_set, 
                                        ATK_TEXT_ATTR_JUSTIFICATION,
     g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_JUSTIFICATION, justify)));
    }
  dir = gtk_widget_get_direction (widget);
  if (dir == GTK_TEXT_DIR_RTL)
    {
      at_set = gail_misc_add_attribute (at_set, 
                                        ATK_TEXT_ATTR_DIRECTION,
     g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_DIRECTION, dir)));
    }

  at_set = gail_misc_layout_get_run_attributes (at_set,
                                                gtk_label_get_layout (label),
                                                label->text,
                                                offset,
                                                start_offset,
                                                end_offset);
  return at_set;
}

static AtkAttributeSet*
gail_label_get_default_attributes (AtkText        *text)
{
  GtkWidget *widget;
  GtkLabel *label;
  AtkAttributeSet *at_set = NULL;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  label = GTK_LABEL (widget);
  
  at_set = gail_misc_get_default_attributes (at_set,
                                             gtk_label_get_layout (label),
                                             widget);
  return at_set;
}

static gunichar 
gail_label_get_character_at_offset (AtkText	         *text,
                                    gint	         offset)
{
  GtkWidget *widget;
  GtkLabel *label;
  const gchar *string;
  gchar *index;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return '\0';

  label = GTK_LABEL (widget);
  string = gtk_label_get_text (label);
  if (offset >= g_utf8_strlen (string, -1))
    return '\0';
  index = g_utf8_offset_to_pointer (string, offset);

  return g_utf8_get_char (index);
}
