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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GTK_PANED_H__
#define __GTK_PANED_H__


#include <gdk/gdk.h>
#include <gtk/gtkcontainer.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TYPE_PANED                  (gtk_paned_get_type ())
#define GTK_PANED(obj)                  (GTK_CHECK_CAST ((obj), GTK_TYPE_PANED, GtkPaned))
#define GTK_PANED_CLASS(klass)          (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_PANED, GtkPanedClass))
#define GTK_IS_PANED(obj)               (GTK_CHECK_TYPE ((obj), GTK_TYPE_PANED))
#define GTK_IS_PANED_CLASS(klass)       (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PANED))


typedef struct _GtkPaned       GtkPaned;
typedef struct _GtkPanedClass  GtkPanedClass;

struct _GtkPaned
{
  GtkContainer container;
  
  GtkWidget *child1;
  GtkWidget *child2;
  
  GdkWindow *handle;
  GdkRectangle groove_rectangle;
  GdkGC *xor_gc;

  /*< public >*/
  guint16 handle_size;
  guint16 gutter_size;

  /*< private >*/
  gint child1_size;
  gint last_allocation;
  gint min_position;
  gint max_position;
  
  guint position_set : 1;
  guint in_drag : 1;
  guint child1_shrink : 1;
  guint child1_resize : 1;
  guint child2_shrink : 1;
  guint child2_resize : 1;
  
  gint16 handle_xpos;
  gint16 handle_ypos;
};

struct _GtkPanedClass
{
  GtkContainerClass parent_class;
};


GtkType gtk_paned_get_type        (void);
void    gtk_paned_add1            (GtkPaned  *paned,
				   GtkWidget *child);
void    gtk_paned_add2            (GtkPaned  *paned,
				   GtkWidget *child);
void    gtk_paned_pack1           (GtkPaned  *paned,
				   GtkWidget *child,
				   gboolean   resize,
				   gboolean   shrink);
void    gtk_paned_pack2           (GtkPaned  *paned,
				   GtkWidget *child,
				   gboolean   resize,
				   gboolean   shrink);
void    gtk_paned_set_position    (GtkPaned  *paned,
				   gint       position);
void    gtk_paned_set_handle_size (GtkPaned *paned,
				   guint16   size);
void    gtk_paned_set_gutter_size (GtkPaned *paned,
				   guint16   size);

/* Internal function */
void    gtk_paned_compute_position (GtkPaned *paned,
				    gint      allocation,
				    gint      child1_req,
				    gint      child2_req);

gboolean _gtk_paned_is_handle_full_size (GtkPaned     *paned);
void     _gtk_paned_get_handle_rect     (GtkPaned     *paned,
					 GdkRectangle *rectangle);
gint     _gtk_paned_get_gutter_size     (GtkPaned     *paned);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_PANED_H__ */
