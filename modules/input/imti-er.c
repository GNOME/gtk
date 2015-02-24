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
 * Modified for Inuktitut - Robert Brady <robert@suse.co.uk>
 * 
 * Modified for Tigrigna - Daniel Yacob <locales@geez.org>
 *
 */

#include "config.h"
#include <stdio.h>
#include <string.h>

#include "gtk/gtk.h"
#include "gdk/gdkkeysyms.h"

#include "gtk/gtkimmodule.h"
#include "gtk/gtkintl.h"

GType type_ti_er_translit = 0;

static void ti_er_class_init (GtkIMContextSimpleClass *class);
static void ti_er_init (GtkIMContextSimple *im_context);

static void
ti_er_register_type (GTypeModule *module)
{
  const GTypeInfo object_info =
  {
    sizeof (GtkIMContextSimpleClass),
    (GBaseInitFunc) NULL,
    (GBaseFinalizeFunc) NULL,
    (GClassInitFunc) ti_er_class_init,
    NULL,           /* class_finalize */
    NULL,           /* class_data */
    sizeof (GtkIMContextSimple),
    0,
    (GInstanceInitFunc) ti_er_init,
  };

  type_ti_er_translit = 
    g_type_module_register_type (module,
				 GTK_TYPE_IM_CONTEXT_SIMPLE,
				 "GtkIMContextTigrignaEritrea",
				 &object_info, 0);
}

#define SYL(a,b) \
  a,  0,  0, 0, 0, 0, b+5, \
  a, 'A', 0, 0, 0, 0, b+3, \
  a, 'E', 0, 0, 0, 0, b+4, \
  a, 'I', 0, 0, 0, 0, b+2, \
  a, 'O', 0, 0, 0, 0, b+6, \
  a, 'U', 0, 0, 0, 0, b+1, \
  a, 'a', 0, 0, 0, 0, b+3, \
  a, 'e', 0, 0, 0, 0, b, \
  a, 'e', 'e', 0, 0, 0, b+4, \
  a, 'i', 0, 0, 0, 0, b+2, \
  a, 'o', 0, 0, 0, 0, b+6, \
  a, 'u', 0, 0, 0, 0, b+1,

#define SYLW1(a,b) \
  a,  0,  0, 0, 0, 0, b+5, \
  a, 'A', 0, 0, 0, 0, b+3, \
  a, 'E', 0, 0, 0, 0, b+4, \
  a, 'I', 0, 0, 0, 0, b+2, \
  a, 'O', 0, 0, 0, 0, b+6, \
  a, 'U', 0, 0, 0, 0, b+1, \
  a, 'W', 0, 0, 0, 0, b+7, \
  a, 'W', 'A', 0, 0, 0, b+7, \
  a, 'W', 'a', 0, 0, 0, b+7,
#define SYLW2(a,b) \
  a, 'a', 0, 0, 0, 0, b+3, \
  a, 'e', 0, 0, 0, 0, b, \
  a, 'e', 'e', 0, 0, 0, b+4, \
  a, 'i', 0, 0, 0, 0, b+2, \
  a, 'o', 0, 0, 0, 0, b+6, \
  a, 'u', 0, 0, 0, 0, b+1, \
  a, 'w', 'w',   0, 0, 0, b+7, \
  a, 'w', 'w', 'a', 0, 0, b+7,

#define SYLW(a,b) \
  SYLW1(a,b)\
  SYLW2(a,b)

#define SYLWW(a,b) \
  a,  0,  0, 0, 0, 0, b+5, \
  a, 'A', 0, 0, 0, 0, b+3, \
  a, 'E', 0, 0, 0, 0, b+4, \
  a, 'I', 0, 0, 0, 0, b+2, \
  a, 'O', 0, 0, 0, 0, b+6, \
  a, 'O', 'O', 0, 0, 0, b+8, \
  a, 'O', 'o', 0, 0, 0, b+8, \
  a, 'U', 0, 0, 0, 0, b+1, \
  a, 'W', 0, 0, 0, 0, b+11, \
  a, 'W', '\'', 0, 0, 0, b+13, \
  a, 'W', 'A', 0, 0, 0, b+11, \
  a, 'W', 'E', 0, 0, 0, b+12, \
  a, 'W', 'I', 0, 0, 0, b+10, \
  a, 'W', 'U', 0, 0, 0, b+13, \
  a, 'W', 'a', 0, 0, 0, b+11, \
  a, 'W', 'e', 0, 0, 0, b+8, \
  a, 'W', 'e', 'e', 0, 0, b+12, \
  a, 'W', 'i', 0, 0, 0, b+10, \
  a, 'W', 'u', 0, 0, 0, b+13, \
  a, 'a', 0, 0, 0, 0, b+3, \
  a, 'e', 0, 0, 0, 0, b, \
  a, 'e', 'e', 0, 0, 0, b+4, \
  a, 'i', 0, 0, 0, 0, b+2, \
  a, 'o', 0, 0, 0, 0, b+6, \
  a, 'o', 'o', 0, 0, 0, b+8, \
  a, 'u', 0, 0, 0, 0, b+1, \
  a, 'w', 'w', 0, 0, 0, b+11, \
  a, 'w', 'w', '\'', 0, 0, b+13, \
  a, 'w', 'w', 'E', 0, 0, b+12, \
  a, 'w', 'w', 'a', 0, 0, b+11, \
  a, 'w', 'w', 'e', 0, 0, b+8, \
  a, 'w', 'w', 'e', 'e', 0, b+12, \
  a, 'w', 'w', 'i', 0, 0, b+10, \
  a, 'w', 'w', 'u', 0, 0, b+13,

static guint16 ti_er_compose_seqs[] = {
  /* do punctuation and numerals here */

  '\'',   0, 0, 0, 0, 0, GDK_KEY_dead_grave,  /* hopefully this has no side effects */
  '\'', '\'', 0, 0, 0, 0, GDK_KEY_apostrophe,
  '\'', '1', 0, 0, 0, 0, 0x1369,
  '\'', '1', '0', 0, 0, 0, 0x1372,
  '\'', '1', '0', '0', 0, 0, 0x137b,
  '\'', '1', '0', 'k', 0, 0, 0x137c,
  /* '\'', '1', '0', '0', '0',  0, 0x137b,
  '\'', '1', '0', '0', '0', '0', 0, 0x137c, */
  '\'', '2', 0, 0, 0, 0, 0x136a,
  '\'', '2', '0', 0, 0, 0, 0x1373,
  '\'', '3', 0, 0, 0, 0, 0x136b,
  '\'', '3', '0', 0, 0, 0, 0x1374,
  '\'', '4', 0, 0, 0, 0, 0x136c,
  '\'', '4', '0', 0, 0, 0, 0x1375,
  '\'', '5', 0, 0, 0, 0, 0x136d,
  '\'', '5', '0', 0, 0, 0, 0x1376,
  '\'', '6', 0, 0, 0, 0, 0x136e,
  '\'', '6', '0', 0, 0, 0, 0x1377,
  '\'', '7', 0, 0, 0, 0, 0x136f,
  '\'', '7', '0', 0, 0, 0, 0x1378,
  '\'', '8', 0, 0, 0, 0, 0x1370,
  '\'', '8', '0', 0, 0, 0, 0x1379,
  '\'', '9', 0, 0, 0, 0, 0x1371,
  '\'', '9', '0', 0, 0, 0, 0x137a,
  ',',  0,  0, 0, 0, 0, 0x1363,
  ',',  ',',  0, 0, 0, 0, ',',
  '-',  0,  0, 0, 0, 0, '-',
  '-',  ':',  0, 0, 0, 0, 0x1365,
  ':',  0,  0, 0, 0, 0, 0x1361,
  ':',  '-',  0, 0, 0, 0, 0x1366,
  ':',  ':',  0, 0, 0, 0, 0x1362,
  ':',  ':',  ':', 0, 0, 0, ':',
  ':',  '|',  ':', 0, 0, 0, 0x1368,
  ';',  0,  0, 0, 0, 0, 0x1364,
  ';',  ';',  0, 0, 0, 0, ';',
  '<',  0,  0, 0, 0, 0, '<',
  '<',  '<',  0, 0, 0, 0, 0x00AB,
  '>',  0,  0, 0, 0, 0, '>',
  '>',  '>',  0, 0, 0, 0, 0x00BB,
  '?',  0,  0, 0, 0, 0, 0x1367,
  '?',  '?',  0, 0, 0, 0, '?',
  'A',  0,  0,  0,  0, 0, 0x12A0,
  'A','A',  0,  0,  0, 0, 0x12D0,
  SYLW('B', 0x1260)
  SYLW('C', 0x1328)
  SYLW('D', 0x12f8)
  'E',  0,  0,  0,  0, 0, 0x12A4,
  'E','E',  0,  0,  0, 0, 0x12D4,
  SYLW1('F', 0x1348)
  'F', 'Y',   0,  0,  0, 0, 0x135A,
  'F', 'Y', 'A',  0,  0, 0, 0x135A,
  'F', 'Y', 'a',  0,  0, 0, 0x135A,
  SYLW2('F', 0x1348)
  SYL('G', 0x1318)
  SYLW('H', 0x1210)
  'I',  0,  0,  0,  0, 0, 0x12A5,
  'I','A',  0,  0,  0, 0, 0x12A3,
  'I','E',  0,  0,  0, 0, 0x12A4,
  'I','I',  0,  0,  0, 0, 0x12D5,
  'I','I','E',  0,  0, 0, 0x12D4,
  'I','I','a',  0,  0, 0, 0x12D3,
  'I','I','e',  0,  0, 0, 0x12D0,
  'I','I','i',  0,  0, 0, 0x12D2,
  'I','I','o',  0,  0, 0, 0x12D6,
  'I','I','u',  0,  0, 0, 0x12D1,
  'I','O',  0,  0,  0, 0, 0x12A6,
  'I','U',  0,  0,  0, 0, 0x12A1,
  'I','W',  0,  0,  0, 0, 0x12A7,
  'I','a',  0,  0,  0, 0, 0x12A3,
  'I','e',  0,  0,  0, 0, 0x12A0,
  'I','i',  0,  0,  0, 0, 0x12A2,
  'I','o',  0,  0,  0, 0, 0x12A6,
  'I','u',  0,  0,  0, 0, 0x12A1,
  SYLWW('K', 0x12b8)
  SYLW('L', 0x1208)
  SYLW1('M', 0x1218)
  'M', 'Y',   0,  0,  0, 0, 0x1359,
  'M', 'Y', 'A',  0,  0, 0, 0x1359,
  'M', 'Y', 'a',  0,  0, 0, 0x1359,
  SYLW2('M', 0x1218)
  SYLW('N', 0x1298)
  'O',  0,  0,  0,  0, 0, 0x12A6,
  'O','O',  0,  0,  0, 0, 0x12D6,
  SYLW('P', 0x1330)
  SYLWW('Q', 0x1250) 
  SYLW1('R', 0x1228)
  'R', 'Y',   0,  0,  0, 0, 0x1358,
  'R', 'Y', 'A',  0,  0, 0, 0x1358,
  'R', 'Y', 'a',  0,  0, 0, 0x1358,
  SYLW2('R', 0x1228)
  'S',  0,  0, 0, 0, 0, 0x1338+5,
  'S', 'A', 0, 0, 0, 0, 0x1338+3,
  'S', 'E', 0, 0, 0, 0, 0x1338+4,
  'S', 'I', 0, 0, 0, 0, 0x1338+2,
  'S', 'O', 0, 0, 0, 0, 0x1338+6,
  'S', 'S', 0, 0, 0, 0, 0x1340+5,
  'S', 'S', 'A', 0, 0, 0, 0x1340+3,
  'S', 'S', 'E', 0, 0, 0, 0x1340+4,
  'S', 'S', 'I', 0, 0, 0, 0x1340+2,
  'S', 'S', 'O', 0, 0, 0, 0x1340+6,
  'S', 'S', 'U', 0, 0, 0, 0x1340+1,
  'S', 'S', 'a', 0, 0, 0, 0x1340+3,
  'S', 'S', 'e', 0, 0, 0, 0x1340,
  'S', 'S', 'e', 'e', 0, 0, 0x1340+4,
  'S', 'S', 'i', 0, 0, 0, 0x1340+2,
  'S', 'S', 'o', 0, 0, 0, 0x1340+6,
  'S', 'S', 'u', 0, 0, 0, 0x1340+1,
  'S', 'U', 0, 0, 0, 0, 0x1338+1,
  'S', 'W', 0, 0, 0, 0, 0x1338+7,
  'S', 'W', 'A', 0, 0, 0, 0x1338+7,
  'S', 'W', 'a', 0, 0, 0, 0x1338+7,
  'S', 'a', 0, 0, 0, 0, 0x1338+3,
  'S', 'e', 0, 0, 0, 0, 0x1338,
  'S', 'e', 'e', 0, 0, 0, 0x1338+4,
  'S', 'i', 0, 0, 0, 0, 0x1338+2,
  'S', 'o', 0, 0, 0, 0, 0x1338+6,
  'S', 'u', 0, 0, 0, 0, 0x1338+1,
  'S', 'w', 'w',   0, 0, 0, 0x1338+7,
  'S', 'w', 'w', 'a', 0, 0, 0x1338+7,
  SYLW('T', 0x1320)
  'U',  0,  0,  0,  0, 0, 0x12A1,
  'U','U',  0,  0,  0, 0, 0x12D1,
  SYLW('V', 0x1268)
  SYL('W', 0x12c8)
  SYLW('X', 0x1238)
  SYL('Y', 0x12e8)
  SYLW('Z', 0x12e0)

  /* much, much work to be done for lone vowels */
  'a',  0,  0,  0,  0, 0, 0x12A3,
  'a','a',  0,  0,  0, 0, 0x12D3,
  'a','a','a',  0,  0, 0, 0x12D0,
  'a','a','a','a',  0, 0, 0x12A0,
  SYLW('b', 0x1260)
  SYLW('c', 0x1278)
  SYLW('d', 0x12f0)
  'e',  0,  0,  0,  0, 0, 0x12A5,
  'e','A',  0,  0,  0, 0, 0x12A3,
  'e','E',  0,  0,  0, 0, 0x12A4,
  'e','I',  0,  0,  0, 0, 0x12A2,
  'e','O',  0,  0,  0, 0, 0x12A6,
  'e','U',  0,  0,  0, 0, 0x12A1,
  'e','W',  0,  0,  0, 0, 0x12A7,
  'e','a',  0,  0,  0, 0, 0x12D0,
  'e','e',  0,  0,  0, 0, 0x12D5,
  'e','e','E',  0,  0, 0, 0x12D4,
  'e','e','a',  0,  0, 0, 0x12D3,
  'e','e','e',  0,  0, 0, 0x12D0,
  'e','e','i',  0,  0, 0, 0x12D2,
  'e','e','o',  0,  0, 0, 0x12D6,
  'e','e','u',  0,  0, 0, 0x12D1,
  'e','i',  0,  0,  0, 0, 0x12A2,
  'e','o',  0,  0,  0, 0, 0x12A6,
  'e','u',  0,  0,  0, 0, 0x12A1,
  SYLW1('f', 0x1348)
  'f', 'Y',   0,  0,  0, 0, 0x135A,
  'f', 'Y', 'A',  0,  0, 0, 0x135A,
  'f', 'Y', 'a',  0,  0, 0, 0x135A,
  SYLW2('f', 0x1348)
  SYLWW('g', 0x1308)
  'h',  0,  0, 0, 0, 0, 0x1200+5,
  'h', 'A', 0, 0, 0, 0, 0x1200+3,
  'h', 'E', 0, 0, 0, 0, 0x1200+4,
  'h', 'I', 0, 0, 0, 0, 0x1200+2,
  'h', 'O', 0, 0, 0, 0, 0x1200+6,
  'h', 'U', 0, 0, 0, 0, 0x1200+1,
  'h', 'W', 0, 0, 0, 0, 0x1280+11,
  'h', 'W', '\'', 0, 0, 0, 0x1280+13,
  'h', 'W', 'A', 0, 0, 0, 0x1280+11,
  'h', 'W', 'E', 0, 0, 0, 0x1280+12,
  'h', 'W', 'I', 0, 0, 0, 0x1280+10,
  'h', 'W', 'U', 0, 0, 0, 0x1280+13,
  'h', 'W', 'a', 0, 0, 0, 0x1280+11,
  'h', 'W', 'e', 0, 0, 0, 0x1280+8,
  'h', 'W', 'e', 'e', 0, 0, 0x1280+12,
  'h', 'W', 'i', 0, 0, 0, 0x1280+10,
  'h', 'W', 'u', 0, 0, 0, 0x1280+13,
  'h', 'a', 0, 0, 0, 0, 0x1200+3,
  'h', 'e', 0, 0, 0, 0, 0x1200,
  'h', 'e', 'e', 0, 0, 0, 0x1200+4,
  'h', 'h', 0, 0, 0, 0, 0x1280+5,
  'h', 'h', 'A', 0, 0, 0, 0x1280+3,
  'h', 'h', 'E', 0, 0, 0, 0x1280+4,
  'h', 'h', 'I', 0, 0, 0, 0x1280+2,
  'h', 'h', 'O', 0, 0, 0, 0x1280+6,
  'h', 'h', 'O', 'O', 0, 0, 0x1280+8,
  'h', 'h', 'U', 0, 0, 0, 0x1280+1,
  'h', 'h', 'W', 0, 0, 0, 0x1280+11,
  'h', 'h', 'W', '\'', 0, 0, 0x1280+13,
  'h', 'h', 'W', 'A', 0, 0, 0x1280+11,
  'h', 'h', 'W', 'E', 0, 0, 0x1280+12,
  'h', 'h', 'W', 'I', 0, 0, 0x1280+10,
  'h', 'h', 'W', 'U', 0, 0, 0x1280+13,
  'h', 'h', 'W', 'a', 0, 0, 0x1280+11,
  'h', 'h', 'W', 'e', 0, 0, 0x1280+8,
  'h', 'h', 'W', 'e', 'e', 0, 0x1280+12,
  'h', 'h', 'W', 'i', 0, 0, 0x1280+10,
  'h', 'h', 'W', 'u', 0, 0, 0x1280+13,
  'h', 'h', 'a', 0, 0, 0, 0x1280+3,
  'h', 'h', 'e', 0, 0, 0, 0x1280,
  'h', 'h', 'e', 'e', 0, 0, 0x1280+4,
  'h', 'h', 'i', 0, 0, 0, 0x1280+2,
  'h', 'h', 'o', 0, 0, 0, 0x1280+6,
  'h', 'h', 'o', 'o', 0, 0, 0x1280+8,
  'h', 'h', 'u', 0, 0, 0, 0x1280+1,
  'h', 'h', 'w', 'w',   0, 0, 0x1280+11,
  'h', 'h', 'w', 'w', 'a', 0, 0x1280+11,
  'h', 'h', 'w', 'w', 0, 0, 0x1280+11,
  'h', 'h', 'w', 'w', '\'', 0, 0x1280+13,
  'h', 'h', 'w', 'w', 'E', 0, 0x1280+12,
  'h', 'h', 'w', 'w', 'a', 0, 0x1280+11,
  'h', 'h', 'w', 'w', 'e', 0, 0x1280+8,
  /* 'h', 'h', 'w', 'w', 'e', 'e', 0, 0x1280+12,  too long for now */
  'h', 'h', 'w', 'w', 'i', 0, 0x1280+10,
  'h', 'h', 'w', 'w', 'u', 0, 0x1280+13,
  'h', 'i', 0, 0, 0, 0, 0x1200+2,
  'h', 'o', 0, 0, 0, 0, 0x1200+6,
  'h', 'u', 0, 0, 0, 0, 0x1200+1,
  'h', 'w', 'w',   0, 0, 0, 0x1280+11,
  'h', 'w', 'w', 'a', 0, 0, 0x1280+11,
  'h', 'w', 'w', 0, 0, 0, 0x1280+11,
  'h', 'w', 'w', '\'', 0, 0, 0x1280+13,
  'h', 'w', 'w', 'E', 0, 0, 0x1280+12,
  'h', 'w', 'w', 'a', 0, 0, 0x1280+11,
  'h', 'w', 'w', 'e', 0, 0, 0x1280+8,
  'h', 'w', 'w', 'e', 'e', 0, 0x1280+12,
  'h', 'w', 'w', 'i', 0, 0, 0x1280+10,
  'h', 'w', 'w', 'u', 0, 0, 0x1280+13,
  'i',  0,  0,  0,  0, 0, 0x12A2,
  'i', 'i', 0,  0,  0, 0, 0x12D2,
  SYLW('j', 0x1300)
  SYLWW('k', 0x12a8)
  SYLW('l', 0x1208)
  SYLW1('m', 0x1218)
  'm', 'Y',   0,  0,  0, 0, 0x1359,
  'm', 'Y', 'A',  0,  0, 0, 0x1359,
  'm', 'Y', 'a',  0,  0, 0, 0x1359,
  SYLW2('m', 0x1218)
  SYLW('n', 0x1290)
  'o',  0,  0,  0,  0, 0, 0x12A6,
  'o','o',  0,  0,  0, 0, 0x12D6,
  SYLW('p', 0x1350)
  SYLWW('q', 0x1240)
  SYLW1('r', 0x1228)
  'r', 'Y',   0,  0,  0, 0, 0x1358,
  'r', 'Y', 'A',  0,  0, 0, 0x1358,
  'r', 'Y', 'a',  0,  0, 0, 0x1358,
  SYLW2('r', 0x1228)
  's',  0,  0, 0, 0, 0, 0x1230+5,
  's', 'A', 0, 0, 0, 0, 0x1230+3,
  's', 'E', 0, 0, 0, 0, 0x1230+4,
  's', 'I', 0, 0, 0, 0, 0x1230+2,
  's', 'O', 0, 0, 0, 0, 0x1230+6,
  's', 'U', 0, 0, 0, 0, 0x1230+1,
  's', 'W', 0, 0, 0, 0, 0x1230+7,
  's', 'W', 'A', 0, 0, 0, 0x1230+7,
  's', 'W', 'a', 0, 0, 0, 0x1230+7,
  's', 'a', 0, 0, 0, 0, 0x1230+3,
  's', 'e', 0, 0, 0, 0, 0x1230,
  's', 'e', 'e', 0, 0, 0, 0x1230+4,
  's', 'i', 0, 0, 0, 0, 0x1230+2,
  's', 'o', 0, 0, 0, 0, 0x1230+6,
  's', 's', 0, 0, 0, 0, 0x1220+5,
  's', 's', 'A', 0, 0, 0, 0x1220+3,
  's', 's', 'E', 0, 0, 0, 0x1220+4,
  's', 's', 'I', 0, 0, 0, 0x1220+2,
  's', 's', 'O', 0, 0, 0, 0x1220+6,
  's', 's', 'U', 0, 0, 0, 0x1220+1,
  's', 's', 'W', 0, 0, 0, 0x1220+7,
  's', 's', 'W', 'A', 0, 0, 0x1220+7,
  's', 's', 'W', 'a', 0, 0, 0x1220+7,
  's', 's', 'a', 0, 0, 0, 0x1220+3,
  's', 's', 'e', 0, 0, 0, 0x1220,
  's', 's', 'e', 'e', 0, 0, 0x1220+4,
  's', 's', 'i', 0, 0, 0, 0x1220+2,
  's', 's', 'o', 0, 0, 0, 0x1220+6,
  's', 's', 'u', 0, 0, 0, 0x1220+1,
  's', 's', 'w', 'w', 0, 0, 0x1220+7,
  's', 's', 'w', 'w', 'a', 0, 0x1220+7,
  's', 'u', 0, 0, 0, 0, 0x1230+1,
  's', 'w', 'w',   0, 0, 0, 0x1230+7,
  's', 'w', 'w', 'a', 0, 0, 0x1230+7,
  SYLW('t', 0x1270)
  'u',  0,  0,  0,  0, 0, 0x12A1,
  'u','u',  0,  0,  0, 0, 0x12D1,
  SYLW('v', 0x1268)
  SYL('w', 0x12c8)
  SYLW('x', 0x1238)
  SYL('y', 0x12e8)
  SYLW('z', 0x12d8)
  GDK_KEY_Shift_L, GDK_KEY_space, 0, 0, 0, 0, 0x1361,
  GDK_KEY_Shift_R, GDK_KEY_space, 0, 0, 0, 0, 0x1361,
};

static void
ti_er_class_init (GtkIMContextSimpleClass *class)
{
}

static void
ti_er_init (GtkIMContextSimple *im_context)
{
  gtk_im_context_simple_add_table (im_context,
				   ti_er_compose_seqs,
				   5,
				   G_N_ELEMENTS (ti_er_compose_seqs) / (5 + 2));
}

static const GtkIMContextInfo ti_er_info = { 
  "ti_er",		   /* ID */
  NC_("input method menu", "Tigrigna-Eritrean (EZ+)"), /* Human readable name */
  GETTEXT_PACKAGE,	   /* Translation domain */
   GTK_LOCALEDIR,	   /* Dir for bindtextdomain (not strictly needed for "gtk+") */
  "ti"			   /* Languages for which this module is the default */
};

static const GtkIMContextInfo *info_list[] = {
  &ti_er_info
};

#ifndef INCLUDE_IM_ti_er
#define MODULE_ENTRY(type, function) G_MODULE_EXPORT type im_module_ ## function
#else
#define MODULE_ENTRY(type, function) type _gtk_immodule_ti_er_ ## function
#endif

MODULE_ENTRY (void, init) (GTypeModule *module)
{
  ti_er_register_type (module);
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
  if (strcmp (context_id, "ti_er") == 0)
    return g_object_new (type_ti_er_translit, NULL);
  else
    return NULL;
}
