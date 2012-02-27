/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat Software
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
 * Author: Owen Taylor <otaylor@redhat.com>
 *
 */

#include "config.h"
#include <string.h>

#include "gtk/gtk.h"
#include "gdk/gdkkeysyms.h"

#include "gtk/gtkimmodule.h"
#include "gtk/gtkintl.h"

GType type_cyrillic_translit = 0;

static void cyrillic_translit_class_init (GtkIMContextSimpleClass *class);
static void cyrillic_translit_init (GtkIMContextSimple *im_context);

static void
cyrillic_translit_register_type (GTypeModule *module)
{
  const GTypeInfo object_info =
  {
    sizeof (GtkIMContextSimpleClass),
    (GBaseInitFunc) NULL,
    (GBaseFinalizeFunc) NULL,
    (GClassInitFunc) cyrillic_translit_class_init,
    NULL,           /* class_finalize */
    NULL,           /* class_data */
    sizeof (GtkIMContextSimple),
    0,
    (GInstanceInitFunc) cyrillic_translit_init,
  };

  type_cyrillic_translit = 
    g_type_module_register_type (module,
				 GTK_TYPE_IM_CONTEXT_SIMPLE,
				 "GtkIMContextCyrillicTranslit",
				 &object_info, 0);
}

/* The sequences here match the sequences used in the emacs quail
 * mode cryllic-translit; they allow entering all characters
 * in iso-8859-5
 */
static guint16 cyrillic_compose_seqs[] = {
  GDK_KEY_apostrophe,    0,      0,      0,      0,      0x44C, 	/* CYRILLIC SMALL LETTER SOFT SIGN */
  GDK_KEY_apostrophe,    GDK_KEY_apostrophe,      0,      0,      0,      0x42C, 	/* CYRILLIC CAPITAL LETTER SOFT SIGN */
  GDK_KEY_slash,    GDK_KEY_C,  GDK_KEY_H,      0,      0,      0x040B, /* CYRILLIC CAPITAL LETTER TSHE */
  GDK_KEY_slash,    GDK_KEY_C,  GDK_KEY_h,      0,      0,      0x040B, /* CYRILLIC CAPITAL LETTER TSHE */
  GDK_KEY_slash,    GDK_KEY_D,  0,      0,      0,      0x0402, /* CYRILLIC CAPITAL LETTER DJE */
  GDK_KEY_slash,    GDK_KEY_E,  0,      0,      0,      0x0404, /* CYRILLIC CAPITAL LETTER UKRAINIAN IE */
  GDK_KEY_slash,    GDK_KEY_G,  0,      0,      0,      0x0403, /* CYRILLIC CAPITAL LETTER GJE */
  GDK_KEY_slash,    GDK_KEY_I,  0,      0,      0,      0x0406, /* CYRILLIC CAPITAL LETTER BYELORUSSIAN-UKRAINIAN I */
  GDK_KEY_slash,    GDK_KEY_J,  0,      0,      0,      0x0408, /* CYRILLIC CAPITAL LETTER JE */
  GDK_KEY_slash,    GDK_KEY_K,  0,      0,      0,      0x040C, /* CYRILLIC CAPITAL LETTER KJE */
  GDK_KEY_slash,    GDK_KEY_L,  0,      0,      0,      0x0409, /* CYRILLIC CAPITAL LETTER LJE */
  GDK_KEY_slash,    GDK_KEY_N,  0,      0,      0,      0x040A, /* CYRILLIC CAPITAL LETTER NJE */
  GDK_KEY_slash,    GDK_KEY_S,  0,      0,      0,      0x0405, /* CYRILLIC CAPITAL LETTER DZE */
  GDK_KEY_slash,    GDK_KEY_S,  GDK_KEY_H,  GDK_KEY_T,  0,      0x0429, /* CYRILLIC CAPITAL LETTER SHCH */
  GDK_KEY_slash,    GDK_KEY_S,  GDK_KEY_H,  GDK_KEY_t,  0,      0x0429, /* CYRILLIC CAPITAL LETTER SHCH */
  GDK_KEY_slash,    GDK_KEY_S,  GDK_KEY_h,  GDK_KEY_t,  0,      0x0429, /* CYRILLIC CAPITAL LETTER SHCH */
  GDK_KEY_slash,    GDK_KEY_T,  0,      0,      0,      0x0429, /* CYRILLIC CAPITAL LETTER SHCH */
  GDK_KEY_slash,    GDK_KEY_Z,  0,      0,      0,      0x040F, /* CYRILLIC CAPITAL LETTER DZHE */
  GDK_KEY_slash,    GDK_KEY_c,  GDK_KEY_h,      0,      0,      0x045B, /* CYRILLIC SMALL LETTER TSHE */
  GDK_KEY_slash,    GDK_KEY_d,  0,      0,      0,      0x0452, /* CYRILLIC SMALL LETTER DJE */
  GDK_KEY_slash,    GDK_KEY_e,  0,      0,      0,      0x0454, /* CYRILLIC SMALL LETTER UKRAINIAN IE */
  GDK_KEY_slash,    GDK_KEY_g,  0,      0,      0,      0x0453, /* CYRILLIC SMALL LETTER GJE */
  GDK_KEY_slash,    GDK_KEY_i,  0,      0,      0,      0x0456, /* CYRILLIC SMALL LETTER BYELORUSSIAN-UKRAINIAN I */
  GDK_KEY_slash,    GDK_KEY_j,  0,      0,      0,      0x0458, /* CYRILLIC SMALL LETTER JE */
  GDK_KEY_slash,    GDK_KEY_k,  0,      0,      0,      0x045C, /* CYRILLIC SMALL LETTER KJE */
  GDK_KEY_slash,    GDK_KEY_l,  0,      0,      0,      0x0459, /* CYRILLIC SMALL LETTER LJE */
  GDK_KEY_slash,    GDK_KEY_n,  0,      0,      0,      0x045A, /* CYRILLIC SMALL LETTER NJE */
  GDK_KEY_slash,    GDK_KEY_s,  0,      0,      0,      0x0455, /* CYRILLIC SMALL LETTER DZE */
  GDK_KEY_slash,    GDK_KEY_s,  GDK_KEY_h,  GDK_KEY_t,  0,      0x0449, /* CYRILLIC SMALL LETTER SHCH */
  GDK_KEY_slash,    GDK_KEY_t,  0,      0,      0,      0x0449, /* CYRILLIC SMALL LETTER SHCH */
  GDK_KEY_slash,    GDK_KEY_z,  0,      0,      0,      0x045F, /* CYRILLIC SMALL LETTER DZHE */
  GDK_KEY_A,	0,	0,	0,	0,	0x0410,	/* CYRILLIC CAPITAL LETTER A */
  GDK_KEY_B,	0,	0,	0,	0,	0x0411,	/* CYRILLIC CAPITAL LETTER BE */
  GDK_KEY_C,	0,	0,	0,	0,	0x0426,	/* CYRILLIC CAPITAL LETTER TSE */
  GDK_KEY_C,	GDK_KEY_H,	0,	0,	0,	0x0427,	/* CYRILLIC CAPITAL LETTER CHE */
  GDK_KEY_C,	GDK_KEY_h,	0,	0,	0,	0x0427,	/* CYRILLIC CAPITAL LETTER CHE */
  GDK_KEY_D,	0,	0,	0,	0,	0x0414,	/* CYRILLIC CAPITAL LETTER DE */
  GDK_KEY_E,	0,	0,	0,	0,	0x0415,	/* CYRILLIC CAPITAL LETTER IE */
  GDK_KEY_E,	GDK_KEY_apostrophe,	0,	0,	0,	0x042D,	/* CYRILLIC CAPITAL LETTER E */
  GDK_KEY_F,	0,	0,	0,	0,	0x0424,	/* CYRILLIC CAPITAL LETTER EF */
  GDK_KEY_G,	0,	0,	0,	0,	0x0413,	/* CYRILLIC CAPITAL LETTER GE */
  GDK_KEY_H,	0,	0,	0,	0,	0x0425,	/* CYRILLIC CAPITAL LETTER HA */
  GDK_KEY_I,	0,	0,	0,	0,	0x0418,	/* CYRILLIC CAPITAL LETTER I */
  GDK_KEY_J,	0,	0,	0,	0,	0x0419,	/* CYRILLIC CAPITAL LETTER SHORT I */
  GDK_KEY_J,	GDK_KEY_apostrophe,	0,	0,	0,	0x0419,	/* CYRILLIC CAPITAL LETTER SHORT I */
  GDK_KEY_J,	GDK_KEY_A,	0,	0,	0,	0x042F,	/* CYRILLIC CAPITAL LETTER YA */
  GDK_KEY_J,	GDK_KEY_I,	0,	0,	0,	0x0407,	/* CYRILLIC CAPITAL LETTER YI */
  GDK_KEY_J,	GDK_KEY_O,	0,	0,	0,	0x0401,	/* CYRILLIC CAPITAL LETTER YO */
  GDK_KEY_J,	GDK_KEY_U,	0,	0,	0,	0x042E,	/* CYRILLIC CAPITAL LETTER YA */
  GDK_KEY_J,	GDK_KEY_a,	0,	0,	0,	0x042F,	/* CYRILLIC CAPITAL LETTER YA */
  GDK_KEY_J,	GDK_KEY_i,	0,	0,	0,	0x0407,	/* CYRILLIC CAPITAL LETTER YI */
  GDK_KEY_J,	GDK_KEY_o,	0,	0,	0,	0x0401,	/* CYRILLIC CAPITAL LETTER YO */
  GDK_KEY_J,	GDK_KEY_u,	0,	0,	0,	0x042E,	/* CYRILLIC CAPITAL LETTER YA */
  GDK_KEY_K,	0,	0,	0,	0,	0x041A,	/* CYRILLIC CAPITAL LETTER KA */
  GDK_KEY_K,	GDK_KEY_H,	0,	0,	0,	0x0425,	/* CYRILLIC CAPITAL LETTER HA */
  GDK_KEY_L,	0,	0,	0,	0,	0x041B,	/* CYRILLIC CAPITAL LETTER EL */
  GDK_KEY_M,	0,	0,	0,	0,	0x041C,	/* CYRILLIC CAPITAL LETTER EM */
  GDK_KEY_N,	0,	0,	0,	0,	0x041D,	/* CYRILLIC CAPITAL LETTER EN */
  GDK_KEY_O,	0,	0,	0,	0,	0x041E,	/* CYRILLIC CAPITAL LETTER O */
  GDK_KEY_P,	0,	0,	0,	0,	0x041F,	/* CYRILLIC CAPITAL LETTER PE */
  GDK_KEY_Q,	0,	0,	0,	0,	0x042F,	/* CYRILLIC CAPITAL LETTER YA */
  GDK_KEY_R,	0,	0,	0,	0,	0x0420,	/* CYRILLIC CAPITAL LETTER ER */
  GDK_KEY_S,	0,	0,	0,	0,	0x0421,	/* CYRILLIC CAPITAL LETTER ES */
  GDK_KEY_S,	GDK_KEY_H,	0,	0,	0,	0x0428,	/* CYRILLIC CAPITAL LETTER SHA */
  GDK_KEY_S,	GDK_KEY_H,	GDK_KEY_C,	GDK_KEY_H,	0, 	0x0429,	/* CYRILLIC CAPITAL LETTER SHCA */
  GDK_KEY_S,	GDK_KEY_H,	GDK_KEY_C,	GDK_KEY_h,	0,	0x0429,	/* CYRILLIC CAPITAL LETTER SHCA */
  GDK_KEY_S,	GDK_KEY_H,	GDK_KEY_c,	GDK_KEY_h,	0,	0x0429,	/* CYRILLIC CAPITAL LETTER SHCA */
  GDK_KEY_S,	GDK_KEY_H,	GDK_KEY_c,	GDK_KEY_h,	0,	0x0429,	/* CYRILLIC CAPITAL LETTER SHCA */
  GDK_KEY_S,	GDK_KEY_J,	0,	0,	0,	0x0429,	/* CYRILLIC CAPITAL LETTER SHCA */
  GDK_KEY_S,	GDK_KEY_h,	0,	0,	0,	0x0428,	/* CYRILLIC CAPITAL LETTER SHA */
  GDK_KEY_S,	GDK_KEY_j,	0,	0,	0,	0x0429,	/* CYRILLIC CAPITAL LETTER SHCA */
  GDK_KEY_T,	0,	0,	0,	0,	0x0422,	/* CYRILLIC CAPITAL LETTER TE */
  GDK_KEY_U,	0,	0,	0,	0,	0x0423,	/* CYRILLIC CAPITAL LETTER U */
  GDK_KEY_U,	GDK_KEY_apostrophe,	0,	0,	0,	0x040E,	/* CYRILLIC CAPITAL LETTER SHORT */
  GDK_KEY_V,	0,	0,	0,	0,	0x0412,	/* CYRILLIC CAPITAL LETTER VE */
  GDK_KEY_W,	0,	0,	0,	0,	0x0412,	/* CYRILLIC CAPITAL LETTER VE */
  GDK_KEY_X,	0,	0,	0,	0,	0x0425,	/* CYRILLIC CAPITAL LETTER HA */
  GDK_KEY_Y,	0,	0,	0,	0,	0x042B,	/* CYRILLIC CAPITAL LETTER YERU */
  GDK_KEY_Y,	GDK_KEY_A,	0,	0,	0,	0x042F,	/* CYRILLIC CAPITAL LETTER YA */
  GDK_KEY_Y,	GDK_KEY_I,	0,	0,	0,	0x0407,	/* CYRILLIC CAPITAL LETTER YI */
  GDK_KEY_Y,	GDK_KEY_O,	0,	0,	0,	0x0401,	/* CYRILLIC CAPITAL LETTER YO */
  GDK_KEY_Y,	GDK_KEY_U,	0,	0,	0,	0x042E,	/* CYRILLIC CAPITAL LETTER YU */
  GDK_KEY_Y,	GDK_KEY_a,	0,	0,	0,	0x042F,	/* CYRILLIC CAPITAL LETTER YA */
  GDK_KEY_Y,	GDK_KEY_i,	0,	0,	0,	0x0407,	/* CYRILLIC CAPITAL LETTER YI */
  GDK_KEY_Y,	GDK_KEY_o,	0,	0,	0,	0x0401,	/* CYRILLIC CAPITAL LETTER YO */
  GDK_KEY_Y,	GDK_KEY_u,	0,	0,	0,	0x042E,	/* CYRILLIC CAPITAL LETTER YU */
  GDK_KEY_Z,	0,	0,	0,	0,	0x0417,	/* CYRILLIC CAPITAL LETTER ZE */
  GDK_KEY_Z,	GDK_KEY_H,	0,	0,	0,	0x0416,	/* CYRILLIC CAPITAL LETTER ZHE */
  GDK_KEY_Z,	GDK_KEY_h,	0,	0,	0,	0x0416,	/* CYRILLIC CAPITAL LETTER ZHE */
  GDK_KEY_a,	0,	0,	0,	0,	0x0430,	/* CYRILLIC SMALL LETTER A */
  GDK_KEY_b,	0,	0,	0,	0,	0x0431,	/* CYRILLIC SMALL LETTER BE */
  GDK_KEY_c,	0,	0,	0,	0,	0x0446,	/* CYRILLIC SMALL LETTER TSE */
  GDK_KEY_c,	GDK_KEY_h,	0,	0,	0,	0x0447,	/* CYRILLIC SMALL LETTER CHE */
  GDK_KEY_d,	0,	0,	0,	0,	0x0434,	/* CYRILLIC SMALL LETTER DE */
  GDK_KEY_e,	0,	0,	0,	0,	0x0435,	/* CYRILLIC SMALL LETTER IE */
  GDK_KEY_e,	GDK_KEY_apostrophe,	0,	0,	0,	0x044D,	/* CYRILLIC SMALL LETTER E */
  GDK_KEY_f,	0,	0,	0,	0,	0x0444,	/* CYRILLIC SMALL LETTER EF */
  GDK_KEY_g,	0,	0,	0,	0,	0x0433,	/* CYRILLIC SMALL LETTER GE */
  GDK_KEY_h,	0,	0,	0,	0,	0x0445,	/* CYRILLIC SMALL LETTER HA */
  GDK_KEY_i,	0,	0,	0,	0,	0x0438,	/* CYRILLIC SMALL LETTER I */
  GDK_KEY_j,	0,	0,	0,	0,	0x0439,	/* CYRILLIC SMALL LETTER SHORT I */
  GDK_KEY_j,	GDK_KEY_apostrophe,	0,	0,	0,	0x0439,	/* CYRILLIC SMALL LETTER SHORT I */
  GDK_KEY_j,	GDK_KEY_a,	0,	0,	0,	0x044F,	/* CYRILLIC SMALL LETTER YU */
  GDK_KEY_j,	GDK_KEY_i,	0,	0,	0,	0x0457,	/* CYRILLIC SMALL LETTER YI */
  GDK_KEY_j,	GDK_KEY_o,	0,	0,	0,	0x0451,	/* CYRILLIC SMALL LETTER YO */
  GDK_KEY_j,	GDK_KEY_u,	0,	0,	0,	0x044E,	/* CYRILLIC SMALL LETTER YA */
  GDK_KEY_k,	0,	0,	0,	0,	0x043A,	/* CYRILLIC SMALL LETTER KA */
  GDK_KEY_k,	GDK_KEY_h,	0,	0,	0,	0x0445,	/* CYRILLIC SMALL LETTER HA */
  GDK_KEY_l,	0,	0,	0,	0,	0x043B,	/* CYRILLIC SMALL LETTER EL */
  GDK_KEY_m,	0,	0,	0,	0,	0x043C,	/* CYRILLIC SMALL LETTER EM */
  GDK_KEY_n,	0,	0,	0,	0,	0x043D,	/* CYRILLIC SMALL LETTER EN */
  GDK_KEY_o,	0,	0,	0,	0,	0x043E,	/* CYRILLIC SMALL LETTER O */
  GDK_KEY_p,	0,	0,	0,	0,	0x043F,	/* CYRILLIC SMALL LETTER PE */
  GDK_KEY_q,	0,	0,	0,	0,	0x044F,	/* CYRILLIC SMALL LETTER YA */
  GDK_KEY_r,	0,	0,	0,	0,	0x0440,	/* CYRILLIC SMALL LETTER ER */
  GDK_KEY_s,	0,	0,	0,	0,	0x0441,	/* CYRILLIC SMALL LETTER ES */
  GDK_KEY_s,	GDK_KEY_h,	0,	0,	0,	0x0448,	/* CYRILLIC SMALL LETTER SHA */
  GDK_KEY_s,	GDK_KEY_h,	GDK_KEY_c,	GDK_KEY_h,	0, 	0x0449,	/* CYRILLIC SMALL LETTER SHCA */
  GDK_KEY_s,	GDK_KEY_j,	0,	0,	0,	0x0449,	/* CYRILLIC SMALL LETTER SHCA */
  GDK_KEY_t,	0,	0,	0,	0,	0x0442,	/* CYRILLIC SMALL LETTER TE */
  GDK_KEY_u,	0,	0,	0,	0,	0x0443,	/* CYRILLIC SMALL LETTER U */
  GDK_KEY_u,	GDK_KEY_apostrophe,	0,	0,	0,	0x045E,	/* CYRILLIC SMALL LETTER SHORT */
  GDK_KEY_v,	0,	0,	0,	0,	0x0432,	/* CYRILLIC SMALL LETTER VE */
  GDK_KEY_w,	0,	0,	0,	0,	0x0432,	/* CYRILLIC SMALL LETTER VE */
  GDK_KEY_x,	0,	0,	0,	0,	0x0445,	/* CYRILLIC SMALL LETTER HA */
  GDK_KEY_y,	0,	0,	0,	0,	0x044B,	/* CYRILLIC SMALL LETTER YERU */
  GDK_KEY_y,	GDK_KEY_a,	0,	0,	0,	0x044F,	/* CYRILLIC SMALL LETTER YU */
  GDK_KEY_y,	GDK_KEY_i,	0,	0,	0,	0x0457,	/* CYRILLIC SMALL LETTER YI */
  GDK_KEY_y,	GDK_KEY_o,	0,	0,	0,	0x0451,	/* CYRILLIC SMALL LETTER YO */
  GDK_KEY_y,	GDK_KEY_u,	0,	0,	0,	0x044E,	/* CYRILLIC SMALL LETTER YA */
  GDK_KEY_z,	0,	0,	0,	0,	0x0437,	/* CYRILLIC SMALL LETTER ZE */
  GDK_KEY_z,	GDK_KEY_h,	0,	0,	0,	0x0436,	/* CYRILLIC SMALL LETTER ZHE */
  GDK_KEY_asciitilde,    0,      0,      0,      0,      0x44A, 	/* CYRILLIC SMALL LETTER HARD SIGN */
  GDK_KEY_asciitilde,    GDK_KEY_asciitilde,      0,      0,      0,      0x42A, 	/* CYRILLIC CAPITAL LETTER HARD SIGN */
};

static void
cyrillic_translit_class_init (GtkIMContextSimpleClass *class)
{
}

static void
cyrillic_translit_init (GtkIMContextSimple *im_context)
{
  gtk_im_context_simple_add_table (im_context,
				   cyrillic_compose_seqs,
				   4,
				   G_N_ELEMENTS (cyrillic_compose_seqs) / (4 + 2));
}

static const GtkIMContextInfo cyrillic_translit_info = { 
  "cyrillic_translit",		   /* ID */
  N_("Cyrillic (Transliterated)"), /* Human readable name */
  GETTEXT_PACKAGE,		   /* Translation domain */
   GTK_LOCALEDIR,		   /* Dir for bindtextdomain (not strictly needed for "gtk+") */
  ""			           /* Languages for which this module is the default */
};

static const GtkIMContextInfo *info_list[] = {
  &cyrillic_translit_info
};

#ifndef INCLUDE_IM_cyrillic_translit
#define MODULE_ENTRY(type, function) G_MODULE_EXPORT type im_module_ ## function
#else
#define MODULE_ENTRY(type, function) type _gtk_immodule_cyrillic_translit_ ## function
#endif

MODULE_ENTRY (void, init) (GTypeModule *module)
{
  cyrillic_translit_register_type (module);
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
  if (strcmp (context_id, "cyrillic_translit") == 0)
    return g_object_new (type_cyrillic_translit, NULL);
  else
    return NULL;
}
