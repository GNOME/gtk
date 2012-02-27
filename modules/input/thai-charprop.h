/* Pango
 * thai-charprop.h:
 *
 * Copyright (C) 1999 Red Hat Software
 * Author: Owen Taylor <otaylor@redhat.com>
 *
 * Software and Language Engineering Laboratory, NECTEC
 * Author: Theppitak Karoonboonyanan <thep@links.nectec.or.th>
 *
 * Copyright (c) 1996-2000 by Sun Microsystems, Inc.
 * Author: Chookij Vanatham <Chookij.Vanatham@Eng.Sun.COM>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __THAI_CHARPROP_H__
#define __THAI_CHARPROP_H__

#include <glib.h>

#define isthai(wc)      (0x0E00 <= (wc) && (wc) < 0x0E60)
#define islao(wc)       (0x0E80 <= (wc) && (wc) < 0x0EE0)
/* ucs2tis()
 * Lao:  [0x0E80..0x0EDF] -> [0x20..0x7F]
 * Thai: [0x0E00..0x0E5F] -> [0xA0..0xFF]
 */
#define ucs2tis(wc)     (((wc) - 0x0E00 + 0x20)^0x80)

/* Define TACTIS character classes */
#define CTRL		0
#define NON		1
#define CONS		2
#define LV		3
#define FV1		4
#define FV2		5
#define FV3		6
#define AM		7
#define BV1		8
#define BV2		9
#define BD		10
#define TONE		11
#define AD1		12
#define AD2		13
#define AD3		14
#define AD4		15
#define AV1		16
#define AV2		17
#define AV3		18
#define BCON		19

#define _ND		0
#define _NC		1
#define _UC		(1<<1)
#define _BC		(1<<2)
#define _SC		(1<<3)
#define _AV		(1<<4)
#define _BV		(1<<5)
#define _TN		(1<<6)
#define _AD		(1<<7)
#define _BD		(1<<8)
#define _AM		(1<<9)

#define NoTailCons	_NC
#define UpTailCons	_UC
#define BotTailCons	_BC
#define SpltTailCons	_SC
#define Cons		(NoTailCons|UpTailCons|BotTailCons|SpltTailCons)
#define AboveVowel	_AV
#define BelowVowel	_BV
#define Tone		_TN
#define AboveDiac	_AD
#define BelowDiac	_BD
#define SaraAm		_AM

#define is_char_type(wc, mask)	(thai_char_type[ucs2tis ((wc))] & (mask))
#define TAC_char_class(wc) \
	(isthai(wc)||islao(wc) ? thai_TAC_char_class[ucs2tis (wc)] : NON)
#define TAC_compose_input(wc1,wc2) \
	thai_TAC_compose_input[TAC_char_class(wc1)][TAC_char_class(wc2)]

extern const gshort thai_char_type[256];
extern const gshort thai_TAC_char_class[256];
extern const gchar  thai_TAC_compose_input[20][20];

#endif /* __THAI_CHARPROP_H__ */
