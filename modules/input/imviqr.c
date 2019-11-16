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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Original author: Owen Taylor <otaylor@redhat.com>
 * 
 * Modified for VIQR - Robert Brady <robert@suse.co.uk>
 *
 */

#include "config.h"
#include <string.h>

#include "gtk/gtk.h"
#include "gdk/gdkkeysyms.h"

#include "gtk/gtkimmodule.h"
#include "gtk/gtkintl.h"

GType type_viqr_translit = 0;

static void viqr_class_init (GtkIMContextSimpleClass *class);
static void viqr_init (GtkIMContextSimple *im_context);

static void
viqr_register_type (GTypeModule *module)
{
  const GTypeInfo object_info =
  {
    sizeof (GtkIMContextSimpleClass),
    (GBaseInitFunc) NULL,
    (GBaseFinalizeFunc) NULL,
    (GClassInitFunc) viqr_class_init,
    NULL,           /* class_finalize */
    NULL,           /* class_data */
    sizeof (GtkIMContextSimple),
    0,
    (GInstanceInitFunc) viqr_init,
  };

  type_viqr_translit = 
    g_type_module_register_type (module,
				 GTK_TYPE_IM_CONTEXT_SIMPLE,
				 "GtkIMContextViqr",
				 &object_info, 0);
}

static guint16 viqr_compose_seqs[] = {
  GDK_KEY_A,                   0,                0, 0, 0, 'A',
  GDK_KEY_A,                   GDK_KEY_apostrophe,   0, 0, 0, 0xc1,
  GDK_KEY_A,  GDK_KEY_parenleft,   0,                0, 0,    0x102,
  GDK_KEY_A,  GDK_KEY_parenleft,   GDK_KEY_apostrophe,   0, 0,    0x1eae,
  GDK_KEY_A,  GDK_KEY_parenleft,   GDK_KEY_period,       0, 0,    0x1eb6,
  GDK_KEY_A,  GDK_KEY_parenleft,   GDK_KEY_question,     0, 0,    0x1eb2,
  GDK_KEY_A,  GDK_KEY_parenleft,   GDK_KEY_grave,        0, 0,    0x1eb0,
  GDK_KEY_A,  GDK_KEY_parenleft,   GDK_KEY_asciitilde,   0, 0,    0x1eb4,
  GDK_KEY_A,                   GDK_KEY_period,       0, 0, 0, 0x1ea0,
  GDK_KEY_A,                   GDK_KEY_question,     0, 0, 0, 0x1ea2,
  GDK_KEY_A,  GDK_KEY_asciicircum, 0,                0, 0,    0xc2,
  GDK_KEY_A,  GDK_KEY_asciicircum, GDK_KEY_apostrophe,   0, 0,    0x1ea4,
  GDK_KEY_A,  GDK_KEY_asciicircum, GDK_KEY_period,       0, 0,    0x1eac,
  GDK_KEY_A,  GDK_KEY_asciicircum, GDK_KEY_question,     0, 0,    0x1ea8,
  GDK_KEY_A,  GDK_KEY_asciicircum, GDK_KEY_grave,        0, 0,    0x1ea6,
  GDK_KEY_A,  GDK_KEY_asciicircum, GDK_KEY_asciitilde,   0, 0,    0x1eaa,
  GDK_KEY_A,                   GDK_KEY_grave,        0, 0, 0, 0xc0,
  GDK_KEY_A,                   GDK_KEY_asciitilde,   0, 0, 0, 0xc3,
  GDK_KEY_D,                   0,                0, 0, 0, 'D',
  GDK_KEY_D,                   GDK_KEY_D,            0, 0, 0, 0x110,
  GDK_KEY_D,                   GDK_KEY_d,            0, 0, 0, 0x110,
  GDK_KEY_E,                   0,                0, 0, 0, 'E',
  GDK_KEY_E,                   GDK_KEY_apostrophe,   0, 0, 0, 0xc9,
  GDK_KEY_E,                   GDK_KEY_period,       0, 0, 0, 0x1eb8,
  GDK_KEY_E,                   GDK_KEY_question,     0, 0, 0, 0x1eba,
  GDK_KEY_E,  GDK_KEY_asciicircum, 0,                0, 0,    0xca,
  GDK_KEY_E,  GDK_KEY_asciicircum, GDK_KEY_apostrophe,   0, 0,    0x1ebe,
  GDK_KEY_E,  GDK_KEY_asciicircum, GDK_KEY_period,       0, 0,    0x1ec6,
  GDK_KEY_E,  GDK_KEY_asciicircum, GDK_KEY_question,     0, 0,    0x1ec2,
  GDK_KEY_E,  GDK_KEY_asciicircum, GDK_KEY_grave,        0, 0,    0x1ec0,
  GDK_KEY_E,  GDK_KEY_asciicircum, GDK_KEY_asciitilde,   0, 0,    0x1ec4,
  GDK_KEY_E,                   GDK_KEY_grave,        0, 0, 0, 0xc8,
  GDK_KEY_E,                   GDK_KEY_asciitilde,   0, 0, 0, 0x1ebc,
  GDK_KEY_I,                   0,                0, 0, 0, 'I',
  GDK_KEY_I,                   GDK_KEY_apostrophe,   0, 0, 0, 0xcd,
  GDK_KEY_I,                   GDK_KEY_period,       0, 0, 0, 0x1eca,
  GDK_KEY_I,                   GDK_KEY_question,     0, 0, 0, 0x1ec8,
  GDK_KEY_I,                   GDK_KEY_grave,        0, 0, 0, 0xcc,
  GDK_KEY_I,                   GDK_KEY_asciitilde,   0, 0, 0, 0x128,
  GDK_KEY_O,                   0,                0, 0, 0, 'O',
  GDK_KEY_O,                   GDK_KEY_apostrophe,   0, 0, 0, 0xD3,
  GDK_KEY_O,  GDK_KEY_plus,        0,                0, 0,    0x1a0,
  GDK_KEY_O,  GDK_KEY_plus,        GDK_KEY_apostrophe,   0, 0,    0x1eda,
  GDK_KEY_O,  GDK_KEY_plus,        GDK_KEY_period,       0, 0,    0x1ee2,
  GDK_KEY_O,  GDK_KEY_plus,        GDK_KEY_question,     0, 0,    0x1ede,
  GDK_KEY_O,  GDK_KEY_plus,        GDK_KEY_grave,        0, 0,    0x1edc,
  GDK_KEY_O,  GDK_KEY_plus,        GDK_KEY_asciitilde,   0, 0,    0x1ee0,
  GDK_KEY_O,                   GDK_KEY_period,       0, 0, 0, 0x1ecc,
  GDK_KEY_O,                   GDK_KEY_question,     0, 0, 0, 0x1ece,
  GDK_KEY_O,  GDK_KEY_asciicircum, 0,                0, 0,    0xd4,
  GDK_KEY_O,  GDK_KEY_asciicircum, GDK_KEY_apostrophe,   0, 0,    0x1ed0,
  GDK_KEY_O,  GDK_KEY_asciicircum, GDK_KEY_period,       0, 0,    0x1ed8,
  GDK_KEY_O,  GDK_KEY_asciicircum, GDK_KEY_question,     0, 0,    0x1ed4,
  GDK_KEY_O,  GDK_KEY_asciicircum, GDK_KEY_grave,        0, 0,    0x1ed2,
  GDK_KEY_O,  GDK_KEY_asciicircum, GDK_KEY_asciitilde,   0, 0,    0x1ed6,
  GDK_KEY_O,                   GDK_KEY_grave,        0, 0, 0, 0xD2,
  GDK_KEY_O,                   GDK_KEY_asciitilde,   0, 0, 0, 0xD5,
  GDK_KEY_U,                   0,                0, 0, 0, 'U',
  GDK_KEY_U,                   GDK_KEY_apostrophe,   0, 0, 0, 0xDA,
  GDK_KEY_U,  GDK_KEY_plus,        0,                0, 0,    0x1af,
  GDK_KEY_U,  GDK_KEY_plus,        GDK_KEY_apostrophe,   0, 0,    0x1ee8,
  GDK_KEY_U,  GDK_KEY_plus,        GDK_KEY_period,       0, 0,    0x1ef0,
  GDK_KEY_U,  GDK_KEY_plus,        GDK_KEY_question,     0, 0,    0x1eec,
  GDK_KEY_U,  GDK_KEY_plus,        GDK_KEY_grave,        0, 0,    0x1eea,
  GDK_KEY_U,  GDK_KEY_plus,        GDK_KEY_asciitilde,   0, 0,    0x1eee,
  GDK_KEY_U,                   GDK_KEY_period,       0, 0, 0, 0x1ee4,
  GDK_KEY_U,                   GDK_KEY_question,     0, 0, 0, 0x1ee6,
  GDK_KEY_U,                   GDK_KEY_grave,        0, 0, 0, 0xd9,
  GDK_KEY_U,                   GDK_KEY_asciitilde,   0, 0, 0, 0x168,
  GDK_KEY_Y,                   0,                0, 0, 0, 'Y',
  GDK_KEY_Y,                   GDK_KEY_apostrophe,   0, 0, 0, 0xdd,
  GDK_KEY_Y,                   GDK_KEY_period,       0, 0, 0, 0x1ef4,
  GDK_KEY_Y,                   GDK_KEY_question,     0, 0, 0, 0x1ef6,
  GDK_KEY_Y,                   GDK_KEY_grave,        0, 0, 0, 0x1ef2,
  GDK_KEY_Y,                   GDK_KEY_asciitilde,   0, 0, 0, 0x1ef8,
  /* Do we need anything else here? */
  GDK_KEY_backslash,           0,                0, 0, 0, 0,
  GDK_KEY_backslash,           GDK_KEY_apostrophe,   0, 0, 0, '\'',
  GDK_KEY_backslash,           GDK_KEY_parenleft,    0, 0, 0, '(',
  GDK_KEY_backslash,           GDK_KEY_plus,         0, 0, 0, '+',
  GDK_KEY_backslash,           GDK_KEY_period,       0, 0, 0, '.',
  GDK_KEY_backslash,           GDK_KEY_question,     0, 0, 0, '?',
  GDK_KEY_backslash,           GDK_KEY_D,            0, 0, 0, 'D',
  GDK_KEY_backslash,           GDK_KEY_backslash,    0, 0, 0, '\\',
  GDK_KEY_backslash,           GDK_KEY_asciicircum,  0, 0, 0, '^',
  GDK_KEY_backslash,           GDK_KEY_grave,        0, 0, 0, '`',
  GDK_KEY_backslash,           GDK_KEY_d,            0, 0, 0, 'd',
  GDK_KEY_backslash,           GDK_KEY_asciitilde,   0, 0, 0, '~',
  GDK_KEY_a,                   0,                0, 0, 0, 'a',
  GDK_KEY_a,                   GDK_KEY_apostrophe,   0, 0, 0, 0xe1,
  GDK_KEY_a, GDK_KEY_parenleft,    0,                0, 0,    0x103,
  GDK_KEY_a, GDK_KEY_parenleft,    GDK_KEY_apostrophe,   0, 0,    0x1eaf,
  GDK_KEY_a, GDK_KEY_parenleft,    GDK_KEY_period,       0, 0,    0x1eb7,
  GDK_KEY_a, GDK_KEY_parenleft,    GDK_KEY_question,     0, 0,    0x1eb3,
  GDK_KEY_a, GDK_KEY_parenleft,    GDK_KEY_grave,        0, 0,    0x1eb1,
  GDK_KEY_a, GDK_KEY_parenleft,    GDK_KEY_asciitilde,   0, 0,    0x1eb5,
  GDK_KEY_a,                   GDK_KEY_period,       0, 0, 0, 0x1ea1,
  GDK_KEY_a,                   GDK_KEY_question,     0, 0, 0, 0x1ea3,
  GDK_KEY_a, GDK_KEY_asciicircum,  0,                0, 0,    0xe2,
  GDK_KEY_a, GDK_KEY_asciicircum,  GDK_KEY_apostrophe,   0, 0,    0x1ea5,
  GDK_KEY_a, GDK_KEY_asciicircum,  GDK_KEY_period,       0, 0,    0x1ead,
  GDK_KEY_a, GDK_KEY_asciicircum,  GDK_KEY_question,     0, 0,    0x1ea9,
  GDK_KEY_a, GDK_KEY_asciicircum,  GDK_KEY_grave,        0, 0,    0x1ea7,
  GDK_KEY_a, GDK_KEY_asciicircum,  GDK_KEY_asciitilde,   0, 0,    0x1eab,
  GDK_KEY_a,                   GDK_KEY_grave,        0, 0, 0, 0xe0,
  GDK_KEY_a,                   GDK_KEY_asciitilde,   0, 0, 0, 0xe3,
  GDK_KEY_d,                   0,                0, 0, 0, 'd',
  GDK_KEY_d,                   GDK_KEY_d,            0, 0, 0, 0x111,
  GDK_KEY_e,                   0,                0, 0, 0, 'e',
  GDK_KEY_e,                   GDK_KEY_apostrophe,   0, 0, 0, 0xe9,
  GDK_KEY_e,                   GDK_KEY_period,       0, 0, 0, 0x1eb9,
  GDK_KEY_e,                   GDK_KEY_question,     0, 0, 0, 0x1ebb,
  GDK_KEY_e, GDK_KEY_asciicircum,  0,                0, 0,    0xea,
  GDK_KEY_e, GDK_KEY_asciicircum,  GDK_KEY_apostrophe,   0, 0,    0x1ebf,
  GDK_KEY_e, GDK_KEY_asciicircum,  GDK_KEY_period,       0, 0,    0x1ec7,
  GDK_KEY_e, GDK_KEY_asciicircum,  GDK_KEY_question,     0, 0,    0x1ec3,
  GDK_KEY_e, GDK_KEY_asciicircum,  GDK_KEY_grave,        0, 0,    0x1ec1,
  GDK_KEY_e, GDK_KEY_asciicircum,  GDK_KEY_asciitilde,   0, 0,    0x1ec5,
  GDK_KEY_e,                   GDK_KEY_grave,        0, 0, 0, 0xe8,
  GDK_KEY_e,                   GDK_KEY_asciitilde,   0, 0, 0, 0x1ebd,
  GDK_KEY_i,                   0,                0, 0, 0, 'i',
  GDK_KEY_i,                   GDK_KEY_apostrophe,   0, 0, 0, 0xed,
  GDK_KEY_i,                   GDK_KEY_period,       0, 0, 0, 0x1ecb,
  GDK_KEY_i,                   GDK_KEY_question,     0, 0, 0, 0x1ec9,
  GDK_KEY_i,                   GDK_KEY_grave,        0, 0, 0, 0xec,
  GDK_KEY_i,                   GDK_KEY_asciitilde,   0, 0, 0, 0x129,
  GDK_KEY_o,                   0,                0, 0, 0, 'o',
  GDK_KEY_o,                   GDK_KEY_apostrophe,   0, 0, 0, 0xF3,
  GDK_KEY_o,  GDK_KEY_plus,        0,                0, 0,    0x1a1,
  GDK_KEY_o,  GDK_KEY_plus,        GDK_KEY_apostrophe,   0, 0,    0x1edb,
  GDK_KEY_o,  GDK_KEY_plus,        GDK_KEY_period,       0, 0,    0x1ee3,
  GDK_KEY_o,  GDK_KEY_plus,        GDK_KEY_question,     0, 0,    0x1edf,
  GDK_KEY_o,  GDK_KEY_plus,        GDK_KEY_grave,        0, 0,    0x1edd,
  GDK_KEY_o,  GDK_KEY_plus,        GDK_KEY_asciitilde,   0, 0,    0x1ee1,
  GDK_KEY_o,                   GDK_KEY_period,       0, 0, 0, 0x1ecd,
  GDK_KEY_o,                   GDK_KEY_question,     0, 0, 0, 0x1ecf,
  GDK_KEY_o,  GDK_KEY_asciicircum, 0,                0, 0,    0xf4,
  GDK_KEY_o,  GDK_KEY_asciicircum, GDK_KEY_apostrophe,   0, 0,    0x1ed1,
  GDK_KEY_o,  GDK_KEY_asciicircum, GDK_KEY_period,       0, 0,    0x1ed9,
  GDK_KEY_o,  GDK_KEY_asciicircum, GDK_KEY_question,     0, 0,    0x1ed5,
  GDK_KEY_o,  GDK_KEY_asciicircum, GDK_KEY_grave,        0, 0,    0x1ed3,
  GDK_KEY_o,  GDK_KEY_asciicircum, GDK_KEY_asciitilde,   0, 0,    0x1ed7,
  GDK_KEY_o,                   GDK_KEY_grave,        0, 0, 0, 0xF2,
  GDK_KEY_o,                   GDK_KEY_asciitilde,   0, 0, 0, 0xF5,
  GDK_KEY_u,                   0,                0, 0, 0, 'u',
  GDK_KEY_u,                   GDK_KEY_apostrophe,   0, 0, 0, 0xFA,
  GDK_KEY_u,  GDK_KEY_plus,        0,                0, 0,    0x1b0,
  GDK_KEY_u,  GDK_KEY_plus,        GDK_KEY_apostrophe,   0, 0,    0x1ee9,
  GDK_KEY_u,  GDK_KEY_plus,        GDK_KEY_period,       0, 0,    0x1ef1,
  GDK_KEY_u,  GDK_KEY_plus,        GDK_KEY_question,     0, 0,    0x1eed,
  GDK_KEY_u,  GDK_KEY_plus,        GDK_KEY_grave,        0, 0,    0x1eeb,
  GDK_KEY_u,  GDK_KEY_plus,        GDK_KEY_asciitilde,   0, 0,    0x1eef,
  GDK_KEY_u,                   GDK_KEY_period,       0, 0, 0, 0x1ee5,
  GDK_KEY_u,                   GDK_KEY_question,     0, 0, 0, 0x1ee7,
  GDK_KEY_u,                   GDK_KEY_grave,        0, 0, 0, 0xf9,
  GDK_KEY_u,                   GDK_KEY_asciitilde,   0, 0, 0, 0x169,
  GDK_KEY_y,                   0,                0, 0, 0, 'y',
  GDK_KEY_y,                   GDK_KEY_apostrophe,   0, 0, 0, 0xfd,
  GDK_KEY_y,                   GDK_KEY_period,       0, 0, 0, 0x1ef5,
  GDK_KEY_y,                   GDK_KEY_question,     0, 0, 0, 0x1ef7,
  GDK_KEY_y,                   GDK_KEY_grave,        0, 0, 0, 0x1ef3,
  GDK_KEY_y,                   GDK_KEY_asciitilde,   0, 0, 0, 0x1ef9,
};

static void
viqr_class_init (GtkIMContextSimpleClass *class)
{
}

static void
viqr_init (GtkIMContextSimple *im_context)
{
  gtk_im_context_simple_add_table (im_context,
				   viqr_compose_seqs,
				   4,
				   G_N_ELEMENTS (viqr_compose_seqs) / (4 + 2));
}

static const GtkIMContextInfo viqr_info = { 
  "viqr",		   /* ID */
  NC_("input method menu", "Vietnamese (VIQR)"), /* Human readable name */
  GETTEXT_PACKAGE,	   /* Translation domain */
   GTK_LOCALEDIR,	   /* Dir for bindtextdomain (not strictly needed for "gtk+") */
  ""			   /* Languages for which this module is the default */
};

static const GtkIMContextInfo *info_list[] = {
  &viqr_info
};

#ifndef INCLUDE_IM_viqr
#define MODULE_ENTRY(type, function) G_MODULE_EXPORT type im_module_ ## function
#else
#define MODULE_ENTRY(type, function) type _gtk_immodule_viqr_ ## function
#endif

MODULE_ENTRY (void, init) (GTypeModule *module)
{
  viqr_register_type (module);
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
  if (strcmp (context_id, "viqr") == 0)
    return g_object_new (type_viqr_translit, NULL);
  else
    return NULL;
}
