/* gtktrayicon.c
 * Copyright (C) 2002 Anders Carlsson <andersca@gnu.org>
 * Copyright (C) 2005 Hans Breuer  <hans@breuer.org>
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
 * This is an implementation of the gtktrayicon interface *not*
 * following the freedesktop.org "system tray" spec,
 * http://www.freedesktop.org/wiki/Standards/systemtray-spec
 */

#include <config.h>
#include <string.h>
#include <libintl.h>

#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtktrayicon.h"

#include "gtkalias.h"

G_DEFINE_TYPE (GtkTrayIcon, gtk_tray_icon, GTK_TYPE_PLUG);

static void
gtk_tray_icon_class_init (GtkTrayIconClass *class)
{
}

static void
gtk_tray_icon_init (GtkTrayIcon *icon)
{
}

GtkTrayIcon *
_gtk_tray_icon_new (const gchar *name)
{
  return g_object_new (GTK_TYPE_TRAY_ICON, 
		       "title", name, 
		       NULL);
}

GtkOrientation 
_gtk_tray_icon_get_orientation (GtkTrayIcon *icon)
{
  g_return_val_if_fail (GTK_IS_TRAY_ICON (icon), GTK_ORIENTATION_HORIZONTAL);
  //FIXME: should we deliver the orientation of the tray ??
  return GTK_ORIENTATION_VERTICAL;
}

