/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat Software
 * Copyright (C) 2000 SuSE Linux Ltd
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
 * Original author: Owen Taylor <otaylor@redhat.com>
 * 
 * Modified for Thai (Broken) - Robert Brady <robert@suse.co.uk>
 *
 */

#include <string.h>

#include <gdk/gdkkeysyms.h>

#include "gtk/gtkintl.h"
#include "gtk/gtkimcontextsimple.h"
#include "gtk/gtkimmodule.h"

GType type_thai_broken = 0;

static void thai_broken_class_init (GtkIMContextSimpleClass *class);
static void thai_broken_init (GtkIMContextSimple *im_context);

static void
thai_broken_register_type (GTypeModule *module)
{
  static const GTypeInfo object_info =
  {
    sizeof (GtkIMContextSimpleClass),
    (GBaseInitFunc) NULL,
    (GBaseFinalizeFunc) NULL,
    (GClassInitFunc) thai_broken_class_init,
    NULL,           /* class_finalize */
    NULL,           /* class_data */
    sizeof (GtkIMContextSimple),
    0,
    (GtkObjectInitFunc) thai_broken_init,
  };

  type_thai_broken = 
    g_type_module_register_type (module,
				 GTK_TYPE_IM_CONTEXT_SIMPLE,
				 "GtkIMContextThaiBroken",
				 &object_info, 0);
}

static guint16 thai_broken_compose_seqs[] = {
  0xa0, 0, 0, 0, 0, 0x0e00,
  0xa1, 0, 0, 0, 0, 0x0e01,
  0xa2, 0, 0, 0, 0, 0x0e02,
  0xa3, 0, 0, 0, 0, 0x0e03,
  0xa4, 0, 0, 0, 0, 0x0e04,
  0xa5, 0, 0, 0, 0, 0x0e05,
  0xa6, 0, 0, 0, 0, 0x0e06,
  0xa7, 0, 0, 0, 0, 0x0e07,
  0xa8, 0, 0, 0, 0, 0x0e08,
  0xa9, 0, 0, 0, 0, 0x0e09,
  0xaa, 0, 0, 0, 0, 0x0e0a,
  0xab, 0, 0, 0, 0, 0x0e0b,
  0xac, 0, 0, 0, 0, 0x0e0c,
  0xad, 0, 0, 0, 0, 0x0e0d,
  0xae, 0, 0, 0, 0, 0x0e0e,
  0xaf, 0, 0, 0, 0, 0x0e0f,
  0xb0, 0, 0, 0, 0, 0x0e10,
  0xb1, 0, 0, 0, 0, 0x0e11,
  0xb2, 0, 0, 0, 0, 0x0e12,
  0xb3, 0, 0, 0, 0, 0x0e13,
  0xb4, 0, 0, 0, 0, 0x0e14,
  0xb5, 0, 0, 0, 0, 0x0e15,
  0xb6, 0, 0, 0, 0, 0x0e16,
  0xb7, 0, 0, 0, 0, 0x0e17,
  0xb8, 0, 0, 0, 0, 0x0e18,
  0xb9, 0, 0, 0, 0, 0x0e19,
  0xba, 0, 0, 0, 0, 0x0e1a,
  0xbb, 0, 0, 0, 0, 0x0e1b,
  0xbc, 0, 0, 0, 0, 0x0e1c,
  0xbd, 0, 0, 0, 0, 0x0e1d,
  0xbe, 0, 0, 0, 0, 0x0e1e,
  0xbf, 0, 0, 0, 0, 0x0e1f,
  0xc0, 0, 0, 0, 0, 0x0e20,
  0xc1, 0, 0, 0, 0, 0x0e21,
  0xc2, 0, 0, 0, 0, 0x0e22,
  0xc3, 0, 0, 0, 0, 0x0e23,
  0xc4, 0, 0, 0, 0, 0x0e24,
  0xc5, 0, 0, 0, 0, 0x0e25,
  0xc6, 0, 0, 0, 0, 0x0e26,
  0xc7, 0, 0, 0, 0, 0x0e27,
  0xc8, 0, 0, 0, 0, 0x0e28,
  0xc9, 0, 0, 0, 0, 0x0e29,
  0xca, 0, 0, 0, 0, 0x0e2a,
  0xcb, 0, 0, 0, 0, 0x0e2b,
  0xcc, 0, 0, 0, 0, 0x0e2c,
  0xcd, 0, 0, 0, 0, 0x0e2d,
  0xce, 0, 0, 0, 0, 0x0e2e,
  0xcf, 0, 0, 0, 0, 0x0e2f,
  0xd0, 0, 0, 0, 0, 0x0e30,
  0xd1, 0, 0, 0, 0, 0x0e31,
  0xd2, 0, 0, 0, 0, 0x0e32,
  0xd3, 0, 0, 0, 0, 0x0e33,
  0xd4, 0, 0, 0, 0, 0x0e34,
  0xd5, 0, 0, 0, 0, 0x0e35,
  0xd6, 0, 0, 0, 0, 0x0e36,
  0xd7, 0, 0, 0, 0, 0x0e37,
  0xd8, 0, 0, 0, 0, 0x0e38,
  0xd9, 0, 0, 0, 0, 0x0e39,
  0xda, 0, 0, 0, 0, 0x0e3a,
  0xdb, 0, 0, 0, 0, 0x0e3b,
  0xdc, 0, 0, 0, 0, 0x0e3c,
  0xdd, 0, 0, 0, 0, 0x0e3d,
  0xde, 0, 0, 0, 0, 0x0e3e,
  0xdf, 0, 0, 0, 0, 0x0e3f,
  0xe0, 0, 0, 0, 0, 0x0e40,
  0xe1, 0, 0, 0, 0, 0x0e41,
  0xe2, 0, 0, 0, 0, 0x0e42,
  0xe3, 0, 0, 0, 0, 0x0e43,
  0xe4, 0, 0, 0, 0, 0x0e44,
  0xe5, 0, 0, 0, 0, 0x0e45,
  0xe6, 0, 0, 0, 0, 0x0e46,
  0xe7, 0, 0, 0, 0, 0x0e47,
  0xe8, 0, 0, 0, 0, 0x0e48,
  0xe9, 0, 0, 0, 0, 0x0e49,
  0xea, 0, 0, 0, 0, 0x0e4a,
  0xeb, 0, 0, 0, 0, 0x0e4b,
  0xec, 0, 0, 0, 0, 0x0e4c,
  0xed, 0, 0, 0, 0, 0x0e4d,
  0xee, 0, 0, 0, 0, 0x0e4e,
  0xef, 0, 0, 0, 0, 0x0e4f,
  0xf0, 0, 0, 0, 0, 0x0e50,
  0xf1, 0, 0, 0, 0, 0x0e51,
  0xf2, 0, 0, 0, 0, 0x0e52,
  0xf3, 0, 0, 0, 0, 0x0e53,
  0xf4, 0, 0, 0, 0, 0x0e54,
  0xf5, 0, 0, 0, 0, 0x0e55,
  0xf6, 0, 0, 0, 0, 0x0e56,
  0xf7, 0, 0, 0, 0, 0x0e57,
  0xf8, 0, 0, 0, 0, 0x0e58,
  0xf9, 0, 0, 0, 0, 0x0e59,
  0xfa, 0, 0, 0, 0, 0x0e5a,
  0xfb, 0, 0, 0, 0, 0x0e5b,
  0xfc, 0, 0, 0, 0, 0x0e5c,
  0xfd, 0, 0, 0, 0, 0x0e5d,
  0xfe, 0, 0, 0, 0, 0x0e5e,
  0xff, 0, 0, 0, 0, 0x0e5f,
};

static void
thai_broken_class_init (GtkIMContextSimpleClass *class)
{
}

static void
thai_broken_init (GtkIMContextSimple *im_context)
{
  gtk_im_context_simple_add_table (im_context,
				   thai_broken_compose_seqs,
				   4,
				   G_N_ELEMENTS (thai_broken_compose_seqs) / (4 + 2));
}

static const GtkIMContextInfo thai_broken_info = { 
  "thai_broken",	   /* ID */
  N_("Thai (Broken)"),     /* Human readable name */
  "gtk+",		   /* Translation domain */
   GTK_LOCALEDIR,	   /* Dir for bindtextdomain (not strictly needed for "gtk+") */
  ""			   /* Languages for which this module is the default */
};

static const GtkIMContextInfo *info_list[] = {
  &thai_broken_info
};

void
im_module_init (GTypeModule *module)
{
  thai_broken_register_type (module);
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
  if (strcmp (context_id, "thai_broken") == 0)
    return GTK_IM_CONTEXT (g_object_new (type_thai_broken, NULL));
  else
    return NULL;
}
