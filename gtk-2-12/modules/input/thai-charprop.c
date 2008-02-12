#include "thai-charprop.h"

const gshort thai_char_type[256] = {
  /*       0,   1,   2,   3,   4,   5,   6,   7,
           8,   9,   A,   B,   C,   D,   E,   F  */

         /* CL1 */
  /*00*/ _ND, _ND, _ND, _ND, _ND, _ND, _ND, _ND,
         _ND, _ND, _ND, _ND, _ND, _ND, _ND, _ND,
  /*10*/ _ND, _ND, _ND, _ND, _ND, _ND, _ND, _ND,
         _ND, _ND, _ND, _ND, _ND, _ND, _ND, _ND,

         /* Lao zone: [U+0E80..U+0EDF] */
  /*20*/ _ND, _NC, _NC, _ND, _NC, _ND, _ND, _NC,
         _NC, _ND, _NC, _ND, _ND, _NC, _ND, _ND,
  /*30*/ _ND, _ND, _ND, _ND, _NC, _NC, _NC, _NC,
         _ND, _NC, _NC, _UC, _NC, _UC, _NC, _UC,
  /*40*/ _ND, _NC, _UC, _NC, _ND, _NC, _ND, _NC,
         _ND, _ND, _NC, _NC, _ND, _NC, _NC, _ND,
  /*50*/ _ND, _AV, _ND, _AM, _AV, _AV, _AV, _AV,
         _BV, _BV, _ND, _AV, _BD, _NC, _ND, _ND,
  /*60*/ _ND, _ND, _ND, _ND, _ND, _ND, _ND, _AD,
         _TN, _TN, _TN, _TN, _AD, _AD, _ND, _ND,
  /*70*/ _ND, _ND, _ND, _ND, _ND, _ND, _ND, _ND,
         _ND, _ND, _ND, _ND, _NC, _NC, _ND, _ND,

         /* CL2 */
  /*80*/ _ND, _ND, _ND, _ND, _ND, _ND, _ND, _ND,
         _ND, _ND, _ND, _ND, _ND, _ND, _ND, _ND,
  /*90*/ _ND, _ND, _ND, _ND, _ND, _ND, _ND, _ND,
         _ND, _ND, _ND, _ND, _ND, _ND, _ND, _ND,

         /* Thai zone: [U+0E00..U+0E5F] */
  /*A0*/ _ND, _NC, _NC, _NC, _NC, _NC, _NC, _NC,
         _NC, _NC, _NC, _NC, _NC, _SC, _BC, _BC,
  /*B0*/ _SC, _NC, _NC, _NC, _NC, _NC, _NC, _NC,
         _NC, _NC, _NC, _UC, _NC, _UC, _NC, _UC,
  /*C0*/ _NC, _NC, _NC, _NC, _ND, _NC, _ND, _NC,
         _NC, _NC, _NC, _NC, _UC, _NC, _NC, _ND,
  /*D0*/ _ND, _AV, _ND, _AM, _AV, _AV, _AV, _AV,
         _BV, _BV, _BD, _ND, _ND, _ND, _ND, _ND,
  /*E0*/ _ND, _ND, _ND, _ND, _ND, _ND, _ND, _AD,
         _TN, _TN, _TN, _TN, _AD, _AD, _AD, _ND,
  /*F0*/ _ND, _ND, _ND, _ND, _ND, _ND, _ND, _ND,
         _ND, _ND, _ND, _ND, _ND, _ND, _ND, _ND,
};

const gshort thai_TAC_char_class[256] = {
  /*	   0,   1,   2,   3,   4,   5,   6,   7,
           8,   9,   A,   B,   C,   D,   E,   F  */

         /* CL1 */
  /*00*/ CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,
         CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,
  /*10*/ CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,
         CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,

         /* Lao zone: [U+0E80..U+0EDF] */
  /*20*/  NON,CONS,CONS, NON,CONS, NON, NON,CONS,
         CONS, NON,CONS, NON, NON,CONS, NON, NON,
  /*30*/  NON, NON, NON, NON,CONS,CONS,CONS,CONS,
          NON,CONS,CONS,CONS,CONS,CONS,CONS,CONS,
  /*40*/  NON,CONS,CONS,CONS, NON,CONS, NON,CONS,
          NON, NON,CONS,CONS, NON,CONS,CONS, NON,
  /*50*/  FV1, AV2, FV1,  AM, AV1, AV3, AV2, AV3,
          BV1, BV2, NON, AV2,BCON, FV3, NON, NON,
  /*60*/   LV,  LV,  LV,  LV,  LV, NON, NON, NON,
         TONE,TONE,TONE,TONE, AD1, AD4, NON, NON,
  /*70*/  NON, NON, NON, NON, NON, NON, NON, NON,
          NON, NON, NON, NON,CONS,CONS, NON,CTRL,

         /* CL2 */
  /*80*/ CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,
         CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,
  /*90*/ CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,
         CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,CTRL,

         /* Thai zone: [U+0E00..U+0E5F] */
  /*A0*/  NON,CONS,CONS,CONS,CONS,CONS,CONS,CONS,
         CONS,CONS,CONS,CONS,CONS,CONS,CONS,CONS,
  /*B0*/ CONS,CONS,CONS,CONS,CONS,CONS,CONS,CONS,
         CONS,CONS,CONS,CONS,CONS,CONS,CONS,CONS,
  /*C0*/ CONS,CONS,CONS,CONS, FV3,CONS, FV3,CONS,
         CONS,CONS,CONS,CONS,CONS,CONS,CONS, NON,
  /*D0*/  FV1, AV2, FV1,  AM, AV1, AV3, AV2, AV3,
          BV1, BV2,  BD, NON, NON, NON, NON, NON,
  /*E0*/   LV,  LV,  LV,  LV,  LV, FV2, NON, AD2,
         TONE,TONE,TONE,TONE, AD1, AD4, AD3, NON,
  /*F0*/  NON, NON, NON, NON, NON, NON, NON, NON,
          NON, NON, NON, NON, NON, NON, NON,CTRL,
};

const gchar thai_TAC_compose_input[20][20] = {
      /* row: Cn-1,  column: Cn */
      /*CTRL NON CONS LV FV1 FV2 FV3  AM BV1 BV2
       * BD TONE AD1 AD2 AD3 AD4 AV1 AV2 AV3 BCON*/
/*CTRL*/{'X','A','A','A','A','A','A','R','R','R',
         'R','R','R','R','R','R','R','R','R','R'},
/*NON */{'X','A','A','A','S','S','A','R','R','R',
         'R','R','R','R','R','R','R','R','R','R'},
/*CONS*/{'X','A','A','A','A','S','A','C','C','C',
         'C','C','C','C','C','C','C','C','C','C'},
/*LV  */{'X','S','A','S','S','S','S','R','R','R',
         'R','R','R','R','R','R','R','R','R','R'},
/*FV1 */{'X','A','A','A','A','S','A','R','R','R',
         'R','R','R','R','R','R','R','R','R','R'},
/*FV2 */{'X','A','A','A','A','S','A','R','R','R',
         'R','R','R','R','R','R','R','R','R','R'},
/*FV3 */{'X','A','A','A','S','A','S','R','R','R',
         'R','R','R','R','R','R','R','R','R','R'},
/*AM  */{'X','A','A','A','S','S','A','R','R','R',
         'R','R','R','R','R','R','R','R','R','R'},
/*BV1 */{'X','A','A','A','S','S','A','R','R','R',
         'R','C','C','R','R','C','R','R','R','R'},
/*BV2 */{'X','A','A','A','S','S','A','R','R','R',
         'R','C','R','R','R','R','R','R','R','R'},
/*BD  */{'X','A','A','A','S','S','A','R','R','R',
         'R','R','R','R','R','R','R','R','R','R'},
/*TONE*/{'X','A','A','A','A','A','A','C','R','R',
         'R','R','R','R','R','R','R','R','R','R'},
/*AD1 */{'X','A','A','A','S','S','A','R','R','R',
         'R','R','R','R','R','R','R','R','R','R'},
/*AD2 */{'X','A','A','A','S','S','A','R','R','R',
         'R','R','R','R','R','R','R','R','R','R'},
/*AD3 */{'X','A','A','A','S','S','A','R','R','R',
         'R','R','R','R','R','R','R','R','R','R'},
/*AD4 */{'X','A','A','A','S','S','A','R','R','R',
         'R','C','R','R','R','R','R','R','R','R'},
/*AV1 */{'X','A','A','A','S','S','A','R','R','R',
         'R','C','C','R','R','C','R','R','R','R'},
/*AV2 */{'X','A','A','A','S','S','A','R','R','R',
         'R','C','R','R','R','R','R','R','R','R'},
/*AV3 */{'X','A','A','A','S','S','A','R','R','R',
         'R','C','R','C','R','R','R','R','R','R'},
/*BCON*/{'X','A','A','A','A','S','A','C','C','C',
         'R','C','R','R','R','C','C','C','C','R'},
};

