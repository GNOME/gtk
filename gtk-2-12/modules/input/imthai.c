/* GTK - The GIMP Toolkit
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
 *
 * Author: Theppitak Karoonboonyanan <thep@linux.thai.net>
 *
 */

#include <config.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>

#include "gtk/gtkintl.h"
#include "gtk/gtkimmodule.h"
#include "gtkimcontextthai.h"

GType type_thai = 0;

static const GtkIMContextInfo thai_info = { 
  "thai",	   /* ID */
  N_("Thai-Lao"),  /* Human readable name */
  "gtk+",	   /* Translation domain */
   GTK_LOCALEDIR,  /* Dir for bindtextdomain (not strictly needed for "gtk+") */
  "lo:th"	   /* Languages for which this module is the default */
};

static const GtkIMContextInfo *info_list[] = {
  &thai_info
};

void
im_module_init (GTypeModule *module)
{
  gtk_im_context_thai_register_type (module);
}

void 
im_module_exit (void)
{
}

void 
im_module_list (const GtkIMContextInfo ***contexts,
		int                      *n_contexts)
{
  *contexts = info_list;
  *n_contexts = G_N_ELEMENTS (info_list);
}

GtkIMContext *
im_module_create (const gchar *context_id)
{
  if (strcmp (context_id, "thai") == 0)
    return gtk_im_context_thai_new ();
  else
    return NULL;
}
