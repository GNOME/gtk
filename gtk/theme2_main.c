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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "gtkthemes.h"
#include "gtkprivate.h"
#include "gtkbutton.h"
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtksignal.h"
#include <gdk_imlib.h>
#include "theme_btn1.xpm"
#include "theme_btn2.xpm"
#include "theme_btn3.xpm"
#include "theme_led_off1.xpm"
#include "theme_led_off2.xpm"
#include "theme_led_on1.xpm"
#include "theme_led_on2.xpm"

struct _imgs
{
   GdkImlibImage *im1;
   GdkImlibImage *im2;
   GdkImlibImage *im3;
   GdkImlibImage *im4;
   GdkImlibImage *im5;
   GdkImlibImage *im6;
   GdkImlibImage *im7;
};

/* Theme functions to export */
void theme_init               (int     *argc,
			       char ***argv);
void theme_exit               (void);

/* internals */

void 
theme_init               (int     *argc,
			  char ***argv)
{
   struct _imgs *imgs;
   GdkImlibBorder bd;
   
   printf("Theme2 Init\n");
   imgs=th_dat.data=malloc(sizeof(struct _imgs));
   gdk_imlib_init();
   bd.left=4;
   bd.right=4;
   bd.top=4;
   bd.bottom=4;
   imgs->im1=gdk_imlib_create_image_from_xpm_data(theme_btn1_xpm);
   gdk_imlib_set_image_border(imgs->im1,&bd);
   imgs->im2=gdk_imlib_create_image_from_xpm_data(theme_btn2_xpm);
   gdk_imlib_set_image_border(imgs->im2,&bd);
   imgs->im3=gdk_imlib_create_image_from_xpm_data(theme_btn3_xpm);
   gdk_imlib_set_image_border(imgs->im3,&bd);
   imgs->im4=gdk_imlib_create_image_from_xpm_data(theme_led_off1_xpm);
   imgs->im5=gdk_imlib_create_image_from_xpm_data(theme_led_off2_xpm);
   imgs->im6=gdk_imlib_create_image_from_xpm_data(theme_led_on1_xpm);
   imgs->im7=gdk_imlib_create_image_from_xpm_data(theme_led_on2_xpm);
}

void 
theme_exit               (void)
{
   printf("Theme2 Exit\n");
}

