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
#ifndef __GTK_TABLE_H__
#define __GTK_TABLE_H__


#include <gdk/gdk.h>
#include <gtk/gtkcontainer.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TABLE(obj)          GTK_CHECK_CAST (obj, gtk_table_get_type (), GtkTable)
#define GTK_TABLE_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_table_get_type (), GtkTableClass)
#define GTK_IS_TABLE(obj)       GTK_CHECK_TYPE (obj, gtk_table_get_type ())


typedef struct _GtkTable        GtkTable;
typedef struct _GtkTableClass   GtkTableClass;
typedef struct _GtkTableChild   GtkTableChild;
typedef struct _GtkTableRowCol  GtkTableRowCol;

struct _GtkTable
{
  GtkContainer container;

  GList *children;
  GtkTableRowCol *rows;
  GtkTableRowCol *cols;
  guint16 nrows;
  guint16 ncols;
  guint16 column_spacing;
  guint16 row_spacing;
  guint homogeneous : 1;
};

struct _GtkTableClass
{
  GtkContainerClass parent_class;
};

struct _GtkTableChild
{
  GtkWidget *widget;
  guint16 left_attach;
  guint16 right_attach;
  guint16 top_attach;
  guint16 bottom_attach;
  guint16 xpadding;
  guint16 ypadding;
  guint xexpand : 1;
  guint yexpand : 1;
  guint xshrink : 1;
  guint yshrink : 1;
  guint xfill : 1;
  guint yfill : 1;
};

struct _GtkTableRowCol
{
  guint16 requisition;
  guint16 allocation;
  guint16 spacing;
  guint need_expand : 1;
  guint need_shrink : 1;
  guint expand : 1;
  guint shrink : 1;
  guint empty : 1;
};


guint      gtk_table_get_type         (void);
GtkWidget* gtk_table_new              (gint           rows,
				       gint           columns,
				       gint           homogeneous);

void       gtk_table_attach           (GtkTable      *table,
				       GtkWidget     *child,
				       gint           left_attach,
				       gint           right_attach,
				       gint           top_attach,
				       gint           bottom_attach,
				       gint           xoptions,
				       gint           yoptions,
				       gint           xpadding,
				       gint           ypadding);
void       gtk_table_attach_defaults  (GtkTable      *table,
				       GtkWidget     *widget,
				       gint           left_attach,
				       gint           right_attach,
				       gint           top_attach,
				       gint           bottom_attach);
void       gtk_table_set_row_spacing  (GtkTable      *table,
				       gint           row,
				       gint           spacing);
void       gtk_table_set_col_spacing  (GtkTable      *table,
				       gint           column,
				       gint           spacing);
void       gtk_table_set_row_spacings (GtkTable      *table,
				       gint           spacing);
void       gtk_table_set_col_spacings (GtkTable      *table,
				       gint           spacing);
void       gtk_table_set_homogeneous  (GtkTable      *table,
				       gint           homogeneous);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_TABLE_H__ */
