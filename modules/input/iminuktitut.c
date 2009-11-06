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
 * Modified for Inuktitut - Robert Brady <robert@suse.co.uk>
 *
 */

#include "config.h"
#include <string.h>

#include "gtk/gtk.h"
#include "gdk/gdkkeysyms.h"

#include "gtk/gtkimmodule.h"
#include "gtk/gtkintl.h"

GType type_inuktitut_translit = 0;

static void inuktitut_class_init (GtkIMContextSimpleClass *class);
static void inuktitut_init (GtkIMContextSimple *im_context);

static void
inuktitut_register_type (GTypeModule *module)
{
  const GTypeInfo object_info =
  {
    sizeof (GtkIMContextSimpleClass),
    (GBaseInitFunc) NULL,
    (GBaseFinalizeFunc) NULL,
    (GClassInitFunc) inuktitut_class_init,
    NULL,           /* class_finalize */
    NULL,           /* class_data */
    sizeof (GtkIMContextSimple),
    0,
    (GInstanceInitFunc) inuktitut_init,
  };

  type_inuktitut_translit = 
    g_type_module_register_type (module,
				 GTK_TYPE_IM_CONTEXT_SIMPLE,
				 "GtkIMContextInuktitut",
				 &object_info, 0);
}

#define SYL(a,b,c,d) \
  a, 0,   0, 0, 0, c, \
  a, 'a', 0, 0, 0, b+7-d, \
  a, 'a','a',0, 0, b+8-d, \
  a, 'i', 0, 0, 0, b, \
  a, 'i','i',0, 0, b+1, \
  a, 'o', 0, 0, 0, b+2, \
  a, 'o','o',0, 0, b+3, \
  a, 'u', 0, 0, 0, b+2, \
  a, 'u','u',0, 0, b+3,

static guint16 inuktitut_compose_seqs[] = {
  'a', 0,   0,   0 ,  0,   0x140a,
  'a', 'a', 0,   0,   0,   0x140b,

  SYL('c', 0x148b, 0x14a1, 2) /* As g */
  SYL('f', 0x1555, 0x155d, 2)
  SYL('g', 0x148b, 0x14a1, 2)
  SYL('h', 0x14ef, 0x1505, 2)

  'i', 0,   0,   0 ,  0,   0x1403,
  'i', 'i', 0,   0,   0,   0x1404,

  SYL('j', 0x1528, 0x153e, 2)
  SYL('k', 0x146d, 0x1483, 2)
  SYL('l', 0x14d5, 0x14ea, 2)
  SYL('m', 0x14a5, 0x14bb, 2)
  SYL('n', 0x14c2, 0x14d0, 2)

  'o', 0,   0,   0 ,  0,   0x1405, /* as u */
  'o', 'o', 0,   0,   0,   0x1406,

  SYL('p', 0x1431, 0x1449, 0)
  SYL('q', 0x157f, 0x1585, 3)
  SYL('r', 0x1546, 0x1550, 2)
  SYL('s', 0x14ef, 0x1505, 2) /* As h */
  SYL('t', 0x144e, 0x1466, 0)

  'u', 0,   0,   0 ,  0,   0x1405,
  'u', 'u', 0,   0,   0,   0x1406,

  SYL('v', 0x1555, 0x155d, 2) /* as f */
  SYL('y', 0x1528, 0x153e, 2) /* As j */

  SYL(GDK_lstroke, 0x15a0, 0x15a6, 3) /* l- */
  SYL(GDK_eng, 0x158f, 0x1595, 3)     /* ng */
};

static void
inuktitut_class_init (GtkIMContextSimpleClass *class)
{
}

static void
inuktitut_init (GtkIMContextSimple *im_context)
{
  gtk_im_context_simple_add_table (im_context,
				   inuktitut_compose_seqs,
				   4,
				   G_N_ELEMENTS (inuktitut_compose_seqs) / (4 + 2));
}

static const GtkIMContextInfo inuktitut_info = { 
  "inuktitut",		   /* ID */
  N_("Inuktitut (Transliterated)"),         /* Human readable name */
  GETTEXT_PACKAGE,	   /* Translation domain */
  GTK_LOCALEDIR,	   /* Dir for bindtextdomain (not strictly needed for "gtk+") */
  "iu"			   /* Languages for which this module is the default */
};

static const GtkIMContextInfo *info_list[] = {
  &inuktitut_info
};

#ifndef INCLUDE_IM_inuktitut
#define MODULE_ENTRY(type, function) G_MODULE_EXPORT type im_module_ ## function
#else
#define MODULE_ENTRY(type, function) type _gtk_immodule_inuktitut_ ## function
#endif

MODULE_ENTRY (void, init) (GTypeModule *module)
{
  inuktitut_register_type (module);
}

MODULE_ENTRY (void, exit) (void)
{
}

MODULE_ENTRY (void, list) (const GtkIMContextInfo ***contexts,
			   int                      *n_contexts)
{
  *contexts = info_list;
  *n_contexts = G_N_ELEMENTS (info_list);
}

MODULE_ENTRY (GtkIMContext *, create) (const gchar *context_id)
{
  if (strcmp (context_id, "inuktitut") == 0)
    return g_object_new (type_inuktitut_translit, NULL);
  else
    return NULL;
}
