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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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
  GList *first_tab;
  GList *focus_tab;

  GtkWidget *menu;
  GdkWindow *panel;

  guint32 timer;

  gint16 tab_border;

  guint show_tabs : 1;
  guint show_border : 1;
  guint tab_pos : 2;
  guint scrollable : 1;
  guint in_child : 2;
  guint click_child : 2;
  guint button : 2;
  guint need_timer : 1;
};

struct _GtkNotebookClass
{
  GtkContainerClass parent_class;

  void (* switch_page)       (GtkNotebook *notebook,
                              GtkNotebookPage *page,
			      gint page_num);
};

struct _GtkNotebookPage
{
  GtkWidget *child;
  GtkWidget *tab_label;
  GtkWidget *menu_label;
  guint default_menu : 1;
  guint default_tab  : 1;
  GtkRequisition requisition;
  GtkAllocation allocation;
};


guint      gtk_notebook_get_type        (void);
GtkWidget* gtk_notebook_new             (void);
void       gtk_notebook_append_page       (GtkNotebook      *notebook,
				           GtkWidget        *child,
				           GtkWidget        *tab_label);
void       gtk_notebook_append_page_menu  (GtkNotebook      *notebook,
					   GtkWidget        *child,
					   GtkWidget        *tab_label,
					   GtkWidget        *menu_label);
void       gtk_notebook_prepend_page      (GtkNotebook      *notebook,
				           GtkWidget        *child,
				           GtkWidget        *tab_label);
void       gtk_notebook_prepend_page_menu (GtkNotebook      *notebook,
					   GtkWidget        *child,
					   GtkWidget        *tab_label,
					   GtkWidget        *menu_label);
void       gtk_notebook_insert_page       (GtkNotebook      *notebook,
				           GtkWidget        *child,
				           GtkWidget        *tab_label,
				           gint              position);
void       gtk_notebook_insert_page_menu  (GtkNotebook      *notebook,
				           GtkWidget        *child,
				           GtkWidget        *tab_label,
					   GtkWidget        *menu_label,
				           gint              position);
void       gtk_notebook_remove_page       (GtkNotebook      *notebook,
				           gint              page_num);
gint       gtk_notebook_current_page      (GtkNotebook      *notebook);
void       gtk_notebook_set_page          (GtkNotebook      *notebook,
				           gint              page_num);
void       gtk_notebook_next_page         (GtkNotebook      *notebook);
void       gtk_notebook_prev_page         (GtkNotebook      *notebook);
void       gtk_notebook_set_tab_pos       (GtkNotebook      *notebook,
					   GtkPositionType   pos);
void       gtk_notebook_set_show_tabs     (GtkNotebook      *notebook,
					   gint              show_tabs);
void       gtk_notebook_set_show_border   (GtkNotebook      *notebook,
					   gint              show_border);
void       gtk_notebook_set_scrollable    (GtkNotebook      *notebook,
					   gint              scrollable);
void       gtk_notebook_set_tab_border    (GtkNotebook      *notebook,
					   gint              border_width);
void       gtk_notebook_popup_enable      (GtkNotebook      *notebook);
void       gtk_notebook_popup_disable     (GtkNotebook      *notebook);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_NOTEBOOK_H__ */
