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

/* Theme functions to export */
void theme_init               (int     *argc,
			       char ***argv);
void theme_exit               (void);

/* internals */

void 
theme_init               (int     *argc,
			  char ***argv)
{
   printf("Theme Init\n");
}

void 
theme_exit               (void)
{
   printf("Theme Exit\n");
}

