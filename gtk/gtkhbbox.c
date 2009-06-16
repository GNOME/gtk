/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"
#include "gtkhbbox.h"
#include "gtkorientable.h"
#include "gtkintl.h"
#include "gtkalias.h"

static gint override_default_spacing = -1;
static gint get_default_spacing (void);
static void set_default_spacing (gint size);

static gint default_layout_style = GTK_BUTTONBOX_EDGE;

static gint
get_default_spacing (void)
{
  if (override_default_spacing != -1)
    return override_default_spacing;
  return GTK_SIZE_ONE_TWELFTH_EM(30);
}

static void
set_default_spacing (gint size)
{
  override_default_spacing = size;
}

G_DEFINE_TYPE (GtkHButtonBox, gtk_hbutton_box, GTK_TYPE_BUTTON_BOX)

static void
gtk_hbutton_box_class_init (GtkHButtonBoxClass *class)
{
}

static void
gtk_hbutton_box_init (GtkHButtonBox *hbutton_box)
{
	gtk_orientable_set_orientation (GTK_ORIENTABLE (hbutton_box),
			GTK_ORIENTATION_HORIZONTAL);
}

GtkWidget*
gtk_hbutton_box_new (void)
{
  GtkHButtonBox *hbutton_box;

  hbutton_box = g_object_new (GTK_TYPE_HBUTTON_BOX, NULL);

  return GTK_WIDGET (hbutton_box);
}


/* set default value for spacing */

void 
gtk_hbutton_box_set_spacing_default (gint spacing)
{
  set_default_spacing (spacing);
}


/* set default value for layout style */

void 
gtk_hbutton_box_set_layout_default (GtkButtonBoxStyle layout)
{
  g_return_if_fail (layout >= GTK_BUTTONBOX_DEFAULT_STYLE &&
		    layout <= GTK_BUTTONBOX_CENTER);

  default_layout_style = layout;
}

/* get default value for spacing */

gint 
gtk_hbutton_box_get_spacing_default (void)
{
  /* This function is just broken: since the default spacing is a
   * constant number of resolution independent units (e.g. 2.5em), the
   * pixel size will vary depending on what screen the widget will
   * appear on (different DPI etc.).
   */
  return gtk_size_to_pixel (NULL, 0, get_default_spacing ());
}


/* get default value for layout style */

GtkButtonBoxStyle
gtk_hbutton_box_get_layout_default (void)
{
  return default_layout_style;
}

#define __GTK_HBUTTON_BOX_C__
#include "gtkaliasdef.c"
