/* GDK - The GIMP Drawing Kit
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.
 */

/*
 * GTK+ DirectFB backend
 * Copyright (C) 2001-2002  convergence integrated media GmbH
 * Copyright (C) 2002       convergence GmbH
 * Written by Denis Oliver Kropp <dok@convergence.de> and
 *            Sven Neumann <sven@convergence.de>
 */

#include "config.h"
#include "gdk.h"

#include <stdlib.h>
#include <string.h>


#include "gdkdirectfb.h"
#include "gdkprivate-directfb.h"

#include "gdkkeysyms.h"
#include "gdkalias.h"

static struct gdk_key *gdk_keys_by_name = NULL;

GdkModifierType  _gdk_directfb_modifiers = 0;

static guint     *directfb_keymap        = NULL;
static gint       directfb_min_keycode   = 0;
static gint       directfb_max_keycode   = 0;


/*
 *  This array needs to be sorted by key values. It can be generated
 *  from gdkkeysyms.h using a few lines of Perl. This is a bit more
 *  complex as one would expect since GDK defines multiple names for a
 *  few key values. We throw the most akward of them away (GDK_Ln and
 *  GDK_Rn).  This code seems to do the trick:
 *
 *      while (<>) {
 *          if (/^\#define GDK\_(\w+) 0x([0-9|a-f|A-F|]+)/) {
 *	        push @{ $names{hex $2} }, $1;
 *          }
 *      }
 *      foreach $value (sort { $a <=> $b } keys %names) {
 *          for (@{ $names{$value} } ) {
 * 	        next if (/^[R|L]\d+/);
 *              print "  { GDK_$_, \"$_\" },\n";
 *          }
 *      }
 */
static const struct gdk_key
{
  guint        keyval;
  const gchar *name;
} gdk_keys_by_keyval[] = {
  { GDK_space, "space" },
  { GDK_exclam, "exclam" },
  { GDK_quotedbl, "quotedbl" },
  { GDK_numbersign, "numbersign" },
  { GDK_dollar, "dollar" },
  { GDK_percent, "percent" },
  { GDK_ampersand, "ampersand" },
  { GDK_apostrophe, "apostrophe" },
  { GDK_quoteright, "quoteright" },
  { GDK_parenleft, "parenleft" },
  { GDK_parenright, "parenright" },
  { GDK_asterisk, "asterisk" },
  { GDK_plus, "plus" },
  { GDK_comma, "comma" },
  { GDK_minus, "minus" },
  { GDK_period, "period" },
  { GDK_slash, "slash" },
  { GDK_0, "0" },
  { GDK_1, "1" },
  { GDK_2, "2" },
  { GDK_3, "3" },
  { GDK_4, "4" },
  { GDK_5, "5" },
  { GDK_6, "6" },
  { GDK_7, "7" },
  { GDK_8, "8" },
  { GDK_9, "9" },
  { GDK_colon, "colon" },
  { GDK_semicolon, "semicolon" },
  { GDK_less, "less" },
  { GDK_equal, "equal" },
  { GDK_greater, "greater" },
  { GDK_question, "question" },
  { GDK_at, "at" },
  { GDK_A, "A" },
  { GDK_B, "B" },
  { GDK_C, "C" },
  { GDK_D, "D" },
  { GDK_E, "E" },
  { GDK_F, "F" },
  { GDK_G, "G" },
  { GDK_H, "H" },
  { GDK_I, "I" },
  { GDK_J, "J" },
  { GDK_K, "K" },
  { GDK_L, "L" },
  { GDK_M, "M" },
  { GDK_N, "N" },
  { GDK_O, "O" },
  { GDK_P, "P" },
  { GDK_Q, "Q" },
  { GDK_R, "R" },
  { GDK_S, "S" },
  { GDK_T, "T" },
  { GDK_U, "U" },
  { GDK_V, "V" },
  { GDK_W, "W" },
  { GDK_X, "X" },
  { GDK_Y, "Y" },
  { GDK_Z, "Z" },
  { GDK_bracketleft, "bracketleft" },
  { GDK_backslash, "backslash" },
  { GDK_bracketright, "bracketright" },
  { GDK_asciicircum, "asciicircum" },
  { GDK_underscore, "underscore" },
  { GDK_grave, "grave" },
  { GDK_quoteleft, "quoteleft" },
  { GDK_a, "a" },
  { GDK_b, "b" },
  { GDK_c, "c" },
  { GDK_d, "d" },
  { GDK_e, "e" },
  { GDK_f, "f" },
  { GDK_g, "g" },
  { GDK_h, "h" },
  { GDK_i, "i" },
  { GDK_j, "j" },
  { GDK_k, "k" },
  { GDK_l, "l" },
  { GDK_m, "m" },
  { GDK_n, "n" },
  { GDK_o, "o" },
  { GDK_p, "p" },
  { GDK_q, "q" },
  { GDK_r, "r" },
  { GDK_s, "s" },
  { GDK_t, "t" },
  { GDK_u, "u" },
  { GDK_v, "v" },
  { GDK_w, "w" },
  { GDK_x, "x" },
  { GDK_y, "y" },
  { GDK_z, "z" },
  { GDK_braceleft, "braceleft" },
  { GDK_bar, "bar" },
  { GDK_braceright, "braceright" },
  { GDK_asciitilde, "asciitilde" },
  { GDK_nobreakspace, "nobreakspace" },
  { GDK_exclamdown, "exclamdown" },
  { GDK_cent, "cent" },
  { GDK_sterling, "sterling" },
  { GDK_currency, "currency" },
  { GDK_yen, "yen" },
  { GDK_brokenbar, "brokenbar" },
  { GDK_section, "section" },
  { GDK_diaeresis, "diaeresis" },
  { GDK_copyright, "copyright" },
  { GDK_ordfeminine, "ordfeminine" },
  { GDK_guillemotleft, "guillemotleft" },
  { GDK_notsign, "notsign" },
  { GDK_hyphen, "hyphen" },
  { GDK_registered, "registered" },
  { GDK_macron, "macron" },
  { GDK_degree, "degree" },
  { GDK_plusminus, "plusminus" },
  { GDK_twosuperior, "twosuperior" },
  { GDK_threesuperior, "threesuperior" },
  { GDK_acute, "acute" },
  { GDK_mu, "mu" },
  { GDK_paragraph, "paragraph" },
  { GDK_periodcentered, "periodcentered" },
  { GDK_cedilla, "cedilla" },
  { GDK_onesuperior, "onesuperior" },
  { GDK_masculine, "masculine" },
  { GDK_guillemotright, "guillemotright" },
  { GDK_onequarter, "onequarter" },
  { GDK_onehalf, "onehalf" },
  { GDK_threequarters, "threequarters" },
  { GDK_questiondown, "questiondown" },
  { GDK_Agrave, "Agrave" },
  { GDK_Aacute, "Aacute" },
  { GDK_Acircumflex, "Acircumflex" },
  { GDK_Atilde, "Atilde" },
  { GDK_Adiaeresis, "Adiaeresis" },
  { GDK_Aring, "Aring" },
  { GDK_AE, "AE" },
  { GDK_Ccedilla, "Ccedilla" },
  { GDK_Egrave, "Egrave" },
  { GDK_Eacute, "Eacute" },
  { GDK_Ecircumflex, "Ecircumflex" },
  { GDK_Ediaeresis, "Ediaeresis" },
  { GDK_Igrave, "Igrave" },
  { GDK_Iacute, "Iacute" },
  { GDK_Icircumflex, "Icircumflex" },
  { GDK_Idiaeresis, "Idiaeresis" },
  { GDK_ETH, "ETH" },
  { GDK_Eth, "Eth" },
  { GDK_Ntilde, "Ntilde" },
  { GDK_Ograve, "Ograve" },
  { GDK_Oacute, "Oacute" },
  { GDK_Ocircumflex, "Ocircumflex" },
  { GDK_Otilde, "Otilde" },
  { GDK_Odiaeresis, "Odiaeresis" },
  { GDK_multiply, "multiply" },
  { GDK_Ooblique, "Ooblique" },
  { GDK_Ugrave, "Ugrave" },
  { GDK_Uacute, "Uacute" },
  { GDK_Ucircumflex, "Ucircumflex" },
  { GDK_Udiaeresis, "Udiaeresis" },
  { GDK_Yacute, "Yacute" },
  { GDK_THORN, "THORN" },
  { GDK_Thorn, "Thorn" },
  { GDK_ssharp, "ssharp" },
  { GDK_agrave, "agrave" },
  { GDK_aacute, "aacute" },
  { GDK_acircumflex, "acircumflex" },
  { GDK_atilde, "atilde" },
  { GDK_adiaeresis, "adiaeresis" },
  { GDK_aring, "aring" },
  { GDK_ae, "ae" },
  { GDK_ccedilla, "ccedilla" },
  { GDK_egrave, "egrave" },
  { GDK_eacute, "eacute" },
  { GDK_ecircumflex, "ecircumflex" },
  { GDK_ediaeresis, "ediaeresis" },
  { GDK_igrave, "igrave" },
  { GDK_iacute, "iacute" },
  { GDK_icircumflex, "icircumflex" },
  { GDK_idiaeresis, "idiaeresis" },
  { GDK_eth, "eth" },
  { GDK_ntilde, "ntilde" },
  { GDK_ograve, "ograve" },
  { GDK_oacute, "oacute" },
  { GDK_ocircumflex, "ocircumflex" },
  { GDK_otilde, "otilde" },
  { GDK_odiaeresis, "odiaeresis" },
  { GDK_division, "division" },
  { GDK_oslash, "oslash" },
  { GDK_ugrave, "ugrave" },
  { GDK_uacute, "uacute" },
  { GDK_ucircumflex, "ucircumflex" },
  { GDK_udiaeresis, "udiaeresis" },
  { GDK_yacute, "yacute" },
  { GDK_thorn, "thorn" },
  { GDK_ydiaeresis, "ydiaeresis" },
  { GDK_Aogonek, "Aogonek" },
  { GDK_breve, "breve" },
  { GDK_Lstroke, "Lstroke" },
  { GDK_Lcaron, "Lcaron" },
  { GDK_Sacute, "Sacute" },
  { GDK_Scaron, "Scaron" },
  { GDK_Scedilla, "Scedilla" },
  { GDK_Tcaron, "Tcaron" },
  { GDK_Zacute, "Zacute" },
  { GDK_Zcaron, "Zcaron" },
  { GDK_Zabovedot, "Zabovedot" },
  { GDK_aogonek, "aogonek" },
  { GDK_ogonek, "ogonek" },
  { GDK_lstroke, "lstroke" },
  { GDK_lcaron, "lcaron" },
  { GDK_sacute, "sacute" },
  { GDK_caron, "caron" },
  { GDK_scaron, "scaron" },
  { GDK_scedilla, "scedilla" },
  { GDK_tcaron, "tcaron" },
  { GDK_zacute, "zacute" },
  { GDK_doubleacute, "doubleacute" },
  { GDK_zcaron, "zcaron" },
  { GDK_zabovedot, "zabovedot" },
  { GDK_Racute, "Racute" },
  { GDK_Abreve, "Abreve" },
  { GDK_Lacute, "Lacute" },
  { GDK_Cacute, "Cacute" },
  { GDK_Ccaron, "Ccaron" },
  { GDK_Eogonek, "Eogonek" },
  { GDK_Ecaron, "Ecaron" },
  { GDK_Dcaron, "Dcaron" },
  { GDK_Dstroke, "Dstroke" },
  { GDK_Nacute, "Nacute" },
  { GDK_Ncaron, "Ncaron" },
  { GDK_Odoubleacute, "Odoubleacute" },
  { GDK_Rcaron, "Rcaron" },
  { GDK_Uring, "Uring" },
  { GDK_Udoubleacute, "Udoubleacute" },
  { GDK_Tcedilla, "Tcedilla" },
  { GDK_racute, "racute" },
  { GDK_abreve, "abreve" },
  { GDK_lacute, "lacute" },
  { GDK_cacute, "cacute" },
  { GDK_ccaron, "ccaron" },
  { GDK_eogonek, "eogonek" },
  { GDK_ecaron, "ecaron" },
  { GDK_dcaron, "dcaron" },
  { GDK_dstroke, "dstroke" },
  { GDK_nacute, "nacute" },
  { GDK_ncaron, "ncaron" },
  { GDK_odoubleacute, "odoubleacute" },
  { GDK_rcaron, "rcaron" },
  { GDK_uring, "uring" },
  { GDK_udoubleacute, "udoubleacute" },
  { GDK_tcedilla, "tcedilla" },
  { GDK_abovedot, "abovedot" },
  { GDK_Hstroke, "Hstroke" },
  { GDK_Hcircumflex, "Hcircumflex" },
  { GDK_Iabovedot, "Iabovedot" },
  { GDK_Gbreve, "Gbreve" },
  { GDK_Jcircumflex, "Jcircumflex" },
  { GDK_hstroke, "hstroke" },
  { GDK_hcircumflex, "hcircumflex" },
  { GDK_idotless, "idotless" },
  { GDK_gbreve, "gbreve" },
  { GDK_jcircumflex, "jcircumflex" },
  { GDK_Cabovedot, "Cabovedot" },
  { GDK_Ccircumflex, "Ccircumflex" },
  { GDK_Gabovedot, "Gabovedot" },
  { GDK_Gcircumflex, "Gcircumflex" },
  { GDK_Ubreve, "Ubreve" },
  { GDK_Scircumflex, "Scircumflex" },
  { GDK_cabovedot, "cabovedot" },
  { GDK_ccircumflex, "ccircumflex" },
  { GDK_gabovedot, "gabovedot" },
  { GDK_gcircumflex, "gcircumflex" },
  { GDK_ubreve, "ubreve" },
  { GDK_scircumflex, "scircumflex" },
  { GDK_kra, "kra" },
  { GDK_kappa, "kappa" },
  { GDK_Rcedilla, "Rcedilla" },
  { GDK_Itilde, "Itilde" },
  { GDK_Lcedilla, "Lcedilla" },
  { GDK_Emacron, "Emacron" },
  { GDK_Gcedilla, "Gcedilla" },
  { GDK_Tslash, "Tslash" },
  { GDK_rcedilla, "rcedilla" },
  { GDK_itilde, "itilde" },
  { GDK_lcedilla, "lcedilla" },
  { GDK_emacron, "emacron" },
  { GDK_gcedilla, "gcedilla" },
  { GDK_tslash, "tslash" },
  { GDK_ENG, "ENG" },
  { GDK_eng, "eng" },
  { GDK_Amacron, "Amacron" },
  { GDK_Iogonek, "Iogonek" },
  { GDK_Eabovedot, "Eabovedot" },
  { GDK_Imacron, "Imacron" },
  { GDK_Ncedilla, "Ncedilla" },
  { GDK_Omacron, "Omacron" },
  { GDK_Kcedilla, "Kcedilla" },
  { GDK_Uogonek, "Uogonek" },
  { GDK_Utilde, "Utilde" },
  { GDK_Umacron, "Umacron" },
  { GDK_amacron, "amacron" },
  { GDK_iogonek, "iogonek" },
  { GDK_eabovedot, "eabovedot" },
  { GDK_imacron, "imacron" },
  { GDK_ncedilla, "ncedilla" },
  { GDK_omacron, "omacron" },
  { GDK_kcedilla, "kcedilla" },
  { GDK_uogonek, "uogonek" },
  { GDK_utilde, "utilde" },
  { GDK_umacron, "umacron" },
  { GDK_overline, "overline" },
  { GDK_kana_fullstop, "kana_fullstop" },
  { GDK_kana_openingbracket, "kana_openingbracket" },
  { GDK_kana_closingbracket, "kana_closingbracket" },
  { GDK_kana_comma, "kana_comma" },
  { GDK_kana_conjunctive, "kana_conjunctive" },
  { GDK_kana_middledot, "kana_middledot" },
  { GDK_kana_WO, "kana_WO" },
  { GDK_kana_a, "kana_a" },
  { GDK_kana_i, "kana_i" },
  { GDK_kana_u, "kana_u" },
  { GDK_kana_e, "kana_e" },
  { GDK_kana_o, "kana_o" },
  { GDK_kana_ya, "kana_ya" },
  { GDK_kana_yu, "kana_yu" },
  { GDK_kana_yo, "kana_yo" },
  { GDK_kana_tsu, "kana_tsu" },
  { GDK_kana_tu, "kana_tu" },
  { GDK_prolongedsound, "prolongedsound" },
  { GDK_kana_A, "kana_A" },
  { GDK_kana_I, "kana_I" },
  { GDK_kana_U, "kana_U" },
  { GDK_kana_E, "kana_E" },
  { GDK_kana_O, "kana_O" },
  { GDK_kana_KA, "kana_KA" },
  { GDK_kana_KI, "kana_KI" },
  { GDK_kana_KU, "kana_KU" },
  { GDK_kana_KE, "kana_KE" },
  { GDK_kana_KO, "kana_KO" },
  { GDK_kana_SA, "kana_SA" },
  { GDK_kana_SHI, "kana_SHI" },
  { GDK_kana_SU, "kana_SU" },
  { GDK_kana_SE, "kana_SE" },
  { GDK_kana_SO, "kana_SO" },
  { GDK_kana_TA, "kana_TA" },
  { GDK_kana_CHI, "kana_CHI" },
  { GDK_kana_TI, "kana_TI" },
  { GDK_kana_TSU, "kana_TSU" },
  { GDK_kana_TU, "kana_TU" },
  { GDK_kana_TE, "kana_TE" },
  { GDK_kana_TO, "kana_TO" },
  { GDK_kana_NA, "kana_NA" },
  { GDK_kana_NI, "kana_NI" },
  { GDK_kana_NU, "kana_NU" },
  { GDK_kana_NE, "kana_NE" },
  { GDK_kana_NO, "kana_NO" },
  { GDK_kana_HA, "kana_HA" },
  { GDK_kana_HI, "kana_HI" },
  { GDK_kana_FU, "kana_FU" },
  { GDK_kana_HU, "kana_HU" },
  { GDK_kana_HE, "kana_HE" },
  { GDK_kana_HO, "kana_HO" },
  { GDK_kana_MA, "kana_MA" },
  { GDK_kana_MI, "kana_MI" },
  { GDK_kana_MU, "kana_MU" },
  { GDK_kana_ME, "kana_ME" },
  { GDK_kana_MO, "kana_MO" },
  { GDK_kana_YA, "kana_YA" },
  { GDK_kana_YU, "kana_YU" },
  { GDK_kana_YO, "kana_YO" },
  { GDK_kana_RA, "kana_RA" },
  { GDK_kana_RI, "kana_RI" },
  { GDK_kana_RU, "kana_RU" },
  { GDK_kana_RE, "kana_RE" },
  { GDK_kana_RO, "kana_RO" },
  { GDK_kana_WA, "kana_WA" },
  { GDK_kana_N, "kana_N" },
  { GDK_voicedsound, "voicedsound" },
  { GDK_semivoicedsound, "semivoicedsound" },
  { GDK_Arabic_comma, "Arabic_comma" },
  { GDK_Arabic_semicolon, "Arabic_semicolon" },
  { GDK_Arabic_question_mark, "Arabic_question_mark" },
  { GDK_Arabic_hamza, "Arabic_hamza" },
  { GDK_Arabic_maddaonalef, "Arabic_maddaonalef" },
  { GDK_Arabic_hamzaonalef, "Arabic_hamzaonalef" },
  { GDK_Arabic_hamzaonwaw, "Arabic_hamzaonwaw" },
  { GDK_Arabic_hamzaunderalef, "Arabic_hamzaunderalef" },
  { GDK_Arabic_hamzaonyeh, "Arabic_hamzaonyeh" },
  { GDK_Arabic_alef, "Arabic_alef" },
  { GDK_Arabic_beh, "Arabic_beh" },
  { GDK_Arabic_tehmarbuta, "Arabic_tehmarbuta" },
  { GDK_Arabic_teh, "Arabic_teh" },
  { GDK_Arabic_theh, "Arabic_theh" },
  { GDK_Arabic_jeem, "Arabic_jeem" },
  { GDK_Arabic_hah, "Arabic_hah" },
  { GDK_Arabic_khah, "Arabic_khah" },
  { GDK_Arabic_dal, "Arabic_dal" },
  { GDK_Arabic_thal, "Arabic_thal" },
  { GDK_Arabic_ra, "Arabic_ra" },
  { GDK_Arabic_zain, "Arabic_zain" },
  { GDK_Arabic_seen, "Arabic_seen" },
  { GDK_Arabic_sheen, "Arabic_sheen" },
  { GDK_Arabic_sad, "Arabic_sad" },
  { GDK_Arabic_dad, "Arabic_dad" },
  { GDK_Arabic_tah, "Arabic_tah" },
  { GDK_Arabic_zah, "Arabic_zah" },
  { GDK_Arabic_ain, "Arabic_ain" },
  { GDK_Arabic_ghain, "Arabic_ghain" },
  { GDK_Arabic_tatweel, "Arabic_tatweel" },
  { GDK_Arabic_feh, "Arabic_feh" },
  { GDK_Arabic_qaf, "Arabic_qaf" },
  { GDK_Arabic_kaf, "Arabic_kaf" },
  { GDK_Arabic_lam, "Arabic_lam" },
  { GDK_Arabic_meem, "Arabic_meem" },
  { GDK_Arabic_noon, "Arabic_noon" },
  { GDK_Arabic_ha, "Arabic_ha" },
  { GDK_Arabic_heh, "Arabic_heh" },
  { GDK_Arabic_waw, "Arabic_waw" },
  { GDK_Arabic_alefmaksura, "Arabic_alefmaksura" },
  { GDK_Arabic_yeh, "Arabic_yeh" },
  { GDK_Arabic_fathatan, "Arabic_fathatan" },
  { GDK_Arabic_dammatan, "Arabic_dammatan" },
  { GDK_Arabic_kasratan, "Arabic_kasratan" },
  { GDK_Arabic_fatha, "Arabic_fatha" },
  { GDK_Arabic_damma, "Arabic_damma" },
  { GDK_Arabic_kasra, "Arabic_kasra" },
  { GDK_Arabic_shadda, "Arabic_shadda" },
  { GDK_Arabic_sukun, "Arabic_sukun" },
  { GDK_Serbian_dje, "Serbian_dje" },
  { GDK_Macedonia_gje, "Macedonia_gje" },
  { GDK_Cyrillic_io, "Cyrillic_io" },
  { GDK_Ukrainian_ie, "Ukrainian_ie" },
  { GDK_Ukranian_je, "Ukranian_je" },
  { GDK_Macedonia_dse, "Macedonia_dse" },
  { GDK_Ukrainian_i, "Ukrainian_i" },
  { GDK_Ukranian_i, "Ukranian_i" },
  { GDK_Ukrainian_yi, "Ukrainian_yi" },
  { GDK_Ukranian_yi, "Ukranian_yi" },
  { GDK_Cyrillic_je, "Cyrillic_je" },
  { GDK_Serbian_je, "Serbian_je" },
  { GDK_Cyrillic_lje, "Cyrillic_lje" },
  { GDK_Serbian_lje, "Serbian_lje" },
  { GDK_Cyrillic_nje, "Cyrillic_nje" },
  { GDK_Serbian_nje, "Serbian_nje" },
  { GDK_Serbian_tshe, "Serbian_tshe" },
  { GDK_Macedonia_kje, "Macedonia_kje" },
  { GDK_Byelorussian_shortu, "Byelorussian_shortu" },
  { GDK_Cyrillic_dzhe, "Cyrillic_dzhe" },
  { GDK_Serbian_dze, "Serbian_dze" },
  { GDK_numerosign, "numerosign" },
  { GDK_Serbian_DJE, "Serbian_DJE" },
  { GDK_Macedonia_GJE, "Macedonia_GJE" },
  { GDK_Cyrillic_IO, "Cyrillic_IO" },
  { GDK_Ukrainian_IE, "Ukrainian_IE" },
  { GDK_Ukranian_JE, "Ukranian_JE" },
  { GDK_Macedonia_DSE, "Macedonia_DSE" },
  { GDK_Ukrainian_I, "Ukrainian_I" },
  { GDK_Ukranian_I, "Ukranian_I" },
  { GDK_Ukrainian_YI, "Ukrainian_YI" },
  { GDK_Ukranian_YI, "Ukranian_YI" },
  { GDK_Cyrillic_JE, "Cyrillic_JE" },
  { GDK_Serbian_JE, "Serbian_JE" },
  { GDK_Cyrillic_LJE, "Cyrillic_LJE" },
  { GDK_Serbian_LJE, "Serbian_LJE" },
  { GDK_Cyrillic_NJE, "Cyrillic_NJE" },
  { GDK_Serbian_NJE, "Serbian_NJE" },
  { GDK_Serbian_TSHE, "Serbian_TSHE" },
  { GDK_Macedonia_KJE, "Macedonia_KJE" },
  { GDK_Byelorussian_SHORTU, "Byelorussian_SHORTU" },
  { GDK_Cyrillic_DZHE, "Cyrillic_DZHE" },
  { GDK_Serbian_DZE, "Serbian_DZE" },
  { GDK_Cyrillic_yu, "Cyrillic_yu" },
  { GDK_Cyrillic_a, "Cyrillic_a" },
  { GDK_Cyrillic_be, "Cyrillic_be" },
  { GDK_Cyrillic_tse, "Cyrillic_tse" },
  { GDK_Cyrillic_de, "Cyrillic_de" },
  { GDK_Cyrillic_ie, "Cyrillic_ie" },
  { GDK_Cyrillic_ef, "Cyrillic_ef" },
  { GDK_Cyrillic_ghe, "Cyrillic_ghe" },
  { GDK_Cyrillic_ha, "Cyrillic_ha" },
  { GDK_Cyrillic_i, "Cyrillic_i" },
  { GDK_Cyrillic_shorti, "Cyrillic_shorti" },
  { GDK_Cyrillic_ka, "Cyrillic_ka" },
  { GDK_Cyrillic_el, "Cyrillic_el" },
  { GDK_Cyrillic_em, "Cyrillic_em" },
  { GDK_Cyrillic_en, "Cyrillic_en" },
  { GDK_Cyrillic_o, "Cyrillic_o" },
  { GDK_Cyrillic_pe, "Cyrillic_pe" },
  { GDK_Cyrillic_ya, "Cyrillic_ya" },
  { GDK_Cyrillic_er, "Cyrillic_er" },
  { GDK_Cyrillic_es, "Cyrillic_es" },
  { GDK_Cyrillic_te, "Cyrillic_te" },
  { GDK_Cyrillic_u, "Cyrillic_u" },
  { GDK_Cyrillic_zhe, "Cyrillic_zhe" },
  { GDK_Cyrillic_ve, "Cyrillic_ve" },
  { GDK_Cyrillic_softsign, "Cyrillic_softsign" },
  { GDK_Cyrillic_yeru, "Cyrillic_yeru" },
  { GDK_Cyrillic_ze, "Cyrillic_ze" },
  { GDK_Cyrillic_sha, "Cyrillic_sha" },
  { GDK_Cyrillic_e, "Cyrillic_e" },
  { GDK_Cyrillic_shcha, "Cyrillic_shcha" },
  { GDK_Cyrillic_che, "Cyrillic_che" },
  { GDK_Cyrillic_hardsign, "Cyrillic_hardsign" },
  { GDK_Cyrillic_YU, "Cyrillic_YU" },
  { GDK_Cyrillic_A, "Cyrillic_A" },
  { GDK_Cyrillic_BE, "Cyrillic_BE" },
  { GDK_Cyrillic_TSE, "Cyrillic_TSE" },
  { GDK_Cyrillic_DE, "Cyrillic_DE" },
  { GDK_Cyrillic_IE, "Cyrillic_IE" },
  { GDK_Cyrillic_EF, "Cyrillic_EF" },
  { GDK_Cyrillic_GHE, "Cyrillic_GHE" },
  { GDK_Cyrillic_HA, "Cyrillic_HA" },
  { GDK_Cyrillic_I, "Cyrillic_I" },
  { GDK_Cyrillic_SHORTI, "Cyrillic_SHORTI" },
  { GDK_Cyrillic_KA, "Cyrillic_KA" },
  { GDK_Cyrillic_EL, "Cyrillic_EL" },
  { GDK_Cyrillic_EM, "Cyrillic_EM" },
  { GDK_Cyrillic_EN, "Cyrillic_EN" },
  { GDK_Cyrillic_O, "Cyrillic_O" },
  { GDK_Cyrillic_PE, "Cyrillic_PE" },
  { GDK_Cyrillic_YA, "Cyrillic_YA" },
  { GDK_Cyrillic_ER, "Cyrillic_ER" },
  { GDK_Cyrillic_ES, "Cyrillic_ES" },
  { GDK_Cyrillic_TE, "Cyrillic_TE" },
  { GDK_Cyrillic_U, "Cyrillic_U" },
  { GDK_Cyrillic_ZHE, "Cyrillic_ZHE" },
  { GDK_Cyrillic_VE, "Cyrillic_VE" },
  { GDK_Cyrillic_SOFTSIGN, "Cyrillic_SOFTSIGN" },
  { GDK_Cyrillic_YERU, "Cyrillic_YERU" },
  { GDK_Cyrillic_ZE, "Cyrillic_ZE" },
  { GDK_Cyrillic_SHA, "Cyrillic_SHA" },
  { GDK_Cyrillic_E, "Cyrillic_E" },
  { GDK_Cyrillic_SHCHA, "Cyrillic_SHCHA" },
  { GDK_Cyrillic_CHE, "Cyrillic_CHE" },
  { GDK_Cyrillic_HARDSIGN, "Cyrillic_HARDSIGN" },
  { GDK_Greek_ALPHAaccent, "Greek_ALPHAaccent" },
  { GDK_Greek_EPSILONaccent, "Greek_EPSILONaccent" },
  { GDK_Greek_ETAaccent, "Greek_ETAaccent" },
  { GDK_Greek_IOTAaccent, "Greek_IOTAaccent" },
  { GDK_Greek_IOTAdiaeresis, "Greek_IOTAdiaeresis" },
  { GDK_Greek_OMICRONaccent, "Greek_OMICRONaccent" },
  { GDK_Greek_UPSILONaccent, "Greek_UPSILONaccent" },
  { GDK_Greek_UPSILONdieresis, "Greek_UPSILONdieresis" },
  { GDK_Greek_OMEGAaccent, "Greek_OMEGAaccent" },
  { GDK_Greek_accentdieresis, "Greek_accentdieresis" },
  { GDK_Greek_horizbar, "Greek_horizbar" },
  { GDK_Greek_alphaaccent, "Greek_alphaaccent" },
  { GDK_Greek_epsilonaccent, "Greek_epsilonaccent" },
  { GDK_Greek_etaaccent, "Greek_etaaccent" },
  { GDK_Greek_iotaaccent, "Greek_iotaaccent" },
  { GDK_Greek_iotadieresis, "Greek_iotadieresis" },
  { GDK_Greek_iotaaccentdieresis, "Greek_iotaaccentdieresis" },
  { GDK_Greek_omicronaccent, "Greek_omicronaccent" },
  { GDK_Greek_upsilonaccent, "Greek_upsilonaccent" },
  { GDK_Greek_upsilondieresis, "Greek_upsilondieresis" },
  { GDK_Greek_upsilonaccentdieresis, "Greek_upsilonaccentdieresis" },
  { GDK_Greek_omegaaccent, "Greek_omegaaccent" },
  { GDK_Greek_ALPHA, "Greek_ALPHA" },
  { GDK_Greek_BETA, "Greek_BETA" },
  { GDK_Greek_GAMMA, "Greek_GAMMA" },
  { GDK_Greek_DELTA, "Greek_DELTA" },
  { GDK_Greek_EPSILON, "Greek_EPSILON" },
  { GDK_Greek_ZETA, "Greek_ZETA" },
  { GDK_Greek_ETA, "Greek_ETA" },
  { GDK_Greek_THETA, "Greek_THETA" },
  { GDK_Greek_IOTA, "Greek_IOTA" },
  { GDK_Greek_KAPPA, "Greek_KAPPA" },
  { GDK_Greek_LAMDA, "Greek_LAMDA" },
  { GDK_Greek_LAMBDA, "Greek_LAMBDA" },
  { GDK_Greek_MU, "Greek_MU" },
  { GDK_Greek_NU, "Greek_NU" },
  { GDK_Greek_XI, "Greek_XI" },
  { GDK_Greek_OMICRON, "Greek_OMICRON" },
  { GDK_Greek_PI, "Greek_PI" },
  { GDK_Greek_RHO, "Greek_RHO" },
  { GDK_Greek_SIGMA, "Greek_SIGMA" },
  { GDK_Greek_TAU, "Greek_TAU" },
  { GDK_Greek_UPSILON, "Greek_UPSILON" },
  { GDK_Greek_PHI, "Greek_PHI" },
  { GDK_Greek_CHI, "Greek_CHI" },
  { GDK_Greek_PSI, "Greek_PSI" },
  { GDK_Greek_OMEGA, "Greek_OMEGA" },
  { GDK_Greek_alpha, "Greek_alpha" },
  { GDK_Greek_beta, "Greek_beta" },
  { GDK_Greek_gamma, "Greek_gamma" },
  { GDK_Greek_delta, "Greek_delta" },
  { GDK_Greek_epsilon, "Greek_epsilon" },
  { GDK_Greek_zeta, "Greek_zeta" },
  { GDK_Greek_eta, "Greek_eta" },
  { GDK_Greek_theta, "Greek_theta" },
  { GDK_Greek_iota, "Greek_iota" },
  { GDK_Greek_kappa, "Greek_kappa" },
  { GDK_Greek_lamda, "Greek_lamda" },
  { GDK_Greek_lambda, "Greek_lambda" },
  { GDK_Greek_mu, "Greek_mu" },
  { GDK_Greek_nu, "Greek_nu" },
  { GDK_Greek_xi, "Greek_xi" },
  { GDK_Greek_omicron, "Greek_omicron" },
  { GDK_Greek_pi, "Greek_pi" },
  { GDK_Greek_rho, "Greek_rho" },
  { GDK_Greek_sigma, "Greek_sigma" },
  { GDK_Greek_finalsmallsigma, "Greek_finalsmallsigma" },
  { GDK_Greek_tau, "Greek_tau" },
  { GDK_Greek_upsilon, "Greek_upsilon" },
  { GDK_Greek_phi, "Greek_phi" },
  { GDK_Greek_chi, "Greek_chi" },
  { GDK_Greek_psi, "Greek_psi" },
  { GDK_Greek_omega, "Greek_omega" },
  { GDK_leftradical, "leftradical" },
  { GDK_topleftradical, "topleftradical" },
  { GDK_horizconnector, "horizconnector" },
  { GDK_topintegral, "topintegral" },
  { GDK_botintegral, "botintegral" },
  { GDK_vertconnector, "vertconnector" },
  { GDK_topleftsqbracket, "topleftsqbracket" },
  { GDK_botleftsqbracket, "botleftsqbracket" },
  { GDK_toprightsqbracket, "toprightsqbracket" },
  { GDK_botrightsqbracket, "botrightsqbracket" },
  { GDK_topleftparens, "topleftparens" },
  { GDK_botleftparens, "botleftparens" },
  { GDK_toprightparens, "toprightparens" },
  { GDK_botrightparens, "botrightparens" },
  { GDK_leftmiddlecurlybrace, "leftmiddlecurlybrace" },
  { GDK_rightmiddlecurlybrace, "rightmiddlecurlybrace" },
  { GDK_topleftsummation, "topleftsummation" },
  { GDK_botleftsummation, "botleftsummation" },
  { GDK_topvertsummationconnector, "topvertsummationconnector" },
  { GDK_botvertsummationconnector, "botvertsummationconnector" },
  { GDK_toprightsummation, "toprightsummation" },
  { GDK_botrightsummation, "botrightsummation" },
  { GDK_rightmiddlesummation, "rightmiddlesummation" },
  { GDK_lessthanequal, "lessthanequal" },
  { GDK_notequal, "notequal" },
  { GDK_greaterthanequal, "greaterthanequal" },
  { GDK_integral, "integral" },
  { GDK_therefore, "therefore" },
  { GDK_variation, "variation" },
  { GDK_infinity, "infinity" },
  { GDK_nabla, "nabla" },
  { GDK_approximate, "approximate" },
  { GDK_similarequal, "similarequal" },
  { GDK_ifonlyif, "ifonlyif" },
  { GDK_implies, "implies" },
  { GDK_identical, "identical" },
  { GDK_radical, "radical" },
  { GDK_includedin, "includedin" },
  { GDK_includes, "includes" },
  { GDK_intersection, "intersection" },
  { GDK_union, "union" },
  { GDK_logicaland, "logicaland" },
  { GDK_logicalor, "logicalor" },
  { GDK_partialderivative, "partialderivative" },
  { GDK_function, "function" },
  { GDK_leftarrow, "leftarrow" },
  { GDK_uparrow, "uparrow" },
  { GDK_rightarrow, "rightarrow" },
  { GDK_downarrow, "downarrow" },
  { GDK_blank, "blank" },
  { GDK_soliddiamond, "soliddiamond" },
  { GDK_checkerboard, "checkerboard" },
  { GDK_ht, "ht" },
  { GDK_ff, "ff" },
  { GDK_cr, "cr" },
  { GDK_lf, "lf" },
  { GDK_nl, "nl" },
  { GDK_vt, "vt" },
  { GDK_lowrightcorner, "lowrightcorner" },
  { GDK_uprightcorner, "uprightcorner" },
  { GDK_upleftcorner, "upleftcorner" },
  { GDK_lowleftcorner, "lowleftcorner" },
  { GDK_crossinglines, "crossinglines" },
  { GDK_horizlinescan1, "horizlinescan1" },
  { GDK_horizlinescan3, "horizlinescan3" },
  { GDK_horizlinescan5, "horizlinescan5" },
  { GDK_horizlinescan7, "horizlinescan7" },
  { GDK_horizlinescan9, "horizlinescan9" },
  { GDK_leftt, "leftt" },
  { GDK_rightt, "rightt" },
  { GDK_bott, "bott" },
  { GDK_topt, "topt" },
  { GDK_vertbar, "vertbar" },
  { GDK_emspace, "emspace" },
  { GDK_enspace, "enspace" },
  { GDK_em3space, "em3space" },
  { GDK_em4space, "em4space" },
  { GDK_digitspace, "digitspace" },
  { GDK_punctspace, "punctspace" },
  { GDK_thinspace, "thinspace" },
  { GDK_hairspace, "hairspace" },
  { GDK_emdash, "emdash" },
  { GDK_endash, "endash" },
  { GDK_signifblank, "signifblank" },
  { GDK_ellipsis, "ellipsis" },
  { GDK_doubbaselinedot, "doubbaselinedot" },
  { GDK_onethird, "onethird" },
  { GDK_twothirds, "twothirds" },
  { GDK_onefifth, "onefifth" },
  { GDK_twofifths, "twofifths" },
  { GDK_threefifths, "threefifths" },
  { GDK_fourfifths, "fourfifths" },
  { GDK_onesixth, "onesixth" },
  { GDK_fivesixths, "fivesixths" },
  { GDK_careof, "careof" },
  { GDK_figdash, "figdash" },
  { GDK_leftanglebracket, "leftanglebracket" },
  { GDK_decimalpoint, "decimalpoint" },
  { GDK_rightanglebracket, "rightanglebracket" },
  { GDK_marker, "marker" },
  { GDK_oneeighth, "oneeighth" },
  { GDK_threeeighths, "threeeighths" },
  { GDK_fiveeighths, "fiveeighths" },
  { GDK_seveneighths, "seveneighths" },
  { GDK_trademark, "trademark" },
  { GDK_signaturemark, "signaturemark" },
  { GDK_trademarkincircle, "trademarkincircle" },
  { GDK_leftopentriangle, "leftopentriangle" },
  { GDK_rightopentriangle, "rightopentriangle" },
  { GDK_emopencircle, "emopencircle" },
  { GDK_emopenrectangle, "emopenrectangle" },
  { GDK_leftsinglequotemark, "leftsinglequotemark" },
  { GDK_rightsinglequotemark, "rightsinglequotemark" },
  { GDK_leftdoublequotemark, "leftdoublequotemark" },
  { GDK_rightdoublequotemark, "rightdoublequotemark" },
  { GDK_prescription, "prescription" },
  { GDK_minutes, "minutes" },
  { GDK_seconds, "seconds" },
  { GDK_latincross, "latincross" },
  { GDK_hexagram, "hexagram" },
  { GDK_filledrectbullet, "filledrectbullet" },
  { GDK_filledlefttribullet, "filledlefttribullet" },
  { GDK_filledrighttribullet, "filledrighttribullet" },
  { GDK_emfilledcircle, "emfilledcircle" },
  { GDK_emfilledrect, "emfilledrect" },
  { GDK_enopencircbullet, "enopencircbullet" },
  { GDK_enopensquarebullet, "enopensquarebullet" },
  { GDK_openrectbullet, "openrectbullet" },
  { GDK_opentribulletup, "opentribulletup" },
  { GDK_opentribulletdown, "opentribulletdown" },
  { GDK_openstar, "openstar" },
  { GDK_enfilledcircbullet, "enfilledcircbullet" },
  { GDK_enfilledsqbullet, "enfilledsqbullet" },
  { GDK_filledtribulletup, "filledtribulletup" },
  { GDK_filledtribulletdown, "filledtribulletdown" },
  { GDK_leftpointer, "leftpointer" },
  { GDK_rightpointer, "rightpointer" },
  { GDK_club, "club" },
  { GDK_diamond, "diamond" },
  { GDK_heart, "heart" },
  { GDK_maltesecross, "maltesecross" },
  { GDK_dagger, "dagger" },
  { GDK_doubledagger, "doubledagger" },
  { GDK_checkmark, "checkmark" },
  { GDK_ballotcross, "ballotcross" },
  { GDK_musicalsharp, "musicalsharp" },
  { GDK_musicalflat, "musicalflat" },
  { GDK_malesymbol, "malesymbol" },
  { GDK_femalesymbol, "femalesymbol" },
  { GDK_telephone, "telephone" },
  { GDK_telephonerecorder, "telephonerecorder" },
  { GDK_phonographcopyright, "phonographcopyright" },
  { GDK_caret, "caret" },
  { GDK_singlelowquotemark, "singlelowquotemark" },
  { GDK_doublelowquotemark, "doublelowquotemark" },
  { GDK_cursor, "cursor" },
  { GDK_leftcaret, "leftcaret" },
  { GDK_rightcaret, "rightcaret" },
  { GDK_downcaret, "downcaret" },
  { GDK_upcaret, "upcaret" },
  { GDK_overbar, "overbar" },
  { GDK_downtack, "downtack" },
  { GDK_upshoe, "upshoe" },
  { GDK_downstile, "downstile" },
  { GDK_underbar, "underbar" },
  { GDK_jot, "jot" },
  { GDK_quad, "quad" },
  { GDK_uptack, "uptack" },
  { GDK_circle, "circle" },
  { GDK_upstile, "upstile" },
  { GDK_downshoe, "downshoe" },
  { GDK_rightshoe, "rightshoe" },
  { GDK_leftshoe, "leftshoe" },
  { GDK_lefttack, "lefttack" },
  { GDK_righttack, "righttack" },
  { GDK_hebrew_doublelowline, "hebrew_doublelowline" },
  { GDK_hebrew_aleph, "hebrew_aleph" },
  { GDK_hebrew_bet, "hebrew_bet" },
  { GDK_hebrew_beth, "hebrew_beth" },
  { GDK_hebrew_gimel, "hebrew_gimel" },
  { GDK_hebrew_gimmel, "hebrew_gimmel" },
  { GDK_hebrew_dalet, "hebrew_dalet" },
  { GDK_hebrew_daleth, "hebrew_daleth" },
  { GDK_hebrew_he, "hebrew_he" },
  { GDK_hebrew_waw, "hebrew_waw" },
  { GDK_hebrew_zain, "hebrew_zain" },
  { GDK_hebrew_zayin, "hebrew_zayin" },
  { GDK_hebrew_chet, "hebrew_chet" },
  { GDK_hebrew_het, "hebrew_het" },
  { GDK_hebrew_tet, "hebrew_tet" },
  { GDK_hebrew_teth, "hebrew_teth" },
  { GDK_hebrew_yod, "hebrew_yod" },
  { GDK_hebrew_finalkaph, "hebrew_finalkaph" },
  { GDK_hebrew_kaph, "hebrew_kaph" },
  { GDK_hebrew_lamed, "hebrew_lamed" },
  { GDK_hebrew_finalmem, "hebrew_finalmem" },
  { GDK_hebrew_mem, "hebrew_mem" },
  { GDK_hebrew_finalnun, "hebrew_finalnun" },
  { GDK_hebrew_nun, "hebrew_nun" },
  { GDK_hebrew_samech, "hebrew_samech" },
  { GDK_hebrew_samekh, "hebrew_samekh" },
  { GDK_hebrew_ayin, "hebrew_ayin" },
  { GDK_hebrew_finalpe, "hebrew_finalpe" },
  { GDK_hebrew_pe, "hebrew_pe" },
  { GDK_hebrew_finalzade, "hebrew_finalzade" },
  { GDK_hebrew_finalzadi, "hebrew_finalzadi" },
  { GDK_hebrew_zade, "hebrew_zade" },
  { GDK_hebrew_zadi, "hebrew_zadi" },
  { GDK_hebrew_qoph, "hebrew_qoph" },
  { GDK_hebrew_kuf, "hebrew_kuf" },
  { GDK_hebrew_resh, "hebrew_resh" },
  { GDK_hebrew_shin, "hebrew_shin" },
  { GDK_hebrew_taw, "hebrew_taw" },
  { GDK_hebrew_taf, "hebrew_taf" },
  { GDK_Thai_kokai, "Thai_kokai" },
  { GDK_Thai_khokhai, "Thai_khokhai" },
  { GDK_Thai_khokhuat, "Thai_khokhuat" },
  { GDK_Thai_khokhwai, "Thai_khokhwai" },
  { GDK_Thai_khokhon, "Thai_khokhon" },
  { GDK_Thai_khorakhang, "Thai_khorakhang" },
  { GDK_Thai_ngongu, "Thai_ngongu" },
  { GDK_Thai_chochan, "Thai_chochan" },
  { GDK_Thai_choching, "Thai_choching" },
  { GDK_Thai_chochang, "Thai_chochang" },
  { GDK_Thai_soso, "Thai_soso" },
  { GDK_Thai_chochoe, "Thai_chochoe" },
  { GDK_Thai_yoying, "Thai_yoying" },
  { GDK_Thai_dochada, "Thai_dochada" },
  { GDK_Thai_topatak, "Thai_topatak" },
  { GDK_Thai_thothan, "Thai_thothan" },
  { GDK_Thai_thonangmontho, "Thai_thonangmontho" },
  { GDK_Thai_thophuthao, "Thai_thophuthao" },
  { GDK_Thai_nonen, "Thai_nonen" },
  { GDK_Thai_dodek, "Thai_dodek" },
  { GDK_Thai_totao, "Thai_totao" },
  { GDK_Thai_thothung, "Thai_thothung" },
  { GDK_Thai_thothahan, "Thai_thothahan" },
  { GDK_Thai_thothong, "Thai_thothong" },
  { GDK_Thai_nonu, "Thai_nonu" },
  { GDK_Thai_bobaimai, "Thai_bobaimai" },
  { GDK_Thai_popla, "Thai_popla" },
  { GDK_Thai_phophung, "Thai_phophung" },
  { GDK_Thai_fofa, "Thai_fofa" },
  { GDK_Thai_phophan, "Thai_phophan" },
  { GDK_Thai_fofan, "Thai_fofan" },
  { GDK_Thai_phosamphao, "Thai_phosamphao" },
  { GDK_Thai_moma, "Thai_moma" },
  { GDK_Thai_yoyak, "Thai_yoyak" },
  { GDK_Thai_rorua, "Thai_rorua" },
  { GDK_Thai_ru, "Thai_ru" },
  { GDK_Thai_loling, "Thai_loling" },
  { GDK_Thai_lu, "Thai_lu" },
  { GDK_Thai_wowaen, "Thai_wowaen" },
  { GDK_Thai_sosala, "Thai_sosala" },
  { GDK_Thai_sorusi, "Thai_sorusi" },
  { GDK_Thai_sosua, "Thai_sosua" },
  { GDK_Thai_hohip, "Thai_hohip" },
  { GDK_Thai_lochula, "Thai_lochula" },
  { GDK_Thai_oang, "Thai_oang" },
  { GDK_Thai_honokhuk, "Thai_honokhuk" },
  { GDK_Thai_paiyannoi, "Thai_paiyannoi" },
  { GDK_Thai_saraa, "Thai_saraa" },
  { GDK_Thai_maihanakat, "Thai_maihanakat" },
  { GDK_Thai_saraaa, "Thai_saraaa" },
  { GDK_Thai_saraam, "Thai_saraam" },
  { GDK_Thai_sarai, "Thai_sarai" },
  { GDK_Thai_saraii, "Thai_saraii" },
  { GDK_Thai_saraue, "Thai_saraue" },
  { GDK_Thai_sarauee, "Thai_sarauee" },
  { GDK_Thai_sarau, "Thai_sarau" },
  { GDK_Thai_sarauu, "Thai_sarauu" },
  { GDK_Thai_phinthu, "Thai_phinthu" },
  { GDK_Thai_maihanakat_maitho, "Thai_maihanakat_maitho" },
  { GDK_Thai_baht, "Thai_baht" },
  { GDK_Thai_sarae, "Thai_sarae" },
  { GDK_Thai_saraae, "Thai_saraae" },
  { GDK_Thai_sarao, "Thai_sarao" },
  { GDK_Thai_saraaimaimuan, "Thai_saraaimaimuan" },
  { GDK_Thai_saraaimaimalai, "Thai_saraaimaimalai" },
  { GDK_Thai_lakkhangyao, "Thai_lakkhangyao" },
  { GDK_Thai_maiyamok, "Thai_maiyamok" },
  { GDK_Thai_maitaikhu, "Thai_maitaikhu" },
  { GDK_Thai_maiek, "Thai_maiek" },
  { GDK_Thai_maitho, "Thai_maitho" },
  { GDK_Thai_maitri, "Thai_maitri" },
  { GDK_Thai_maichattawa, "Thai_maichattawa" },
  { GDK_Thai_thanthakhat, "Thai_thanthakhat" },
  { GDK_Thai_nikhahit, "Thai_nikhahit" },
  { GDK_Thai_leksun, "Thai_leksun" },
  { GDK_Thai_leknung, "Thai_leknung" },
  { GDK_Thai_leksong, "Thai_leksong" },
  { GDK_Thai_leksam, "Thai_leksam" },
  { GDK_Thai_leksi, "Thai_leksi" },
  { GDK_Thai_lekha, "Thai_lekha" },
  { GDK_Thai_lekhok, "Thai_lekhok" },
  { GDK_Thai_lekchet, "Thai_lekchet" },
  { GDK_Thai_lekpaet, "Thai_lekpaet" },
  { GDK_Thai_lekkao, "Thai_lekkao" },
  { GDK_Hangul_Kiyeog, "Hangul_Kiyeog" },
  { GDK_Hangul_SsangKiyeog, "Hangul_SsangKiyeog" },
  { GDK_Hangul_KiyeogSios, "Hangul_KiyeogSios" },
  { GDK_Hangul_Nieun, "Hangul_Nieun" },
  { GDK_Hangul_NieunJieuj, "Hangul_NieunJieuj" },
  { GDK_Hangul_NieunHieuh, "Hangul_NieunHieuh" },
  { GDK_Hangul_Dikeud, "Hangul_Dikeud" },
  { GDK_Hangul_SsangDikeud, "Hangul_SsangDikeud" },
  { GDK_Hangul_Rieul, "Hangul_Rieul" },
  { GDK_Hangul_RieulKiyeog, "Hangul_RieulKiyeog" },
  { GDK_Hangul_RieulMieum, "Hangul_RieulMieum" },
  { GDK_Hangul_RieulPieub, "Hangul_RieulPieub" },
  { GDK_Hangul_RieulSios, "Hangul_RieulSios" },
  { GDK_Hangul_RieulTieut, "Hangul_RieulTieut" },
  { GDK_Hangul_RieulPhieuf, "Hangul_RieulPhieuf" },
  { GDK_Hangul_RieulHieuh, "Hangul_RieulHieuh" },
  { GDK_Hangul_Mieum, "Hangul_Mieum" },
  { GDK_Hangul_Pieub, "Hangul_Pieub" },
  { GDK_Hangul_SsangPieub, "Hangul_SsangPieub" },
  { GDK_Hangul_PieubSios, "Hangul_PieubSios" },
  { GDK_Hangul_Sios, "Hangul_Sios" },
  { GDK_Hangul_SsangSios, "Hangul_SsangSios" },
  { GDK_Hangul_Ieung, "Hangul_Ieung" },
  { GDK_Hangul_Jieuj, "Hangul_Jieuj" },
  { GDK_Hangul_SsangJieuj, "Hangul_SsangJieuj" },
  { GDK_Hangul_Cieuc, "Hangul_Cieuc" },
  { GDK_Hangul_Khieuq, "Hangul_Khieuq" },
  { GDK_Hangul_Tieut, "Hangul_Tieut" },
  { GDK_Hangul_Phieuf, "Hangul_Phieuf" },
  { GDK_Hangul_Hieuh, "Hangul_Hieuh" },
  { GDK_Hangul_A, "Hangul_A" },
  { GDK_Hangul_AE, "Hangul_AE" },
  { GDK_Hangul_YA, "Hangul_YA" },
  { GDK_Hangul_YAE, "Hangul_YAE" },
  { GDK_Hangul_EO, "Hangul_EO" },
  { GDK_Hangul_E, "Hangul_E" },
  { GDK_Hangul_YEO, "Hangul_YEO" },
  { GDK_Hangul_YE, "Hangul_YE" },
  { GDK_Hangul_O, "Hangul_O" },
  { GDK_Hangul_WA, "Hangul_WA" },
  { GDK_Hangul_WAE, "Hangul_WAE" },
  { GDK_Hangul_OE, "Hangul_OE" },
  { GDK_Hangul_YO, "Hangul_YO" },
  { GDK_Hangul_U, "Hangul_U" },
  { GDK_Hangul_WEO, "Hangul_WEO" },
  { GDK_Hangul_WE, "Hangul_WE" },
  { GDK_Hangul_WI, "Hangul_WI" },
  { GDK_Hangul_YU, "Hangul_YU" },
  { GDK_Hangul_EU, "Hangul_EU" },
  { GDK_Hangul_YI, "Hangul_YI" },
  { GDK_Hangul_I, "Hangul_I" },
  { GDK_Hangul_J_Kiyeog, "Hangul_J_Kiyeog" },
  { GDK_Hangul_J_SsangKiyeog, "Hangul_J_SsangKiyeog" },
  { GDK_Hangul_J_KiyeogSios, "Hangul_J_KiyeogSios" },
  { GDK_Hangul_J_Nieun, "Hangul_J_Nieun" },
  { GDK_Hangul_J_NieunJieuj, "Hangul_J_NieunJieuj" },
  { GDK_Hangul_J_NieunHieuh, "Hangul_J_NieunHieuh" },
  { GDK_Hangul_J_Dikeud, "Hangul_J_Dikeud" },
  { GDK_Hangul_J_Rieul, "Hangul_J_Rieul" },
  { GDK_Hangul_J_RieulKiyeog, "Hangul_J_RieulKiyeog" },
  { GDK_Hangul_J_RieulMieum, "Hangul_J_RieulMieum" },
  { GDK_Hangul_J_RieulPieub, "Hangul_J_RieulPieub" },
  { GDK_Hangul_J_RieulSios, "Hangul_J_RieulSios" },
  { GDK_Hangul_J_RieulTieut, "Hangul_J_RieulTieut" },
  { GDK_Hangul_J_RieulPhieuf, "Hangul_J_RieulPhieuf" },
  { GDK_Hangul_J_RieulHieuh, "Hangul_J_RieulHieuh" },
  { GDK_Hangul_J_Mieum, "Hangul_J_Mieum" },
  { GDK_Hangul_J_Pieub, "Hangul_J_Pieub" },
  { GDK_Hangul_J_PieubSios, "Hangul_J_PieubSios" },
  { GDK_Hangul_J_Sios, "Hangul_J_Sios" },
  { GDK_Hangul_J_SsangSios, "Hangul_J_SsangSios" },
  { GDK_Hangul_J_Ieung, "Hangul_J_Ieung" },
  { GDK_Hangul_J_Jieuj, "Hangul_J_Jieuj" },
  { GDK_Hangul_J_Cieuc, "Hangul_J_Cieuc" },
  { GDK_Hangul_J_Khieuq, "Hangul_J_Khieuq" },
  { GDK_Hangul_J_Tieut, "Hangul_J_Tieut" },
  { GDK_Hangul_J_Phieuf, "Hangul_J_Phieuf" },
  { GDK_Hangul_J_Hieuh, "Hangul_J_Hieuh" },
  { GDK_Hangul_RieulYeorinHieuh, "Hangul_RieulYeorinHieuh" },
  { GDK_Hangul_SunkyeongeumMieum, "Hangul_SunkyeongeumMieum" },
  { GDK_Hangul_SunkyeongeumPieub, "Hangul_SunkyeongeumPieub" },
  { GDK_Hangul_PanSios, "Hangul_PanSios" },
  { GDK_Hangul_KkogjiDalrinIeung, "Hangul_KkogjiDalrinIeung" },
  { GDK_Hangul_SunkyeongeumPhieuf, "Hangul_SunkyeongeumPhieuf" },
  { GDK_Hangul_YeorinHieuh, "Hangul_YeorinHieuh" },
  { GDK_Hangul_AraeA, "Hangul_AraeA" },
  { GDK_Hangul_AraeAE, "Hangul_AraeAE" },
  { GDK_Hangul_J_PanSios, "Hangul_J_PanSios" },
  { GDK_Hangul_J_KkogjiDalrinIeung, "Hangul_J_KkogjiDalrinIeung" },
  { GDK_Hangul_J_YeorinHieuh, "Hangul_J_YeorinHieuh" },
  { GDK_Korean_Won, "Korean_Won" },
  { GDK_OE, "OE" },
  { GDK_oe, "oe" },
  { GDK_Ydiaeresis, "Ydiaeresis" },
  { GDK_EcuSign, "EcuSign" },
  { GDK_ColonSign, "ColonSign" },
  { GDK_CruzeiroSign, "CruzeiroSign" },
  { GDK_FFrancSign, "FFrancSign" },
  { GDK_LiraSign, "LiraSign" },
  { GDK_MillSign, "MillSign" },
  { GDK_NairaSign, "NairaSign" },
  { GDK_PesetaSign, "PesetaSign" },
  { GDK_RupeeSign, "RupeeSign" },
  { GDK_WonSign, "WonSign" },
  { GDK_NewSheqelSign, "NewSheqelSign" },
  { GDK_DongSign, "DongSign" },
  { GDK_EuroSign, "EuroSign" },
  { GDK_3270_Duplicate, "3270_Duplicate" },
  { GDK_3270_FieldMark, "3270_FieldMark" },
  { GDK_3270_Right2, "3270_Right2" },
  { GDK_3270_Left2, "3270_Left2" },
  { GDK_3270_BackTab, "3270_BackTab" },
  { GDK_3270_EraseEOF, "3270_EraseEOF" },
  { GDK_3270_EraseInput, "3270_EraseInput" },
  { GDK_3270_Reset, "3270_Reset" },
  { GDK_3270_Quit, "3270_Quit" },
  { GDK_3270_PA1, "3270_PA1" },
  { GDK_3270_PA2, "3270_PA2" },
  { GDK_3270_PA3, "3270_PA3" },
  { GDK_3270_Test, "3270_Test" },
  { GDK_3270_Attn, "3270_Attn" },
  { GDK_3270_CursorBlink, "3270_CursorBlink" },
  { GDK_3270_AltCursor, "3270_AltCursor" },
  { GDK_3270_KeyClick, "3270_KeyClick" },
  { GDK_3270_Jump, "3270_Jump" },
  { GDK_3270_Ident, "3270_Ident" },
  { GDK_3270_Rule, "3270_Rule" },
  { GDK_3270_Copy, "3270_Copy" },
  { GDK_3270_Play, "3270_Play" },
  { GDK_3270_Setup, "3270_Setup" },
  { GDK_3270_Record, "3270_Record" },
  { GDK_3270_ChangeScreen, "3270_ChangeScreen" },
  { GDK_3270_DeleteWord, "3270_DeleteWord" },
  { GDK_3270_ExSelect, "3270_ExSelect" },
  { GDK_3270_CursorSelect, "3270_CursorSelect" },
  { GDK_3270_PrintScreen, "3270_PrintScreen" },
  { GDK_3270_Enter, "3270_Enter" },
  { GDK_ISO_Lock, "ISO_Lock" },
  { GDK_ISO_Level2_Latch, "ISO_Level2_Latch" },
  { GDK_ISO_Level3_Shift, "ISO_Level3_Shift" },
  { GDK_ISO_Level3_Latch, "ISO_Level3_Latch" },
  { GDK_ISO_Level3_Lock, "ISO_Level3_Lock" },
  { GDK_ISO_Group_Latch, "ISO_Group_Latch" },
  { GDK_ISO_Group_Lock, "ISO_Group_Lock" },
  { GDK_ISO_Next_Group, "ISO_Next_Group" },
  { GDK_ISO_Next_Group_Lock, "ISO_Next_Group_Lock" },
  { GDK_ISO_Prev_Group, "ISO_Prev_Group" },
  { GDK_ISO_Prev_Group_Lock, "ISO_Prev_Group_Lock" },
  { GDK_ISO_First_Group, "ISO_First_Group" },
  { GDK_ISO_First_Group_Lock, "ISO_First_Group_Lock" },
  { GDK_ISO_Last_Group, "ISO_Last_Group" },
  { GDK_ISO_Last_Group_Lock, "ISO_Last_Group_Lock" },
  { GDK_ISO_Left_Tab, "ISO_Left_Tab" },
  { GDK_ISO_Move_Line_Up, "ISO_Move_Line_Up" },
  { GDK_ISO_Move_Line_Down, "ISO_Move_Line_Down" },
  { GDK_ISO_Partial_Line_Up, "ISO_Partial_Line_Up" },
  { GDK_ISO_Partial_Line_Down, "ISO_Partial_Line_Down" },
  { GDK_ISO_Partial_Space_Left, "ISO_Partial_Space_Left" },
  { GDK_ISO_Partial_Space_Right, "ISO_Partial_Space_Right" },
  { GDK_ISO_Set_Margin_Left, "ISO_Set_Margin_Left" },
  { GDK_ISO_Set_Margin_Right, "ISO_Set_Margin_Right" },
  { GDK_ISO_Release_Margin_Left, "ISO_Release_Margin_Left" },
  { GDK_ISO_Release_Margin_Right, "ISO_Release_Margin_Right" },
  { GDK_ISO_Release_Both_Margins, "ISO_Release_Both_Margins" },
  { GDK_ISO_Fast_Cursor_Left, "ISO_Fast_Cursor_Left" },
  { GDK_ISO_Fast_Cursor_Right, "ISO_Fast_Cursor_Right" },
  { GDK_ISO_Fast_Cursor_Up, "ISO_Fast_Cursor_Up" },
  { GDK_ISO_Fast_Cursor_Down, "ISO_Fast_Cursor_Down" },
  { GDK_ISO_Continuous_Underline, "ISO_Continuous_Underline" },
  { GDK_ISO_Discontinuous_Underline, "ISO_Discontinuous_Underline" },
  { GDK_ISO_Emphasize, "ISO_Emphasize" },
  { GDK_ISO_Center_Object, "ISO_Center_Object" },
  { GDK_ISO_Enter, "ISO_Enter" },
  { GDK_dead_grave, "dead_grave" },
  { GDK_dead_acute, "dead_acute" },
  { GDK_dead_circumflex, "dead_circumflex" },
  { GDK_dead_tilde, "dead_tilde" },
  { GDK_dead_macron, "dead_macron" },
  { GDK_dead_breve, "dead_breve" },
  { GDK_dead_abovedot, "dead_abovedot" },
  { GDK_dead_diaeresis, "dead_diaeresis" },
  { GDK_dead_abovering, "dead_abovering" },
  { GDK_dead_doubleacute, "dead_doubleacute" },
  { GDK_dead_caron, "dead_caron" },
  { GDK_dead_cedilla, "dead_cedilla" },
  { GDK_dead_ogonek, "dead_ogonek" },
  { GDK_dead_iota, "dead_iota" },
  { GDK_dead_voiced_sound, "dead_voiced_sound" },
  { GDK_dead_semivoiced_sound, "dead_semivoiced_sound" },
  { GDK_dead_belowdot, "dead_belowdot" },
  { GDK_AccessX_Enable, "AccessX_Enable" },
  { GDK_AccessX_Feedback_Enable, "AccessX_Feedback_Enable" },
  { GDK_RepeatKeys_Enable, "RepeatKeys_Enable" },
  { GDK_SlowKeys_Enable, "SlowKeys_Enable" },
  { GDK_BounceKeys_Enable, "BounceKeys_Enable" },
  { GDK_StickyKeys_Enable, "StickyKeys_Enable" },
  { GDK_MouseKeys_Enable, "MouseKeys_Enable" },
  { GDK_MouseKeys_Accel_Enable, "MouseKeys_Accel_Enable" },
  { GDK_Overlay1_Enable, "Overlay1_Enable" },
  { GDK_Overlay2_Enable, "Overlay2_Enable" },
  { GDK_AudibleBell_Enable, "AudibleBell_Enable" },
  { GDK_First_Virtual_Screen, "First_Virtual_Screen" },
  { GDK_Prev_Virtual_Screen, "Prev_Virtual_Screen" },
  { GDK_Next_Virtual_Screen, "Next_Virtual_Screen" },
  { GDK_Last_Virtual_Screen, "Last_Virtual_Screen" },
  { GDK_Terminate_Server, "Terminate_Server" },
  { GDK_Pointer_Left, "Pointer_Left" },
  { GDK_Pointer_Right, "Pointer_Right" },
  { GDK_Pointer_Up, "Pointer_Up" },
  { GDK_Pointer_Down, "Pointer_Down" },
  { GDK_Pointer_UpLeft, "Pointer_UpLeft" },
  { GDK_Pointer_UpRight, "Pointer_UpRight" },
  { GDK_Pointer_DownLeft, "Pointer_DownLeft" },
  { GDK_Pointer_DownRight, "Pointer_DownRight" },
  { GDK_Pointer_Button_Dflt, "Pointer_Button_Dflt" },
  { GDK_Pointer_Button1, "Pointer_Button1" },
  { GDK_Pointer_Button2, "Pointer_Button2" },
  { GDK_Pointer_Button3, "Pointer_Button3" },
  { GDK_Pointer_Button4, "Pointer_Button4" },
  { GDK_Pointer_Button5, "Pointer_Button5" },
  { GDK_Pointer_DblClick_Dflt, "Pointer_DblClick_Dflt" },
  { GDK_Pointer_DblClick1, "Pointer_DblClick1" },
  { GDK_Pointer_DblClick2, "Pointer_DblClick2" },
  { GDK_Pointer_DblClick3, "Pointer_DblClick3" },
  { GDK_Pointer_DblClick4, "Pointer_DblClick4" },
  { GDK_Pointer_DblClick5, "Pointer_DblClick5" },
  { GDK_Pointer_Drag_Dflt, "Pointer_Drag_Dflt" },
  { GDK_Pointer_Drag1, "Pointer_Drag1" },
  { GDK_Pointer_Drag2, "Pointer_Drag2" },
  { GDK_Pointer_Drag3, "Pointer_Drag3" },
  { GDK_Pointer_Drag4, "Pointer_Drag4" },
  { GDK_Pointer_EnableKeys, "Pointer_EnableKeys" },
  { GDK_Pointer_Accelerate, "Pointer_Accelerate" },
  { GDK_Pointer_DfltBtnNext, "Pointer_DfltBtnNext" },
  { GDK_Pointer_DfltBtnPrev, "Pointer_DfltBtnPrev" },
  { GDK_Pointer_Drag5, "Pointer_Drag5" },
  { GDK_BackSpace, "BackSpace" },
  { GDK_Tab, "Tab" },
  { GDK_Linefeed, "Linefeed" },
  { GDK_Clear, "Clear" },
  { GDK_Return, "Return" },
  { GDK_Pause, "Pause" },
  { GDK_Scroll_Lock, "Scroll_Lock" },
  { GDK_Sys_Req, "Sys_Req" },
  { GDK_Escape, "Escape" },
  { GDK_Multi_key, "Multi_key" },
  { GDK_Kanji, "Kanji" },
  { GDK_Muhenkan, "Muhenkan" },
  { GDK_Henkan_Mode, "Henkan_Mode" },
  { GDK_Henkan, "Henkan" },
  { GDK_Romaji, "Romaji" },
  { GDK_Hiragana, "Hiragana" },
  { GDK_Katakana, "Katakana" },
  { GDK_Hiragana_Katakana, "Hiragana_Katakana" },
  { GDK_Zenkaku, "Zenkaku" },
  { GDK_Hankaku, "Hankaku" },
  { GDK_Zenkaku_Hankaku, "Zenkaku_Hankaku" },
  { GDK_Touroku, "Touroku" },
  { GDK_Massyo, "Massyo" },
  { GDK_Kana_Lock, "Kana_Lock" },
  { GDK_Kana_Shift, "Kana_Shift" },
  { GDK_Eisu_Shift, "Eisu_Shift" },
  { GDK_Eisu_toggle, "Eisu_toggle" },
  { GDK_Hangul, "Hangul" },
  { GDK_Hangul_Start, "Hangul_Start" },
  { GDK_Hangul_End, "Hangul_End" },
  { GDK_Hangul_Hanja, "Hangul_Hanja" },
  { GDK_Hangul_Jamo, "Hangul_Jamo" },
  { GDK_Hangul_Romaja, "Hangul_Romaja" },
  { GDK_Codeinput, "Codeinput" },
  { GDK_Kanji_Bangou, "Kanji_Bangou" },
  { GDK_Hangul_Codeinput, "Hangul_Codeinput" },
  { GDK_Hangul_Jeonja, "Hangul_Jeonja" },
  { GDK_Hangul_Banja, "Hangul_Banja" },
  { GDK_Hangul_PreHanja, "Hangul_PreHanja" },
  { GDK_Hangul_PostHanja, "Hangul_PostHanja" },
  { GDK_SingleCandidate, "SingleCandidate" },
  { GDK_Hangul_SingleCandidate, "Hangul_SingleCandidate" },
  { GDK_MultipleCandidate, "MultipleCandidate" },
  { GDK_Zen_Koho, "Zen_Koho" },
  { GDK_Hangul_MultipleCandidate, "Hangul_MultipleCandidate" },
  { GDK_PreviousCandidate, "PreviousCandidate" },
  { GDK_Mae_Koho, "Mae_Koho" },
  { GDK_Hangul_PreviousCandidate, "Hangul_PreviousCandidate" },
  { GDK_Hangul_Special, "Hangul_Special" },
  { GDK_Home, "Home" },
  { GDK_Left, "Left" },
  { GDK_Up, "Up" },
  { GDK_Right, "Right" },
  { GDK_Down, "Down" },
  { GDK_Prior, "Prior" },
  { GDK_Page_Up, "Page_Up" },
  { GDK_Next, "Next" },
  { GDK_Page_Down, "Page_Down" },
  { GDK_End, "End" },
  { GDK_Begin, "Begin" },
  { GDK_Select, "Select" },
  { GDK_Print, "Print" },
  { GDK_Execute, "Execute" },
  { GDK_Insert, "Insert" },
  { GDK_Undo, "Undo" },
  { GDK_Redo, "Redo" },
  { GDK_Menu, "Menu" },
  { GDK_Find, "Find" },
  { GDK_Cancel, "Cancel" },
  { GDK_Help, "Help" },
  { GDK_Break, "Break" },
  { GDK_Mode_switch, "Mode_switch" },
  { GDK_script_switch, "script_switch" },
  { GDK_ISO_Group_Shift, "ISO_Group_Shift" },
  { GDK_kana_switch, "kana_switch" },
  { GDK_Arabic_switch, "Arabic_switch" },
  { GDK_Greek_switch, "Greek_switch" },
  { GDK_Hebrew_switch, "Hebrew_switch" },
  { GDK_Hangul_switch, "Hangul_switch" },
  { GDK_Num_Lock, "Num_Lock" },
  { GDK_KP_Space, "KP_Space" },
  { GDK_KP_Tab, "KP_Tab" },
  { GDK_KP_Enter, "KP_Enter" },
  { GDK_KP_F1, "KP_F1" },
  { GDK_KP_F2, "KP_F2" },
  { GDK_KP_F3, "KP_F3" },
  { GDK_KP_F4, "KP_F4" },
  { GDK_KP_Home, "KP_Home" },
  { GDK_KP_Left, "KP_Left" },
  { GDK_KP_Up, "KP_Up" },
  { GDK_KP_Right, "KP_Right" },
  { GDK_KP_Down, "KP_Down" },
  { GDK_KP_Prior, "KP_Prior" },
  { GDK_KP_Page_Up, "KP_Page_Up" },
  { GDK_KP_Next, "KP_Next" },
  { GDK_KP_Page_Down, "KP_Page_Down" },
  { GDK_KP_End, "KP_End" },
  { GDK_KP_Begin, "KP_Begin" },
  { GDK_KP_Insert, "KP_Insert" },
  { GDK_KP_Delete, "KP_Delete" },
  { GDK_KP_Multiply, "KP_Multiply" },
  { GDK_KP_Add, "KP_Add" },
  { GDK_KP_Separator, "KP_Separator" },
  { GDK_KP_Subtract, "KP_Subtract" },
  { GDK_KP_Decimal, "KP_Decimal" },
  { GDK_KP_Divide, "KP_Divide" },
  { GDK_KP_0, "KP_0" },
  { GDK_KP_1, "KP_1" },
  { GDK_KP_2, "KP_2" },
  { GDK_KP_3, "KP_3" },
  { GDK_KP_4, "KP_4" },
  { GDK_KP_5, "KP_5" },
  { GDK_KP_6, "KP_6" },
  { GDK_KP_7, "KP_7" },
  { GDK_KP_8, "KP_8" },
  { GDK_KP_9, "KP_9" },
  { GDK_KP_Equal, "KP_Equal" },
  { GDK_F1, "F1" },
  { GDK_F2, "F2" },
  { GDK_F3, "F3" },
  { GDK_F4, "F4" },
  { GDK_F5, "F5" },
  { GDK_F6, "F6" },
  { GDK_F7, "F7" },
  { GDK_F8, "F8" },
  { GDK_F9, "F9" },
  { GDK_F10, "F10" },
  { GDK_F11, "F11" },
  { GDK_F12, "F12" },
  { GDK_F13, "F13" },
  { GDK_F14, "F14" },
  { GDK_F15, "F15" },
  { GDK_F16, "F16" },
  { GDK_F17, "F17" },
  { GDK_F18, "F18" },
  { GDK_F19, "F19" },
  { GDK_F20, "F20" },
  { GDK_F21, "F21" },
  { GDK_F22, "F22" },
  { GDK_F23, "F23" },
  { GDK_F24, "F24" },
  { GDK_F25, "F25" },
  { GDK_F26, "F26" },
  { GDK_F27, "F27" },
  { GDK_F28, "F28" },
  { GDK_F29, "F29" },
  { GDK_F30, "F30" },
  { GDK_F31, "F31" },
  { GDK_F32, "F32" },
  { GDK_F33, "F33" },
  { GDK_F34, "F34" },
  { GDK_F35, "F35" },
  { GDK_Shift_L, "Shift_L" },
  { GDK_Shift_R, "Shift_R" },
  { GDK_Control_L, "Control_L" },
  { GDK_Control_R, "Control_R" },
  { GDK_Caps_Lock, "Caps_Lock" },
  { GDK_Shift_Lock, "Shift_Lock" },
  { GDK_Meta_L, "Meta_L" },
  { GDK_Meta_R, "Meta_R" },
  { GDK_Alt_L, "Alt_L" },
  { GDK_Alt_R, "Alt_R" },
  { GDK_Super_L, "Super_L" },
  { GDK_Super_R, "Super_R" },
  { GDK_Hyper_L, "Hyper_L" },
  { GDK_Hyper_R, "Hyper_R" },
  { GDK_Delete, "Delete" },
  { GDK_VoidSymbol, "VoidSymbol" }
};

#define GDK_NUM_KEYS (G_N_ELEMENTS (gdk_keys_by_keyval))


static gint
gdk_keys_keyval_compare (const void *pkey,
                         const void *pbase)
{
  return (*(gint *) pkey) - ((struct gdk_key *) pbase)->keyval;
}

gchar *
gdk_keyval_name (guint keyval)
{
  struct gdk_key *found;

  /* these keyvals have two entries (PageUp/Prior | PageDown/Next) */
  switch (keyval)
    {
    case GDK_Page_Up:
      return "Page_Up";
    case GDK_Page_Down:
      return "Page_Down";
    case GDK_KP_Page_Up:
      return "KP_Page_Up";
    case GDK_KP_Page_Down:
      return "KP_Page_Down";
    }

  found = bsearch (&keyval, gdk_keys_by_keyval,
                   GDK_NUM_KEYS, sizeof (struct gdk_key),
                   gdk_keys_keyval_compare);

  if (found)
    return (gchar *) found->name;
  else
    return NULL;
}

static gint
gdk_key_compare_by_name (const void *a,
                         const void *b)
{
  return strcmp (((const struct gdk_key *) a)->name,
                 ((const struct gdk_key *) b)->name);
}

static gint
gdk_keys_name_compare (const void *pkey,
                       const void *pbase)
{
  return strcmp ((const char *) pkey,
                 ((const struct gdk_key *) pbase)->name);
}

guint
gdk_keyval_from_name (const gchar *keyval_name)
{
  struct gdk_key *found;

  g_return_val_if_fail (keyval_name != NULL, 0);

  if (gdk_keys_by_name == NULL)
    {
      gdk_keys_by_name = g_new (struct gdk_key, GDK_NUM_KEYS);

      memcpy (gdk_keys_by_name, gdk_keys_by_keyval,
              GDK_NUM_KEYS * sizeof (struct gdk_key));

      qsort (gdk_keys_by_name, GDK_NUM_KEYS, sizeof (struct gdk_key),
             gdk_key_compare_by_name);
    }

  found = bsearch (keyval_name, gdk_keys_by_name,
                   GDK_NUM_KEYS, sizeof (struct gdk_key),
                   gdk_keys_name_compare);

  if (found)
    return found->keyval;
  else
    return GDK_VoidSymbol;
}

static void
gdk_directfb_convert_modifiers (DFBInputDeviceModifierMask dfbmod,
                                DFBInputDeviceLockState    dfblock)
{
  if (dfbmod & DIMM_ALT)
    _gdk_directfb_modifiers |= GDK_MOD1_MASK;
  else
    _gdk_directfb_modifiers &= ~GDK_MOD1_MASK;

  if (dfbmod & DIMM_ALTGR)
    _gdk_directfb_modifiers |= GDK_MOD2_MASK;
  else
    _gdk_directfb_modifiers &= ~GDK_MOD2_MASK;

  if (dfbmod & DIMM_CONTROL)
    _gdk_directfb_modifiers |= GDK_CONTROL_MASK;
  else
    _gdk_directfb_modifiers &= ~GDK_CONTROL_MASK;

  if (dfbmod & DIMM_SHIFT)
    _gdk_directfb_modifiers |= GDK_SHIFT_MASK;
  else
    _gdk_directfb_modifiers &= ~GDK_SHIFT_MASK;

  if (dfblock & DILS_CAPS)
    _gdk_directfb_modifiers |= GDK_LOCK_MASK;
  else
    _gdk_directfb_modifiers &= ~GDK_LOCK_MASK;
}

static guint
gdk_directfb_translate_key (DFBInputDeviceKeyIdentifier key_id,
                            DFBInputDeviceKeySymbol     key_symbol)
{
  guint keyval = GDK_VoidSymbol;

  /* special case numpad */
  if (key_id >= DIKI_KP_DIV && key_id <= DIKI_KP_9)
    {
      switch (key_symbol)
        {
        case DIKS_SLASH:         keyval = GDK_KP_Divide;    break;
        case DIKS_ASTERISK:      keyval = GDK_KP_Multiply;  break;
        case DIKS_PLUS_SIGN:     keyval = GDK_KP_Add;       break;
        case DIKS_MINUS_SIGN:    keyval = GDK_KP_Subtract;  break;
        case DIKS_ENTER:         keyval = GDK_KP_Enter;     break;
        case DIKS_SPACE:         keyval = GDK_KP_Space;     break;
        case DIKS_TAB:           keyval = GDK_KP_Tab;       break;
        case DIKS_EQUALS_SIGN:   keyval = GDK_KP_Equal;     break;
        case DIKS_COMMA:
        case DIKS_PERIOD:        keyval = GDK_KP_Decimal;   break;
        case DIKS_HOME:          keyval = GDK_KP_Home;      break;
        case DIKS_END:           keyval = GDK_KP_End;       break;
        case DIKS_PAGE_UP:       keyval = GDK_KP_Page_Up;   break;
        case DIKS_PAGE_DOWN:     keyval = GDK_KP_Page_Down; break;
        case DIKS_CURSOR_LEFT:   keyval = GDK_KP_Left;      break;
        case DIKS_CURSOR_RIGHT:  keyval = GDK_KP_Right;     break;
        case DIKS_CURSOR_UP:     keyval = GDK_KP_Up;        break;
        case DIKS_CURSOR_DOWN:   keyval = GDK_KP_Down;      break;
        case DIKS_BEGIN:         keyval = GDK_KP_Begin;     break;

        case DIKS_0 ... DIKS_9:
          keyval = GDK_KP_0 + key_symbol - DIKS_0;
          break;
        case DIKS_F1 ... DIKS_F4:
          keyval = GDK_KP_F1 + key_symbol - DIKS_F1;
          break;

        default:
          break;
        }
    }
  else
    {
      switch (DFB_KEY_TYPE (key_symbol))
        {
        case DIKT_UNICODE:
          switch (key_symbol)
            {
            case DIKS_NULL:       keyval = GDK_VoidSymbol; break;
            case DIKS_BACKSPACE:  keyval = GDK_BackSpace;  break;
            case DIKS_TAB:        keyval = GDK_Tab;        break;
            case DIKS_RETURN:     keyval = GDK_Return;     break;
            case DIKS_CANCEL:     keyval = GDK_Cancel;     break;
            case DIKS_ESCAPE:     keyval = GDK_Escape;     break;
            case DIKS_SPACE:      keyval = GDK_space;      break;
            case DIKS_DELETE:     keyval = GDK_Delete;     break;

            default:
              keyval = gdk_unicode_to_keyval (key_symbol);
              if (keyval & 0x01000000)
                keyval = GDK_VoidSymbol;
            }
          break;

        case DIKT_SPECIAL:
          switch (key_symbol)
            {
            case DIKS_CURSOR_LEFT:   keyval = GDK_Left;      break;
            case DIKS_CURSOR_RIGHT:  keyval = GDK_Right;     break;
            case DIKS_CURSOR_UP:     keyval = GDK_Up;        break;
            case DIKS_CURSOR_DOWN:   keyval = GDK_Down;      break;
            case DIKS_INSERT:        keyval = GDK_Insert;    break;
            case DIKS_HOME:          keyval = GDK_Home;      break;
            case DIKS_END:           keyval = GDK_End;       break;
            case DIKS_PAGE_UP:       keyval = GDK_Page_Up;   break;
            case DIKS_PAGE_DOWN:     keyval = GDK_Page_Down; break;
            case DIKS_PRINT:         keyval = GDK_Print;     break;
            case DIKS_PAUSE:         keyval = GDK_Pause;     break;
            case DIKS_SELECT:        keyval = GDK_Select;    break;
            case DIKS_CLEAR:         keyval = GDK_Clear;     break;
            case DIKS_MENU:          keyval = GDK_Menu;      break;
            case DIKS_HELP:          keyval = GDK_Help;      break;
            case DIKS_NEXT:          keyval = GDK_Next;      break;
            case DIKS_BEGIN:         keyval = GDK_Begin;     break;
            case DIKS_BREAK:         keyval = GDK_Break;     break;
            default:
              break;
            }
          break;

        case DIKT_FUNCTION:
          keyval = GDK_F1 + key_symbol - DIKS_F1;
          if (keyval > GDK_F35)
            keyval = GDK_VoidSymbol;
          break;

        case DIKT_MODIFIER:
          switch (key_id)
            {
            case DIKI_SHIFT_L:    keyval = GDK_Shift_L;     break;
            case DIKI_SHIFT_R:    keyval = GDK_Shift_R;     break;
            case DIKI_CONTROL_L:  keyval = GDK_Control_L;   break;
            case DIKI_CONTROL_R:  keyval = GDK_Control_R;   break;
            case DIKI_ALT_L:      keyval = GDK_Alt_L;       break;
            case DIKI_ALT_R:      keyval = GDK_Alt_R;       break;
            case DIKI_META_L:     keyval = GDK_Meta_L;      break;
            case DIKI_META_R:     keyval = GDK_Meta_R;      break;
            case DIKI_SUPER_L:    keyval = GDK_Super_L;     break;
            case DIKI_SUPER_R:    keyval = GDK_Super_R;     break;
            case DIKI_HYPER_L:    keyval = GDK_Hyper_L;     break;
            case DIKI_HYPER_R:    keyval = GDK_Hyper_R;     break;
            default:
              break;
            }
          break;

        case DIKT_LOCK:
          switch (key_symbol)
            {
            case DIKS_CAPS_LOCK:    keyval = GDK_Caps_Lock;   break;
            case DIKS_NUM_LOCK:     keyval = GDK_Num_Lock;    break;
            case DIKS_SCROLL_LOCK:  keyval = GDK_Scroll_Lock; break;
            default:
              break;
            }
          break;

        case DIKT_DEAD:
          /* dead keys are handled directly by directfb */
          break;

        case DIKT_CUSTOM:
          break;
        }
    }

  return keyval;
}

void
_gdk_directfb_keyboard_init (void)
{
  DFBInputDeviceDescription  desc;
  gint                       i, n, length;
  IDirectFBInputDevice      *keyboard = _gdk_display->keyboard;

  if (!keyboard)
    return;
  if (directfb_keymap)
    return;

  keyboard->GetDescription (keyboard, &desc);
  _gdk_display->keymap = g_object_new (gdk_keymap_get_type (), NULL);

  if (desc.min_keycode < 0 || desc.max_keycode < desc.min_keycode)
    return;

  directfb_min_keycode = desc.min_keycode;
  directfb_max_keycode = desc.max_keycode;

  length = directfb_max_keycode - desc.min_keycode + 1;


  directfb_keymap = g_new0 (guint, 4 * length);

  for (i = 0; i < length; i++)
    {
      DFBInputDeviceKeymapEntry  entry;

      if (keyboard->GetKeymapEntry (keyboard,
                                    i + desc.min_keycode, &entry) != DFB_OK)
        continue;
      for (n = 0; n < 4; n++) {
        directfb_keymap[i * 4 + n] =
          gdk_directfb_translate_key (entry.identifier, entry.symbols[n]);
      }
    }
}

void
_gdk_directfb_keyboard_exit (void)
{
  if (!directfb_keymap)
    return;

  g_free (directfb_keymap);
  g_free (_gdk_display->keymap);
  _gdk_display->keymap = NULL;
  directfb_keymap = NULL;
}

void
gdk_directfb_translate_key_event (DFBWindowEvent *dfb_event,
                                  GdkEventKey    *event)
{
  gint  len;
  gchar buf[6];

  g_return_if_fail (dfb_event != NULL);
  g_return_if_fail (event != NULL);

  gdk_directfb_convert_modifiers (dfb_event->modifiers, dfb_event->locks);

  event->state            = _gdk_directfb_modifiers;
  event->group            = (dfb_event->modifiers & DIMM_ALTGR) ? 1 : 0;
  event->hardware_keycode = dfb_event->key_code;
  event->keyval           = gdk_directfb_translate_key (dfb_event->key_id,
                                                        dfb_event->key_symbol);
  /* If the device driver didn't send a key_code (happens with remote
     controls), we try to find a suitable key_code by looking at the
     default keymap. */
  if (dfb_event->key_code == -1 && directfb_keymap)
    {
      gint i;

      for (i = directfb_min_keycode; i <= directfb_max_keycode; i++)
        {
          if (directfb_keymap[(i - directfb_min_keycode) * 4] == event->keyval)
            {
              event->hardware_keycode = i;
              break;
            }
        }
    }

  len = g_unichar_to_utf8 (dfb_event->key_symbol, buf);

  event->string = g_strndup (buf, len);
  event->length = len;
}

/**
 * gdk_keymap_get_caps_lock_state:
 * @keymap: a #GdkKeymap
 *
 * Returns whether the Caps Lock modifer is locked.
 *
 * Returns: %TRUE if Caps Lock is on
 *
 * Since: 2.16
 */
gboolean
gdk_keymap_get_caps_lock_state (GdkKeymap *keymap)
{
  IDirectFBInputDevice *keyboard = _gdk_display->keyboard;

  if (keyboard)
    {
      DFBInputDeviceLockState  state;

      if (keyboard->GetLockState (keyboard, &state) == DFB_OK)
        return ((state & DILS_CAPS) != 0);
    }

  return FALSE;
}

/**
 * gdk_keymap_get_entries_for_keycode:
 * @keymap: a #GdkKeymap or %NULL to use the default keymap
 * @hardware_keycode: a keycode
 * @keys: return location for array of #GdkKeymapKey, or NULL
 * @keyvals: return location for array of keyvals, or NULL
 * @n_entries: length of @keys and @keyvals
 *
 * Returns the keyvals bound to @hardware_keycode.
 * The Nth #GdkKeymapKey in @keys is bound to the Nth
 * keyval in @keyvals. Free the returned arrays with g_free().
 * When a keycode is pressed by the user, the keyval from
 * this list of entries is selected by considering the effective
 * keyboard group and level. See gdk_keymap_translate_keyboard_state().
 *
 * Returns: %TRUE if there were any entries
 **/
gboolean
gdk_keymap_get_entries_for_keycode (GdkKeymap     *keymap,
                                    guint          hardware_keycode,
                                    GdkKeymapKey **keys,
                                    guint        **keyvals,
                                    gint          *n_entries)
{
  gint num = 0;
  gint i, j;
  gint index;

  index = (hardware_keycode - directfb_min_keycode) * 4;

  if (directfb_keymap && (hardware_keycode >= directfb_min_keycode &&
                          hardware_keycode <= directfb_max_keycode))
    {
      for (i = 0; i < 4; i++)
        if (directfb_keymap[index + i] != GDK_VoidSymbol)
          num++;
    }

  if (keys)
    {
      *keys = g_new (GdkKeymapKey, num);

      for (i = 0, j = 0; num > 0 && i < 4; i++)
        if (directfb_keymap[index + i] != GDK_VoidSymbol)
          {
            (*keys)[j].keycode = hardware_keycode;
            (*keys)[j].level   = j % 2;
            (*keys)[j].group   = j > DIKSI_BASE_SHIFT ? 1 : 0;
            j++;
          }
    }
  if (keyvals)
    {
      *keyvals = g_new (guint, num);

      for (i = 0, j = 0; num > 0 && i < 4; i++)
        if (directfb_keymap[index + i] != GDK_VoidSymbol)
          {
            (*keyvals)[j] = directfb_keymap[index + i];
            j++;
          }
    }

  if (n_entries)
    *n_entries = num;

  return (num > 0);
}

static inline void
append_keymap_key (GArray *array,
                   guint   keycode,
                   gint    group,
                   gint    level)
{
  GdkKeymapKey key = { keycode, group, level };

  g_array_append_val (array, key);
}

/**
 * gdk_keymap_get_entries_for_keyval:
 * @keymap: a #GdkKeymap, or %NULL to use the default keymap
 * @keyval: a keyval, such as %GDK_a, %GDK_Up, %GDK_Return, etc.
 * @keys: return location for an array of #GdkKeymapKey
 * @n_keys: return location for number of elements in returned array
 *
 * Obtains a list of keycode/group/level combinations that will
 * generate @keyval. Groups and levels are two kinds of keyboard mode;
 * in general, the level determines whether the top or bottom symbol
 * on a key is used, and the group determines whether the left or
 * right symbol is used. On US keyboards, the shift key changes the
 * keyboard level, and there are no groups. A group switch key might
 * convert a keyboard between Hebrew to English modes, for example.
 * #GdkEventKey contains a %group field that indicates the active
 * keyboard group. The level is computed from the modifier mask.
 * The returned array should be freed
 * with g_free().
 *
 * Return value: %TRUE if keys were found and returned
 **/
gboolean
gdk_keymap_get_entries_for_keyval (GdkKeymap     *keymap,
                                   guint          keyval,
                                   GdkKeymapKey **keys,
                                   gint          *n_keys)
{
  GArray *retval;
  gint    i, j;

  g_return_val_if_fail (keys != NULL, FALSE);
  g_return_val_if_fail (n_keys != NULL, FALSE);
  g_return_val_if_fail (keyval != GDK_VoidSymbol, FALSE);

  retval = g_array_new (FALSE, FALSE, sizeof (GdkKeymapKey));

  for (i = directfb_min_keycode;
       directfb_keymap && i <= directfb_max_keycode;
       i++)
    {
      gint index = i - directfb_min_keycode;

      for (j = 0; j < 4; j++)
        {
          if (directfb_keymap[index * 4 + j] == keyval)
            append_keymap_key (retval, i, j % 2, j > DIKSI_BASE_SHIFT ? 1 : 0);
        }
    }

  if (retval->len > 0)
    {
      *keys = (GdkKeymapKey *) retval->data;
      *n_keys = retval->len;
    }
  else
    {
      *keys = NULL;
      *n_keys = 0;
    }

  g_array_free (retval, retval->len > 0 ? FALSE : TRUE);

  return (*n_keys > 0);
}

gboolean
gdk_keymap_translate_keyboard_state (GdkKeymap       *keymap,
                                     guint            keycode,
                                     GdkModifierType  state,
                                     gint             group,
                                     guint           *keyval,
                                     gint            *effective_group,
                                     gint            *level,
                                     GdkModifierType *consumed_modifiers)
{
  if (directfb_keymap &&
      (keycode >= directfb_min_keycode && keycode <= directfb_max_keycode) &&
      (group == 0 || group == 1))
    {
      gint index = (keycode - directfb_min_keycode) * 4;
      gint i     = (state & GDK_SHIFT_MASK) ? 1 : 0;

      if (directfb_keymap[index + i + 2 * group] != GDK_VoidSymbol)
        {
          if (keyval)
            *keyval = directfb_keymap[index + i + 2 * group];

          if (group && directfb_keymap[index + i] == *keyval)
            {
              if (effective_group)
                *effective_group = 0;
              if (consumed_modifiers)
                *consumed_modifiers = 0;
            }
          else
            {
              if (effective_group)
                *effective_group = group;
              if (consumed_modifiers)
                *consumed_modifiers = GDK_MOD2_MASK;
            }
          if (level)
            *level = i;

          if (i && directfb_keymap[index + 2 * *effective_group] != *keyval)
            if (consumed_modifiers)
              *consumed_modifiers |= GDK_SHIFT_MASK;

          return TRUE;
        }
    }
  if (keyval)
    *keyval             = 0;
  if (effective_group)
    *effective_group    = 0;
  if (level)
    *level              = 0;
  if (consumed_modifiers)
    *consumed_modifiers = 0;

  return FALSE;
}

GdkKeymap *
gdk_keymap_get_for_display (GdkDisplay *display)
{
  if (display == NULL)
    return NULL;

  g_assert (GDK_IS_DISPLAY_DFB (display));

  GdkDisplayDFB *gdisplay = GDK_DISPLAY_DFB (display);

  g_assert (gdisplay->keymap != NULL);

  return gdisplay->keymap;
}

PangoDirection
gdk_keymap_get_direction (GdkKeymap *keymap)
{
  return PANGO_DIRECTION_NEUTRAL;
}

/**
 * gdk_keymap_lookup_key:
 * @keymap: a #GdkKeymap or %NULL to use the default keymap
 * @key: a #GdkKeymapKey with keycode, group, and level initialized
 *
 * Looks up the keyval mapped to a keycode/group/level triplet.
 * If no keyval is bound to @key, returns 0. For normal user input,
 * you want to use gdk_keymap_translate_keyboard_state() instead of
 * this function, since the effective group/level may not be
 * the same as the current keyboard state.
 *
 * Return value: a keyval, or 0 if none was mapped to the given @key
 **/
guint
gdk_keymap_lookup_key (GdkKeymap          *keymap,
                       const GdkKeymapKey *key)
{
  g_warning ("gdk_keymap_lookup_key unimplemented \n");
  return 0;
}

void
gdk_keymap_add_virtual_modifiers (GdkKeymap       *keymap,
                                  GdkModifierType *state)
{
  g_warning ("gdk_keymap_add_virtual_modifiers unimplemented \n");
}

gboolean
gdk_keymap_map_virtual_modifiers (GdkKeymap       *keymap,
                                  GdkModifierType *state)
{
  g_warning ("gdk_keymap_map_virtual_modifiers unimplemented \n");

  return TRUE;
}

#define __GDK_KEYS_DIRECTFB_C__
#include "gdkaliasdef.c"
