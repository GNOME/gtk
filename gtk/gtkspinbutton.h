/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GtkSpinButton widget for GTK+
 * Copyright (C) 1998 Lars Hamann and Stefan Jeske
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

#ifndef __GTK_SPIN_BUTTON_H__
#define __GTK_SPIN_BUTTON_H__


#include <gdk/gdk.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkadjustment.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
  
  
#define GTK_SPIN_BUTTON(obj)	      GTK_CHECK_CAST (obj, gtk_spin_button_get_type (), GtkSpinButton)
#define GTK_SPIN_BUTTON_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_spin_button_get_type (), GtkSpinButtonClass)
#define GTK_IS_SPIN_BUTTON(obj)	      GTK_CHECK_TYPE (obj, gtk_spin_button_get_type ())
  
  
typedef enum
{
  GTK_UPDATE_ALWAYS	   = 1 << 0,
  GTK_UPDATE_IF_VALID	   = 1 << 1,
  GTK_UPDATE_SNAP_TO_TICKS = 1 << 2
} GtkSpinButtonUpdatePolicy;
  
  
typedef struct _GtkSpinButton	    GtkSpinButton;
typedef struct _GtkSpinButtonClass  GtkSpinButtonClass;


struct _GtkSpinButton
{
  GtkEntry entry;
  
  GtkAdjustment *adjustment;
  
  GdkWindow *panel;
  
  guint32 timer;
  
  gfloat climb_rate;
  gfloat timer_step;
  
  guint8 update_policy;
  
  guint in_child : 2;
  guint click_child : 2;
  guint button : 2;
  guint need_timer : 1;
  guint timer_calls : 3;
  guint digits : 3;
  guint numeric : 1;
  guint wrap : 1;
};

struct _GtkSpinButtonClass
{
  GtkEntryClass parent_class;
};

guint		gtk_spin_button_get_type	   (void);

void		gtk_spin_button_construct	   (GtkSpinButton  *spin_button,
						    GtkAdjustment  *adjustment,
						    gfloat	    climb_rate,
						    gint	    digits);

GtkWidget*	gtk_spin_button_new		   (GtkAdjustment  *adjustment,
						    gfloat	    climb_rate,
						    gint	    digits);

void		gtk_spin_button_set_adjustment	   (GtkSpinButton  *spin_button,
						    GtkAdjustment  *adjustment);

GtkAdjustment*	gtk_spin_button_get_adjustment	   (GtkSpinButton  *spin_button);

void		gtk_spin_button_set_digits	   (GtkSpinButton  *spin_button,
						    gint	    digits);

gfloat		gtk_spin_button_get_value_as_float (GtkSpinButton  *spin_button);

gint		gtk_spin_button_get_value_as_int   (GtkSpinButton  *spin_button);

void		gtk_spin_button_set_value	   (GtkSpinButton  *spin_button, 
						    gfloat	    value);

void		gtk_spin_button_set_update_policy  (GtkSpinButton  *spin_button,
						    GtkSpinButtonUpdatePolicy  policy);

void		gtk_spin_button_set_numeric	   (GtkSpinButton  *spin_button,
						    gint	    numeric);

void		gtk_spin_button_spin		   (GtkSpinButton *spin_button,
						    guint	   direction,
						    gfloat	   step);

void		gtk_spin_button_set_wrap	   (GtkSpinButton  *spin_button,
						    gint	    wrap);




#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_SPIN_BUTTON_H__ */
