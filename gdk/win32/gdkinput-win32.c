/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1999 Tor Lillqvist
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

#include "config.h"

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
#define PACKETMODE (PK_BUTTONS)
#include <pktdef.h>

/* If USE_SYSCONTEXT is on, we open the Wintab device (hmm, what if
 * there are several?) as a system pointing device, i.e. it controls
 * the normal Windows cursor. This seems much more natural.
 */
#define USE_SYSCONTEXT 1	/* The code for the other choice is not
				 * good at all.
				 */

#define DEBUG_WINTAB 1		/* Verbose debug messages enabled */

#endif

#if defined(HAVE_WINTAB) || defined(HAVE_WHATEVER_OTHER)
#define HAVE_SOME_XINPUT
#endif

#define TWOPI (2.*G_PI)

/* Forward declarations */

#if !USE_SYSCONTEXT
static GdkInputWindow *gdk_input_window_find_within (GdkWindow *window);
#endif

#ifdef HAVE_WINTAB

static GdkDevicePrivate *gdk_input_find_dev_from_ctx (HCTX hctx,
						      UINT id);
static GList     *wintab_contexts;

static GdkWindow *wintab_window;

#endif /* HAVE_WINTAB */

gboolean
gdk_device_get_history  (GdkDevice         *device,
			 GdkWindow         *window,
			 guint32            start,
			 guint32            stop,
			 GdkTimeCoord    ***events,
			 gint              *n_events)
{
  g_return_val_if_fail (window != NULL, FALSE);
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (events != NULL, FALSE);
  g_return_val_if_fail (n_events != NULL, FALSE);

  *n_events = 0;
  *events = NULL;

  if (GDK_WINDOW_DESTROYED (window))
    return FALSE;
    
  if (GDK_IS_CORE (device))
    return FALSE;
  else
    return _gdk_device_get_history (device, window, start, stop, events, n_events);
}

#ifdef HAVE_WINTAB

static GdkDevicePrivate *
gdk_input_find_dev_from_ctx (HCTX hctx,
			     UINT cursor)
{
  GList *tmp_list = gdk_input_devices;
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
	  lc->lcBtnDnMask, lc->lcBtnUpMask);
  g_print ("lcInOrgX = %d, lcInOrgY = %d, lcInOrgZ = %d\n",
	  lc->lcInOrgX, lc->lcInOrgY, lc->lcInOrgZ);
  g_print ("lcInExtX = %d, lcInExtY = %d, lcInExtZ = %d\n",
	  lc->lcInExtX, lc->lcInExtY, lc->lcInExtZ);
  g_print ("lcOutOrgX = %d, lcOutOrgY = %d, lcOutOrgZ = %d\n",
	  lc->lcOutOrgX, lc->lcOutOrgY, lc->lcOutOrgZ);
  g_print ("lcOutExtX = %d, lcOutExtY = %d, lcOutExtZ = %d\n",
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

#endif

static void
gdk_input_wintab_init (void)
{
  GdkDevicePrivate *gdkdev;
  GdkWindowAttr wa;
  WORD specversion;
  LOGCONTEXT defcontext;
  HCTX *hctx;
  UINT ndevices, ncursors, ncsrtypes, firstcsr, hardware;
  BOOL active;
  AXIS axis_x, axis_y, axis_npressure, axis_or[3];
  int i, j, k;
  int devix, cursorix;
  char devname[100], csrname[100];

  gdk_input_devices = NULL;
  wintab_contexts = NULL;

  if (!gdk_input_ignore_wintab &&
      WTInfo (0, 0, NULL))
    {
      WTInfo (WTI_INTERFACE, IFC_SPECVERSION, &specversion);
      GDK_NOTE (MISC, g_print ("Wintab interface version %d.%d\n",
			       HIBYTE (specversion), LOBYTE (specversion)));
#if USE_SYSCONTEXT
      WTInfo (WTI_DEFSYSCTX, 0, &defcontext);
#if DEBUG_WINTAB
      GDK_NOTE (MISC, (g_print("DEFSYSCTX:\n"), print_lc(&defcontext)));
#endif
#else
      WTInfo (WTI_DEFCONTEXT, 0, &defcontext);
#if DEBUG_WINTAB
      GDK_NOTE (MISC, (g_print("DEFCONTEXT:\n"), print_lc(&defcontext)));
#endif
#endif
      WTInfo (WTI_INTERFACE, IFC_NDEVICES, &ndevices);
      WTInfo (WTI_INTERFACE, IFC_NCURSORS, &ncursors);
#if DEBUG_WINTAB
      GDK_NOTE (MISC, g_print ("NDEVICES: %d, NCURSORS: %d\n",
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
	  g_warning ("gdk_input_init: gdk_window_new failed");
	  return;
	}
      gdk_drawable_ref (wintab_window);
      
      for (devix = 0; devix < ndevices; devix++)
	{
	  LOGCONTEXT lc;

	  WTInfo (WTI_DEVICES + devix, DVC_NAME, devname);
      
	  WTInfo (WTI_DEVICES + devix, DVC_NCSRTYPES, &ncsrtypes);
	  WTInfo (WTI_DEVICES + devix, DVC_FIRSTCSR, &firstcsr);
	  WTInfo (WTI_DEVICES + devix, DVC_HARDWARE, &hardware);
	  WTInfo (WTI_DEVICES + devix, DVC_X, &axis_x);
	  WTInfo (WTI_DEVICES + devix, DVC_Y, &axis_y);
	  WTInfo (WTI_DEVICES + devix, DVC_NPRESSURE, &axis_npressure);
	  WTInfo (WTI_DEVICES + devix, DVC_ORIENTATION, axis_or);

	  if (HIBYTE (specversion) > 1 || LOBYTE (specversion) >= 1)
	    {
	      WTInfo (WTI_DDCTXS + devix, CTX_NAME, lc.lcName);
	      WTInfo (WTI_DDCTXS + devix, CTX_OPTIONS, &lc.lcOptions);
	      lc.lcOptions |= CXO_MESSAGES;
#if USE_SYSCONTEXT
	      lc.lcOptions |= CXO_SYSTEM;
#endif
	      lc.lcStatus = 0;
	      WTInfo (WTI_DDCTXS + devix, CTX_LOCKS, &lc.lcLocks);
	      lc.lcMsgBase = WT_DEFBASE;
	      lc.lcDevice = devix;
	      lc.lcPktRate = 50;
	      lc.lcPktData = PACKETDATA;
	      lc.lcPktMode = PK_BUTTONS; /* We want buttons in relative mode */
	      lc.lcMoveMask = PACKETDATA;
	      lc.lcBtnDnMask = lc.lcBtnUpMask = ~0;
	      WTInfo (WTI_DDCTXS + devix, CTX_INORGX, &lc.lcInOrgX);
	      WTInfo (WTI_DDCTXS + devix, CTX_INORGY, &lc.lcInOrgY);
	      WTInfo (WTI_DDCTXS + devix, CTX_INORGZ, &lc.lcInOrgZ);
	      WTInfo (WTI_DDCTXS + devix, CTX_INEXTX, &lc.lcInExtX);
	      WTInfo (WTI_DDCTXS + devix, CTX_INEXTY, &lc.lcInExtY);
	      WTInfo (WTI_DDCTXS + devix, CTX_INEXTZ, &lc.lcInExtZ);
	      lc.lcOutOrgX = axis_x.axMin;
	      lc.lcOutOrgY = axis_y.axMin;
	      lc.lcOutExtX = axis_x.axMax - axis_x.axMin;
	      lc.lcOutExtY = axis_y.axMax - axis_y.axMin;
	      lc.lcOutExtY = -lc.lcOutExtY; /* We want Y growing downward */
	      WTInfo (WTI_DDCTXS + devix, CTX_SENSX, &lc.lcSensX);
	      WTInfo (WTI_DDCTXS + devix, CTX_SENSY, &lc.lcSensY);
	      WTInfo (WTI_DDCTXS + devix, CTX_SENSZ, &lc.lcSensZ);
	      WTInfo (WTI_DDCTXS + devix, CTX_SYSMODE, &lc.lcSysMode);
	      lc.lcSysOrgX = lc.lcSysOrgY = 0;
	      WTInfo (WTI_DDCTXS + devix, CTX_SYSEXTX, &lc.lcSysExtX);
	      WTInfo (WTI_DDCTXS + devix, CTX_SYSEXTY, &lc.lcSysExtY);
	      WTInfo (WTI_DDCTXS + devix, CTX_SYSSENSX, &lc.lcSysSensX);
	      WTInfo (WTI_DDCTXS + devix, CTX_SYSSENSY, &lc.lcSysSensY);
	    }
	  else
	    {
	      lc = defcontext;
	      lc.lcOptions |= CXO_MESSAGES;
	      lc.lcMsgBase = WT_DEFBASE;
	      lc.lcPktRate = 50;
	      lc.lcPktData = PACKETDATA;
	      lc.lcPktMode = PACKETMODE;
	      lc.lcMoveMask = PACKETDATA;
	      lc.lcBtnUpMask = lc.lcBtnDnMask = ~0;
#if 0
	      lc.lcOutExtY = -lc.lcOutExtY; /* Y grows downward */
#else
	      lc.lcOutOrgX = axis_x.axMin;
	      lc.lcOutOrgY = axis_y.axMin;
	      lc.lcOutExtX = axis_x.axMax - axis_x.axMin;
	      lc.lcOutExtY = axis_y.axMax - axis_y.axMin;
	      lc.lcOutExtY = -lc.lcOutExtY; /* We want Y growing downward */
#endif
	    }
#if DEBUG_WINTAB
	  GDK_NOTE (MISC, (g_print("context for device %d:\n", devix),
			   print_lc(&lc)));
#endif
	  hctx = g_new (HCTX, 1);
          if ((*hctx = WTOpen (GDK_WINDOW_HWND (wintab_window), &lc, TRUE)) == NULL)
	    {
	      g_warning ("gdk_input_init: WTOpen failed");
	      return;
	    }
	  GDK_NOTE (MISC, g_print ("opened Wintab device %d %#x\n",
				   devix, *hctx));

	  wintab_contexts = g_list_append (wintab_contexts, hctx);
#if 0
	  WTEnable (*hctx, TRUE);
#endif
	  WTOverlap (*hctx, TRUE);

#if DEBUG_WINTAB
	  GDK_NOTE (MISC, (g_print("context for device %d after WTOpen:\n", devix),
			   print_lc(&lc)));
#endif
	  for (cursorix = firstcsr; cursorix < firstcsr + ncsrtypes; cursorix++)
	    {
	      active = FALSE;
	      WTInfo (WTI_CURSORS + cursorix, CSR_ACTIVE, &active);
	      if (!active)
		continue;
	      gdkdev = g_new (GdkDevicePrivate, 1);
	      WTInfo (WTI_CURSORS + cursorix, CSR_NAME, csrname);
	      gdkdev->info.name = g_strconcat (devname, " ", csrname, NULL);
	      gdkdev->info.source = GDK_SOURCE_PEN;
	      gdkdev->info.mode = GDK_MODE_SCREEN;
#if USE_SYSCONTEXT
	      gdkdev->info.has_cursor = TRUE;
#else
	      gdkdev->info.has_cursor = FALSE;
#endif
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
		  gdkdev->info.axes[k].min = axis_x.axMax;
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
		  gdkdev->info.axes[k].min = axis_y.axMax;
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
		  gdkdev->info.axes[k].min = axis_npressure.axMin;
		  gdkdev->info.axes[k].min = axis_npressure.axMax;
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
		      gdkdev->info.axes[k].min = 1000;
		      k++;
		    }
		}
	      gdkdev->info.num_keys = 0;
	      gdkdev->info.keys = NULL;
	      GDK_NOTE (EVENTS,
			g_print ("device: (%d) %s axes: %d\n",
				 cursorix,
				 gdkdev->info.name,
				 gdkdev->info.num_axes));
	      for (i = 0; i < gdkdev->info.num_axes; i++)
		GDK_NOTE (EVENTS,
			  g_print ("...axis %d: %d--%d@%d (%d--%d@%d)\n",
				   i,
				   gdkdev->axes[i].xmin_value, 
				   gdkdev->axes[i].xmax_value, 
				   gdkdev->axes[i].xresolution, 
				   gdkdev->axes[i].min_value, 
				   gdkdev->axes[i].max_value, 
				   gdkdev->axes[i].resolution));
	      gdk_input_devices = g_list_append (gdk_input_devices,
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

#if !USE_SYSCONTEXT

static GdkInputWindow *
gdk_input_window_find_within (GdkWindow *window)
{
  GList *list;
  GdkWindow *tmpw;
  GdkInputWindow *candidate = NULL;

  for (list = gdk_input_windows; list != NULL; list = list->next)
    {
      tmpw = ((GdkInputWindow *) (tmp_list->data))->window;
      if (tmpw == window
	  || IsChild (GDK_WINDOW_HWND (window), GDK_WINDOW_HWND (tmpw)))
	{
	  if (candidate)
	    return NULL;		/* Multiple hits */
	  candidate = (GdkInputWindow *) (list->data);
	}
    }

  return candidate;
}

#endif /* USE_SYSCONTEXT */

#endif /* HAVE_WINTAB */

static void
gdk_input_translate_coordinates (GdkDevicePrivate *gdkdev,
				 GdkInputWindow   *input_window,
				 gint             *axis_data,
				 gdouble          *axis_out,
				 gdouble          *x_out,
				 gdouble          *y_out)
{
  GdkWindowImplWin32 *impl;

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
      x_scale = gdk_screen_width() / device_width;
      y_scale = gdk_screen_height() / device_height;

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
    *x_ret = rect.left;
  if (y_ret)
    *y_ret = rect.top;
}

GdkTimeCoord *
gdk_input_motion_events (GdkWindow *window,
			 guint32    deviceid,
			 guint32    start,
			 guint32    stop,
			 gint      *nevents_return)
{
  g_return_val_if_fail (window != NULL, NULL);
  if (GDK_WINDOW_DESTROYED (window))
    return NULL;

  *nevents_return = 0;
  return NULL;		/* ??? */
}

void
_gdk_input_configure_event (GdkEventConfigure *event,
			    GdkWindow         *window)
{
  GdkInputWindow *input_window;
  int root_x, root_y;

  input_window = gdk_input_window_find (window);
  g_return_if_fail (window != NULL);

  gdk_input_get_root_relative_geometry (GDK_WINDOW_HWND (window),
					&root_x, &root_y);

  input_window->root_x = root_x;
  input_window->root_y = root_y;
}

void 
_gdk_input_enter_event (GdkEventCrossing *event, 
			GdkWindow        *window)
{
  GdkInputWindow *input_window;
  int root_x, root_y;

  input_window = gdk_input_window_find (window);
  g_return_if_fail (window != NULL);

  gdk_input_get_root_relative_geometry (GDK_WINDOW_HWND (window), &root_x, &root_y);

  input_window->root_x = root_x;
  input_window->root_y = root_y;
}

gint 
_gdk_input_other_event (GdkEvent  *event,
			MSG       *msg,
			GdkWindow *window)
{
#ifdef HAVE_WINTAB
#if !USE_SYSCONTEXT
  GdkWindow *current_window;
#endif
  GdkWindowObject *obj;
  GdkWindowImplWin32 *impl;
  GdkInputWindow *input_window;
  GdkDevicePrivate *gdkdev;
  GdkEventMask masktest;
  POINT pt;

  PACKET packet;
  gint k;
  gint x, y;

  if (event->any.window != wintab_window)
    {
      g_warning ("_gdk_input_other_event: not wintab_window?");
      return FALSE;
    }

#if USE_SYSCONTEXT
  window = gdk_window_at_pointer (&x, &y);
  if (window == NULL)
    window = gdk_parent_root;

  gdk_drawable_ref (window);

  GDK_NOTE (EVENTS,
	    g_print ("gdk_input_win32_other_event: window=%#x (%d,%d)\n",
		     GDK_WINDOW_HWND (window), x, y));
  
#else
  /* ??? This code is pretty bogus */
  current_window = gdk_win32_handle_table_lookup (GetActiveWindow ());
  if (current_window == NULL)
    return FALSE;
  
  input_window = gdk_input_window_find_within (current_window);
  if (input_window == NULL)
    return FALSE;
#endif

  if (msg->message == WT_PACKET)
    {
      if (!WTPacket ((HCTX) msg->lParam, msg->wParam, &packet))
	return FALSE;
    }

  obj = GDK_WINDOW_OBJECT (window);
  impl = GDK_WINDOW_IMPL_WIN32 (obj->impl);

  switch (msg->message)
    {
    case WT_PACKET:
      if (window == gdk_parent_root)
	{
	  GDK_NOTE (EVENTS, g_print ("...is root\n"));
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

      if (HIWORD (packet.pkButtons) != TBN_NONE)
	{
	  /* Gdk buttons are numbered 1.. */
	  event->button.button = 1 + LOWORD (packet.pkButtons);

	  if (HIWORD (packet.pkButtons) == TBN_UP)
	    {
	      event->any.type = GDK_BUTTON_RELEASE;
	      masktest = GDK_BUTTON_RELEASE_MASK;
	      gdkdev->button_state &= ~(1 << LOWORD (packet.pkButtons));
	    }
	  else
	    {
	      event->any.type = GDK_BUTTON_PRESS;
	      masktest = GDK_BUTTON_PRESS_MASK;
	      gdkdev->button_state |= 1 << LOWORD (packet.pkButtons);
	    }
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

      /* Now we can check if the window wants the event, and
       * propagate if necessary.
       */
    dijkstra:
      if (!impl->extension_events_selected
	  || !(obj->extension_events & masktest))
	{
	  GDK_NOTE (EVENTS, g_print ("...not selected\n"));

	  if (obj->parent == GDK_WINDOW_OBJECT (gdk_parent_root))
	    return FALSE;
	  
	  pt.x = x;
	  pt.y = y;
	  ClientToScreen (GDK_WINDOW_HWND (window), &pt);
	  gdk_drawable_unref (window);
	  window = (GdkWindow *) obj->parent;
	  obj = GDK_WINDOW_OBJECT (window);
	  gdk_drawable_ref (window);
	  ScreenToClient (GDK_WINDOW_HWND (window), &pt);
	  x = pt.x;
	  y = pt.y;
	  GDK_NOTE (EVENTS, g_print ("...propagating to %#x, (%d,%d)\n",
				     GDK_WINDOW_HWND (window), x, y));
	  goto dijkstra;
	}

      input_window = gdk_input_window_find (window);

      g_assert (input_window != NULL);

      if (gdkdev->info.mode == GDK_MODE_WINDOW
	  && input_window->mode == GDK_EXTENSION_EVENTS_CURSOR)
	return FALSE;

      event->any.window = window;

      if (event->any.type == GDK_BUTTON_PRESS
	  || event->any.type == GDK_BUTTON_RELEASE)
	{
	  event->button.time = msg->time;
	  event->button.device = &gdkdev->info;
	  
#if 0
#if USE_SYSCONTEXT
	  /* Buttons 1 to 3 will come in as WM_[LMR]BUTTON{DOWN,UP} */
	  if (event->button.button <= 3)
	    return FALSE;
#endif
#endif
	  gdk_input_translate_coordinates (gdkdev, input_window,
					   gdkdev->last_axis_data,
					   event->button.axes,
					   &event->button.x, 
					   &event->button.y);

	  event->button.state = ((gdkdev->button_state << 8)
				 & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK
				    | GDK_BUTTON3_MASK | GDK_BUTTON4_MASK
				    | GDK_BUTTON5_MASK));
	  GDK_NOTE (EVENTS, g_print ("WINTAB button %s:%d %g,%g\n",
				     (event->button.type == GDK_BUTTON_PRESS ?
				      "press" : "release"),
				     event->button.button,
				     event->button.x, event->button.y));
	}
      else
	{
	  event->motion.time = msg->time;
	  event->motion.is_hint = FALSE;
	  event->motion.device = &gdkdev->info;

	  gdk_input_translate_coordinates (gdkdev, input_window,
					   gdkdev->last_axis_data,
					   event->motion.axes,
					   &event->motion.x, 
					   &event->motion.y);

	  event->motion.state = ((gdkdev->button_state << 8)
				 & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK
				    | GDK_BUTTON3_MASK | GDK_BUTTON4_MASK
				    | GDK_BUTTON5_MASK));

	  GDK_NOTE (EVENTS, g_print ("WINTAB motion: %g,%g\n",
				     event->motion.x, event->motion.y));

	  /* Check for missing release or press events for the normal
	   * pressure button. At least on my ArtPadII I sometimes miss a
	   * release event?
	   */
	  if ((gdkdev->pktdata & PK_NORMAL_PRESSURE
	       && (event->motion.state & GDK_BUTTON1_MASK)
	       && packet.pkNormalPressure <= MAX (0, gdkdev->npbtnmarks[0] - 2))
	      || (gdkdev->pktdata & PK_NORMAL_PRESSURE
		  && !(event->motion.state & GDK_BUTTON1_MASK)
		  && packet.pkNormalPressure > gdkdev->npbtnmarks[1] + 2))
	    {
	      GdkEvent *event2 = gdk_event_copy (event);
	      if (event->motion.state & GDK_BUTTON1_MASK)
		{
		  event2->button.type = GDK_BUTTON_RELEASE;
		  gdkdev->button_state &= ~1;
		}
	      else
		{
		  event2->button.type = GDK_BUTTON_PRESS;
		  gdkdev->button_state |= 1;
		}
	      event2->button.state = ((gdkdev->button_state << 8)
				      & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK
					 | GDK_BUTTON3_MASK | GDK_BUTTON4_MASK
					 | GDK_BUTTON5_MASK));
	      event2->button.button = 1;
	      GDK_NOTE (EVENTS, g_print ("WINTAB synthesized button %s: %d %g,%gg\n",
					 (event2->button.type == GDK_BUTTON_PRESS ?
					  "press" : "release"),
					 event2->button.button,
					 event2->button.x,
					 event2->button.y));
	      gdk_event_queue_append (event2);
	    }
	}
      return TRUE;

    case WT_PROXIMITY:
      if (LOWORD (msg->lParam) == 0)
	{
	  event->proximity.type = GDK_PROXIMITY_OUT;
	  gdk_input_ignore_core = FALSE;
	}
      else
	{
	  event->proximity.type = GDK_PROXIMITY_IN;
	  gdk_input_ignore_core = TRUE;
	}
      event->proximity.time = msg->time;
      event->proximity.device = &gdkdev->info;

      GDK_NOTE (EVENTS, g_print ("WINTAB proximity %s\n",
				 (event->proximity.type == GDK_PROXIMITY_IN ?
				  "in" : "out")));
      return TRUE;
    }
#endif
  return -1;
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
  gint result;

  tmp_list = gdk_input_windows;
  new_window = NULL;
  need_ungrab = FALSE;

  GDK_NOTE (MISC, g_print ("gdk_input_win32_grab_pointer: %#x %d %#x\n",
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
      
      tmp_list = gdk_input_devices;
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
      tmp_list = gdk_input_devices;
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

  GDK_NOTE (MISC, g_print ("gdk_input_win32_ungrab_pointer\n"));

  tmp_list = gdk_input_windows;
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

      tmp_list = gdk_input_devices;
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
#endif
}

gint 
_gdk_input_window_none_event (GdkEvent *event,
			      MSG      *msg)
{
  return -1;
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
  gint i;

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
      
      if (mask)
	gdk_window_get_pointer (window, NULL, NULL, mask);
      
      gdkdev = (GdkDevicePrivate *)device;
      input_window = gdk_input_window_find (window);
      g_return_if_fail (input_window != NULL);

#if 0 /* FIXME */
      state = XQueryDeviceState (gdk_display, gdkdev->xdevice);
      input_class = state->data;
      for (i = 0; i < state->num_classes; i++)
	{
	  switch (input_class->class)
	    {
	    case ValuatorClass:
	      if (axes)
		gdk_input_translate_coordinates (gdkdev, input_window,
						 ((XValuatorState *)input_class)->valuators,
						 axes, NULL, NULL);
	      break;
	      
	    case ButtonClass:
	      if (mask)
		{
		  *mask &= 0xFF;
		  if (((XButtonState *)input_class)->num_buttons > 0)
		    *mask |= ((XButtonState *)input_class)->buttons[0] << 7;
		  /* GDK_BUTTON1_MASK = 1 << 8, and button n is stored
		   * in bit 1<<(n%8) in byte n/8. n = 1,2,... */
		}
	      break;
	    }
	  input_class = (XInputClass *)(((char *)input_class)+input_class->length);
	}
      XFreeDeviceState (state);
#endif
    }
}

void 
gdk_input_init (void)
{
  gdk_input_ignore_core = FALSE;
  gdk_input_devices = NULL;

#ifdef HAVE_WINTAB
  gdk_input_wintab_init ();
#endif /* HAVE_WINTAB */

  gdk_input_devices = g_list_append (gdk_input_devices, &gdk_input_core_info);
}

