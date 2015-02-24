/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "gtk/gtkintl.h"
#include "gtk/gtkimmodule.h"
#include "gtkimcontextxim.h"
#include <string.h>

static const GtkIMContextInfo xim_ja_info = { 
  "xim",		           /* ID */
  NC_("input method menu", "X Input Method"), /* Human readable name */
  GETTEXT_PACKAGE,		   /* Translation domain */
  GTK_LOCALEDIR,		   /* Dir for bindtextdomain (not strictly needed for "gtk+") */
  "ko:ja:th:zh"		           /* Languages for which this module is the default */
};

static const GtkIMContextInfo *info_list[] = {
  &xim_ja_info
};

#ifndef INCLUDE_IM_xim
#define MODULE_ENTRY(type, function) G_MODULE_EXPORT type im_module_ ## function
#else
#define MODULE_ENTRY(type, function) type _gtk_immodule_xim_ ## function
#endif

MODULE_ENTRY (void, init) (GTypeModule *type_module)
{
  gtk_im_context_xim_register_type (type_module);
}

MODULE_ENTRY (void, exit) (void)
{
  gtk_im_context_xim_shutdown ();
}

MODULE_ENTRY (void, list) (const GtkIMContextInfo ***contexts,
			   int                      *n_contexts)
{
  *contexts = info_list;
  *n_contexts = G_N_ELEMENTS (info_list);
}

MODULE_ENTRY (GtkIMContext *, create) (const gchar *context_id)
{
  if (strcmp (context_id, "xim") == 0)
    return gtk_im_context_xim_new ();
  else
    return NULL;
}
