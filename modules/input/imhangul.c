/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat Software
 * Copyright (C) 2002 Yusuke Tabata
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
 * Authors: Owen Taylor <otaylor@redhat.com>
 *          Yusuke Tabata <tee@kuis.kyoto-u.ac.jp>
 *
 * This module is a port of the korean-hangul module from Emacs.
 */

#include <string.h>

#include <gdk/gdkkeysyms.h>

#include "gtk/gtkintl.h"
#include "gtk/gtkimcontextsimple.h"
#include "gtk/gtkimmodule.h"

GType type_hangul = 0;

static void hangul_class_init (GtkIMContextSimpleClass *class);
static void hangul_init (GtkIMContextSimple *im_context);

static void
hangul_register_type (GTypeModule *module)
{
  static const GTypeInfo object_info =
  {
    sizeof (GtkIMContextSimpleClass),
    (GBaseInitFunc) NULL,
    (GBaseFinalizeFunc) NULL,
    (GClassInitFunc) hangul_class_init,
    NULL,           /* class_finalize */
    NULL,           /* class_data */
    sizeof (GtkIMContextSimple),
    0,
    (GtkObjectInitFunc) hangul_init,
  };

  type_hangul = 
    g_type_module_register_type (module,
				 GTK_TYPE_IM_CONTEXT_SIMPLE,
				 "GtkIMContextHangul",
				 &object_info, 0);
}

static guint16 hangul_compose_seqs[] = {
#include "imhangul-defs.h"
};

static void
hangul_class_init (GtkIMContextSimpleClass *class)
{
}

static void
hangul_init (GtkIMContextSimple *im_context)
{
  gtk_im_context_simple_add_table (im_context,
				   hangul_compose_seqs,
				   4,
				   G_N_ELEMENTS (hangul_compose_seqs) / (4 + 2));
}

static const GtkIMContextInfo hangul_info = { 
  "hangul",		   /* ID */
  /*N_("Hangul"),*/
  "Hangul (KSC 5601)",             /* Human readable name */
  "gtk+",			   /* Translation domain */
  /* GTK_LOCALEDIR, */
   "",		   /* Dir for bindtextdomain (not strictly needed for "gtk+") */
   ""			           /* Languages for which this module is the default */
};

static const GtkIMContextInfo *info_list[] = {
  &hangul_info
};

void
im_module_init (GTypeModule *module)
{
  hangul_register_type (module);
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
  if (strcmp (context_id, "hangul") == 0)
    return GTK_IM_CONTEXT (g_object_new (type_hangul, NULL));
  else
    return NULL;
}
