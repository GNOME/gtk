/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc.
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
 */

#include <unicode.h>
#include <gdk/gdkkeysyms.h>
#include "gtksignal.h"
#include "gtkimcontextsimple.h"

static void     gtk_im_context_simple_class_init (GtkIMContextSimpleClass *class);
static void     gtk_im_context_simple_init (GtkIMContextSimple *im_context_simple);

static gboolean gtk_im_context_simple_filter_keypress (GtkIMContext *context,
						       GdkEventKey  *key);

GtkType
gtk_im_context_simple_get_type (void)
{
  static GtkType im_context_simple_type = 0;

  if (!im_context_simple_type)
    {
      static const GtkTypeInfo im_context_simple_info =
      {
	"GtkIMContextSimple",
	sizeof (GtkIMContextSimple),
	sizeof (GtkIMContextSimpleClass),
	(GtkClassInitFunc) gtk_im_context_simple_class_init,
	(GtkObjectInitFunc) gtk_im_context_simple_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      im_context_simple_type = gtk_type_unique (GTK_TYPE_IM_CONTEXT, &im_context_simple_info);
    }

  return im_context_simple_type;
}

static void
gtk_im_context_simple_class_init (GtkIMContextSimpleClass *class)
{
  GtkIMContextClass *im_context_class = GTK_IM_CONTEXT_CLASS (class);
  
  im_context_class->filter_keypress = gtk_im_context_simple_filter_keypress;
}

static void
gtk_im_context_simple_init (GtkIMContextSimple *im_context_simple)
{
}

GtkIMContext *
gtk_im_context_simple_new (void)
{
  return GTK_IM_CONTEXT (gtk_type_new (GTK_TYPE_IM_CONTEXT_SIMPLE));
}

static struct {
  guint keyval;
  unicode_char_t ch;
} keyval_to_unicode[] = {
  { GDK_space,      0x020 },
  { GDK_exclam,     0x021 },
  { GDK_quotedbl,   0x022 },
  { GDK_numbersign, 0x023 },
  { GDK_dollar,     0x024 },
  { GDK_percent,    0x025 },
  { GDK_ampersand,  0x026 },
  { GDK_apostrophe, 0x027 },
  { GDK_quoteright, 0x027 },
  { GDK_parenleft,  0x028 },
  { GDK_parenright, 0x029 },
  { GDK_asterisk,   0x02a },
  { GDK_plus,       0x02b },
  { GDK_comma,      0x02c },
  { GDK_minus, 0x02d },
  { GDK_period, 0x02e },
  { GDK_slash, 0x02f },
  { GDK_0, 0x030 },
  { GDK_1, 0x031 },
  { GDK_2, 0x032 },
  { GDK_3, 0x033 },
  { GDK_4, 0x034 },
  { GDK_5, 0x035 },
  { GDK_6, 0x036 },
  { GDK_7, 0x037 },
  { GDK_8, 0x038 },
  { GDK_9, 0x039 },
  { GDK_colon, 0x03a },
  { GDK_semicolon, 0x03b },
  { GDK_less, 0x03c },
  { GDK_equal, 0x03d },
  { GDK_greater, 0x03e },
  { GDK_question, 0x03f },
  { GDK_at, 0x040 },
  { GDK_A, 0x041 },
  { GDK_B, 0x042 },
  { GDK_C, 0x043 },
  { GDK_D, 0x044 },
  { GDK_E, 0x045 },
  { GDK_F, 0x046 },
  { GDK_G, 0x047 },
  { GDK_H, 0x048 },
  { GDK_I, 0x049 },
  { GDK_J, 0x04a },
  { GDK_K, 0x04b },
  { GDK_L, 0x04c },
  { GDK_M, 0x04d },
  { GDK_N, 0x04e },
  { GDK_O, 0x04f },
  { GDK_P, 0x050 },
  { GDK_Q, 0x051 },
  { GDK_R, 0x052 },
  { GDK_S, 0x053 },
  { GDK_T, 0x054 },
  { GDK_U, 0x055 },
  { GDK_V, 0x056 },
  { GDK_W, 0x057 },
  { GDK_X, 0x058 },
  { GDK_Y, 0x059 },
  { GDK_Z, 0x05a },
  { GDK_bracketleft, 0x05b },
  { GDK_backslash, 0x05c },
  { GDK_bracketright, 0x05d },
  { GDK_asciicircum, 0x05e },
  { GDK_underscore, 0x05f },
  { GDK_grave, 0x060 },
  { GDK_a, 0x061 },
  { GDK_b, 0x062 },
  { GDK_c, 0x063 },
  { GDK_d, 0x064 },
  { GDK_e, 0x065 },
  { GDK_f, 0x066 },
  { GDK_g, 0x067 },
  { GDK_h, 0x068 },
  { GDK_i, 0x069 },
  { GDK_j, 0x06a },
  { GDK_k, 0x06b },
  { GDK_l, 0x06c },
  { GDK_m, 0x06d },
  { GDK_n, 0x06e },
  { GDK_o, 0x06f },
  { GDK_p, 0x070 },
  { GDK_q, 0x071 },
  { GDK_r, 0x072 },
  { GDK_s, 0x073 },
  { GDK_t, 0x074 },
  { GDK_u, 0x075 },
  { GDK_v, 0x076 },
  { GDK_w, 0x077 },
  { GDK_x, 0x078 },
  { GDK_y, 0x079 },
  { GDK_z, 0x07a },
  { GDK_braceleft, 0x07b },
  { GDK_bar, 0x07c },
  { GDK_braceright, 0x07d },
  { GDK_asciitilde, 0x07e },
  { GDK_nobreakspace, 0x0a0 },
  { GDK_exclamdown, 0x0a1 },
  { GDK_cent, 0x0a2 },
  { GDK_sterling, 0x0a3 },
  { GDK_currency, 0x0a4 },
  { GDK_yen, 0x0a5 },
  { GDK_brokenbar, 0x0a6 },
  { GDK_section, 0x0a7 },
  { GDK_diaeresis, 0x0a8 },
  { GDK_copyright, 0x0a9 },
  { GDK_ordfeminine, 0x0aa },
  { GDK_guillemotleft, 0x0ab },
  { GDK_notsign, 0x0ac },
  { GDK_hyphen, 0x0ad },
  { GDK_registered, 0x0ae },
  { GDK_macron, 0x0af },
  { GDK_degree, 0x0b0 },
  { GDK_plusminus, 0x0b1 },
  { GDK_twosuperior, 0x0b2 },
  { GDK_threesuperior, 0x0b3 },
  { GDK_acute, 0x0b4 },
  { GDK_mu, 0x0b5 },
  { GDK_paragraph, 0x0b6 },
  { GDK_periodcentered, 0x0b7 },
  { GDK_cedilla,        0x0b8 },
  { GDK_onesuperior,    0x0b9 },
  { GDK_masculine,      0x0ba },
  { GDK_guillemotright, 0x0bb },
  { GDK_onequarter,     0x0bc },
  { GDK_onehalf,        0x0bd },
  { GDK_threequarters,  0x0be },
  { GDK_questiondown,   0x0bf },
  { GDK_Agrave,         0x0c0 },
  { GDK_Aacute,         0x0c1 },
  { GDK_Acircumflex,    0x0c2 },
  { GDK_Atilde,         0x0c3 },
  { GDK_Adiaeresis,     0x0c4 },
  { GDK_Aring,          0x0c5 },
  { GDK_AE,             0x0c6 },
  { GDK_Ccedilla,       0x0c7 },
  { GDK_Egrave,         0x0c8 },
  { GDK_Eacute,         0x0c9 },
  { GDK_Ecircumflex,    0x0ca },
  { GDK_Ediaeresis,     0x0cb },
  { GDK_Igrave,         0x0cc },
  { GDK_Iacute,         0x0cd },
  { GDK_Icircumflex,    0x0ce },
  { GDK_Idiaeresis,     0x0cf },
  { GDK_Eth,            0x0d0 },
  { GDK_Ntilde,         0x0d1 },
  { GDK_Ograve,         0x0d2 },
  { GDK_Oacute, 0x0d3 },
  { GDK_Ocircumflex, 0x0d4 },
  { GDK_Otilde, 0x0d5 },
  { GDK_Odiaeresis, 0x0d6 },
  { GDK_multiply, 0x0d7 },
  { GDK_Ooblique, 0x0d8 },
  { GDK_Ugrave, 0x0d9 },
  { GDK_Uacute, 0x0da },
  { GDK_Ucircumflex, 0x0db },
  { GDK_Udiaeresis, 0x0dc },
  { GDK_Yacute, 0x0dd },
  { GDK_Thorn, 0x0de },
  { GDK_ssharp, 0x0df },
  { GDK_agrave, 0x0e0 },
  { GDK_aacute, 0x0e1 },
  { GDK_acircumflex, 0x0e2 },
  { GDK_atilde, 0x0e3 },
  { GDK_adiaeresis, 0x0e4 },
  { GDK_aring, 0x0e5 },
  { GDK_ae, 0x0e6 },
  { GDK_ccedilla, 0x0e7 },
  { GDK_egrave, 0x0e8 },
  { GDK_eacute, 0x0e9 },
  { GDK_ecircumflex, 0x0ea },
  { GDK_ediaeresis, 0x0eb },
  { GDK_igrave, 0x0ec },
  { GDK_iacute, 0x0ed },
  { GDK_icircumflex, 0x0ee },
  { GDK_idiaeresis, 0x0ef },
  { GDK_eth, 0x0f0 },
  { GDK_ntilde, 0x0f1 },
  { GDK_ograve, 0x0f2 },
  { GDK_oacute, 0x0f3 },
  { GDK_ocircumflex,      0x0f4 },
  { GDK_otilde,           0x0f5 },
  { GDK_odiaeresis,       0x0f6 },
  { GDK_division,         0x0f7 },
  { GDK_oslash,           0x0f8 },
  { GDK_ugrave,           0x0f9 },
  { GDK_uacute,           0x0fa },
  { GDK_ucircumflex,      0x0fb },
  { GDK_udiaeresis,       0x0fc },
  { GDK_yacute,           0x0fd },
  { GDK_thorn,            0x0fe },
  { GDK_ydiaeresis,       0x0ff },
  { GDK_hebrew_aleph,     0x5d0 },
  { GDK_hebrew_bet,       0x5d1 },
  { GDK_hebrew_gimel,     0x5d2 },
  { GDK_hebrew_dalet,     0x5d3 },
  { GDK_hebrew_he,        0x5d4 },
  { GDK_hebrew_waw,       0x5d5 },
  { GDK_hebrew_zayin,     0x5d6 },
  { GDK_hebrew_het,       0x5d7 },
  { GDK_hebrew_tet,       0x5d8 },
  { GDK_hebrew_yod,       0x5d9 },
  { GDK_hebrew_finalkaph, 0x5da },
  { GDK_hebrew_kaph,      0x5db },
  { GDK_hebrew_lamed,     0x5dc },
  { GDK_hebrew_finalmem,  0x5dd },
  { GDK_hebrew_mem,       0x5de },
  { GDK_hebrew_finalnun,  0x5df },
  { GDK_hebrew_nun,       0x5e0 },
  { GDK_hebrew_samech,    0x5e1 },
  { GDK_hebrew_ayin,      0x5e2 },
  { GDK_hebrew_finalpe,   0x5e3 },
  { GDK_hebrew_pe,        0x5e4 },
  { GDK_hebrew_finalzade, 0x5e5 },
  { GDK_hebrew_zade,      0x5e6 },
  { GDK_hebrew_qoph,      0x5e7 },
  { GDK_hebrew_resh,      0x5e8 },
  { GDK_hebrew_shin,      0x5e9 },
  { GDK_hebrew_taw,       0x5ea }
};

static unicode_char_t
find_unicode (gint keyval)
{
  gint first = 0;
  gint last = G_N_ELEMENTS (keyval_to_unicode) - 1;

#define KEYVAL(ind) keyval_to_unicode[ind].keyval
  
  if (KEYVAL (first) >= keyval)
    return KEYVAL(first) == keyval ? keyval_to_unicode[first].ch : 0;
  if (KEYVAL (last) <= keyval)
    return KEYVAL(last) == keyval ? keyval_to_unicode[last].ch : 0;

  /* Invariant: KEYVAL(first) < keyval < KEYVAL(LAST) */

  do
    {
      gint middle = (first + last) / 2;
      if (KEYVAL(middle) > keyval)
	last = middle;
      else if (KEYVAL(middle) == keyval)
	return keyval_to_unicode[middle].ch;
      else
	first = middle;
    }
  while (last > first + 1);

  return 0;

#undef KEVAL  
}

/**
 * unicode_guchar4_to_utf8:
 * @ch: a ISO10646 character code
 * @out: output buffer, must have at least 6 bytes of space.
 * 
 * Convert a single character to utf8
 * 
 * Return value: number of bytes written
 **/
static int
ucs4_to_utf8 (unicode_char_t c, char *outbuf)
{
  size_t len = 0;
  int first;
  int i;

  if (c < 0x80)
    {
      first = 0;
      len = 1;
    }
  else if (c < 0x800)
    {
      first = 0xc0;
      len = 2;
    }
  else if (c < 0x10000)
    {
      first = 0xe0;
      len = 3;
    }
   else if (c < 0x200000)
    {
      first = 0xf0;
      len = 4;
    }
  else if (c < 0x4000000)
    {
      first = 0xf8;
      len = 5;
    }
  else
    {
      first = 0xfc;
      len = 6;
    }

  for (i = len - 1; i > 0; --i)
    {
      outbuf[i] = (c & 0x3f) | 0x80;
      c >>= 6;
    }
  outbuf[0] = c | first;

  return len;
}

static gboolean
gtk_im_context_simple_filter_keypress (GtkIMContext *context,
				       GdkEventKey  *event)
{
  unicode_char_t ch = find_unicode (event->keyval);
	  
  if (ch != 0)
    {
      gchar buf[7];
      gint len;
      
      len = ucs4_to_utf8 (ch, buf);
      buf[len] = '\0';

      gtk_signal_emit_by_name (GTK_OBJECT (context), "commit", &buf);
      
      return TRUE;
    }
  else
    return FALSE;
}
