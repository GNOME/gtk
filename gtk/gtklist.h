/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __GTK_LIST_H__
#define __GTK_LIST_H__


#include <gdk/gdk.h>
#include <gtk/gtkenums.h>
#include <gtk/gtkcontainer.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_LIST(obj)	       (GTK_CHECK_CAST (obj, gtk_list_get_type (), GtkList))
#define GTK_LIST_CLASS(klass)  (GTK_CHECK_CLASS_CAST (klass, gtk_list_get_type (), GtkListClass))
#define GTK_IS_LIST(obj)       (GTK_CHECK_TYPE (obj, gtk_list_get_type ()))


typedef struct _GtkList	      GtkList;
typedef struct _GtkListClass  GtkListClass;

struct _GtkList
{
  GtkContainer container;

  GList *children;
  GList *selection;

  guint32 timer;
  guint16 selection_start_pos;
  guint16 selection_end_pos;
  guint selection_mode : 2;
  guint scroll_direction : 1;
  guint have_grab : 1;
};

struct _GtkListClass
{
  GtkContainerClass parent_class;

  void (* selection_changed) (GtkList	*list);
  void (* select_child)	     (GtkList	*list,
			      GtkWidget *child);
  void (* unselect_child)    (GtkList	*list,
			      GtkWidget *child);
};


GtkType	   gtk_list_get_type		  (void);
GtkWidget* gtk_list_new			  (void);
void	   gtk_list_insert_items	  (GtkList	    *list,
					   GList	    *items,
					   gint		     position);
void	   gtk_list_append_items	  (GtkList	    *list,
					   GList	    *items);
void	   gtk_list_prepend_items	  (GtkList	    *list,
					   GList	    *items);
void	   gtk_list_remove_items	  (GtkList	    *list,
					   GList	    *items);
void	   gtk_list_remove_items_no_unref (GtkList	    *list,
					   GList	    *items);
void	   gtk_list_clear_items		  (GtkList	    *list,
					   gint		     start,
					   gint		     end);
void	   gtk_list_select_item		  (GtkList	    *list,
					   gint		     item);
void	   gtk_list_unselect_item	  (GtkList	    *list,
					   gint		     item);
void	   gtk_list_select_child	  (GtkList	    *list,
					   GtkWidget	    *child);
void	   gtk_list_unselect_child	  (GtkList	    *list,
					   GtkWidget	    *child);
gint	   gtk_list_child_position	  (GtkList	    *list,
					   GtkWidget	    *child);
void	   gtk_list_set_selection_mode	  (GtkList	    *list,
					   GtkSelectionMode  mode);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_LIST_H__ */
