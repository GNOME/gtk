/* MS-Windows Engine (aka GTK-Wimp)
 *
 * Copyright (C) 2003, 2004 Dom Lachowicz <cinamod@hotmail.com>
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

/*
 * These are the real values of these UXTHEME constants, provided so that we can
 * compile/link on Win32 platforms that aren't WinXP, and also build against
 * MinGW 1.0/1.1, which also doesn't have these things defined in its header files
 */

#ifndef XP_THEME_DFNS_H
#define XP_THEME_DFNS_H

typedef HANDLE HTHEME;

#define ETDT_ENABLE         0x00000002
#define ETDT_USETABTEXTURE  0x00000004
#define ETDT_ENABLETAB      (ETDT_ENABLE  | ETDT_USETABTEXTURE)

#define BP_PUSHBUTTON 1
#define BP_CHECKBOX 3

#define HP_HEADERITEM 1

#define CP_DROPDOWNBUTTON 1

#define TABP_TABITEM 1
#define TABP_TABITEMLEFTEDGE 2
#define TABP_PANE 9
#define TABP_BODY 10

#define SBP_ARROWBTN 1
#define SBP_THUMBBTNHORZ 2
#define SBP_THUMBBTNVERT 3
#define SBP_LOWERTRACKHORZ 5
#define SBP_LOWERTRACKVERT 6
#define SBP_GRIPPERHORZ 8
#define SBP_GRIPPERVERT 9

#define EP_EDITTEXT 1

#define SPNP_UP 1
#define SPNP_DOWN 2

#define BP_RADIOBUTTON 2

#define TVP_GLYPH 2

#define PP_BAR 1
#define PP_BARVERT 2
#define PP_CHUNK 3
#define PP_CHUNKVERT 4

#define TTP_STANDARD 1

#define RP_GRIPPER 1
#define RP_GRIPPERVERT 2
#define RP_BAND 3
#define RP_CHEVRON 4

#define TP_BUTTON 1
#define TS_NORMAL 1
#define TS_HOT 2
#define TS_PRESSED 3
#define TS_DISABLED 4

#define TTSS_NORMAL 1

#define CHEVS_NORMAL 1
#define CHEVS_HOT 2
#define CHEVS_PRESSED 3

#define TIS_NORMAL 1
#define TIS_HOT 2
#define TIS_SELECTED 3
#define TIS_DISABLED 4

#define ETS_NORMAL 1
#define ETS_FOCUSED 5
#define ETS_READONLY 6

#define SCRBS_NORMAL 1
#define SCRBS_HOT 2
#define SCRBS_PRESSED 3
#define SCRBS_DISABLED 4

#define ABS_UPNORMAL 1
#define ABS_UPHOT 2
#define ABS_UPPRESSED 3
#define ABS_UPDISABLED 4
#define ABS_DOWNNORMAL 5
#define ABS_DOWNHOT 6
#define ABS_DOWNPRESSED 7
#define ABS_DOWNDISABLED 8
#define ABS_LEFTNORMAL 9
#define ABS_LEFTHOT 10
#define ABS_LEFTPRESSED 11
#define ABS_LEFTDISABLED 12
#define ABS_RIGHTNORMAL 13
#define ABS_RIGHTHOT 14
#define ABS_RIGHTPRESSED 15
#define ABS_RIGHTDISABLED 16

#define CBS_UNCHECKEDNORMAL 1
#define CBS_UNCHECKEDHOT 2
#define CBS_UNCHECKEDPRESSED 3
#define CBS_UNCHECKEDDISABLED 4
#define CBS_CHECKEDNORMAL 5
#define CBS_CHECKEDHOT 6
#define CBS_CHECKEDPRESSED 7
#define CBS_CHECKEDDISABLED 8

#define PBS_NORMAL 1
#define PBS_HOT 2
#define PBS_PRESSED 3
#define PBS_DISABLED 4
#define PBS_DEFAULTED 5

#define DNS_NORMAL 1
#define DNS_HOT 2
#define DNS_PRESSED 3
#define DNS_DISABLED 4

#define UPS_NORMAL 1
#define UPS_HOT 2
#define UPS_PRESSED 3
#define UPS_DISABLED 4

#define GLPS_CLOSED 1
#define GLPS_OPENED 2

#define MP_MENUITEM 1
#define MP_SEPARATOR 6
#define MS_NORMAL 1
#define MS_SELECTED 2
#define MS_DEMOTED 3

#define SP_PANE 1
#define SP_GRIPPER 2

#endif /* XP_THEME_DFNS_H */
