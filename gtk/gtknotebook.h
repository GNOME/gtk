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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef __GTK_NOTEBOOK_H__
#define __GTK_NOTEBOOK_H__


#include <gdk/gdk.h>
#include <gtk/gtkcontainer.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_NOTEBOOK(obj)          GTK_CHECK_CAST (obj, gtk_notebook_get_type (), GtkNotebook)
#define GTK_NOTEBOOK_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_notebook_get_type (), GtkNotebookClass)
#define GTK_IS_NOTEBOOK(obj)       GTK_CHECK_TYPE (obj, gtk_notebook_get_type ())


typedef struct _GtkNotebook       GtkNotebook;
typedef struct _GtkNotebookClass  GtkNotebookClass;
typedef struct _GtkNotebookPage   GtkNotebookPage;

struct _GtkNotebook
{
  GtkContainer container;

  GtkNotebookPage *cur_page;
  GList *children;

  guint show_tabs : 1;
  guint show_border : 1;
  guint tab_pos : 2;
};

struct _GtkNotebookClass
{
  GtkContainerClass parent_class;
};

struct _GtkNotebookPage
{
  GtkWidget *child;
  GtkWidget *tab_label;
  GtkRequisition requisition;
  GtkAllocation allocation;
};


guint      gtk_notebook_get_type        (void);
GtkWidget* gtk_notebook_new             (void);
void       gtk_notebook_append_page     (GtkNotebook      *notebook,
				         GtkWidget        *child,
				         GtkWidget        *tab_label);
void       gtk_notebook_prepend_page    (GtkNotebook      *notebook,
				         GtkWidget        *child,
				         GtkWidget        *tab_label);
void       gtk_notebook_insert_page     (GtkNotebook      *notebook,
				         GtkWidget        *child,
				         GtkWidget        *tab_label,
				         gint              position);
void       gtk_notebook_remove_page     (GtkNotebook      *notebook,
				         gint              page_num);
gint       gtk_notebook_current_page    (GtkNotebook      *notebook);
void       gtk_notebook_set_page        (GtkNotebook      *notebook,
				         gint              page_num);
void       gtk_notebook_next_page       (GtkNotebook      *notebook);
void       gtk_notebook_prev_page       (GtkNotebook      *notebook);
void       gtk_notebook_set_tab_pos     (GtkNotebook      *notebook,
					 GtkPositionType   pos);
void       gtk_notebook_set_show_tabs   (GtkNotebook      *notebook,
					 gint              show_tabs);
void       gtk_notebook_set_show_border (GtkNotebook      *notebook,
					 gint              show_border);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_NOTEBOOK_H__ */
