/* GTK - The GIMP Toolkit
 * gdkasync.c: Utility functions using the Xlib asynchronous interfaces
 * Copyright (C) 2003, Red Hat, Inc.
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
/* Portions of code in this file are based on code from Xlib
 */
/*
Copyright 1986, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

*/
#include <X11/Xlibint.h>
#include "gdkasync.h"
#include "gdkx.h"

typedef struct _SetInputFocusState SetInputFocusState;

struct _SetInputFocusState
{
  Display *dpy;
  Window window;
  _XAsyncHandler async;
  gulong set_input_focus_req;
  gulong get_input_focus_req;
};

static Bool
set_input_focus_handler (Display *dpy,
			 xReply  *rep,
			 char    *buf,
			 int      len,
			 XPointer data)
{
  SetInputFocusState *state = (SetInputFocusState *)data;  

  if (dpy->last_request_read == state->set_input_focus_req)
    {
      if (rep->generic.type == X_Error &&
	  rep->error.errorCode == BadMatch)
	{
	  /* Consume BadMatch errors, since we have no control
	   * over them.
	   */
	  return True;
	}
    }
  
  if (dpy->last_request_read == state->get_input_focus_req)
    {
      xGetInputFocusReply replbuf;
      xGetInputFocusReply *repl;
      
      if (rep->generic.type != X_Error)
	{
	  /* Actually does nothing, since there are no additional bytes
	   * to read, but maintain good form.
	   */
	  repl = (xGetInputFocusReply *)
	    _XGetAsyncReply(dpy, (char *)&replbuf, rep, buf, len,
			    (sizeof(xGetInputFocusReply) - sizeof(xReply)) >> 2,
			    True);
	}

      DeqAsyncHandler(state->dpy, &state->async);

      g_free (state);
      
      return (rep->generic.type != X_Error);
    }

  return False;
}

void
_gdk_x11_set_input_focus_safe (GdkDisplay             *display,
			       Window                  window,
			       int                     revert_to,
			       Time                    time)
{
  Display *dpy;
  SetInputFocusState *state;
  
  dpy = GDK_DISPLAY_XDISPLAY (display);

  state = g_new (SetInputFocusState, 1);

  state->dpy = dpy;
  state->window = window;
  
  LockDisplay(dpy);

  state->async.next = dpy->async_handlers;
  state->async.handler = set_input_focus_handler;
  state->async.data = (XPointer) state;
  dpy->async_handlers = &state->async;

  {
    xSetInputFocusReq *req;
    
    GetReq(SetInputFocus, req);
    req->focus = window;
    req->revertTo = revert_to;
    req->time = time;
    state->set_input_focus_req = dpy->request;
  }

  /*
   * XSync (dpy, 0)
   */
  {
    xReq *req;
    
    GetEmptyReq(GetInputFocus, req);
    state->get_input_focus_req = dpy->request;
  }
  
  UnlockDisplay(dpy);
  SyncHandle();
}
