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
#ifndef __GTK_ADJUSTMENT_H__
#define __GTK_ADJUSTMENT_H__


#include <gdk/gdk.h>
#include <gtk/gtkdata.h>


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */


#define GTK_ADJUSTMENT(obj)	     (GTK_CHECK_CAST (obj, gtk_adjustment_get_type (), GtkAdjustment))
#define GTK_ADJUSTMENT_CLASS(klass)  (GTK_CHECK_CLASS_CAST (klass, gtk_adjustment_get_type (), GtkAdjustmentClass))
#define GTK_IS_ADJUSTMENT(obj)	     (GTK_CHECK_TYPE (obj, gtk_adjustment_get_type ()))


typedef struct _GtkAdjustment	    GtkAdjustment;
typedef struct _GtkAdjustmentClass  GtkAdjustmentClass;

struct _GtkAdjustment
{
  GtkData data;
  
  gfloat lower;
  gfloat upper;
  gfloat value;
  gfloat step_increment;
  gfloat page_increment;
  gfloat page_size;
};

struct _GtkAdjustmentClass
{
  GtkDataClass parent_class;
  
  void (* changed)	 (GtkAdjustment *adjustment);
  void (* value_changed) (GtkAdjustment *adjustment);
};


GtkType	   gtk_adjustment_get_type   (void);
GtkObject* gtk_adjustment_new	     (gfloat		  value,
				      gfloat		  lower,
				      gfloat		  upper,
				      gfloat		  step_increment,
				      gfloat		  page_increment,
				      gfloat		  page_size);
void	   gtk_adjustment_set_value  (GtkAdjustment	 *adjustment,
				      gfloat		  value);
void	   gtk_adjustment_clamp_page (GtkAdjustment	 *adjustment,
				      gfloat		  lower,
				      gfloat		  upper);
     




#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_ADJUSTMENT_H__ */
