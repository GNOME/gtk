/* Copyright (C) 2006 Openismus GmbH
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

#include "gtkimcontextmultipress.h"
#include <gtk/gtkimmodule.h> /* For GtkIMContextInfo */
#include <config.h>
#include <glib/gi18n.h>
#include <string.h> /* For strcmp() */

#define CONTEXT_ID "multipress"
 
/** NOTE: Change the default language from "" to "*" to enable this input method by default for all locales.
 */
static const GtkIMContextInfo info = { 
  CONTEXT_ID,		   /* ID */
  N_("Multipress"),     /* Human readable name */
  GETTEXT_PACKAGE,	   /* Translation domain. Defined in configure.ac */
  MULTIPRESS_LOCALEDIR,	   /* Dir for bindtextdomain (not strictly needed for "gtk+"). Defined in the Makefile.am */
  ""			   /* Languages for which this module is the default */
};

static const GtkIMContextInfo *info_list[] = {
  &info
};

void
im_module_init (GTypeModule *module)
{
  gtk_im_context_multipress_register_type(module);
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
  if (strcmp (context_id, CONTEXT_ID) == 0)
  {
    GtkIMContext* imcontext = GTK_IM_CONTEXT(g_object_new (GTK_TYPE_IM_CONTEXT_MULTIPRESS, NULL));
    return imcontext;
  }
  else
    return NULL;
}
