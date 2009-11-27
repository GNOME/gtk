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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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

GType type_ipa = 0;

static void ipa_class_init (GtkIMContextSimpleClass *class);
static void ipa_init (GtkIMContextSimple *im_context);

static void
ipa_register_type (GTypeModule *module)
{
  const GTypeInfo object_info =
  {
    sizeof (GtkIMContextSimpleClass),
    (GBaseInitFunc) NULL,
    (GBaseFinalizeFunc) NULL,
    (GClassInitFunc) ipa_class_init,
    NULL,           /* class_finalize */
    NULL,           /* class_data */
    sizeof (GtkIMContextSimple),
    0,
    (GInstanceInitFunc) ipa_init,
  };

  type_ipa = 
    g_type_module_register_type (module,
				 GTK_TYPE_IM_CONTEXT_SIMPLE,
				 "GtkIMContextIpa",
				 &object_info, 0);
}

/* The sequences here match the sequences used in the emacs quail
 * mode cryllic-translit; they allow entering all characters
 * in iso-8859-5
 */
static guint16 ipa_compose_seqs[] = {
  GDK_ampersand, 0,           0,      0,      0,      0x263, 	/* LATIN SMALL LETTER GAMMA */
  GDK_apostrophe, 0,          0,      0,      0,      0x2C8, 	/* MODIFIER LETTER VERTICAL LINE */
  GDK_slash,  GDK_apostrophe, 0,      0,      0,      0x2CA, 	/* MODIFIER LETTER ACUTE ACCENT */
  GDK_slash,  GDK_slash,      0,      0,      0,      0x02F, 	/* SOLIDUS */
  GDK_slash,  GDK_3,          0,      0,      0,      0x25B, 	/* LATIN SMALL LETTER OPEN E */
  GDK_slash,  GDK_A,          0,      0,      0,      0x252, 	/* LATIN LETTER TURNED ALPHA */
  GDK_slash,  GDK_R,          0,      0,      0,      0x281, 	/* LATIN LETTER SMALL CAPITAL INVERTED R */
  GDK_slash,  GDK_a,          0,      0,      0,      0x250, 	/* LATIN SMALL LETTER TURNED A */
  GDK_slash,  GDK_c,          0,      0,      0,      0x254, 	/* LATIN SMALL LETTER OPEN O */
  GDK_slash,  GDK_e,          0,      0,      0,      0x259, 	/* LATIN SMALL LETTER SCHWA */
  GDK_slash,  GDK_h,          0,      0,      0,      0x265, 	/* LATIN SMALL LETTER TURNED H */
  GDK_slash,  GDK_m,          0,      0,      0,      0x26F, 	/* LATIN SMALL LETTER TURNED M */
  GDK_slash,  GDK_r,          0,      0,      0,      0x279, 	/* LATIN SMALL LETTER TURNED R */
  GDK_slash,  GDK_v,          0,      0,      0,      0x28C, 	/* LATIN SMALL LETTER TURNED V */
  GDK_slash,  GDK_w,          0,      0,      0,      0x28D, 	/* LATIN SMALL LETTER TURNED W */
  GDK_slash,  GDK_y,          0,      0,      0,      0x28E, 	/* LATIN SMALL LETTER TRUEND Y*/
  GDK_3,      0,              0,      0,      0,      0x292, 	/* LATIN SMALL LETTER EZH */
  GDK_colon,  0,              0,      0,      0,      0x2D0, 	/* MODIFIER LETTER TRIANGULAR COLON */
  GDK_A,      0,              0,      0,      0,      0x251, 	/* LATIN SMALL LETTER ALPHA */
  GDK_E,      0,              0,      0,      0,      0x25B, 	/* LATIN SMALL LETTER OPEN E */
  GDK_I,      0,              0,      0,      0,      0x26A, 	/* LATIN LETTER SMALL CAPITAL I */
  GDK_L,      0,              0,      0,      0,      0x29F, 	/* LATIN LETTER SMALL CAPITAL L */
  GDK_M,      0,              0,      0,      0,      0x28D, 	/* LATIN SMALL LETTER TURNED W */
  GDK_O,      0,              0,      0,      0,      0x04F, 	/* LATIN LETTER SMALL CAPITAL OE */
  GDK_O,      GDK_E,          0,      0,      0,      0x276, 	/* LATIN LETTER SMALL CAPITAL OE */
  GDK_R,      0,              0,      0,      0,      0x280, 	/* LATIN LETTER SMALL CAPITAL R */
  GDK_U,      0,              0,      0,      0,      0x28A, 	/* LATIN SMALL LETTER UPSILON */
  GDK_Y,      0,              0,      0,      0,      0x28F, 	/* LATIN LETTER SMALL CAPITAL Y */
  GDK_grave,  0,              0,      0,      0,      0x2CC, 	/* MODIFIER LETTER LOW VERTICAL LINE */
  GDK_a,      0,              0,      0,      0,      0x061, 	/* LATIN SMALL LETTER A */
  GDK_a,      GDK_e,          0,      0,      0,      0x0E6, 	/* LATIN SMALL LETTER AE */
  GDK_c,      0,              0,      0,      0,      0x063,    /* LATIN SMALL LETTER C */
  GDK_c,      GDK_comma,      0,      0,      0,      0x0E7,    /* LATIN SMALL LETTER C WITH CEDILLA */
  GDK_d,      0,              0,      0,      0,      0x064, 	/* LATIN SMALL LETTER E */
  GDK_d,      GDK_apostrophe, 0,      0,      0,      0x064, 	/* LATIN SMALL LETTER D */
  GDK_d,      GDK_h,          0,      0,      0,      0x0F0, 	/* LATIN SMALL LETTER ETH */
  GDK_e,      0,              0,      0,      0,      0x065, 	/* LATIN SMALL LETTER E */
  GDK_e,      GDK_minus,      0,      0,      0,      0x25A, 	/* LATIN SMALL LETTER SCHWA WITH HOOK */
  GDK_e,      GDK_bar,        0,      0,      0,      0x25A, 	/* LATIN SMALL LETTER SCHWA WITH HOOK */
  GDK_g,      0,              0,      0,      0,      0x067, 	/* LATIN SMALL LETTER G */
  GDK_g,      GDK_n,          0,      0,      0,      0x272, 	/* LATIN SMALL LETTER N WITH LEFT HOOK */
  GDK_i,      0,              0,      0,      0,      0x069, 	/* LATIN SMALL LETTER I */
  GDK_i,      GDK_minus,      0,      0,      0,      0x268, 	/* LATIN SMALL LETTER I WITH STROKE */
  GDK_n,      0,              0,      0,      0,      0x06e, 	/* LATIN SMALL LETTER N */
  GDK_n,      GDK_g,          0,      0,      0,      0x14B, 	/* LATIN SMALL LETTER ENG */
  GDK_o,      0,              0,      0,      0,      0x06f, 	/* LATIN SMALL LETTER O */
  GDK_o,      GDK_minus,      0,      0,      0,      0x275, 	/* LATIN LETTER BARRED O */
  GDK_o,      GDK_slash,      0,      0,      0,      0x0F8, 	/* LATIN SMALL LETTER O WITH STROKE */
  GDK_o,      GDK_e,          0,      0,      0,      0x153, 	/* LATIN SMALL LIGATURE OE */
  GDK_o,      GDK_bar,        0,      0,      0,      0x251, 	/* LATIN SMALL LETTER ALPHA */
  GDK_s,      0,              0,      0,      0,      0x073, 	/* LATIN SMALL LETTER_ESH */
  GDK_s,      GDK_h,          0,      0,      0,      0x283, 	/* LATIN SMALL LETTER_ESH */
  GDK_t,      0,              0,      0,      0,      0x074, 	/* LATIN SMALL LETTER T */
  GDK_t,      GDK_h,          0,      0,      0,      0x3B8, 	/* GREEK SMALL LETTER THETA */
  GDK_u,      0,              0,      0,      0,      0x075, 	/* LATIN SMALL LETTER U */
  GDK_u,      GDK_minus,      0,      0,      0,      0x289, 	/* LATIN LETTER U BAR */
  GDK_z,      0,              0,      0,      0,      0x07A, 	/* LATIN SMALL LETTER Z */
  GDK_z,      GDK_h,          0,      0,      0,      0x292, 	/* LATIN SMALL LETTER EZH */
  GDK_bar,    GDK_o,          0,      0,      0,      0x252, 	/* LATIN LETTER TURNED ALPHA */

  GDK_asciitilde, 0,          0,      0,      0,      0x303,    /* COMBINING TILDE */

};

static void
ipa_class_init (GtkIMContextSimpleClass *class)
{
}

static void
ipa_init (GtkIMContextSimple *im_context)
{
  gtk_im_context_simple_add_table (im_context,
				   ipa_compose_seqs,
				   4,
				   G_N_ELEMENTS (ipa_compose_seqs) / (4 + 2));
}

static const GtkIMContextInfo ipa_info = { 
  "ipa",		   /* ID */
  N_("IPA"),                       /* Human readable name */
  GETTEXT_PACKAGE,		   /* Translation domain */
   GTK_LOCALEDIR,		   /* Dir for bindtextdomain (not strictly needed for "gtk+") */
  ""			           /* Languages for which this module is the default */
};

static const GtkIMContextInfo *info_list[] = {
  &ipa_info
};

#ifndef INCLUDE_IM_ipa
#define MODULE_ENTRY(type, function) G_MODULE_EXPORT type im_module_ ## function
#else
#define MODULE_ENTRY(type, function) type _gtk_immodule_ipa_ ## function
#endif

MODULE_ENTRY (void, init) (GTypeModule *module)
{
  ipa_register_type (module);
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
  if (strcmp (context_id, "ipa") == 0)
    return g_object_new (type_ipa, NULL);
  else
    return NULL;
}
