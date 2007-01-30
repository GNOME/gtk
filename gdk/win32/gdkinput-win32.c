/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2002 Tor Lillqvist
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
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "gdk.h"
#include "gdkinput.h"
#include "gdkinternals.h"
#include "gdkprivate-win32.h"
#include "gdkinput-win32.h"

#ifdef HAVE_WINTAB

#define PACKETDATA (PK_CONTEXT | PK_CURSOR | PK_BUTTONS | PK_X | PK_Y  | PK_NORMAL_PRESSURE | PK_ORIENTATION)
/* We want everything in absolute mode */
#define PACKETMODE (0)
#include <pktdef.h>

#define DEBUG_WINTAB 1		/* Verbose debug messages enabled */

#define PROXIMITY_OUT_DELAY 200 /* In milliseconds, see set_ignore_core */

#endif

#if defined(HAVE_WINTAB) || defined(HAVE_WHATEVER_OTHER)
#define HAVE_SOME_XINPUT
#endif

#define TWOPI (2.*G_PI)

/* Forward declarations */

#ifdef HAVE_WINTAB

static GdkDevicePrivate *gdk_input_find_dev_from_ctx (HCTX hctx,
						      UINT id);
static GList     *wintab_contexts = NULL;

static GdkWindow *wintab_window = NULL;

#endif /* HAVE_WINTAB */

#ifdef HAVE_SOME_XINPUT

static GdkWindow *x_grab_window = NULL; /* Window that currently holds
					 * the extended inputs grab
					 */
static GdkEventMask x_grab_mask;
static gboolean x_grab_owner_events;

#endif /* HAVE_SOME_XINPUT */

#ifdef HAVE_WINTAB

static GdkDevicePrivate *
gdk_input_find_dev_from_ctx (HCTX hctx,
			     UINT cursor)
{
  GList *tmp_list = _gdk_input_devices;
  GdkDevicePrivate *gdkdev;

  while (tmp_list)
    {
      gdkdev = (GdkDevicePrivate *) (tmp_list->data);
      if (gdkdev->hctx == hctx && gdkdev->cursor == cursor)
	return gdkdev;
      tmp_list = tmp_list->next;
    }
  return NULL;
}

#if DEBUG_WINTAB

static void
print_lc(LOGCONTEXT *lc)
{
  g_print ("lcName = %s\n", lc->lcName);
  g_print ("lcOptions =");
  if (lc->lcOptions & CXO_SYSTEM) g_print (" CXO_SYSTEM");
  if (lc->lcOptions & CXO_PEN) g_print (" CXO_PEN");
  if (lc->lcOptions & CXO_MESSAGES) g_print (" CXO_MESSAGES");
  if (lc->lcOptions & CXO_MARGIN) g_print (" CXO_MARGIN");
  if (lc->lcOptions & CXO_MGNINSIDE) g_print (" CXO_MGNINSIDE");
  if (lc->lcOptions & CXO_CSRMESSAGES) g_print (" CXO_CSRMESSAGES");
  g_print ("\n");
  g_print ("lcStatus =");
  if (lc->lcStatus & CXS_DISABLED) g_print (" CXS_DISABLED");
  if (lc->lcStatus & CXS_OBSCURED) g_print (" CXS_OBSCURED");
  if (lc->lcStatus & CXS_ONTOP) g_print (" CXS_ONTOP");
  g_print ("\n");
  g_print ("lcLocks =");
  if (lc->lcLocks & CXL_INSIZE) g_print (" CXL_INSIZE");
  if (lc->lcLocks & CXL_INASPECT) g_print (" CXL_INASPECT");
  if (lc->lcLocks & CXL_SENSITIVITY) g_print (" CXL_SENSITIVITY");
  if (lc->lcLocks & CXL_MARGIN) g_print (" CXL_MARGIN");
  g_print ("\n");
  g_print ("lcMsgBase = %#x, lcDevice = %#x, lcPktRate = %d\n",
	  lc->lcMsgBase, lc->lcDevice, lc->lcPktRate);
  g_print ("lcPktData =");
  if (lc->lcPktData & PK_CONTEXT) g_print (" PK_CONTEXT");
  if (lc->lcPktData & PK_STATUS) g_print (" PK_STATUS");
  if (lc->lcPktData & PK_TIME) g_print (" PK_TIME");
  if (lc->lcPktData & PK_CHANGED) g_print (" PK_CHANGED");
  if (lc->lcPktData & PK_SERIAL_NUMBER) g_print (" PK_SERIAL_NUMBER");
  if (lc->lcPktData & PK_CURSOR) g_print (" PK_CURSOR");
  if (lc->lcPktData & PK_BUTTONS) g_print (" PK_BUTTONS");
  if (lc->lcPktData & PK_X) g_print (" PK_X");
  if (lc->lcPktData & PK_Y) g_print (" PK_Y");
  if (lc->lcPktData & PK_Z) g_print (" PK_Z");
  if (lc->lcPktData & PK_NORMAL_PRESSURE) g_print (" PK_NORMAL_PRESSURE");
  if (lc->lcPktData & PK_TANGENT_PRESSURE) g_print (" PK_TANGENT_PRESSURE");
  if (lc->lcPktData & PK_ORIENTATION) g_print (" PK_ORIENTATION");
  if (lc->lcPktData & PK_ROTATION) g_print (" PK_ROTATION");
  g_print ("\n");
  g_print ("lcPktMode =");
  if (lc->lcPktMode & PK_CONTEXT) g_print (" PK_CONTEXT");
  if (lc->lcPktMode & PK_STATUS) g_print (" PK_STATUS");
  if (lc->lcPktMode & PK_TIME) g_print (" PK_TIME");
  if (lc->lcPktMode & PK_CHANGED) g_print (" PK_CHANGED");
  if (lc->lcPktMode & PK_SERIAL_NUMBER) g_print (" PK_SERIAL_NUMBER");
  if (lc->lcPktMode & PK_CURSOR) g_print (" PK_CURSOR");
  if (lc->lcPktMode & PK_BUTTONS) g_print (" PK_BUTTONS");
  if (lc->lcPktMode & PK_X) g_print (" PK_X");
  if (lc->lcPktMode & PK_Y) g_print (" PK_Y");
  if (lc->lcPktMode & PK_Z) g_print (" PK_Z");
  if (lc->lcPktMode & PK_NORMAL_PRESSURE) g_print (" PK_NORMAL_PRESSURE");
  if (lc->lcPktMode & PK_TANGENT_PRESSURE) g_print (" PK_TANGENT_PRESSURE");
  if (lc->lcPktMode & PK_ORIENTATION) g_print (" PK_ORIENTATION");
  if (lc->lcPktMode & PK_ROTATION) g_print (" PK_ROTATION");
  g_print ("\n");
  g_print ("lcMoveMask =");
  if (lc->lcMoveMask & PK_CONTEXT) g_print (" PK_CONTEXT");
  if (lc->lcMoveMask & PK_STATUS) g_print (" PK_STATUS");
  if (lc->lcMoveMask & PK_TIME) g_print (" PK_TIME");
  if (lc->lcMoveMask & PK_CHANGED) g_print (" PK_CHANGED");
  if (lc->lcMoveMask & PK_SERIAL_NUMBER) g_print (" PK_SERIAL_NUMBER");
  if (lc->lcMoveMask & PK_CURSOR) g_print (" PK_CURSOR");
  if (lc->lcMoveMask & PK_BUTTONS) g_print (" PK_BUTTONS");
  if (lc->lcMoveMask & PK_X) g_print (" PK_X");
  if (lc->lcMoveMask & PK_Y) g_print (" PK_Y");
  if (lc->lcMoveMask & PK_Z) g_print (" PK_Z");
  if (lc->lcMoveMask & PK_NORMAL_PRESSURE) g_print (" PK_NORMAL_PRESSURE");
  if (lc->lcMoveMask & PK_TANGENT_PRESSURE) g_print (" PK_TANGENT_PRESSURE");
  if (lc->lcMoveMask & PK_ORIENTATION) g_print (" PK_ORIENTATION");
  if (lc->lcMoveMask & PK_ROTATION) g_print (" PK_ROTATION");
  g_print ("\n");
  g_print ("lcBtnDnMask = %#x, lcBtnUpMask = %#x\n",
	  (guint) lc->lcBtnDnMask, (guint) lc->lcBtnUpMask);
  g_print ("lcInOrgX = %ld, lcInOrgY = %ld, lcInOrgZ = %ld\n",
	  lc->lcInOrgX, lc->lcInOrgY, lc->lcInOrgZ);
  g_print ("lcInExtX = %ld, lcInExtY = %ld, lcInExtZ = %ld\n",
	  lc->lcInExtX, lc->lcInExtY, lc->lcInExtZ);
  g_print ("lcOutOrgX = %ld, lcOutOrgY = %ld, lcOutOrgZ = %ld\n",
	  lc->lcOutOrgX, lc->lcOutOrgY, lc->lcOutOrgZ);
  g_print ("lcOutExtX = %ld, lcOutExtY = %ld, lcOutExtZ = %ld\n",
	  lc->lcOutExtX, lc->lcOutExtY, lc->lcOutExtZ);
  g_print ("lcSensX = %g, lcSensY = %g, lcSensZ = %g\n",
	  lc->lcSensX / 65536., lc->lcSensY / 65536., lc->lcSensZ / 65536.);
  g_print ("lcSysMode = %d\n", lc->lcSysMode);
  g_print ("lcSysOrgX = %d, lcSysOrgY = %d\n",
	  lc->lcSysOrgX, lc->lcSysOrgY);
  g_print ("lcSysExtX = %d, lcSysExtY = %d\n",
	  lc->lcSysExtX, lc->lcSysExtY);
  g_print ("lcSysSensX = %g, lcSysSensY = %g\n",
	  lc->lcSysSensX / 65536., lc->lcSysSensY / 65536.);
}

static void
print_cursor (int index)
{
  int size;
  int i;
  char *name;
  BOOL active;
  WTPKT wtpkt;
  BYTE buttons;
  BYTE buttonbits;
  char *btnnames;
  char *p;
  BYTE buttonmap[32];
  BYTE sysbtnmap[32];
  BYTE npbutton;
  UINT npbtnmarks[2];
  UINT *npresponse;
  BYTE tpbutton;
  UINT tpbtnmarks[2];
  UINT *tpresponse;
  DWORD physid;
  UINT mode;
  UINT minpktdata;
  UINT minbuttons;
  UINT capabilities;

  size = WTInfo (WTI_CURSORS + index, CSR_NAME, NULL);
  name = g_malloc (size + 1);
  WTInfo (WTI_CURSORS + index, CSR_NAME, name);
  g_print ("NAME: %s\n", name);
  WTInfo (WTI_CURSORS + index, CSR_ACTIVE, &active);
  g_print ("ACTIVE: %s\n", active ? "YES" : "NO");
  WTInfo (WTI_CURSORS + index, CSR_PKTDATA, &wtpkt);
  g_print ("PKTDATA: %#x:", (guint) wtpkt);
#define BIT(x) if (wtpkt & PK_##x) g_print (" " #x)
  BIT (CONTEXT);
  BIT (STATUS);
  BIT (TIME);
  BIT (CHANGED);
  BIT (SERIAL_NUMBER);
  BIT (BUTTONS);
  BIT (X);
  BIT (Y);
  BIT (Z);
  BIT (NORMAL_PRESSURE);
  BIT (TANGENT_PRESSURE);
  BIT (ORIENTATION);
  BIT (ROTATION);
#undef BIT
  g_print ("\n");
  WTInfo (WTI_CURSORS + index, CSR_BUTTONS, &buttons);
  g_print ("BUTTONS: %d\n", buttons);
  WTInfo (WTI_CURSORS + index, CSR_BUTTONBITS, &buttonbits);
  g_print ("BUTTONBITS: %d\n", buttonbits);
  size = WTInfo (WTI_CURSORS + index, CSR_BTNNAMES, NULL);
  g_print ("BTNNAMES:");
  if (size > 0)
    {
      btnnames = g_malloc (size + 1);
      WTInfo (WTI_CURSORS + index, CSR_BTNNAMES, btnnames);
      p = btnnames;
      while (*p)
	{
	  g_print (" %s", p);
	  p += strlen (p) + 1;
	}
    }
  g_print ("\n");
  WTInfo (WTI_CURSORS + index, CSR_BUTTONMAP, buttonmap);
  g_print ("BUTTONMAP:");
  for (i = 0; i < buttons; i++)
    g_print (" %d", buttonmap[i]);
  g_print ("\n");
  WTInfo (WTI_CURSORS + index, CSR_SYSBTNMAP, sysbtnmap);
  g_print ("SYSBTNMAP:");
  for (i = 0; i < buttons; i++)
    g_print (" %d", sysbtnmap[i]);
  g_print ("\n");
  WTInfo (WTI_CURSORS + index, CSR_NPBUTTON, &npbutton);
  g_print ("NPBUTTON: %d\n", npbutton);
  WTInfo (WTI_CURSORS + index, CSR_NPBTNMARKS, npbtnmarks);
  g_print ("NPBTNMARKS: %d %d\n", npbtnmarks[0], npbtnmarks[1]);
  size = WTInfo (WTI_CURSORS + index, CSR_NPRESPONSE, NULL);
  g_print ("NPRESPONSE:");
  if (size > 0)
    {
      npresponse = g_malloc (size);
      WTInfo (WTI_CURSORS + index, CSR_NPRESPONSE, npresponse);
      for (i = 0; i < size / sizeof (UINT); i++)
	g_print (" %d", npresponse[i]);
    }
  g_print ("\n");
  WTInfo (WTI_CURSORS + index, CSR_TPBUTTON, &tpbutton);
  g_print ("TPBUTTON: %d\n", tpbutton);
  WTInfo (WTI_CURSORS + index, CSR_TPBTNMARKS, tpbtnmarks);
  g_print ("TPBTNMARKS: %d %d\n", tpbtnmarks[0], tpbtnmarks[1]);
  size = WTInfo (WTI_CURSORS + index, CSR_TPRESPONSE, NULL);
  g_print ("TPRESPONSE:");
  if (size > 0)
    {
      tpresponse = g_malloc (size);
      WTInfo (WTI_CURSORS + index, CSR_TPRESPONSE, tpresponse);
      for (i = 0; i < size / sizeof (UINT); i++)
	g_print (" %d", tpresponse[i]);
    }
  g_print ("\n");
  WTInfo (WTI_CURSORS + index, CSR_PHYSID, &physid);
  g_print ("PHYSID: %#x\n", (guint) physid);
  WTInfo (WTI_CURSORS + index, CSR_CAPABILITIES, &capabilities);
  g_print ("CAPABILITIES: %#x:", capabilities);
#define BIT(x) if (capabilities & CRC_##x) g_print (" " #x)
  BIT (MULTIMODE);
  BIT (AGGREGATE);
  BIT (INVERT);
#undef BIT
  g_print ("\n");
  if (capabilities & CRC_MULTIMODE)
    {
      WTInfo (WTI_CURSORS + index, CSR_MODE, &mode);
      g_print ("MODE: %d\n", mode);
    }
  if (capabilities & CRC_AGGREGATE)
    {
      WTInfo (WTI_CURSORS + index, CSR_MINPKTDATA, &minpktdata);
      g_print ("MINPKTDATA: %d\n", minpktdata);
      WTInfo (WTI_CURSORS + index, CSR_MINBUTTONS, &minbuttons);
      g_print ("MINBUTTONS: %d\n", minbuttons);
    }
}
#endif

void
_gdk_input_wintab_init_check (void)
{
  static gboolean wintab_initialized = FALSE;
  GdkDevicePrivate *gdkdev;
  GdkWindowAttr wa;
  WORD specversion;
  HCTX *hctx;
  UINT ndevices, ncursors, ncsrtypes, firstcsr, hardware;
  BOOL active;
  DWORD physid;
  AXIS axis_x, axis_y, axis_npressure, axis_or[3];
  int i, k;
  int devix, cursorix;
  char devname[100], csrname[100];
  BOOL defcontext_done;

  if (wintab_initialized)
    return;
  
  wintab_initialized = TRUE;
  
  wintab_contexts = NULL;

  if (!_gdk_input_ignore_wintab &&
      WTInfo (0, 0, NULL))
    {
      WTInfo (WTI_INTERFACE, IFC_SPECVERSION, &specversion);
      GDK_NOTE (INPUT, g_print ("Wintab interface version %d.%d\n",
			       HIBYTE (specversion), LOBYTE (specversion)));
      WTInfo (WTI_INTERFACE, IFC_NDEVICES, &ndevices);
      WTInfo (WTI_INTERFACE, IFC_NCURSORS, &ncursors);
#if DEBUG_WINTAB
      GDK_NOTE (INPUT, g_print ("NDEVICES: %d, NCURSORS: %d\n",
			       ndevices, ncursors));
#endif
      /* Create a dummy window to receive wintab events */
      wa.wclass = GDK_INPUT_OUTPUT;
      wa.event_mask = GDK_ALL_EVENTS_MASK;
      wa.width = 2;
      wa.height = 2;
      wa.x = -100;
      wa.y = -100;
      wa.window_type = GDK_WINDOW_TOPLEVEL;
      if ((wintab_window = gdk_window_new (NULL, &wa, GDK_WA_X|GDK_WA_Y)) == NULL)
	{
	  g_warning ("gdk_input_wintab_init: gdk_window_new failed");
	  return;
	}
      g_object_ref (wintab_window);
      
      for (devix = 0; devix < ndevices; devix++)
	{
	  LOGCONTEXT lc;
	  
	  /* We open the Wintab device (hmm, what if there are several?) as a
	   * system pointing device, i.e. it controls the normal Windows
	   * cursor. This seems much more natural.
	   */

	  WTInfo (WTI_DEVICES + devix, DVC_NAME, devname);
      
	  WTInfo (WTI_DEVICES + devix, DVC_NCSRTYPES, &ncsrtypes);
	  WTInfo (WTI_DEVICES + devix, DVC_FIRSTCSR, &firstcsr);
	  WTInfo (WTI_DEVICES + devix, DVC_HARDWARE, &hardware);
	  WTInfo (WTI_DEVICES + devix, DVC_X, &axis_x);
	  WTInfo (WTI_DEVICES + devix, DVC_Y, &axis_y);
	  WTInfo (WTI_DEVICES + devix, DVC_NPRESSURE, &axis_npressure);
	  WTInfo (WTI_DEVICES + devix, DVC_ORIENTATION, axis_or);

	  defcontext_done = FALSE;
	  if (HIBYTE (specversion) > 1 || LOBYTE (specversion) >= 1)
	    {
	      /* Try to get device-specific default context */
	      /* Some drivers, e.g. Aiptek, don't provide this info */
	      if (WTInfo (WTI_DSCTXS + devix, 0, &lc) > 0)
		defcontext_done = TRUE;
#if DEBUG_WINTAB
	      if (defcontext_done)
		GDK_NOTE (INPUT, (g_print("Using device-specific default context\n")));
	      else
		GDK_NOTE (INPUT, (g_print("Note: Driver did not provide device specific default context info despite claiming to support version 1.1\n")));
#endif
	    }

	  if (!defcontext_done)
	    WTInfo (WTI_DEFSYSCTX, 0, &lc);
#if DEBUG_WINTAB
	  GDK_NOTE (INPUT, (g_print("Default context:\n"), print_lc(&lc)));
#endif
	  lc.lcOptions |= CXO_MESSAGES;
	  lc.lcStatus = 0;
	  lc.lcMsgBase = WT_DEFBASE;
	  lc.lcPktRate = 50;
	  lc.lcPktData = PACKETDATA;
	  lc.lcPktMode = PACKETMODE;
	  lc.lcMoveMask = PACKETDATA;
	  lc.lcBtnUpMask = lc.lcBtnDnMask = ~0;
	  lc.lcOutOrgX = axis_x.axMin;
	  lc.lcOutOrgY = axis_y.axMin;
	  lc.lcOutExtX = axis_x.axMax - axis_x.axMin;
	  lc.lcOutExtY = axis_y.axMax - axis_y.axMin;
	  lc.lcOutExtY = -lc.lcOutExtY; /* We want Y growing downward */
#if DEBUG_WINTAB
	  GDK_NOTE (INPUT, (g_print("context for device %d:\n", devix),
			   print_lc(&lc)));
#endif
	  hctx = g_new (HCTX, 1);
          if ((*hctx = WTOpen (GDK_WINDOW_HWND (wintab_window), &lc, TRUE)) == NULL)
	    {
	      g_warning ("gdk_input_wintab_init: WTOpen failed");
	      return;
	    }
	  GDK_NOTE (INPUT, g_print ("opened Wintab device %d %p\n",
				   devix, *hctx));

	  wintab_contexts = g_list_append (wintab_contexts, hctx);
#if 0
	  WTEnable (*hctx, TRUE);
#endif
	  WTOverlap (*hctx, TRUE);

#if DEBUG_WINTAB
	  GDK_NOTE (INPUT, (g_print("context for device %d after WTOpen:\n", devix),
			   print_lc(&lc)));
#endif
	  /* Increase packet queue size to reduce the risk of lost packets */
	  /* According to the specs, if the function fails we must try again */
	  /* with a smaller queue size */
	  GDK_NOTE (INPUT, g_print("Attempting to increase queue size\n"));
	  for (i = 32; i >= 1; i >>= 1)
	    {
	      if (WTQueueSizeSet(*hctx, i))
		{
		  GDK_NOTE (INPUT, g_print("Queue size set to %d\n", i));
		  break;
		}
	    }
	  if (!i)
	    GDK_NOTE (INPUT, g_print("Whoops, no queue size could be set\n"));
	  for (cursorix = firstcsr; cursorix < firstcsr + ncsrtypes; cursorix++)
	    {
#ifdef DEBUG_WINTAB
	      GDK_NOTE (INPUT, (g_print("Cursor %d:\n", cursorix), print_cursor (cursorix)));
#endif
	      active = FALSE;
	      WTInfo (WTI_CURSORS + cursorix, CSR_ACTIVE, &active);
	      if (!active)
		continue;

	      /* Wacom tablets seem to report cursors corresponding to
	       * nonexistent pens or pucks. At least my ArtPad II
	       * reports six cursors: a puck, pressure stylus and
	       * eraser stylus, and then the same three again. I only
	       * have a pressure-sensitive pen. The puck instances,
	       * and the second instances of the styluses report
	       * physid zero. So at least for Wacom, skip cursors with
	       * physid zero.
	       */
	      WTInfo (WTI_CURSORS + cursorix, CSR_PHYSID, &physid);
	      if (strcmp (devname, "WACOM Tablet") == 0 && physid == 0)
		continue;

	      gdkdev = g_object_new (GDK_TYPE_DEVICE, NULL);

	      WTInfo (WTI_CURSORS + cursorix, CSR_NAME, csrname);
	      gdkdev->info.name = g_strconcat (devname, " ", csrname, NULL);
	      gdkdev->info.source = GDK_SOURCE_PEN;
	      gdkdev->info.mode = GDK_MODE_SCREEN;
	      gdkdev->info.has_cursor = TRUE;
	      gdkdev->hctx = *hctx;
	      gdkdev->cursor = cursorix;
	      WTInfo (WTI_CURSORS + cursorix, CSR_PKTDATA, &gdkdev->pktdata);
	      gdkdev->info.num_axes = 0;
	      if (gdkdev->pktdata & PK_X)
		gdkdev->info.num_axes++;
	      if (gdkdev->pktdata & PK_Y)
		gdkdev->info.num_axes++;
	      if (gdkdev->pktdata & PK_NORMAL_PRESSURE)
		gdkdev->info.num_axes++;
	      /* The wintab driver for the Wacom ArtPad II reports
	       * PK_ORIENTATION in CSR_PKTDATA, but the tablet doesn't
	       * actually sense tilt. Catch this by noticing that the
	       * orientation axis's azimuth resolution is zero.
	       */
	      if ((gdkdev->pktdata & PK_ORIENTATION)
		  && axis_or[0].axResolution == 0)
		gdkdev->pktdata &= ~PK_ORIENTATION;

	      if (gdkdev->pktdata & PK_ORIENTATION)
		gdkdev->info.num_axes += 2; /* x and y tilt */
	      WTInfo (WTI_CURSORS + cursorix, CSR_NPBTNMARKS, &gdkdev->npbtnmarks);
	      gdkdev->info.axes = g_new (GdkDeviceAxis, gdkdev->info.num_axes);
	      gdkdev->axes = g_new (GdkAxisInfo, gdkdev->info.num_axes);
	      gdkdev->last_axis_data = g_new (gint, gdkdev->info.num_axes);
	      
	      k = 0;
	      if (gdkdev->pktdata & PK_X)
		{
		  gdkdev->axes[k].xresolution =
		    gdkdev->axes[k].resolution = axis_x.axResolution / 65535.;
		  gdkdev->axes[k].xmin_value =
		    gdkdev->axes[k].min_value = axis_x.axMin;
		  gdkdev->axes[k].xmax_value =
		    gdkdev->axes[k].max_value = axis_x.axMax;
		  gdkdev->info.axes[k].use = GDK_AXIS_X;
		  gdkdev->info.axes[k].min = axis_x.axMin;
		  gdkdev->info.axes[k].max = axis_x.axMax;
		  k++;
		}
	      if (gdkdev->pktdata & PK_Y)
		{
		  gdkdev->axes[k].xresolution =
		    gdkdev->axes[k].resolution = axis_y.axResolution / 65535.;
		  gdkdev->axes[k].xmin_value =
		    gdkdev->axes[k].min_value = axis_y.axMin;
		  gdkdev->axes[k].xmax_value =
		    gdkdev->axes[k].max_value = axis_y.axMax;
		  gdkdev->info.axes[k].use = GDK_AXIS_Y;
		  gdkdev->info.axes[k].min = axis_y.axMin;
		  gdkdev->info.axes[k].max = axis_y.axMax;
		  k++;
		}
	      if (gdkdev->pktdata & PK_NORMAL_PRESSURE)
		{
		  gdkdev->axes[k].xresolution =
		    gdkdev->axes[k].resolution = axis_npressure.axResolution / 65535.;
		  gdkdev->axes[k].xmin_value =
		    gdkdev->axes[k].min_value = axis_npressure.axMin;
		  gdkdev->axes[k].xmax_value =
		    gdkdev->axes[k].max_value = axis_npressure.axMax;
		  gdkdev->info.axes[k].use = GDK_AXIS_PRESSURE;
		  /* GIMP seems to expect values in the range 0-1 */
		  gdkdev->info.axes[k].min = 0.0; /*axis_npressure.axMin;*/
		  gdkdev->info.axes[k].max = 1.0; /*axis_npressure.axMax;*/
		  k++;
		}
	      if (gdkdev->pktdata & PK_ORIENTATION)
		{
		  GdkAxisUse axis;

		  gdkdev->orientation_axes[0] = axis_or[0];
		  gdkdev->orientation_axes[1] = axis_or[1];
		  for (axis = GDK_AXIS_XTILT; axis <= GDK_AXIS_YTILT; axis++)
		    {
		      /* Wintab gives us aximuth and altitude, which
		       * we convert to x and y tilt in the -1000..1000 range
		       */
		      gdkdev->axes[k].xresolution =
			gdkdev->axes[k].resolution = 1000;
		      gdkdev->axes[k].xmin_value =
			gdkdev->axes[k].min_value = -1000;
		      gdkdev->axes[k].xmax_value =
			gdkdev->axes[k].max_value = 1000;
		      gdkdev->info.axes[k].use = axis;
		      gdkdev->info.axes[k].min = -1000;
		      gdkdev->info.axes[k].max = 1000;
		      k++;
		    }
		}
	      gdkdev->info.num_keys = 0;
	      gdkdev->info.keys = NULL;
	      GDK_NOTE (INPUT,
			g_print ("device: (%d) %s axes: %d\n",
				 cursorix,
				 gdkdev->info.name,
				 gdkdev->info.num_axes));
	      for (i = 0; i < gdkdev->info.num_axes; i++)
		GDK_NOTE (INPUT,
			  g_print ("... axis %d: %d--%d@%d (%d--%d@%d)\n",
				   i,
				   gdkdev->axes[i].xmin_value, 
				   gdkdev->axes[i].xmax_value, 
				   gdkdev->axes[i].xresolution, 
				   gdkdev->axes[i].min_value, 
				   gdkdev->axes[i].max_value, 
				   gdkdev->axes[i].resolution));
	      _gdk_input_devices = g_list_append (_gdk_input_devices,
						 gdkdev);
	    }
	}
    }
}

static void
decode_tilt (gint   *axis_data,
	     AXIS   *axes,
	     PACKET *packet)
{
  /* As I don't have a tilt-sensing tablet,
   * I cannot test this code.
   */
  
  double az, el;

  az = TWOPI * packet->pkOrientation.orAzimuth /
    (axes[0].axResolution / 65536.);
  el = TWOPI * packet->pkOrientation.orAltitude /
    (axes[1].axResolution / 65536.);
  
  /* X tilt */
  axis_data[0] = cos (az) * cos (el) * 1000;
  /* Y tilt */
  axis_data[1] = sin (az) * cos (el) * 1000;
}

#endif /* HAVE_WINTAB */

static void
gdk_input_translate_coordinates (GdkDevicePrivate *gdkdev,
				 GdkInputWindow   *input_window,
				 gint             *axis_data,
				 gdouble          *axis_out,
				 gdouble          *x_out,
				 gdouble          *y_out)
{
  GdkWindowImplWin32 *impl, *root_impl;

  int i;
  int x_axis = 0;
  int y_axis = 0;

  double device_width, device_height;
  double x_offset, y_offset, x_scale, y_scale;

  impl = GDK_WINDOW_IMPL_WIN32 (GDK_WINDOW_OBJECT (input_window->window)->impl);

  for (i=0; i<gdkdev->info.num_axes; i++)
    {
      switch (gdkdev->info.axes[i].use)
	{
	case GDK_AXIS_X:
	  x_axis = i;
	  break;
	case GDK_AXIS_Y:
	  y_axis = i;
	  break;
	default:
	  break;
	}
    }
  
  device_width = gdkdev->axes[x_axis].max_value - 
		   gdkdev->axes[x_axis].min_value;
  device_height = gdkdev->axes[y_axis].max_value - 
                    gdkdev->axes[y_axis].min_value;

  if (gdkdev->info.mode == GDK_MODE_SCREEN) 
    {
      root_impl = GDK_WINDOW_IMPL_WIN32 (GDK_WINDOW_OBJECT (_gdk_root)->impl);
      x_scale = root_impl->width / device_width;
      y_scale = root_impl->height / device_height;

      x_offset = - input_window->root_x;
      y_offset = - input_window->root_y;
    }
  else				/* GDK_MODE_WINDOW */
    {
      double device_aspect = (device_height*gdkdev->axes[y_axis].resolution) /
	(device_width*gdkdev->axes[x_axis].resolution);

      if (device_aspect * impl->width >= impl->height)
	{
	  /* device taller than window */
	  x_scale = impl->width / device_width;
	  y_scale = (x_scale * gdkdev->axes[x_axis].resolution)
	    / gdkdev->axes[y_axis].resolution;

	  x_offset = 0;
	  y_offset = -(device_height * y_scale - 
			       impl->height)/2;
	}
      else
	{
	  /* window taller than device */
	  y_scale = impl->height / device_height;
	  x_scale = (y_scale * gdkdev->axes[y_axis].resolution)
	    / gdkdev->axes[x_axis].resolution;

	  y_offset = 0;
	  x_offset = - (device_width * x_scale - impl->width)/2;
	}
    }

  for (i=0; i<gdkdev->info.num_axes; i++)
    {
      switch (gdkdev->info.axes[i].use)
	{
	case GDK_AXIS_X:
	  axis_out[i] = x_offset + x_scale*axis_data[x_axis];
	  if (x_out)
	    *x_out = axis_out[i];
	  break;
	case GDK_AXIS_Y:
	  axis_out[i] = y_offset + y_scale*axis_data[y_axis];
	  if (y_out)
	    *y_out = axis_out[i];
	  break;
	default:
	  axis_out[i] =
	    (gdkdev->info.axes[i].max * (axis_data[i] - gdkdev->axes[i].min_value) +
	     gdkdev->info.axes[i].min * (gdkdev->axes[i].max_value - axis_data[i])) /
	    (gdkdev->axes[i].max_value - gdkdev->axes[i].min_value);
	  break;
	}
    }
}

static void
gdk_input_get_root_relative_geometry (HWND w,
				      int  *x_ret,
				      int  *y_ret)
{
  RECT rect;

  GetWindowRect (w, &rect);

  if (x_ret)
    *x_ret = rect.left + _gdk_offset_x;
  if (y_ret)
    *y_ret = rect.top + _gdk_offset_y;
}

void
_gdk_input_configure_event (GdkWindow         *window)
{
  GdkInputWindow *input_window;
  int root_x, root_y;

  input_window = _gdk_input_window_find (window);
  g_return_if_fail (window != NULL);

  gdk_input_get_root_relative_geometry (GDK_WINDOW_HWND (window),
					&root_x, &root_y);

  input_window->root_x = root_x;
  input_window->root_y = root_y;
}

void 
_gdk_input_enter_event (GdkWindow        *window)
{
  GdkInputWindow *input_window;
  int root_x, root_y;

  input_window = _gdk_input_window_find (window);
  g_return_if_fail (window != NULL);

  gdk_input_get_root_relative_geometry (GDK_WINDOW_HWND (window), &root_x, &root_y);

  input_window->root_x = root_x;
  input_window->root_y = root_y;
}

/*
 * Get the currently active keyboard modifiers (ignoring the mouse buttons)
 * We could use gdk_window_get_pointer but that function does a lot of other
 * expensive things besides getting the modifiers. This code is somewhat based
 * on build_pointer_event_state from gdkevents-win32.c
 */
static guint
get_modifier_key_state (void)
{
  guint state;
  
  state = 0;
  /* High-order bit is up/down, low order bit is toggled/untoggled */
  if (GetKeyState (VK_CONTROL) < 0)
    state |= GDK_CONTROL_MASK;
  if (GetKeyState (VK_SHIFT) < 0)
    state |= GDK_SHIFT_MASK;
  if (GetKeyState (VK_MENU) < 0)
    state |= GDK_MOD1_MASK;
  if (GetKeyState (VK_CAPITAL) & 0x1)
    state |= GDK_LOCK_MASK;

  return state;
}

#ifdef HAVE_WINTAB

static guint ignore_core_timer = 0;

static gboolean
ignore_core_timefunc (gpointer data)
{
  /* The delay has passed */
  _gdk_input_ignore_core = FALSE;
  ignore_core_timer = 0;

  return FALSE; /* remove timeout */
}

/*
 * Set or unset the _gdk_input_ignore_core variable that tells GDK
 * to ignore events for the core pointer when the tablet is in proximity
 * The unsetting is delayed slightly so that if a tablet event arrives
 * just after proximity out, it does not cause a core pointer event
 * which e.g. causes GIMP to switch tools.
 */
static void
set_ignore_core (gboolean ignore)
{
  if (ignore)
    {
      _gdk_input_ignore_core = TRUE;
      /* Remove any pending clear */
      if (ignore_core_timer)
        {
	  g_source_remove (ignore_core_timer);
	  ignore_core_timer = 0;
	}
    }
  else
    if (!ignore_core_timer)
      ignore_core_timer = g_timeout_add (PROXIMITY_OUT_DELAY,
					 ignore_core_timefunc, NULL);
}
#endif /* HAVE_WINTAB */

gboolean 
_gdk_input_other_event (GdkEvent  *event,
			MSG       *msg,
			GdkWindow *window)
{
#ifdef HAVE_WINTAB
  GdkDisplay *display;
  GdkWindowObject *obj, *grab_obj;
  GdkInputWindow *input_window;
  GdkDevicePrivate *gdkdev = NULL;
  GdkEventMask masktest;
  guint key_state;
  POINT pt;

  PACKET packet;
  gint k;
  gint x, y;
  guint translated_buttons, button_diff, button_mask;
  /* Translation from tablet button state to GDK button state for
   * buttons 1-3 - swap button 2 and 3.
   */
  static guint button_map[8] = {0, 1, 4, 5, 2, 3, 6, 7};

  if (event->any.window != wintab_window)
    {
      g_warning ("_gdk_input_other_event: not wintab_window?");
      return FALSE;
    }

  window = gdk_window_at_pointer (&x, &y);
  if (window == NULL)
    window = _gdk_root;

  g_object_ref (window);
  display = gdk_drawable_get_display (window);

  GDK_NOTE (EVENTS_OR_INPUT,
	    g_print ("gdk_input_win32_other_event: window=%p %+d%+d\n",
		     GDK_WINDOW_HWND (window), x, y));
  
  if (msg->message == WT_PACKET)
    {
      if (!WTPacket ((HCTX) msg->lParam, msg->wParam, &packet))
	return FALSE;
    }

  obj = GDK_WINDOW_OBJECT (window);

  switch (msg->message)
    {
    case WT_PACKET:
      /* Don't produce any button or motion events while a window is being
       * moved or resized, see bug #151090. */
      if (_sizemove_in_progress)
	{
	  GDK_NOTE (EVENTS_OR_INPUT, g_print ("... ignored when moving/sizing\n"));
	  return FALSE;
	}
      if (window == _gdk_root && x_grab_window == NULL)
	{
	  GDK_NOTE (EVENTS_OR_INPUT, g_print ("... is root\n"));
	  return FALSE;
	}

      if ((gdkdev = gdk_input_find_dev_from_ctx ((HCTX) msg->lParam,
						 packet.pkCursor)) == NULL)
	return FALSE;

      if (gdkdev->info.mode == GDK_MODE_DISABLED)
	return FALSE;
      
      k = 0;
      if (gdkdev->pktdata & PK_X)
	gdkdev->last_axis_data[k++] = packet.pkX;
      if (gdkdev->pktdata & PK_Y)
	gdkdev->last_axis_data[k++] = packet.pkY;
      if (gdkdev->pktdata & PK_NORMAL_PRESSURE)
	gdkdev->last_axis_data[k++] = packet.pkNormalPressure;
      if (gdkdev->pktdata & PK_ORIENTATION)
	{
	  decode_tilt (gdkdev->last_axis_data + k,
		       gdkdev->orientation_axes, &packet);
	  k += 2;
	}

      g_assert (k == gdkdev->info.num_axes);

      translated_buttons = button_map[packet.pkButtons & 0x07] | (packet.pkButtons & ~0x07);

      if (translated_buttons != gdkdev->button_state)
	{
	  /* At least one button has changed state so produce a button event
	   * If more than one button has changed state (unlikely),
	   * just care about the first and act on the next the next time
	   * we get a packet
	   */
	  button_diff = translated_buttons ^ gdkdev->button_state;
	  
	  /* Gdk buttons are numbered 1.. */
	  event->button.button = 1;

	  for (button_mask = 1; button_mask != 0x80000000;
	       button_mask <<= 1, event->button.button++)
	    {
	      if (button_diff & button_mask)
	        {
		  /* Found a button that has changed state */
		  break;
		}
	    }

	  if (!(translated_buttons & button_mask))
	    {
	      event->any.type = GDK_BUTTON_RELEASE;
	      masktest = GDK_BUTTON_RELEASE_MASK;
	    }
	  else
	    {
	      event->any.type = GDK_BUTTON_PRESS;
	      masktest = GDK_BUTTON_PRESS_MASK;
	    }
	  gdkdev->button_state ^= button_mask;
	}
      else
	{
	  event->any.type = GDK_MOTION_NOTIFY;
	  masktest = GDK_POINTER_MOTION_MASK;
	  if (gdkdev->button_state & (1 << 0))
	    masktest |= GDK_BUTTON_MOTION_MASK | GDK_BUTTON1_MOTION_MASK;
	  if (gdkdev->button_state & (1 << 1))
	    masktest |= GDK_BUTTON_MOTION_MASK | GDK_BUTTON2_MOTION_MASK;
	  if (gdkdev->button_state & (1 << 2))
	    masktest |= GDK_BUTTON_MOTION_MASK | GDK_BUTTON3_MOTION_MASK;
	}

      /* See if input is grabbed */
      /* FIXME: x_grab_owner_events should probably be handled somehow */
      if (x_grab_window != NULL)
	{
	  grab_obj = GDK_WINDOW_OBJECT (x_grab_window);
	  if (!GDK_WINDOW_IMPL_WIN32 (grab_obj->impl)->extension_events_selected
	      || !(grab_obj->extension_events & masktest)
	      || !(x_grab_mask && masktest))
	    {
	      GDK_NOTE (EVENTS_OR_INPUT, 
			g_print ("... grabber doesn't want it\n"));
	      return FALSE;
	    }
	  GDK_NOTE (EVENTS_OR_INPUT, g_print ("... to grabber\n"));

	  g_object_ref(x_grab_window);
	  g_object_unref(window);
	  window = x_grab_window;
	  obj = grab_obj;
	}
      /* Now we can check if the window wants the event, and
       * propagate if necessary.
       */
    dijkstra:
      if (!GDK_WINDOW_IMPL_WIN32 (obj->impl)->extension_events_selected
	  || !(obj->extension_events & masktest))
	{
	  GDK_NOTE (EVENTS_OR_INPUT, g_print ("... not selected\n"));

	  if (obj->parent == GDK_WINDOW_OBJECT (_gdk_root))
	    return FALSE;

	  /* It is not good to propagate the extended events up to the parent
	   * if this window wants normal (not extended) motion/button events */
	  if (obj->event_mask & masktest)
	    {
	      GDK_NOTE (EVENTS_OR_INPUT, 
			g_print ("... wants ordinary event, ignoring this\n"));
	      return FALSE;
	    }
	  
	  pt.x = x;
	  pt.y = y;
	  ClientToScreen (GDK_WINDOW_HWND (window), &pt);
	  g_object_unref (window);
	  window = (GdkWindow *) obj->parent;
	  obj = GDK_WINDOW_OBJECT (window);
	  g_object_ref (window);
	  ScreenToClient (GDK_WINDOW_HWND (window), &pt);
	  x = pt.x;
	  y = pt.y;
	  GDK_NOTE (EVENTS_OR_INPUT, g_print ("... propagating to %p %+d%+d\n",
					      GDK_WINDOW_HWND (window), x, y));
	  goto dijkstra;
	}

      input_window = _gdk_input_window_find (window);

      g_assert (input_window != NULL);

      if (gdkdev->info.mode == GDK_MODE_WINDOW
	  && input_window->mode == GDK_EXTENSION_EVENTS_CURSOR)
	return FALSE;

      event->any.window = window;
      key_state = get_modifier_key_state ();
      if (event->any.type == GDK_BUTTON_PRESS
	  || event->any.type == GDK_BUTTON_RELEASE)
	{
	  event->button.time = _gdk_win32_get_next_tick (msg->time);
	  event->button.device = &gdkdev->info;
	  
	  event->button.axes = g_new(gdouble, gdkdev->info.num_axes);

	  gdk_input_translate_coordinates (gdkdev, input_window,
					   gdkdev->last_axis_data,
					   event->button.axes,
					   &event->button.x, 
					   &event->button.y);

	  /* Also calculate root coordinates. Note that input_window->root_x
	     is in GDK root coordinates. */
	  event->button.x_root = event->button.x + input_window->root_x;
	  event->button.y_root = event->button.y + input_window->root_y;

	  event->button.state = ((gdkdev->button_state << 8)
				 & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK
				    | GDK_BUTTON3_MASK | GDK_BUTTON4_MASK
				    | GDK_BUTTON5_MASK))
				| key_state;
	  GDK_NOTE (EVENTS_OR_INPUT,
		    g_print ("WINTAB button %s:%d %g,%g\n",
			     (event->button.type == GDK_BUTTON_PRESS ?
			      "press" : "release"),
			     event->button.button,
			     event->button.x, event->button.y));
	}
      else
	{
	  event->motion.time = _gdk_win32_get_next_tick (msg->time);
	  event->motion.is_hint = FALSE;
	  event->motion.device = &gdkdev->info;

	  event->motion.axes = g_new(gdouble, gdkdev->info.num_axes);

	  gdk_input_translate_coordinates (gdkdev, input_window,
					   gdkdev->last_axis_data,
					   event->motion.axes,
					   &event->motion.x, 
					   &event->motion.y);

	  /* Also calculate root coordinates. Note that input_window->root_x
	     is in GDK root coordinates. */
	  event->motion.x_root = event->motion.x + input_window->root_x;
	  event->motion.y_root = event->motion.y + input_window->root_y;

	  event->motion.state = ((gdkdev->button_state << 8)
				 & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK
				    | GDK_BUTTON3_MASK | GDK_BUTTON4_MASK
				    | GDK_BUTTON5_MASK))
				| key_state;

	  GDK_NOTE (EVENTS_OR_INPUT,
		    g_print ("WINTAB motion: %g,%g\n",
			     event->motion.x, event->motion.y));
	}
      return TRUE;

    case WT_PROXIMITY:
      if (LOWORD (msg->lParam) == 0)
	{
	  event->proximity.type = GDK_PROXIMITY_OUT;
	  set_ignore_core (FALSE);
	}
      else
	{
	  event->proximity.type = GDK_PROXIMITY_IN;
	  set_ignore_core (TRUE);
	}
      event->proximity.time = _gdk_win32_get_next_tick (msg->time);
      event->proximity.device = &gdkdev->info;

      GDK_NOTE (EVENTS_OR_INPUT,
		g_print ("WINTAB proximity %s\n",
			 (event->proximity.type == GDK_PROXIMITY_IN ?
			  "in" : "out")));
      return TRUE;
    }
#endif
  return FALSE;
}

gboolean
_gdk_input_enable_window (GdkWindow        *window,
			  GdkDevicePrivate *gdkdev)
{
#ifdef HAVE_SOME_XINPUT
  GdkWindowImplWin32 *impl = GDK_WINDOW_IMPL_WIN32 (GDK_WINDOW_OBJECT (window)->impl);

  impl->extension_events_selected = TRUE;
#endif

  return TRUE;
}

gboolean
_gdk_input_disable_window (GdkWindow        *window,
			   GdkDevicePrivate *gdkdev)
{
#ifdef HAVE_SOME_XINPUT
  GdkWindowImplWin32 *impl = GDK_WINDOW_IMPL_WIN32 (GDK_WINDOW_OBJECT (window)->impl);

  impl->extension_events_selected = FALSE;
#endif

  return TRUE;
}

gint
_gdk_input_grab_pointer (GdkWindow    *window,
			 gint          owner_events,
			 GdkEventMask  event_mask,
			 GdkWindow    *confine_to,
			 guint32       time)
{
#ifdef HAVE_SOME_XINPUT
  GdkInputWindow *input_window, *new_window;
  gboolean need_ungrab;
  GdkDevicePrivate *gdkdev;
  GList *tmp_list;

  tmp_list = _gdk_input_windows;
  new_window = NULL;
  need_ungrab = FALSE;

  GDK_NOTE (INPUT, g_print ("gdk_input_win32_grab_pointer: %p %d %p\n",
			   GDK_WINDOW_HWND (window),
			   owner_events,
			   (confine_to ? GDK_WINDOW_HWND (confine_to) : 0)));

  while (tmp_list)
    {
      input_window = (GdkInputWindow *)tmp_list->data;

      if (input_window->window == window)
	new_window = input_window;
      else if (input_window->grabbed)
	{
	  input_window->grabbed = FALSE;
	  need_ungrab = TRUE;
	}

      tmp_list = tmp_list->next;
    }

  if (new_window)
    {
      new_window->grabbed = TRUE;
      x_grab_window = window;
      x_grab_mask = event_mask;
      x_grab_owner_events = owner_events;

      /* FIXME: Do we need to handle confine_to and time? */
      
      tmp_list = _gdk_input_devices;
      while (tmp_list)
	{
	  gdkdev = (GdkDevicePrivate *)tmp_list->data;
	  if (!GDK_IS_CORE (gdkdev) && gdkdev->hctx)
	    {
#if 0	      
	      /* XXX */
	      gdk_input_common_find_events (window, gdkdev,
					    event_mask,
					    event_classes, &num_classes);
	      
	      result = XGrabDevice( GDK_DISPLAY(), gdkdev->xdevice,
				    GDK_WINDOW_XWINDOW (window),
				    owner_events, num_classes, event_classes,
				    GrabModeAsync, GrabModeAsync, time);
	      
	      /* FIXME: if failure occurs on something other than the first
		 device, things will be badly inconsistent */
	      if (result != Success)
		return result;
#endif
	    }
	  tmp_list = tmp_list->next;
	}
    }
  else
    { 
      x_grab_window = NULL;
#if 0
      tmp_list = _gdk_input_devices;
      while (tmp_list)
	{
	  gdkdev = (GdkDevicePrivate *)tmp_list->data;
	  if (!GDK_IS_CORE (gdkdev) && gdkdev->hctx &&
	      ((gdkdev->button_state != 0) || need_ungrab))
	    {
#if 0
	      /* XXX */
	      XUngrabDevice (gdk_display, gdkdev->xdevice, time);
#endif
	      gdkdev->button_state = 0;
	    }
	  
	  tmp_list = tmp_list->next;
	}
#endif
    }
#endif

  return GDK_GRAB_SUCCESS;
}

void 
_gdk_input_ungrab_pointer (guint32 time)
{
#ifdef HAVE_SOME_XINPUT
  GdkInputWindow *input_window;
  GdkDevicePrivate *gdkdev;
  GList *tmp_list;

  GDK_NOTE (INPUT, g_print ("gdk_input_win32_ungrab_pointer\n"));

  tmp_list = _gdk_input_windows;
  while (tmp_list)
    {
      input_window = (GdkInputWindow *)tmp_list->data;
      if (input_window->grabbed)
	break;
      tmp_list = tmp_list->next;
    }

  if (tmp_list)			/* we found a grabbed window */
    {
      input_window->grabbed = FALSE;

      tmp_list = _gdk_input_devices;
      while (tmp_list)
	{
	  gdkdev = (GdkDevicePrivate *)tmp_list->data;
#if 0
	  /* XXX */
	  if (!GDK_IS_CORE (gdkdev) && gdkdev->xdevice)
	    XUngrabDevice (gdk_display, gdkdev->xdevice, time);
#endif
	  tmp_list = tmp_list->next;
	}
    }
  x_grab_window = NULL;
#endif
}

gboolean
_gdk_device_get_history (GdkDevice         *device,
			 GdkWindow         *window,
			 guint32            start,
			 guint32            stop,
			 GdkTimeCoord    ***events,
			 gint              *n_events)
{
  return FALSE;
}

void 
gdk_device_get_state (GdkDevice       *device,
		      GdkWindow       *window,
		      gdouble         *axes,
		      GdkModifierType *mask)
{
  g_return_if_fail (device != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_IS_CORE (device))
    {
      gint x_int, y_int;
      
      gdk_window_get_pointer (window, &x_int, &y_int, mask);

      if (axes)
	{
	  axes[0] = x_int;
	  axes[1] = y_int;
	}
    }
  else
    {
      GdkDevicePrivate *gdkdev;
      GdkInputWindow *input_window;
      
      gdkdev = (GdkDevicePrivate *)device;
      /* For now just use the last known button and axis state of the device.
       * Since graphical tablets send an insane amount of motion events each
       * second, the info should be fairly up to date */
      if (mask)
	{
	  gdk_window_get_pointer (window, NULL, NULL, mask);
	  *mask &= 0xFF; /* Mask away core pointer buttons */
	  *mask |= ((gdkdev->button_state << 8)
		    & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK
		       | GDK_BUTTON3_MASK | GDK_BUTTON4_MASK
		       | GDK_BUTTON5_MASK));
	}
      input_window = _gdk_input_window_find (window);
      g_return_if_fail (input_window != NULL);
      /* For some reason, input_window is sometimes NULL when I use The GIMP 2
       * (bug #141543?). Avoid crashing if debugging is disabled. */
      if (axes && gdkdev->last_axis_data && input_window)
	gdk_input_translate_coordinates (gdkdev, input_window,
					 gdkdev->last_axis_data,
					 axes, NULL, NULL);
    }
}

#ifdef HAVE_WINTAB
void
_gdk_input_set_tablet_active (void)
{
  GList *tmp_list;
  HCTX *hctx;

  /* Bring the contexts to the top of the overlap order when one of the
   * application's windows is activated */
  
  if (!wintab_contexts)
    return; /* No tablet devices found, or Wintab not initialized yet */
  
  GDK_NOTE (INPUT, g_print ("_gdk_input_set_tablet_active: "
	"Bringing Wintab contexts to the top of the overlap order\n"));

  tmp_list = wintab_contexts;
  while (tmp_list)
    {
      hctx = (HCTX *) (tmp_list->data);
      WTOverlap (*hctx, TRUE);
      tmp_list = tmp_list->next;
    }
}
#endif /* HAVE_WINTAB */

void 
_gdk_input_init (GdkDisplay *display)
{
  _gdk_input_ignore_core = FALSE;
  _gdk_input_devices = NULL;

  _gdk_init_input_core (display);
#ifdef HAVE_WINTAB
#ifdef WINTAB_NO_LAZY_INIT
  /* Normally, Wintab is only initialized when the application performs
   * an action that requires it, such as enabling extended input events
   * for a window or enumerating the devices.
   */
  _gdk_input_wintab_init_check ();
#endif /* WINTAB_NO_LAZY_INIT */
#endif /* HAVE_WINTAB */

  _gdk_input_devices = g_list_append (_gdk_input_devices, display->core_pointer);
}

