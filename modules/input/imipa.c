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
  GDK_KEY_ampersand, 0,           0,      0,      0,      0x263, 	/* LATIN SMALL LETTER GAMMA */
  GDK_KEY_apostrophe, 0,          0,      0,      0,      0x2C8, 	/* MODIFIER LETTER VERTICAL LINE */
  GDK_KEY_slash,  GDK_KEY_apostrophe, 0,      0,      0,      0x2CA, 	/* MODIFIER LETTER ACUTE ACCENT */
  GDK_KEY_slash,  GDK_KEY_slash,      0,      0,      0,      0x02F, 	/* SOLIDUS */
  GDK_KEY_slash,  GDK_KEY_3,          0,      0,      0,      0x25B, 	/* LATIN SMALL LETTER OPEN E */
  GDK_KEY_slash,  GDK_KEY_A,          0,      0,      0,      0x252, 	/* LATIN LETTER TURNED ALPHA */
  GDK_KEY_slash,  GDK_KEY_R,          0,      0,      0,      0x281, 	/* LATIN LETTER SMALL CAPITAL INVERTED R */
  GDK_KEY_slash,  GDK_KEY_a,          0,      0,      0,      0x250, 	/* LATIN SMALL LETTER TURNED A */
  GDK_KEY_slash,  GDK_KEY_c,          0,      0,      0,      0x254, 	/* LATIN SMALL LETTER OPEN O */
  GDK_KEY_slash,  GDK_KEY_e,          0,      0,      0,      0x259, 	/* LATIN SMALL LETTER SCHWA */
  GDK_KEY_slash,  GDK_KEY_h,          0,      0,      0,      0x265, 	/* LATIN SMALL LETTER TURNED H */
  GDK_KEY_slash,  GDK_KEY_m,          0,      0,      0,      0x26F, 	/* LATIN SMALL LETTER TURNED M */
  GDK_KEY_slash,  GDK_KEY_r,          0,      0,      0,      0x279, 	/* LATIN SMALL LETTER TURNED R */
  GDK_KEY_slash,  GDK_KEY_v,          0,      0,      0,      0x28C, 	/* LATIN SMALL LETTER TURNED V */
  GDK_KEY_slash,  GDK_KEY_w,          0,      0,      0,      0x28D, 	/* LATIN SMALL LETTER TURNED W */
  GDK_KEY_slash,  GDK_KEY_y,          0,      0,      0,      0x28E, 	/* LATIN SMALL LETTER TRUEND Y*/
  GDK_KEY_3,      0,              0,      0,      0,      0x292, 	/* LATIN SMALL LETTER EZH */
  GDK_KEY_colon,  0,              0,      0,      0,      0x2D0, 	/* MODIFIER LETTER TRIANGULAR COLON */
  GDK_KEY_A,      0,              0,      0,      0,      0x251, 	/* LATIN SMALL LETTER ALPHA */
  GDK_KEY_E,      0,              0,      0,      0,      0x25B, 	/* LATIN SMALL LETTER OPEN E */
  GDK_KEY_I,      0,              0,      0,      0,      0x26A, 	/* LATIN LETTER SMALL CAPITAL I */
  GDK_KEY_L,      0,              0,      0,      0,      0x29F, 	/* LATIN LETTER SMALL CAPITAL L */
  GDK_KEY_M,      0,              0,      0,      0,      0x28D, 	/* LATIN SMALL LETTER TURNED W */
  GDK_KEY_O,      0,              0,      0,      0,      0x04F, 	/* LATIN LETTER SMALL CAPITAL OE */
  GDK_KEY_O,      GDK_KEY_E,          0,      0,      0,      0x276, 	/* LATIN LETTER SMALL CAPITAL OE */
  GDK_KEY_R,      0,              0,      0,      0,      0x280, 	/* LATIN LETTER SMALL CAPITAL R */
  GDK_KEY_U,      0,              0,      0,      0,      0x28A, 	/* LATIN SMALL LETTER UPSILON */
  GDK_KEY_Y,      0,              0,      0,      0,      0x28F, 	/* LATIN LETTER SMALL CAPITAL Y */
  GDK_KEY_grave,  0,              0,      0,      0,      0x2CC, 	/* MODIFIER LETTER LOW VERTICAL LINE */
  GDK_KEY_a,      0,              0,      0,      0,      0x061, 	/* LATIN SMALL LETTER A */
  GDK_KEY_a,      GDK_KEY_e,          0,      0,      0,      0x0E6, 	/* LATIN SMALL LETTER AE */
  GDK_KEY_c,      0,              0,      0,      0,      0x063,    /* LATIN SMALL LETTER C */
  GDK_KEY_c,      GDK_KEY_comma,      0,      0,      0,      0x0E7,    /* LATIN SMALL LETTER C WITH CEDILLA */
  GDK_KEY_d,      0,              0,      0,      0,      0x064, 	/* LATIN SMALL LETTER E */
  GDK_KEY_d,      GDK_KEY_apostrophe, 0,      0,      0,      0x064, 	/* LATIN SMALL LETTER D */
  GDK_KEY_d,      GDK_KEY_h,          0,      0,      0,      0x0F0, 	/* LATIN SMALL LETTER ETH */
  GDK_KEY_e,      0,              0,      0,      0,      0x065, 	/* LATIN SMALL LETTER E */
  GDK_KEY_e,      GDK_KEY_minus,      0,      0,      0,      0x25A, 	/* LATIN SMALL LETTER SCHWA WITH HOOK */
  GDK_KEY_e,      GDK_KEY_bar,        0,      0,      0,      0x25A, 	/* LATIN SMALL LETTER SCHWA WITH HOOK */
  GDK_KEY_g,      0,              0,      0,      0,      0x067, 	/* LATIN SMALL LETTER G */
  GDK_KEY_g,      GDK_KEY_n,          0,      0,      0,      0x272, 	/* LATIN SMALL LETTER N WITH LEFT HOOK */
  GDK_KEY_i,      0,              0,      0,      0,      0x069, 	/* LATIN SMALL LETTER I */
  GDK_KEY_i,      GDK_KEY_minus,      0,      0,      0,      0x268, 	/* LATIN SMALL LETTER I WITH STROKE */
  GDK_KEY_n,      0,              0,      0,      0,      0x06e, 	/* LATIN SMALL LETTER N */
  GDK_KEY_n,      GDK_KEY_g,          0,      0,      0,      0x14B, 	/* LATIN SMALL LETTER ENG */
  GDK_KEY_o,      0,              0,      0,      0,      0x06f, 	/* LATIN SMALL LETTER O */
  GDK_KEY_o,      GDK_KEY_minus,      0,      0,      0,      0x275, 	/* LATIN LETTER BARRED O */
  GDK_KEY_o,      GDK_KEY_slash,      0,      0,      0,      0x0F8, 	/* LATIN SMALL LETTER O WITH STROKE */
  GDK_KEY_o,      GDK_KEY_e,          0,      0,      0,      0x153, 	/* LATIN SMALL LIGATURE OE */
  GDK_KEY_o,      GDK_KEY_bar,        0,      0,      0,      0x251, 	/* LATIN SMALL LETTER ALPHA */
  GDK_KEY_s,      0,              0,      0,      0,      0x073, 	/* LATIN SMALL LETTER_ESH */
  GDK_KEY_s,      GDK_KEY_h,          0,      0,      0,      0x283, 	/* LATIN SMALL LETTER_ESH */
  GDK_KEY_t,      0,              0,      0,      0,      0x074, 	/* LATIN SMALL LETTER T */
  GDK_KEY_t,      GDK_KEY_h,          0,      0,      0,      0x3B8, 	/* GREEK SMALL LETTER THETA */
  GDK_KEY_u,      0,              0,      0,      0,      0x075, 	/* LATIN SMALL LETTER U */
  GDK_KEY_u,      GDK_KEY_minus,      0,      0,      0,      0x289, 	/* LATIN LETTER U BAR */
  GDK_KEY_z,      0,              0,      0,      0,      0x07A, 	/* LATIN SMALL LETTER Z */
  GDK_KEY_z,      GDK_KEY_h,          0,      0,      0,      0x292, 	/* LATIN SMALL LETTER EZH */
  GDK_KEY_bar,    GDK_KEY_o,          0,      0,      0,      0x252, 	/* LATIN LETTER TURNED ALPHA */

  GDK_KEY_asciitilde, 0,          0,      0,      0,      0x303,    /* COMBINING TILDE */

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
  NC_("input method menu", "IPA"), /* Human readable name */
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
