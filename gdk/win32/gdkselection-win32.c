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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/*
GTK+ selection works like this:
There are three selections that matter - GDK_SELECTION_CLIPBOARD,
GDK_SELECTION_PRIMARY and DND. Primary selection is only handled
internally by GTK+ (it's not portable to Windows). DND is actually
represented by two selections - LOCAL and OLE2, one for each DnD protocol,
but they work the same way.

"Target" is a GdkAtom describing a clipboard format.

For Clipboard:
GTK+ calls gtk_clipboard_set_contents(), which first ensures the
clipboard is owned by the clipboard widget (which also indirectly
causes an SelectionRequest xevent to be sent to it), then clears the old
supported targets from the clipboard, then adds all the
targets it's given to the clipboard. No data is sent anywhere.

gtk_clipboard_set_contents() is also given a callback to invoke when
the actual data is needed. This callback is implemented by the widget
from which the data can be put into clipboard.

GTK+ might also call gtk_clipboard_set_can_store(), which sets the
targets for which the data can be put into system clipboard, so that
it remains usable even if the application is no longer around. Usually
all data formats are storable, except for the shortcut formats, which
refer to actual widgets directly, and are thus only working while
the application is alive.

("C:" means clipboard client (requestor), "S:" means clipboard server (provider))

When something needs to be obtained from clipboard, GTK+ calls
C: gtk_selection_convert().
That function has a shortcut where it directly gets the selection contents by calling
S: gtk_selection_invoke_handler(),
asking the widget to provide data, and then calling
C: gtk_selection_retrieval_report()
to report the data back to the caller.

If that shortcut isn't possible (selection is owned by another process),
gtk_selection_convert() calls
C:gdk_selection_convert() (_gdk_x11_display_convert_selection())

On X11 gdk_selection_convert() just calls
C:XConvertSelection(),
which sends SelectionRequest xevent to the window that owns the selection.
The client gives its clipboard window as the requestor for that event,
and gives the property as GDK_SELECTION.

Server-side GTK+ catches SelectionRequest in a
S:_gtk_selection_request()
event handler, which calls
S:gtk_selection_invoke_handler()
to get the data, and then calls
S:gdk_property_change() (_gdk_x11_window_change_property())
to submit the data, by setting the property given by the message sender
(GDK_SELECTION) on the requestor window (our client clipboard window).

On X11 data submission takes from of
S:XChangeProperty()
call, which causes SelectionNotify (and PropertyNotify for INCR)
xevent to be sent, which client-side GTK+ catches and handles in
C:_gtk_selection_notify()
(and
C:_gtk_selection_property_notify(),
for INCR)
event handler, which calls
C:gtk_selection_retrieval_report()
to report back to the caller. The caller gets the property
data from the window, and returns it up the stack.

On X11 the "TARGETS" target might be given in a SelectionRequest xmessage to request
all supported targets for a selection.

If data must be stored on the clipboard, because the application is quitting,
GTK+ will call
S:gdk_clipboard_store() -> gdk_display_store_clipboard() (gdk_x11_display_store_clipboard())
on all the clipboards it owns.
X11 gdk_display_store_clipboard() puts a list of storeable targets into GDK_SELECTION
property of the clipboard window, then calls
S:XConvertSelection()
on the clipboard manager window (retrieved from the CLIPBOARD_MANAGER atom),
and the clipboard manager responds by requesting all these formats and storing the data,
then responds with SelectionNotify xevent to allow the application to quit.

When clipboard owner changes, the old owner receives SelectionClear xevent,
GTK+ handles it by clearing the clipboard object on its own level, GDK
is not involved.

On Windows:
Clipboard is opened by OpenClipboard(), emptied by EmptyClipboard() (which also
makes the window the clipboard owner), data is put into it by SetClipboardData().
Clipboard is closed with CloseClipboard().
If SetClipboardData() is given a NULL data value, the owner will later
receive WM_RENDERFORMAT message, in response to which it must call
SetClipboardData() with the provided handle and the actual data this time.
This way applications can avoid storing everything in the clipboard
all the time, only putting the data there as it is requested by other applications.
At some undefined points of time an application might get WM_RENDERALLFORMATS
message, it should respond by opening the clipboard and rendering
into it all the data that it offers, as if responding to multiple WM_RENDERFORMAT
messages.

On GDK-Win32:
GTK+ calls gtk_clipboard_set_contents(), which first ensures the
clipboard is owned by the clipboard widget (calls OpenClipboard(),
then EmptyClipboard() to become the owner, then
sends a TARGETS GDK_SELECTION_REQUEST to itself, without closing the clipboard),
then clears the old supported targets from the clipboard, then adds all the
targets it's given to the clipboard. No data is sent anywhere.

gtk_clipboard_set_contents() is also given a callback to invoke when
the actual data is needed. This callback is implemented by the widget
from which the data can be put into clipboard.

GTK+ might also call gtk_clipboard_set_can_store(), which sets the
targets for which the data can be put into system clipboard, so that
it remains usable even if the application is no longer around. Usually
all data formats are storable, except for the shortcut formats, which
refer to actual widgets directly, and are thus only working while
the application is alive.

("C:" means clipboard client (requestor), "S:" means clipboard server (provider))
("transmute" here means "change the format of some data"; this term is used here
 instead of "convert" to avoid clashing with g(t|d)k_selection_convert(), which
 is completely unrelated)

When something needs to be obtained from clipboard, GTK+ calls
C: gtk_selection_convert().
That function has a shortcut where it directly gets the selection contents by calling
S: gtk_selection_invoke_handler(),
asking the widget to provide data, and then calling
C: gtk_selection_retrieval_report()
to report the data back to the caller.

If that shortcut isn't possible (selection is owned by another process),
gtk_selection_convert() calls
C:gdk_selection_convert() (_gdk_win32_display_convert_selection())

On GDK-Win32 gdk_selection_convert() just calls
C:OpenClipboard()
to open clipboard (if that fails, it shedules a timeout to regularly
try to open clipboard for the next 30 seconds, and do the actions
outlined below once the clipboard is opened, or notify about
conversion failure after 30 seconds),
C:EnumClipboardFormats() (2000+)
to get the list of supported formats, figures out the format it should
use to request the data (first it looks for supported formats with names
that match the target name, then looks through compatibility
formats for the target and checks whether these are supported).
Note that it has no list of supported targets at hand,
just the single requested target, and thus it might have
to do some transmutation between formats; the caller up the stack
either only supports just one format that it asks for,
or supports multiple formats and asks for them in sequence (from
the most preferred to the least preferred), until one call succeeds,
or supports multiple formats and asks for the TARGETS format first,
and then figures out what to ask for - GDK can't know that.
Either way, GDK has to call
C:GetClipboardData()
to get the data (this causes WM_RENDERFORMAT to be sent to the owner,
if the owner uses delayed rendering for the requested format, otherwise
it just picks the data right from the OS)

Server-side GDK catches WM_RENDERFORMAT, figures out a target
to request (this one is easier, as it has the list of supported
targets saved up), and posts a GDK_SELECTION_REQUEST event, then runs the main loop,
while GTK+ catches the event in a
S:_gtk_selection_request()
event handler, which calls
S:gtk_selection_invoke_handler()
to get the data, and then calls
S:gdk_property_change() (_gdk_win32_window_change_property())
to submit the data, by first transmuting it to the format actually requested
by the sender of WM_RENDERFORMAT, and then by returning thedata back up the stack,
to the WM_RENDERFORMAT handler, which then calls
S:SetClipboardData()
with the handle provided by the sender.

Meanwhile, the client code, still in
C:_gdk_win32_display_convert_selection(),
gets the data in response to GetClipboardData(),
transmutes it (if needed) to the target format, sets the requested
window property to that data (unlike change_property!),
calls
C:CloseClipboard() (if there are no more clipboard opeartions
scheduled)
and posts a GDK_SELECTION_NOTIFY event, which GTK+ catches in
C:_gtk_selection_notify()
event handler, which calls
C:gtk_selection_retrieval_report()
to report back to the caller. The caller gets the property
data from the window, and returns it up the stack.

On GDK-Win32 the "TARGETS" target might be given in a GDK_SELECTION_REQUEST to request
all supported targets for a selection.
Note that this server side -
client side should call gdk_selection_convert() -> gdk_selection_convert() with "TARGETS" target
to get the list of targets offered by the clipboard holder. It never causes GDK_SELECTION_REQUEST
to be generated, just queries the system clipboard.
On server side GDK_SELECTION_REQUEST is only generated internally:
in response to WM_RENDERFORMAT (it renders a target),
in response to idataobject_getdata() (it renders a target),
after DnD ends (with a DELETE target, this is caught by GTK to make it delete the selection),
and in response to owner change, with TARGETS target, which makes it register its formats by calling
S:SetClipboardData(..., NULL)

If data must be stored on the clipboard, because the application is quitting,
GTK+ will call
S:gdk_clipboard_store() -> gdk_display_store_clipboard() (gdk_win32_display_store_clipboard())
on all the clipboards it owns.
GDK-Win32 gdk_display_store_clipboard() sends WM_RENDERALLFORMATS to the window,
then posts a GDK_SELECTION_NOTIFY event allow the application to quit.

When clipboard owner changes, the old owner receives WM_DESTROYCLIPBOARD message,
GDK handles it by posting a GDK_SELECTION_CLEAR event, which
GTK+ handles by clearing the clipboard object on its own level.

Any operations that require OpenClipboard()/CloseClipboard() combo (i.e.
everything, except for WM_RENDERFORMAT handling) must be put into a queue,
and then a once-per-second-for-up-to-30-seconds timeout must be added.
The timeout function must call OpenClipboard(),
and then proceed to perform the queued actions on the clipboard, once it opened,
or return and try again a second later, as long as there are still items in the queue,
and remove the queue items that are older than 30 seconds.
Once the queue is empty, the clipboard is closed.

DND:
GDK-Win32:
S:idataobject_getdata()
sends a GDK_SELECTION_REQUEST event, which results in a call to
S:_gdk_win32_window_change_property()
which passes clipboard data back via the selection singleton.
GDK-Win32 uses delayed rendering for all formats, even text.

GTK+ will call
C:gtk_selection_convert() -> gdk_selection_convert() (_gdk_win32_display_convert_selection())
to get the data associated with the drag, when GTK+ apps want to inspect the data,
but with a OLE2_DND selection instead of CLIPBOARD selection.

_gdk_win32_display_convert_selection() queries the droptarget global variable,
which should already contain a matched list of supported formats and targets,
picks a format there, then queries it from the IDataObject that the droptarget kept around.
Then optionally transmutes the data, and sets the property. Then posts GDK_SELECTION_NOTIFY.

GTK+ catches that event and processes it, causeing "selection-received" signal to
be emitted on the selection widget, and its handler is
C:gtk_drag_selection_received(),
which emits the "drag-data-received" signal for the app.
*/

#include "config.h"
#include <string.h>
#include <stdlib.h>

/* For C-style COM wrapper macros */
#define COBJMACROS

/* for CIDA */
#include <shlobj.h>

#include "gdkproperty.h"
#include "gdkselection.h"
#include "gdkdisplay.h"
#include "gdkprivate-win32.h"
#include "gdkselection-win32.h"
#include "gdk/gdkdndprivate.h"
#include "gdkwin32dnd-private.h"
#include "gdkwin32.h"

typedef struct _GdkWin32ClipboardQueueInfo GdkWin32ClipboardQueueInfo;

typedef enum _GdkWin32ClipboardQueueAction GdkWin32ClipboardQueueAction;

enum _GdkWin32ClipboardQueueAction
{
  GDK_WIN32_CLIPBOARD_QUEUE_ACTION_CONVERT = 0,
  GDK_WIN32_CLIPBOARD_QUEUE_ACTION_TARGETS
};

struct _GdkWin32ClipboardQueueInfo
{
  GdkDisplay                   *display;
  GdkWindow                    *requestor;
  GdkAtom                       selection;
  GdkAtom                       target;
  guint32                       time;

  /* Number of seconds since we started our
   * attempts to open clipboard.
   */
  guint32                       idle_time;

  /* What to do once clipboard is opened */
  GdkWin32ClipboardQueueAction  action;
};

/* List of GdkWin32ClipboardQueueInfo slices */
static GList *clipboard_queue = NULL;

#define HIDA_GetPIDLFolder(pida) (LPCITEMIDLIST)(((LPBYTE)pida)+(pida)->aoffset[0])
#define HIDA_GetPIDLItem(pida, i) (LPCITEMIDLIST)(((LPBYTE)pida)+(pida)->aoffset[i+1])

G_DEFINE_TYPE (GdkWin32Selection, gdk_win32_selection, G_TYPE_OBJECT)

static void
gdk_win32_selection_class_init (GdkWin32SelectionClass *klass)
{
}

void
_gdk_win32_selection_init (void)
{
  _win32_selection = GDK_WIN32_SELECTION (g_object_new (GDK_TYPE_WIN32_SELECTION, NULL));
}

static void
gdk_win32_selection_init (GdkWin32Selection *win32_selection)
{
  GArray             *atoms;
  GArray             *cfs;
  GSList             *pixbuf_formats;
  GSList             *rover;
  int                 i;
  GArray             *comp;
  GdkSelTargetFormat  fmt;

  win32_selection->ignore_destroy_clipboard = FALSE;
  win32_selection->clipboard_opened_for = INVALID_HANDLE_VALUE;

  win32_selection->dnd_target_state = GDK_WIN32_DND_NONE;
  win32_selection->dnd_source_state = GDK_WIN32_DND_NONE;
  win32_selection->dnd_data_object_target = NULL;
  win32_selection->property_change_format = 0;
  win32_selection->property_change_data = NULL;
  win32_selection->property_change_target_atom = 0;

  atoms = g_array_sized_new (FALSE, TRUE, sizeof (GdkAtom), GDK_WIN32_ATOM_INDEX_LAST);
  g_array_set_size (atoms, GDK_WIN32_ATOM_INDEX_LAST);
  cfs = g_array_sized_new (FALSE, TRUE, sizeof (UINT), GDK_WIN32_CF_INDEX_LAST);
  g_array_set_size (cfs, GDK_WIN32_CF_INDEX_LAST);

  win32_selection->known_atoms = atoms;
  win32_selection->known_clipboard_formats = cfs;

  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_GDK_SELECTION) = gdk_atom_intern_static_string ("GDK_SELECTION");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_CLIPBOARD_MANAGER) = gdk_atom_intern_static_string ("CLIPBOARD_MANAGER");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_WM_TRANSIENT_FOR) = gdk_atom_intern_static_string ("WM_TRANSIENT_FOR");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_TARGETS) = gdk_atom_intern_static_string ("TARGETS");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_DELETE) = gdk_atom_intern_static_string ("DELETE");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_SAVE_TARGETS) = gdk_atom_intern_static_string ("SAVE_TARGETS");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_UTF8_STRING) = gdk_atom_intern_static_string ("UTF8_STRING");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_TEXT) = gdk_atom_intern_static_string ("TEXT");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_COMPOUND_TEXT) = gdk_atom_intern_static_string ("COMPOUND_TEXT");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_TEXT_URI_LIST) = gdk_atom_intern_static_string ("text/uri-list");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_TEXT_HTML) = gdk_atom_intern_static_string ("text/html");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_IMAGE_PNG) = gdk_atom_intern_static_string ("image/png");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_IMAGE_JPEG) = gdk_atom_intern_static_string ("image/jpeg");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_IMAGE_BMP) = gdk_atom_intern_static_string ("image/bmp");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_IMAGE_GIF) = gdk_atom_intern_static_string ("image/gif");

  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_LOCAL_DND_SELECTION) = gdk_atom_intern_static_string ("LocalDndSelection");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_DROPFILES_DND) = gdk_atom_intern_static_string ("DROPFILES_DND");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_OLE2_DND) = gdk_atom_intern_static_string ("OLE2_DND");

  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_PNG)= gdk_atom_intern_static_string ("PNG");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_JFIF) = gdk_atom_intern_static_string ("JFIF");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_GIF) = gdk_atom_intern_static_string ("GIF");

  /* These are a bit unusual. It's here to allow GTK+ applications
   * to actually support CF_DIB and Shell ID List clipboard formats on their own,
   * instead of allowing GDK to use them internally for interoperability.
   */
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_CF_DIB) = gdk_atom_intern_static_string ("CF_DIB");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_CFSTR_SHELLIDLIST) = gdk_atom_intern_static_string (CFSTR_SHELLIDLIST);
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_CF_UNICODETEXT) = gdk_atom_intern_static_string ("CF_UNICODETEXT");
  _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_CF_TEXT) = gdk_atom_intern_static_string ("CF_TEXT");

  /* MS Office 2007, at least, offers images in common file formats
   * using clipboard format names like "PNG" and "JFIF". So we follow
   * the lead and map the GDK target name "image/png" to the clipboard
   * format name "PNG" etc.
   */
  _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_PNG) = RegisterClipboardFormatA ("PNG");
  _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_JFIF) = RegisterClipboardFormatA ("JFIF");
  _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_GIF) = RegisterClipboardFormatA ("GIF");

  _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_UNIFORMRESOURCELOCATORW) = RegisterClipboardFormatA ("UniformResourceLocatorW");
  _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_CFSTR_SHELLIDLIST) = RegisterClipboardFormatA (CFSTR_SHELLIDLIST);
  _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_HTML_FORMAT) = RegisterClipboardFormatA ("HTML Format");
  _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_TEXT_HTML) = RegisterClipboardFormatA ("text/html");

  _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_IMAGE_PNG) = RegisterClipboardFormatA ("image/png");
  _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_IMAGE_JPEG) = RegisterClipboardFormatA ("image/jpeg");
  _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_IMAGE_BMP) = RegisterClipboardFormatA ("image/bmp");
  _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_IMAGE_GIF) = RegisterClipboardFormatA ("image/gif");
  _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_TEXT_URI_LIST) = RegisterClipboardFormatA ("text/uri-list");
  _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_UTF8_STRING) = RegisterClipboardFormatA ("UTF8_STRING");

  win32_selection->sel_prop_table = g_hash_table_new (NULL, NULL);
  win32_selection->sel_owner_table = g_hash_table_new (NULL, NULL);

  pixbuf_formats = gdk_pixbuf_get_formats ();

  win32_selection->n_known_pixbuf_formats = 0;
  for (rover = pixbuf_formats; rover != NULL; rover = rover->next)
    {
      gchar **mime_types =
	gdk_pixbuf_format_get_mime_types ((GdkPixbufFormat *) rover->data);

      gchar **mime_type;

      for (mime_type = mime_types; *mime_type != NULL; mime_type++)
	win32_selection->n_known_pixbuf_formats++;
    }

  win32_selection->known_pixbuf_formats = g_new (GdkAtom, win32_selection->n_known_pixbuf_formats);

  i = 0;
  for (rover = pixbuf_formats; rover != NULL; rover = rover->next)
    {
      gchar **mime_types =
	gdk_pixbuf_format_get_mime_types ((GdkPixbufFormat *) rover->data);

      gchar **mime_type;

      for (mime_type = mime_types; *mime_type != NULL; mime_type++)
	win32_selection->known_pixbuf_formats[i++] = gdk_atom_intern (*mime_type, FALSE);
    }

  g_slist_free (pixbuf_formats);

  win32_selection->dnd_selection_targets = g_array_new (FALSE, FALSE, sizeof (GdkSelTargetFormat));
  win32_selection->clipboard_selection_targets = g_array_new (FALSE, FALSE, sizeof (GdkSelTargetFormat));
  win32_selection->compatibility_formats = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify) g_array_unref);

  /* GTK+ actually has more text formats, but it's unlikely that we'd
   * get anything other than UTF8_STRING these days.
   * GTKTEXTBUFFERCONTENTS format can potentially be converted to
   * W32-compatible rich text format, but that's too complex to address right now.
   */
  comp = g_array_sized_new (FALSE, FALSE, sizeof (GdkSelTargetFormat), 3);
  fmt.target = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_UTF8_STRING);

  fmt.format = _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_UTF8_STRING);
  fmt.transmute = FALSE;
  g_array_append_val (comp, fmt);

  fmt.format = CF_UNICODETEXT;
  fmt.transmute = TRUE;
  g_array_append_val (comp, fmt);

  fmt.format = CF_TEXT;
  g_array_append_val (comp, fmt);

  g_hash_table_replace (win32_selection->compatibility_formats, fmt.target, comp);


  comp = g_array_sized_new (FALSE, FALSE, sizeof (GdkSelTargetFormat), 3);
  fmt.target = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_IMAGE_PNG);

  fmt.format = _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_IMAGE_PNG);
  fmt.transmute = FALSE;
  g_array_append_val (comp, fmt);

  fmt.format = _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_PNG);
  g_array_append_val (comp, fmt);

  g_hash_table_replace (win32_selection->compatibility_formats, fmt.target, comp);


  comp = g_array_sized_new (FALSE, FALSE, sizeof (GdkSelTargetFormat), 4);
  fmt.target = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_IMAGE_JPEG);

  fmt.format = _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_IMAGE_JPEG);
  fmt.transmute = FALSE;
  g_array_append_val (comp, fmt);

  fmt.format = _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_JFIF);
  g_array_append_val (comp, fmt);

  g_hash_table_replace (win32_selection->compatibility_formats, fmt.target, comp);


  comp = g_array_sized_new (FALSE, FALSE, sizeof (GdkSelTargetFormat), 4);
  fmt.target = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_IMAGE_GIF);

  fmt.format = _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_IMAGE_GIF);
  fmt.transmute = FALSE;
  g_array_append_val (comp, fmt);

  fmt.format = _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_GIF);
  g_array_append_val (comp, fmt);

  g_hash_table_replace (win32_selection->compatibility_formats, fmt.target, comp);


  comp = g_array_sized_new (FALSE, FALSE, sizeof (GdkSelTargetFormat), 2);
  fmt.target = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_IMAGE_BMP);

  fmt.format = _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_IMAGE_BMP);
  fmt.transmute = FALSE;
  g_array_append_val (comp, fmt);

  fmt.format = CF_DIB;
  fmt.transmute = TRUE;
  g_array_append_val (comp, fmt);

  g_hash_table_replace (win32_selection->compatibility_formats, fmt.target, comp);


/* Not implemented, but definitely possible
  comp = g_array_sized_new (FALSE, FALSE, 2);
  fmt.target = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_TEXT_URI_LIST);

  fmt.format = _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_TEXT_URI_LIST);
  fmt.transmute = FALSE;
  g_array_append_val (comp, fmt);

  fmt.format = _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_CFSTR_SHELLIDLIST);
  fmt.transmute = TRUE;
  g_array_append_val (comp, fmt);

  g_hash_table_replace (win32_selection->compatibility_formats, fmt.target, comp);
*/

  win32_selection->compatibility_targets = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify) g_array_unref);

  comp = g_array_sized_new (FALSE, FALSE, sizeof (GdkSelTargetFormat), 2);
  fmt.format = CF_TEXT;
  fmt.transmute = FALSE;

  fmt.target = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_CF_TEXT);
  g_array_append_val (comp, fmt);

  fmt.target = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_UTF8_STRING);
  fmt.transmute = TRUE;
  g_array_append_val (comp, fmt);

  g_hash_table_replace (win32_selection->compatibility_targets, GINT_TO_POINTER (CF_TEXT), comp);


  comp = g_array_sized_new (FALSE, FALSE, sizeof (GdkSelTargetFormat), 2);
  fmt.format = CF_UNICODETEXT;
  fmt.transmute = FALSE;

  fmt.target = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_CF_UNICODETEXT);
  g_array_append_val (comp, fmt);

  fmt.target = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_UTF8_STRING);
  fmt.transmute = TRUE;
  g_array_append_val (comp, fmt);

  g_hash_table_replace (win32_selection->compatibility_targets, GINT_TO_POINTER (CF_UNICODETEXT), comp);


  comp = g_array_sized_new (FALSE, FALSE, sizeof (GdkSelTargetFormat), 3);
  fmt.format = _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_PNG);
  fmt.transmute = FALSE;

  fmt.target = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_PNG);
  g_array_append_val (comp, fmt);

  fmt.target = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_IMAGE_PNG);
  g_array_append_val (comp, fmt);

  g_hash_table_replace (win32_selection->compatibility_targets, GINT_TO_POINTER (_gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_PNG)), comp);


  comp = g_array_sized_new (FALSE, FALSE, sizeof (GdkSelTargetFormat), 4);
  fmt.format = _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_JFIF);
  fmt.transmute = FALSE;

  fmt.target = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_JFIF);
  g_array_append_val (comp, fmt);

  fmt.target = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_IMAGE_JPEG);
  g_array_append_val (comp, fmt);

  g_hash_table_replace (win32_selection->compatibility_targets, GINT_TO_POINTER (_gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_JFIF)), comp);


  comp = g_array_sized_new (FALSE, FALSE, sizeof (GdkSelTargetFormat), 4);
  fmt.format = _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_GIF);
  fmt.transmute = FALSE;

  fmt.target = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_GIF);
  g_array_append_val (comp, fmt);

  fmt.target = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_IMAGE_GIF);
  g_array_append_val (comp, fmt);

  g_hash_table_replace (win32_selection->compatibility_targets, GINT_TO_POINTER (_gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_GIF)), comp);


  comp = g_array_sized_new (FALSE, FALSE, sizeof (GdkSelTargetFormat), 3);
  fmt.format = CF_DIB;
  fmt.transmute = FALSE;

  fmt.target = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_CF_DIB);
  g_array_append_val (comp, fmt);

  fmt.target = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_IMAGE_BMP);
  fmt.transmute = TRUE;
  g_array_append_val (comp, fmt);

  g_hash_table_replace (win32_selection->compatibility_targets, GINT_TO_POINTER (CF_DIB), comp);


  comp = g_array_sized_new (FALSE, FALSE, sizeof (GdkSelTargetFormat), 3);
  fmt.format = _gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_CFSTR_SHELLIDLIST);
  fmt.transmute = FALSE;

  fmt.target = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_CFSTR_SHELLIDLIST);
  g_array_append_val (comp, fmt);

  fmt.target = _gdk_atom_array_index (atoms, GDK_WIN32_ATOM_INDEX_TEXT_URI_LIST);
  fmt.transmute = TRUE;
  g_array_append_val (comp, fmt);

  g_hash_table_replace (win32_selection->compatibility_targets, GINT_TO_POINTER (_gdk_cf_array_index (cfs, GDK_WIN32_CF_INDEX_CFSTR_SHELLIDLIST)), comp);
}

/* The specifications for COMPOUND_TEXT and STRING specify that C0 and
 * C1 are not allowed except for \n and \t, however the X conversions
 * routines for COMPOUND_TEXT only enforce this in one direction,
 * causing cut-and-paste of \r and \r\n separated text to fail.
 * This routine strips out all non-allowed C0 and C1 characters
 * from the input string and also canonicalizes \r, and \r\n to \n
 */
static gchar *
sanitize_utf8 (const gchar *src,
	       gint         length)
{
  GString *result = g_string_sized_new (length + 1);
  const gchar *p = src;
  const gchar *endp = src + length;

  while (p < endp)
    {
      if (*p == '\r')
	{
	  p++;
	  if (*p == '\n')
	    p++;

	  g_string_append_c (result, '\n');
	}
      else
	{
	  gunichar ch = g_utf8_get_char (p);
	  char buf[7];
	  gint buflen;

	  if (!((ch < 0x20 && ch != '\t' && ch != '\n') || (ch >= 0x7f && ch < 0xa0)))
	    {
	      buflen = g_unichar_to_utf8 (ch, buf);
	      g_string_append_len (result, buf, buflen);
	    }

	  p = g_utf8_next_char (p);
	}
    }
  g_string_append_c (result, '\0');

  return g_string_free (result, FALSE);
}

static gchar *
_gdk_utf8_to_string_target_internal (const gchar *str,
				     gint         length)
{
  GError *error = NULL;

  gchar *tmp_str = sanitize_utf8 (str, length);
  gchar *result =  g_convert_with_fallback (tmp_str, -1,
					    "ISO-8859-1", "UTF-8",
					    NULL, NULL, NULL, &error);
  if (!result)
    {
      g_warning ("Error converting from UTF-8 to STRING: %s",
		 error->message);
      g_error_free (error);
    }

  g_free (tmp_str);
  return result;
}

static void
selection_property_store (GdkWindow *owner,
			  GdkAtom    type,
			  gint       format,
			  guchar    *data,
			  gint       length)
{
  GdkSelProp *prop;
  GdkWin32Selection *win32_sel = _gdk_win32_selection_get ();

  prop = g_hash_table_lookup (win32_sel->sel_prop_table, GDK_WINDOW_HWND (owner));

  if (prop != NULL)
    {
      g_free (prop->data);
      g_free (prop);
      g_hash_table_remove (win32_sel->sel_prop_table, GDK_WINDOW_HWND (owner));
    }

  prop = g_new (GdkSelProp, 1);

  prop->data = data;
  prop->length = length;
  prop->bitness = format;
  prop->target = type;

  g_hash_table_insert (win32_sel->sel_prop_table, GDK_WINDOW_HWND (owner), prop);
}

void
_gdk_dropfiles_store (gchar *data)
{
  GdkWin32Selection *win32_sel = _gdk_win32_selection_get ();

  if (data != NULL)
    {
      g_assert (win32_sel->dropfiles_prop == NULL);

      win32_sel->dropfiles_prop = g_new (GdkSelProp, 1);
      win32_sel->dropfiles_prop->data = (guchar *) data;
      win32_sel->dropfiles_prop->length = strlen (data) + 1;
      win32_sel->dropfiles_prop->bitness = 8;
      win32_sel->dropfiles_prop->target = _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_TEXT_URI_LIST);
    }
  else
    {
      if (win32_sel->dropfiles_prop != NULL)
	{
	  g_free (win32_sel->dropfiles_prop->data);
	  g_free (win32_sel->dropfiles_prop);
	}
      win32_sel->dropfiles_prop = NULL;
    }
}

static void
generate_selection_notify (GdkWindow *requestor,
			   GdkAtom    selection,
			   GdkAtom    target,
			   GdkAtom    property,
			   guint32    time)
{
  GdkEvent tmp_event;

  memset (&tmp_event, 0, sizeof (tmp_event));
  tmp_event.selection.type = GDK_SELECTION_NOTIFY;
  tmp_event.selection.window = requestor;
  tmp_event.selection.send_event = FALSE;
  tmp_event.selection.selection = selection;
  tmp_event.selection.target = target;
  tmp_event.selection.property = property;
  tmp_event.selection.requestor = 0;
  tmp_event.selection.time = time;

  gdk_event_put (&tmp_event);
}

void
_gdk_win32_clear_clipboard_queue ()
{
  GList *tmp_list, *next;
  GdkWin32ClipboardQueueInfo *info;
  GdkWin32Selection *win32_sel = _gdk_win32_selection_get ();

  GDK_NOTE (DND, g_print ("Clear clipboard queue\n"));

  for (tmp_list = clipboard_queue; tmp_list; tmp_list = next)
    {
      info = (GdkWin32ClipboardQueueInfo *) tmp_list->data;
      next = g_list_next (tmp_list);
      clipboard_queue = g_list_remove_link (clipboard_queue, tmp_list);
      g_list_free (tmp_list);
      switch (info->action)
        {
        case GDK_WIN32_CLIPBOARD_QUEUE_ACTION_TARGETS:
          break;
        case GDK_WIN32_CLIPBOARD_QUEUE_ACTION_CONVERT:
          generate_selection_notify (info->requestor, info->selection, info->target, GDK_NONE, info->time);
          break;
        }
      g_clear_object (&info->requestor);
      g_slice_free (GdkWin32ClipboardQueueInfo, info);
    }

  win32_sel->targets_request_pending = FALSE;
}

/* Send ourselves a selection request message with
 * the TARGETS target, we will do multiple SetClipboarData(...,NULL)
 * calls in response to announce the formats we support.
 */
static void
send_targets_request (guint time)
{
  GdkWindow *owner;
  GdkEvent tmp_event;
  GdkWin32Selection *win32_sel = _gdk_win32_selection_get ();

  if (win32_sel->targets_request_pending)
    return;

  owner = _gdk_win32_display_get_selection_owner (gdk_display_get_default (),
                                                  GDK_SELECTION_CLIPBOARD);

  if (owner == NULL)
    return;

  if (win32_sel->clipboard_opened_for == INVALID_HANDLE_VALUE)
    {
      if (OpenClipboard (GDK_WINDOW_HWND (owner)))
        {
          win32_sel->clipboard_opened_for = GDK_WINDOW_HWND (owner);
          GDK_NOTE (DND, g_print ("Opened clipboard for 0x%p @ %s:%d\n", win32_sel->clipboard_opened_for, __FILE__, __LINE__));
        }
    }

  GDK_NOTE (DND, g_print ("... sending GDK_SELECTION_REQUEST to ourselves\n"));
  memset (&tmp_event, 0, sizeof (tmp_event));
  tmp_event.selection.type = GDK_SELECTION_REQUEST;
  tmp_event.selection.window = owner;
  tmp_event.selection.send_event = FALSE;
  tmp_event.selection.selection = GDK_SELECTION_CLIPBOARD;
  tmp_event.selection.target = _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_TARGETS);
  tmp_event.selection.property = _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_GDK_SELECTION);
  tmp_event.selection.requestor = owner;
  tmp_event.selection.time = time;
  win32_sel->property_change_target_atom = _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_TARGETS);

  gdk_event_put (&tmp_event);
  win32_sel->targets_request_pending = TRUE;
}

#define CLIPBOARD_IDLE_ABORT_TIME 30

static const gchar *
predefined_name (UINT fmt)
{
#define CASE(fmt) case fmt: return #fmt
  switch (fmt)
    {
    CASE (CF_TEXT);
    CASE (CF_BITMAP);
    CASE (CF_METAFILEPICT);
    CASE (CF_SYLK);
    CASE (CF_DIF);
    CASE (CF_TIFF);
    CASE (CF_OEMTEXT);
    CASE (CF_DIB);
    CASE (CF_PALETTE);
    CASE (CF_PENDATA);
    CASE (CF_RIFF);
    CASE (CF_WAVE);
    CASE (CF_UNICODETEXT);
    CASE (CF_ENHMETAFILE);
    CASE (CF_HDROP);
    CASE (CF_LOCALE);
    CASE (CF_DIBV5);
    CASE (CF_MAX);

    CASE (CF_OWNERDISPLAY);
    CASE (CF_DSPTEXT);
    CASE (CF_DSPBITMAP);
    CASE (CF_DSPMETAFILEPICT);
    CASE (CF_DSPENHMETAFILE);
#undef CASE
    default:
      return NULL;
    }
}

gchar *
_gdk_win32_get_clipboard_format_name (UINT      fmt,
                                      gboolean *is_predefined)
{
  gint registered_name_w_len = 1024;
  wchar_t *registered_name_w = g_malloc (registered_name_w_len);
  gchar *registered_name = NULL;
  int gcfn_result;
  const gchar *predef = predefined_name (fmt);

  /* FIXME: cache the result in a hash table */

  do
    {
      gcfn_result = GetClipboardFormatNameW (fmt, registered_name_w, registered_name_w_len);

      if (gcfn_result > 0 && gcfn_result < registered_name_w_len)
        {
          registered_name = g_utf16_to_utf8 (registered_name_w, -1, NULL, NULL, NULL);
          g_clear_pointer (&registered_name_w, g_free);
          if (!registered_name)
            gcfn_result = 0;
          else
            *is_predefined = FALSE;
          break;
        }

      /* If GetClipboardFormatNameW() used up all the space, it means that
       * we probably need a bigger buffer, but cap this at 1 kilobyte.
       */
      if (gcfn_result == 0 || registered_name_w_len > 1024 * 1024)
        {
          gcfn_result = 0;
          g_clear_pointer (&registered_name_w, g_free);
          break;
        }

      registered_name_w_len *= 2;
      registered_name_w = g_realloc (registered_name_w, registered_name_w_len);
      gcfn_result = registered_name_w_len;
    } while (gcfn_result == registered_name_w_len);

  if (registered_name == NULL &&
      predef != NULL)
    {
      registered_name = g_strdup (predef);
      *is_predefined = TRUE;
    }

  return registered_name;
}

static GArray *
get_compatibility_formats_for_target (GdkAtom target)
{
  GArray *result = NULL;
  gint i;
  GdkWin32Selection *win32_sel = _gdk_win32_selection_get ();

  result = g_hash_table_lookup (win32_sel->compatibility_formats, target);

  if (result != NULL)
    return result;

  for (i = 0; i < win32_sel->n_known_pixbuf_formats; i++)
    {
      if (target != win32_sel->known_pixbuf_formats[i])
        continue;

      /* Any format known to gdk-pixbuf can be presented as PNG or BMP */
      result = g_hash_table_lookup (win32_sel->compatibility_formats,
                                    _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_IMAGE_PNG));
      break;
    }

  return result;
}

static GArray *
_gdk_win32_selection_get_compatibility_targets_for_format (UINT format)
{
  GArray *result = NULL;
  GdkWin32Selection *win32_sel = _gdk_win32_selection_get ();

  result = g_hash_table_lookup (win32_sel->compatibility_targets, GINT_TO_POINTER (format));

  if (result != NULL)
    return result;

  /* TODO: reverse gdk-pixbuf conversion? We have to somehow
   * match gdk-pixbuf format names to the corresponding clipboard
   * format names. The former are known only at runtime,
   * the latter are presently unknown...
   * Maybe try to get the data and then just feed it to gdk-pixbuf,
   * see if it knows what it is?
   */

  return result;
}

void
_gdk_win32_add_format_to_targets (UINT     format,
                                  GArray  *array,
                                  GList  **list)
{
  gboolean predef;
  gchar *format_name = _gdk_win32_get_clipboard_format_name (format, &predef);
  GdkAtom target_atom;
  GdkSelTargetFormat target_selformat;
  GArray *target_selformats;
  gint i,j;

  if (format_name != NULL)
    {
      target_atom = gdk_atom_intern (format_name, FALSE);
      GDK_NOTE (DND, g_print ("Maybe add as-is format %s (0x%p)\n", format_name, target_atom));
      g_free (format_name);
      if (array && target_atom != 0)
        {
          for (j = 0; j < array->len; j++)
            if (g_array_index (array, GdkSelTargetFormat, j).target == target_atom)
              break;
          if (j == array->len)
            {
              target_selformat.format = format;
              target_selformat.target = target_atom;
              target_selformat.transmute = FALSE;
              g_array_append_val (array, target_selformat);
            }
        }
      if (list && target_atom != 0 && g_list_find (*list, target_atom) == NULL)
        *list = g_list_prepend (*list, target_atom);
    }

 target_selformats = _gdk_win32_selection_get_compatibility_targets_for_format (format);

 if (array && target_selformats != NULL)
   for (i = 0; i < target_selformats->len; i++)
     {
       target_selformat = g_array_index (target_selformats, GdkSelTargetFormat, i);

       for (j = 0; j < array->len; j++)
         if (g_array_index (array, GdkSelTargetFormat, j).target == target_selformat.target &&
             g_array_index (array, GdkSelTargetFormat, j).format == target_selformat.format)
           break;

       if (j == array->len)
         g_array_append_val (array, target_selformat);
     }

 if (list && target_selformats != NULL)
   for (i = 0; i < target_selformats->len; i++)
     {
       target_selformat = g_array_index (target_selformats, GdkSelTargetFormat, i);

       if (g_list_find (*list, target_selformat.target) == NULL)
         *list = g_list_prepend (*list, target_selformat.target);
     }
}

static void
transmute_cf_unicodetext_to_utf8_string (const guchar    *data,
                                         gint             length,
                                         guchar         **set_data,
                                         gint            *set_data_length,
                                         GDestroyNotify  *set_data_destroy)
{
  wchar_t *ptr, *p, *q, *endp;
  gchar *result;
  glong wclen, u8_len;

  /* Replace CR and CR+LF with LF */
  ptr = (wchar_t *) data;
  p = ptr;
  q = ptr;
  endp = ptr + length / 2;
  wclen = 0;

  while (p < endp)
    {
      if (*p != L'\r')
        {
          *q++ = *p;
          wclen++;
        }
      else if (p + 1 >= endp || *(p + 1) != L'\n')
        {
          *q++ = L'\n';
          wclen++;
        }

      p++;
    }

  result = g_utf16_to_utf8 (ptr, wclen, NULL, &u8_len, NULL);

  if (result)
    {
      *set_data = (guchar *) result;

      if (set_data_length)
        *set_data_length = u8_len + 1;
      if (set_data_destroy)
        *set_data_destroy = (GDestroyNotify) g_free;
    }
}

static void
transmute_utf8_string_to_cf_unicodetext (const guchar    *data,
                                         gint             length,
                                         guchar         **set_data,
                                         gint            *set_data_length,
                                         GDestroyNotify  *set_data_destroy)
{
  glong wclen;
  GError *err = NULL;
  glong size;
  gint i;
  wchar_t *wcptr, *p;

  wcptr = g_utf8_to_utf16 ((char *) data, length, NULL, &wclen, &err);
  if (err != NULL)
    {
      g_warning ("Failed to convert utf8: %s", err->message);
      g_clear_error (&err);
      return;
    }

  wclen++; /* Terminating 0 */
  size = wclen * 2;
  for (i = 0; i < wclen; i++)
    if (wcptr[i] == L'\n' && (i == 0 || wcptr[i - 1] != L'\r'))
      size += 2;

  *set_data = g_malloc0 (size);
  if (set_data_length)
    *set_data_length = size;
  if (set_data_destroy)
    *set_data_destroy = (GDestroyNotify) g_free;

  p = (wchar_t *) *set_data;

  for (i = 0; i < wclen; i++)
    {
      if (wcptr[i] == L'\n' && (i == 0 || wcptr[i - 1] != L'\r'))
	*p++ = L'\r';
      *p++ = wcptr[i];
    }

  g_free (wcptr);
}

static int
wchar_to_str (const wchar_t  *wstr,
              char         **retstr,
              UINT            cp)
{
  char *str;
  int   len;
  int   lenc;

  len = WideCharToMultiByte (cp, 0, wstr, -1, NULL, 0, NULL, NULL);

  if (len <= 0)
    return -1;

  str = g_malloc (sizeof (char) * len);

  lenc = WideCharToMultiByte (cp, 0, wstr, -1, str, len, NULL, NULL);

  if (lenc != len)
    {
      g_free (str);

      return -3;
    }

  *retstr = str;

  return 0;
}

static void
transmute_utf8_string_to_cf_text (const guchar    *data,
                                  gint             length,
                                  guchar         **set_data,
                                  gint            *set_data_length,
                                  GDestroyNotify  *set_data_destroy)
{
  glong rlen;
  GError *err = NULL;
  glong size;
  gint i;
  char *strptr, *p;
  wchar_t *wcptr;

  wcptr = g_utf8_to_utf16 ((char *) data, length, NULL, NULL, &err);
  if (err != NULL)
    {
      g_warning ("Failed to convert utf8: %s", err->message);
      g_clear_error (&err);
      return;
    }

  if (wchar_to_str (wcptr, &strptr, CP_ACP) != 0)
    {
      g_warning ("Failed to convert utf-16 %S to ACP", wcptr);
      g_free (wcptr);
      return;
    }

  rlen = strlen (strptr);
  g_free (wcptr);

  rlen++; /* Terminating 0 */
  size = rlen * sizeof (char);
  for (i = 0; i < rlen; i++)
    if (strptr[i] == '\n' && (i == 0 || strptr[i - 1] != '\r'))
      size += sizeof (char);

  *set_data = g_malloc0 (size);
  if (set_data_length)
    *set_data_length = size;
  if (set_data_destroy)
    *set_data_destroy = (GDestroyNotify) g_free;

  p = (char *) *set_data;

  for (i = 0; i < rlen; i++)
    {
      if (strptr[i] == '\n' && (i == 0 || strptr[i - 1] != '\r'))
	*p++ = '\r';
      *p++ = strptr[i];
    }

  g_free (strptr);
}

static int
str_to_wchar (const char  *str,
              wchar_t    **wretstr,
              UINT         cp)
{
  wchar_t *wstr;
  int      len;
  int      lenc;

  len = MultiByteToWideChar (cp, 0, str, -1, NULL, 0);

  if (len <= 0)
    return -1;

  wstr = g_malloc (sizeof (wchar_t) * len);

  lenc = MultiByteToWideChar (cp, 0, str, -1, wstr, len);

  if (lenc != len)
    {
      g_free (wstr);

      return -3;
    }

  *wretstr = wstr;

  return 0;
}

static void
transmute_cf_text_to_utf8_string (const guchar    *data,
                                  gint             length,
                                  guchar         **set_data,
                                  gint            *set_data_length,
                                  GDestroyNotify  *set_data_destroy)
{
  char *ptr, *p, *q, *endp;
  gchar *result;
  glong wclen, u8_len;
  wchar_t *wstr;

  /* Replace CR and CR+LF with LF */
  ptr = (gchar *) data;
  p = ptr;
  q = ptr;
  endp = ptr + length / 2;
  wclen = 0;

  while (p < endp)
    {
      if (*p != '\r')
        {
          *q++ = *p;
          wclen++;
        }
      else if (p + 1 > endp || *(p + 1) != '\n')
        {
          *q++ = '\n';
          wclen++;
        }

      p++;
    }

  if (str_to_wchar (ptr, &wstr, CP_ACP) < 0)
    return;

  result = g_utf16_to_utf8 (wstr, -1, NULL, &u8_len, NULL);

  if (result)
    {
      *set_data = (guchar *) result;

      if (set_data_length)
        *set_data_length = u8_len + 1;
      if (set_data_destroy)
        *set_data_destroy = (GDestroyNotify) g_free;
    }

  g_free (wstr);
}

static void
transmute_cf_dib_to_image_bmp (const guchar    *data,
                               gint             length,
                               guchar         **set_data,
                               gint            *set_data_length,
                               GDestroyNotify  *set_data_destroy)
{
  /* Need to add a BMP file header so gdk-pixbuf can load
   * it.
   *
   * If the data is from Mozilla Firefox or IE7, and
   * starts with an "old fashioned" BITMAPINFOHEADER,
   * i.e. with biSize==40, and biCompression == BI_RGB and
   * biBitCount==32, we assume that the "extra" byte in
   * each pixel in fact is alpha.
   *
   * The gdk-pixbuf bmp loader doesn't trust 32-bit BI_RGB
   * bitmaps to in fact have alpha, so we have to convince
   * it by changing the bitmap header to a version 5
   * BI_BITFIELDS one with explicit alpha mask indicated.
   *
   * The RGB bytes that are in bitmaps on the clipboard
   * originating from Firefox or IE7 seem to be
   * premultiplied with alpha. The gdk-pixbuf bmp loader
   * of course doesn't expect that, so we have to undo the
   * premultiplication before feeding the bitmap to the
   * bmp loader.
   *
   * Note that for some reason the bmp loader used to want
   * the alpha bytes in its input to actually be
   * 255-alpha, but here we assume that this has been
   * fixed before this is committed.
   */
  BITMAPINFOHEADER *bi = (BITMAPINFOHEADER *) data;
  BITMAPFILEHEADER *bf;
  gpointer result;
  gint data_length = length;
  gint new_length;
  gboolean make_dibv5 = FALSE;
  BITMAPV5HEADER *bV5;
  guchar *p;
  guint i;

  if (bi->biSize == sizeof (BITMAPINFOHEADER) &&
      bi->biPlanes == 1 &&
      bi->biBitCount == 32 &&
      bi->biCompression == BI_RGB &&
#if 0
      /* Maybe check explicitly for Mozilla or IE7?
       *
       * If the clipboard format
       * application/x-moz-nativeimage is present, that is
       * a reliable indicator that the data is offered by
       * Mozilla one would think. For IE7,
       * UniformResourceLocatorW is presumably not that
       * uniqie, so probably need to do some
       * GetClipboardOwner(), GetWindowThreadProcessId(),
       * OpenProcess(), GetModuleFileNameEx() dance to
       * check?
       */
      (IsClipboardFormatAvailable
       (RegisterClipboardFormatA ("application/x-moz-nativeimage")) ||
       IsClipboardFormatAvailable
       (RegisterClipboardFormatA ("UniformResourceLocatorW"))) &&
#endif
      TRUE)
    {
      /* We turn the BITMAPINFOHEADER into a
       * BITMAPV5HEADER before feeding it to gdk-pixbuf.
       */
      new_length = (data_length +
		    sizeof (BITMAPFILEHEADER) +
		    (sizeof (BITMAPV5HEADER) - sizeof (BITMAPINFOHEADER)));
      make_dibv5 = TRUE;
    }
  else
    {
      new_length = data_length + sizeof (BITMAPFILEHEADER);
    }

  result = g_try_malloc (new_length);

  if (result == NULL)
    return;

  bf = (BITMAPFILEHEADER *) result;
  bf->bfType = 0x4d42; /* "BM" */
  bf->bfSize = new_length;
  bf->bfReserved1 = 0;
  bf->bfReserved2 = 0;

  *set_data = result;

  if (set_data_length)
    *set_data_length = new_length;
  if (set_data_destroy)
    *set_data_destroy = g_free;

  if (!make_dibv5)
    {
      bf->bfOffBits = (sizeof (BITMAPFILEHEADER) +
		       bi->biSize +
		       bi->biClrUsed * sizeof (RGBQUAD));

      if (bi->biCompression == BI_BITFIELDS && bi->biBitCount >= 16)
        {
          /* Screenshots taken with PrintScreen or
           * Alt + PrintScreen are found on the clipboard in
           * this format. In this case the BITMAPINFOHEADER is
           * followed by three DWORD specifying the masks of the
           * red green and blue components, so adjust the offset
           * accordingly. */
          bf->bfOffBits += (3 * sizeof (DWORD));
        }

      memcpy ((char *) result + sizeof (BITMAPFILEHEADER),
	      bi,
	      data_length);

      return;
    }

  bV5 = (BITMAPV5HEADER *) ((char *) result + sizeof (BITMAPFILEHEADER));

  bV5->bV5Size = sizeof (BITMAPV5HEADER);
  bV5->bV5Width = bi->biWidth;
  bV5->bV5Height = bi->biHeight;
  bV5->bV5Planes = 1;
  bV5->bV5BitCount = 32;
  bV5->bV5Compression = BI_BITFIELDS;
  bV5->bV5SizeImage = 4 * bV5->bV5Width * ABS (bV5->bV5Height);
  bV5->bV5XPelsPerMeter = bi->biXPelsPerMeter;
  bV5->bV5YPelsPerMeter = bi->biYPelsPerMeter;
  bV5->bV5ClrUsed = 0;
  bV5->bV5ClrImportant = 0;
  /* Now the added mask fields */
  bV5->bV5RedMask   = 0x00ff0000;
  bV5->bV5GreenMask = 0x0000ff00;
  bV5->bV5BlueMask  = 0x000000ff;
  bV5->bV5AlphaMask = 0xff000000;
  ((char *) &bV5->bV5CSType)[3] = 's';
  ((char *) &bV5->bV5CSType)[2] = 'R';
  ((char *) &bV5->bV5CSType)[1] = 'G';
  ((char *) &bV5->bV5CSType)[0] = 'B';
  /* Ignore colorspace and profile fields */
  bV5->bV5Intent = LCS_GM_GRAPHICS;
  bV5->bV5Reserved = 0;

  bf->bfOffBits = (sizeof (BITMAPFILEHEADER) +
		   bV5->bV5Size);

  p = ((guchar *) result) + sizeof (BITMAPFILEHEADER) + sizeof (BITMAPV5HEADER);
  memcpy (p, ((char *) bi) + bi->biSize,
          data_length - sizeof (BITMAPINFOHEADER));

  for (i = 0; i < bV5->bV5SizeImage/4; i++)
    {
      if (p[3] != 0)
        {
          gdouble inverse_alpha = 255./p[3];

          p[0] = p[0] * inverse_alpha + 0.5;
          p[1] = p[1] * inverse_alpha + 0.5;
          p[2] = p[2] * inverse_alpha + 0.5;
        }

      p += 4;
    }
}

static void
transmute_cf_shell_id_list_to_text_uri_list (const guchar    *data,
                                             gint             length,
                                             guchar         **set_data,
                                             gint            *set_data_length,
                                             GDestroyNotify  *set_data_destroy)
{
  guint i;
  CIDA *cida = (CIDA *) data;
  guint number_of_ids = cida->cidl;
  GString *result = g_string_new (NULL);
  PCIDLIST_ABSOLUTE folder_id = HIDA_GetPIDLFolder (cida);
  wchar_t path_w[MAX_PATH + 1];

  for (i = 0; i < number_of_ids; i++)
    {
      PCUIDLIST_RELATIVE file_id = HIDA_GetPIDLItem (cida, i);
      PIDLIST_ABSOLUTE file_id_full = ILCombine (folder_id, file_id);

      if (SHGetPathFromIDListW (file_id_full, path_w))
        {
          gchar *filename = g_utf16_to_utf8 (path_w, -1, NULL, NULL, NULL);

          if (filename)
            {
              gchar *uri = g_filename_to_uri (filename, NULL, NULL);

              if (uri)
                {
                  g_string_append (result, uri);
                  g_string_append (result, "\r\n");
                  g_free (uri);
                }

              g_free (filename);
            }
        }

      ILFree (file_id_full);
    }

  *set_data = (guchar *) result->str;
  if (set_data_length)
    *set_data_length = result->len;
  if (set_data_destroy)
    *set_data_destroy = g_free;

  g_string_free (result, FALSE);
}

void
transmute_image_bmp_to_cf_dib (const guchar    *data,
                               gint             length,
                               guchar         **set_data,
                               gint            *set_data_length,
                               GDestroyNotify  *set_data_destroy)
{
  gint size;
  guchar *ptr;

  g_return_if_fail (length >= sizeof (BITMAPFILEHEADER));

  /* No conversion is needed, just strip the BITMAPFILEHEADER */
  size = length - sizeof (BITMAPFILEHEADER);
  ptr = g_malloc (size);

  memcpy (ptr, data + sizeof (BITMAPFILEHEADER), size);

  *set_data = ptr;

  if (set_data_length)
    *set_data_length = size;
  if (set_data_destroy)
    *set_data_destroy = g_free;
}

static void
transmute_selection_format (UINT          from_format,
                            GdkAtom       to_target,
                            const guchar *data,
                            gint          length,
                            guchar      **set_data,
                            gint         *set_data_length)
{
  if ((to_target == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_IMAGE_PNG) &&
       from_format == _gdk_win32_selection_cf (GDK_WIN32_CF_INDEX_PNG)) ||
      (to_target == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_IMAGE_JPEG) &&
       from_format == _gdk_win32_selection_cf (GDK_WIN32_CF_INDEX_JFIF)) ||
      (to_target == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_GIF) &&
       from_format == _gdk_win32_selection_cf (GDK_WIN32_CF_INDEX_GIF)))
    {
      /* No transmutation needed */
      *set_data = g_memdup (data, length);
      *set_data_length = length;
    }
  else if (to_target == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_UTF8_STRING) &&
           from_format == CF_UNICODETEXT)
    {
      transmute_cf_unicodetext_to_utf8_string (data, length, set_data, set_data_length, NULL);
    }
  else if (to_target == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_UTF8_STRING) &&
           from_format == CF_TEXT)
    {
      transmute_cf_text_to_utf8_string (data, length, set_data, set_data_length, NULL);
    }
  else if (to_target == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_IMAGE_BMP) &&
           (from_format == CF_DIB || from_format == CF_DIBV5))
    {
      transmute_cf_dib_to_image_bmp (data, length, set_data, set_data_length, NULL);
    }
  else if (to_target == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_TEXT_URI_LIST) &&
           from_format == _gdk_win32_selection_cf (GDK_WIN32_CF_INDEX_CFSTR_SHELLIDLIST))
    {
      transmute_cf_shell_id_list_to_text_uri_list (data, length, set_data, set_data_length, NULL);
    }
  else
    {
      g_warning ("Don't know how to transmute format 0x%x to target 0x%p", from_format, to_target);
    }
}

void
transmute_selection_target (GdkAtom       from_target,
                            UINT          to_format,
                            const guchar *data,
                            gint          length,
                            guchar      **set_data,
                            gint         *set_data_length)
{
  if ((from_target == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_IMAGE_PNG) &&
       to_format == _gdk_win32_selection_cf (GDK_WIN32_CF_INDEX_PNG)) ||
      (from_target == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_IMAGE_JPEG) &&
       to_format == _gdk_win32_selection_cf (GDK_WIN32_CF_INDEX_JFIF)) ||
      (from_target == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_GIF) &&
       to_format == _gdk_win32_selection_cf (GDK_WIN32_CF_INDEX_GIF)))
    {
      /* No conversion needed */
      *set_data = g_memdup (data, length);
      *set_data_length = length;
    }
  else if (from_target == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_UTF8_STRING) &&
           to_format == CF_UNICODETEXT)
    {
      transmute_utf8_string_to_cf_unicodetext (data, length, set_data, set_data_length, NULL);
    }
  else if (from_target == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_UTF8_STRING) &&
           to_format == CF_TEXT)
    {
      transmute_utf8_string_to_cf_text (data, length, set_data, set_data_length, NULL);
    }
  else if (from_target == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_IMAGE_BMP) &&
           to_format == CF_DIB)
    {
      transmute_image_bmp_to_cf_dib (data, length, set_data, set_data_length, NULL);
    }
  else if (from_target == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_IMAGE_BMP) &&
           to_format == CF_DIBV5)
    {
      transmute_image_bmp_to_cf_dib (data, length, set_data, set_data_length, NULL);
    }
/*
  else if (from_target == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_TEXT_URI_LIST) &&
           to_format == _gdk_win32_selection_cf (GDK_WIN32_CF_INDEX_CFSTR_SHELLIDLIST))
    {
      transmute_text_uri_list_to_shell_id_list (data, length, set_data, set_data_length, NULL);
    }
*/
  else
    {
      g_warning ("Don't know how to transmute from target 0x%p to format 0x%x", from_target, to_format);
    }
}

static GdkAtom
convert_clipboard_selection_to_targets_target (GdkWindow *requestor)
{
  gint fmt;
  int i;
  int format_count = CountClipboardFormats ();
  GArray *targets = g_array_sized_new (FALSE, FALSE, sizeof (GdkSelTargetFormat), format_count);

  for (fmt = 0; 0 != (fmt = EnumClipboardFormats (fmt)); )
    _gdk_win32_add_format_to_targets (fmt, targets, NULL);

  GDK_NOTE (DND, {
      g_print ("... ");
      for (i = 0; i < targets->len; i++)
        {
          gchar *atom_name = gdk_atom_name (g_array_index (targets, GdkSelTargetFormat, i).target);

          g_print ("%s", atom_name);
          g_free (atom_name);
          if (i < targets->len - 1)
            g_print (", ");
        }
      g_print ("\n");
    });

  if (targets->len > 0)
    {
      gint len = targets->len;
      GdkAtom *targets_only = g_new0 (GdkAtom, len);

      for (i = 0; i < targets->len; i++)
        targets_only[i] = g_array_index (targets, GdkSelTargetFormat, i).target;

      g_array_free (targets, TRUE);
      selection_property_store (requestor, GDK_SELECTION_TYPE_ATOM,
                                32, (guchar *) targets_only,
                                len * sizeof (GdkAtom));
      return _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_GDK_SELECTION);
    }
  else
    {
      g_array_free (targets, TRUE);
      return GDK_NONE;
    }
}

static GdkAtom
convert_clipboard_selection_to_target (GdkWindow *requestor,
                                       GdkAtom    target)
{
  UINT format;
  HANDLE hdata;
  guchar *ptr;
  gint length;
  gboolean transmute = FALSE;
  GdkAtom result = _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_GDK_SELECTION);
  gboolean found;
  gchar *atom_name;

  atom_name = gdk_atom_name (target);

  for (format = 0, found = FALSE;
       !found && 0 != (format = EnumClipboardFormats (format));
      )
    {
      gboolean predef;
      gchar *format_name = _gdk_win32_get_clipboard_format_name (format, &predef);

      if (format_name == NULL)
        continue;

      found = g_strcmp0 (format_name, atom_name) == 0;
      g_free (format_name);
    }

  g_free (atom_name);

  if (format == 0)
    {
      gint i;
      GArray *compat_formats = get_compatibility_formats_for_target (target);

      for (i = 0; compat_formats != NULL && i < compat_formats->len; i++)
        {
          if (!IsClipboardFormatAvailable (g_array_index (compat_formats, GdkSelTargetFormat, i).format))
            continue;

          format = g_array_index (compat_formats, GdkSelTargetFormat, i).format;
          transmute = g_array_index (compat_formats, GdkSelTargetFormat, i).transmute;
          break;
        }
    }

  if (format == 0)
    return GDK_NONE;

  if ((hdata = GetClipboardData (format)) == NULL)
    return GDK_NONE;

  if ((ptr = GlobalLock (hdata)) != NULL)
    {
      guchar *data = NULL;
      gint data_len = 0;
      length = GlobalSize (hdata);

      GDK_NOTE (DND, g_print ("... format 0x%x: %d bytes\n", format, length));

      if (transmute)
        {
          transmute_selection_format (format, target, ptr, length, &data, &data_len);
        }
      else
        {
          data = g_memdup (ptr, length);
          data_len = length;
        }

      if (data)
        selection_property_store (requestor, target,
                                  8, data, data_len);
      else
        result = GDK_NONE;

      GlobalUnlock (hdata);
    }

  return result;
}

static GdkAtom
convert_selection_with_opened_clipboard (GdkDisplay *display,
                                          GdkWindow  *requestor,
                                          GdkAtom     target,
                                          guint32     time)
{
  GdkAtom property;

  if (target == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_TARGETS))
    property = convert_clipboard_selection_to_targets_target (requestor);
  else
    property = convert_clipboard_selection_to_target (requestor, target);

  return property;
}

static void
announce_delayrendered_targets_with_opened_clipboard (GdkWin32Selection *win32_sel)
{
  gint i;
  /* Announce the formats we support, but don't actually put any data out there.
   * Other processes will send us WM_RENDERFORMAT to get the data.
   */
  for (i = 0; i < win32_sel->clipboard_selection_targets->len; i++)
    {
      GdkSelTargetFormat *fmt = &g_array_index (win32_sel->clipboard_selection_targets, GdkSelTargetFormat, i);

      /* Some calls here may be duplicates, but we don't really care */
      if (fmt->format != 0)
        SetClipboardData (fmt->format, NULL);
    }
}

static gboolean
open_clipboard_timeout (gpointer data)
{
  GList *tmp_list, *next;
  GdkWin32ClipboardQueueInfo *info;
  GdkWin32Selection *win32_sel = _gdk_win32_selection_get ();

  GDK_NOTE (DND, g_print ("Open clipboard timeout ticks\n"));

  /* Clear out old and invalid entries */
  for (tmp_list = clipboard_queue; tmp_list; tmp_list = next)
    {
      info = (GdkWin32ClipboardQueueInfo *) tmp_list->data;
      next = g_list_next (tmp_list);

      if (GDK_WINDOW_DESTROYED (info->requestor) ||
          info->idle_time >= CLIPBOARD_IDLE_ABORT_TIME)
        {
          clipboard_queue = g_list_remove_link (clipboard_queue, tmp_list);
          g_list_free (tmp_list);
          switch (info->action)
            {
            case GDK_WIN32_CLIPBOARD_QUEUE_ACTION_TARGETS:
              break;
            case GDK_WIN32_CLIPBOARD_QUEUE_ACTION_CONVERT:
              generate_selection_notify (info->requestor, info->selection, info->target, GDK_NONE, info->time);
              break;
            }
          g_clear_object (&info->requestor);
          g_slice_free (GdkWin32ClipboardQueueInfo, info);
        }
    }

  if (clipboard_queue == NULL)
    {
      GDK_NOTE (DND, g_print ("Stopping open clipboard timer\n"));

      if (win32_sel->clipboard_opened_for != INVALID_HANDLE_VALUE)
        {
          API_CALL (CloseClipboard, ());
          win32_sel->clipboard_opened_for = INVALID_HANDLE_VALUE;
          GDK_NOTE (DND, g_print ("Closed clipboard @ %s:%d\n", __FILE__, __LINE__));
        }

      return FALSE;
    }

  for (tmp_list = clipboard_queue; tmp_list; tmp_list = next)
    {
      GdkAtom property;

      info = (GdkWin32ClipboardQueueInfo *) tmp_list->data;
      next = g_list_next (tmp_list);

      /* CONVERT works with any opened clipboard,
       * but TARGETS needs to open the clipboard with the hande of the
       * owner window.
       */
      if (info->action == GDK_WIN32_CLIPBOARD_QUEUE_ACTION_TARGETS &&
          win32_sel->clipboard_opened_for == NULL)
        {
          GDK_NOTE (DND, g_print ("Need to re-open clipboard, closing\n"));
          API_CALL (CloseClipboard, ());
          win32_sel->clipboard_opened_for = INVALID_HANDLE_VALUE;
        }

      if (win32_sel->clipboard_opened_for == INVALID_HANDLE_VALUE)
        {
          if (!OpenClipboard (GDK_WINDOW_HWND (info->requestor)))
            {
              info->idle_time += 1;
              continue;
            }
          win32_sel->clipboard_opened_for = GDK_WINDOW_HWND (info->requestor);
          GDK_NOTE (DND, g_print ("Opened clipboard for 0x%p @ %s:%d\n", win32_sel->clipboard_opened_for, __FILE__, __LINE__));
        }

      clipboard_queue = g_list_remove_link (clipboard_queue, tmp_list);
      g_list_free (tmp_list);

      switch (info->action)
        {
        case GDK_WIN32_CLIPBOARD_QUEUE_ACTION_CONVERT:
          property = convert_selection_with_opened_clipboard (info->display,
                                                              info->requestor,
                                                              info->target,
                                                              info->time);
          generate_selection_notify (info->requestor,
                                     GDK_SELECTION_CLIPBOARD,
                                     info->target,
                                     property,
                                     info->time);
          break;
        case GDK_WIN32_CLIPBOARD_QUEUE_ACTION_TARGETS:
          announce_delayrendered_targets_with_opened_clipboard (win32_sel);
          break;
        default:
          g_assert_not_reached ();
        }

      g_clear_object (&info->requestor);
      g_slice_free (GdkWin32ClipboardQueueInfo, info);
    }

  if (clipboard_queue != NULL)
    return TRUE;

  if (win32_sel->clipboard_opened_for != INVALID_HANDLE_VALUE)
    {
      API_CALL (CloseClipboard, ());
      win32_sel->clipboard_opened_for = INVALID_HANDLE_VALUE;
      GDK_NOTE (DND, g_print ("Closed clipboard @ %s:%d\n", __FILE__, __LINE__));
    }

  GDK_NOTE (DND, g_print ("Stopping open clipboard timer\n"));

  return FALSE;
}

static void
queue_open_clipboard (GdkWin32ClipboardQueueAction  action,
                      GdkDisplay                   *display,
                      GdkWindow                    *requestor,
                      GdkAtom                       target,
                      guint32                       time)
{
  guint id;
  GList *tmp_list, *next;
  GdkWin32ClipboardQueueInfo *info;
  GdkWin32Selection *win32_sel = _gdk_win32_selection_get ();

  for (tmp_list = clipboard_queue; tmp_list; tmp_list = next)
    {
      info = (GdkWin32ClipboardQueueInfo *) tmp_list->data;
      next = g_list_next (tmp_list);

      if (info->action == action &&
          info->requestor == requestor)
	return;
    }

  info = g_slice_new0 (GdkWin32ClipboardQueueInfo);

  info->display = display;
  info->requestor = g_object_ref (requestor);
  info->selection = GDK_SELECTION_CLIPBOARD;
  info->target = target;
  info->idle_time = 0;
  info->time = time;
  info->action = action;

  GDK_NOTE (DND, g_print ("Queueing open clipboard\n"));

  if (win32_sel->clipboard_opened_for == INVALID_HANDLE_VALUE &&
      clipboard_queue == NULL)
    {
      id = gdk_threads_add_timeout_seconds (1, (GSourceFunc) open_clipboard_timeout, NULL);
      g_source_set_name_by_id (id, "[gdk-win32] open_clipboard_timeout");
      GDK_NOTE (DND, g_print ("Started open clipboard timer\n"));
    }

  clipboard_queue = g_list_append (clipboard_queue, info);
}

gboolean
_gdk_win32_display_set_selection_owner (GdkDisplay *display,
					GdkWindow  *owner,
					GdkAtom     selection,
					guint32     time,
					gboolean    send_event)
{
  HWND hwnd;
  GdkWin32Selection *win32_sel = _gdk_win32_selection_get ();

  g_return_val_if_fail (selection != GDK_NONE, FALSE);

  GDK_NOTE (DND, {
      gchar *sel_name = gdk_atom_name (selection);

      g_print ("gdk_selection_owner_set_for_display: %p %s\n",
	       (owner ? GDK_WINDOW_HWND (owner) : NULL),
	       sel_name);
      g_free (sel_name);
    });

  if (selection != GDK_SELECTION_CLIPBOARD)
    {
      if (owner != NULL)
        g_hash_table_insert (win32_sel->sel_owner_table, selection, GDK_WINDOW_HWND (owner));
      else
        g_hash_table_remove (win32_sel->sel_owner_table, selection);

      return TRUE;
    }

  /* Rest of this function handles the CLIPBOARD selection */
  if (owner != NULL)
    {
      if (GDK_WINDOW_DESTROYED (owner))
	return FALSE;

      hwnd = GDK_WINDOW_HWND (owner);
    }
  else
    hwnd = NULL;

  if (win32_sel->clipboard_opened_for != hwnd &&
      win32_sel->clipboard_opened_for != INVALID_HANDLE_VALUE)
    {
      API_CALL (CloseClipboard, ());
      win32_sel->clipboard_opened_for = INVALID_HANDLE_VALUE;
      GDK_NOTE (DND, g_print ("Closed clipboard @ %s:%d\n", __FILE__, __LINE__));
    }

  if (!OpenClipboard (hwnd))
    {
      if (GetLastError () != ERROR_ACCESS_DENIED)
        WIN32_API_FAILED ("OpenClipboard");

      return FALSE;
    }

  win32_sel->clipboard_opened_for = hwnd;
  GDK_NOTE (DND, g_print ("Opened clipboard for 0x%p @ %s:%d\n", win32_sel->clipboard_opened_for, __FILE__, __LINE__));
  win32_sel->ignore_destroy_clipboard = TRUE;
  GDK_NOTE (DND, g_print ("... EmptyClipboard()\n"));
  if (!API_CALL (EmptyClipboard, ()))
    {
      win32_sel->ignore_destroy_clipboard = FALSE;
      API_CALL (CloseClipboard, ());
      win32_sel->clipboard_opened_for = INVALID_HANDLE_VALUE;
      GDK_NOTE (DND, g_print ("Closed clipboard @ %s:%d\n", __FILE__, __LINE__));
      return FALSE;
    }
  win32_sel->ignore_destroy_clipboard = FALSE;

  /* Any queued clipboard operations were just made pointless
   * by EmptyClipboard().
   */
  _gdk_win32_clear_clipboard_queue ();

  /* This is kind of risky, but we don't close the clipboard
   * to ensure that it's still open when GDK_SELECTION_REQUEST
   * is handled.
   */
  if (owner == NULL)
    {
      if (!API_CALL (CloseClipboard, ()))
        return FALSE;
      GDK_NOTE (DND, g_print ("Closed clipboard @ %s:%d\n", __FILE__, __LINE__));
      win32_sel->clipboard_opened_for = INVALID_HANDLE_VALUE;
    }

  send_targets_request (time);

  return TRUE;
}

GdkWindow*
_gdk_win32_display_get_selection_owner (GdkDisplay *display,
                                        GdkAtom     selection)
{
  GdkWindow *window;
  HWND selection_owner;
  GdkWin32Selection *win32_sel = _gdk_win32_selection_get ();

  g_return_val_if_fail (selection != GDK_NONE, NULL);

  if (selection == GDK_SELECTION_CLIPBOARD)
    selection_owner = GetClipboardOwner ();
  else
    selection_owner = g_hash_table_lookup (win32_sel->sel_owner_table, selection);

  if (selection_owner)
    window = gdk_win32_window_lookup_for_display (display,
                                                  selection_owner);
  else
    window = NULL;

  GDK_NOTE (DND, {
      gchar *sel_name = gdk_atom_name (selection);

      g_print ("gdk_selection_owner_get: %s = %p\n",
	       sel_name,
	       (window ? GDK_WINDOW_HWND (window) : NULL));
      g_free (sel_name);
    });

  return window;
}

static GdkAtom
convert_dnd_selection_to_target (GdkAtom    target,
                                 GdkWindow *requestor)
{
  GdkWin32Selection *win32_sel = _gdk_win32_selection_get ();
  UINT format;
  gint i, with_transmute;
  guchar *ptr;
  gint length;
  gboolean transmute = FALSE;
  GdkWin32DragContext *context_win32;
  FORMATETC fmt;
  STGMEDIUM storage;
  HRESULT hr;
  GdkAtom result = _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_OLE2_DND);

  g_assert (win32_sel->target_drag_context != NULL);
  g_assert (win32_sel->dnd_data_object_target != NULL);

  context_win32 = GDK_WIN32_DRAG_CONTEXT (win32_sel->target_drag_context);

  fmt.ptd = NULL;
  fmt.dwAspect = DVASPECT_CONTENT;
  fmt.lindex = -1;
  fmt.tymed = TYMED_HGLOBAL;

  /* We rely on GTK+ applications to synthesize the DELETE request
   * for themselves, since they do know whether a DnD operation was a
   * move and whether was successful. Therefore, we do not need to
   * actually send anything here. Just report back without storing
   * any data.
   */
  if (target == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_DELETE))
    return result;

  for (format = 0, with_transmute = 0; format == 0 && with_transmute < 2; with_transmute++)
    {
      for (i = 0;
           i < context_win32->droptarget_format_target_map->len;
           i++)
        {
          GdkSelTargetFormat selformat = g_array_index (context_win32->droptarget_format_target_map, GdkSelTargetFormat, i);

          if (selformat.target != target ||
              selformat.transmute != (with_transmute == 0 ? FALSE : TRUE))
            continue;

          fmt.cfFormat = selformat.format;

          hr = IDataObject_QueryGetData (win32_sel->dnd_data_object_target, &fmt);

          if (!SUCCEEDED (hr) || hr != S_OK)
            continue;

          format = selformat.format;
          transmute = selformat.transmute;
          break;
        }
    }

  if (format == 0)
    return GDK_NONE;

  hr = IDataObject_GetData (win32_sel->dnd_data_object_target, &fmt, &storage);

  if (!SUCCEEDED (hr) || hr != S_OK)
    return GDK_NONE;

  if ((ptr = GlobalLock (storage.hGlobal)) != NULL)
    {
      guchar *data = NULL;
      gint    data_len = 0;

      SetLastError (0);
      length = GlobalSize (storage.hGlobal);

      if (GetLastError () == NO_ERROR)
        {
          if (transmute)
            {
              transmute_selection_format (format, target, ptr, length, &data, &data_len);
            }
          else
            {
              data = g_memdup (ptr, length);
              data_len = length;
            }

          if (data)
            selection_property_store (requestor, target, 8,
                                      data, data_len);
          else
            result = GDK_NONE;
        }
      else
        result = GDK_NONE;

      GlobalUnlock (storage.hGlobal);
    }
  else
    result = GDK_NONE;

  ReleaseStgMedium (&storage);

  return result;
}

void
_gdk_win32_display_convert_selection (GdkDisplay *display,
				      GdkWindow *requestor,
				      GdkAtom    selection,
				      GdkAtom    target,
				      guint32    time)
{
  GdkWin32Selection *win32_sel = _gdk_win32_selection_get ();
  GdkAtom property = _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_GDK_SELECTION);

  g_return_if_fail (selection != GDK_NONE);
  g_return_if_fail (requestor != NULL);

  if (GDK_WINDOW_DESTROYED (requestor))
    return;

  GDK_NOTE (DND, {
      gchar *sel_name = gdk_atom_name (selection);
      gchar *tgt_name = gdk_atom_name (target);

      g_print ("gdk_selection_convert: %p %s %s\n",
	       GDK_WINDOW_HWND (requestor),
	       sel_name, tgt_name);
      g_free (sel_name);
      g_free (tgt_name);
    });

  if (selection == GDK_SELECTION_CLIPBOARD)
    {
      if (win32_sel->clipboard_opened_for != INVALID_HANDLE_VALUE ||
          OpenClipboard (GDK_WINDOW_HWND (requestor)))
        {
          if (win32_sel->clipboard_opened_for == INVALID_HANDLE_VALUE)
            {
              win32_sel->clipboard_opened_for = GDK_WINDOW_HWND (requestor);
              GDK_NOTE (DND, g_print ("Opened clipboard for 0x%p @ %s:%d\n", win32_sel->clipboard_opened_for, __FILE__, __LINE__));
            }

          queue_open_clipboard (GDK_WIN32_CLIPBOARD_QUEUE_ACTION_CONVERT, display, requestor, target, time);
          open_clipboard_timeout (NULL);
          return;
        }
      else
        {
          queue_open_clipboard (GDK_WIN32_CLIPBOARD_QUEUE_ACTION_CONVERT, display, requestor, target, time);
          /* Do not generate a selection notify message */
          return;
        }
    }
  else if (selection == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_DROPFILES_DND))
    {
      /* This means he wants the names of the dropped files.
       * gdk_dropfiles_filter already has stored the text/uri-list
       * data temporarily in dropfiles_prop.
       */
      if (win32_sel->dropfiles_prop != NULL)
	{
	  selection_property_store
	    (requestor, win32_sel->dropfiles_prop->target, win32_sel->dropfiles_prop->bitness,
	     win32_sel->dropfiles_prop->data, win32_sel->dropfiles_prop->length);
	  g_clear_pointer (&win32_sel->dropfiles_prop, g_free);
	}
    }
  else if (selection == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_OLE2_DND))
    {
      property = convert_dnd_selection_to_target (target, requestor);
    }
  else
    property = GDK_NONE;

  /* Generate a selection notify message so that we actually fetch the
   * data (if property == GDK_SELECTION) or indicating failure (if
   * property == GDK_NONE).
   */
  generate_selection_notify (requestor, selection, target, property, time);
}

void
_gdk_win32_selection_property_change (GdkWin32Selection *win32_sel,
                                      GdkWindow         *window,
                                      GdkAtom            property,
                                      GdkAtom            type,
                                      gint               format,
                                      GdkPropMode        mode,
                                      const guchar      *data,
                                      gint               nelements)
{
  if (property == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_GDK_SELECTION) &&
      win32_sel->property_change_target_atom == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_TARGETS))
    {
      win32_sel->property_change_target_atom = 0;

      if (win32_sel->clipboard_opened_for == INVALID_HANDLE_VALUE &&
          OpenClipboard (GDK_WINDOW_HWND (window)))
        {
          win32_sel->clipboard_opened_for = GDK_WINDOW_HWND (window);
          GDK_NOTE (DND, g_print ("Opened clipboard for 0x%p @ %s:%d\n", win32_sel->clipboard_opened_for, __FILE__, __LINE__));
        }

      if (win32_sel->clipboard_opened_for == INVALID_HANDLE_VALUE)
        {
          queue_open_clipboard (GDK_WIN32_CLIPBOARD_QUEUE_ACTION_TARGETS, NULL, window, type, GDK_CURRENT_TIME);
          return;
        }
      else
        {
          queue_open_clipboard (GDK_WIN32_CLIPBOARD_QUEUE_ACTION_TARGETS, NULL, window, type, GDK_CURRENT_TIME);
          open_clipboard_timeout (NULL);
        }
    }
  else if ((property == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_OLE2_DND) ||
            property == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_LOCAL_DND_SELECTION)) &&
           mode == GDK_PROP_MODE_REPLACE &&
           win32_sel->property_change_target_atom == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_DELETE))
    {
      /* no-op on Windows */
      win32_sel->property_change_target_atom = 0;
    }
  else if (mode == GDK_PROP_MODE_REPLACE &&
           (win32_sel->property_change_target_atom == 0 ||
            win32_sel->property_change_data == NULL ||
            win32_sel->property_change_format == 0))
    {
      g_warning ("Setting selection property with 0x%p == NULL or 0x%x == 0 or 0x%p == 0",
                 win32_sel->property_change_data,
                 win32_sel->property_change_format,
                 win32_sel->property_change_target_atom);
    }
  else if (mode == GDK_PROP_MODE_REPLACE &&
           win32_sel->property_change_data != NULL &&
           win32_sel->property_change_format != 0)
    {
      guchar *set_data = NULL;
      gint set_data_length = 0;
      gint byte_length = format / 8 * nelements;

      if (win32_sel->property_change_transmute)
        transmute_selection_target (type,
                                    win32_sel->property_change_format,
                                    data,
                                    byte_length,
                                    &set_data,
                                    &set_data_length);
      else
        {
          set_data_length = byte_length;
          set_data = g_memdup (data, set_data_length);
        }

      if (set_data != NULL && set_data_length > 0)
        {
          HGLOBAL hdata;

          if ((hdata = GlobalAlloc (GMEM_MOVEABLE, set_data_length)) != 0)
            {
              gchar *ucptr;
              win32_sel->property_change_data->tymed = TYMED_HGLOBAL;
              win32_sel->property_change_data->pUnkForRelease = NULL;
              win32_sel->property_change_data->hGlobal = hdata;
              ucptr = GlobalLock (hdata);
              memcpy (ucptr, set_data, set_data_length);
              GlobalUnlock (hdata);
            }
          else
            {
	      WIN32_API_FAILED ("GlobalAlloc");
            }

          g_free (set_data);
        }

      win32_sel->property_change_format = 0;
      win32_sel->property_change_data = 0;
      win32_sel->property_change_target_atom = 0;
    }
  else
    {
      GDK_NOTE (DND, {
          gchar *prop_name = gdk_atom_name (property);
          gchar *type_name = gdk_atom_name (type);
          gchar *datastring = _gdk_win32_data_to_string (data, MIN (10, format*nelements/8));

          g_warning ("Unsupported property change on window 0x%p, %s property %s, %d-bit, target 0x%x of %d bytes: %s",
                     window,
                     (mode == GDK_PROP_MODE_REPLACE ? "REPLACE" :
                      (mode == GDK_PROP_MODE_PREPEND ? "PREPEND" :
                       (mode == GDK_PROP_MODE_APPEND ? "APPEND" :
                        "???"))),
                     prop_name,
                     format,
                     type_name,
                     nelements,
                     datastring);
          g_free (datastring);
          g_free (prop_name);
          g_free (type_name);
        });
    }
}

gint
_gdk_win32_display_get_selection_property (GdkDisplay *display,
					   GdkWindow  *requestor,
					   guchar    **data,
					   GdkAtom    *ret_type,
					   gint       *ret_format)
{
  GdkWin32Selection *win32_sel = _gdk_win32_selection_get ();
  GdkSelProp *prop;

  g_return_val_if_fail (requestor != NULL, 0);
  g_return_val_if_fail (GDK_IS_WINDOW (requestor), 0);

  if (GDK_WINDOW_DESTROYED (requestor))
    return 0;

  GDK_NOTE (DND, g_print ("gdk_selection_property_get: %p",
			   GDK_WINDOW_HWND (requestor)));

  prop = g_hash_table_lookup (win32_sel->sel_prop_table, GDK_WINDOW_HWND (requestor));

  if (prop == NULL)
    {
      GDK_NOTE (DND, g_print (" (nothing)\n"));
      *data = NULL;

      return 0;
    }

  *data = g_malloc (prop->length + 1);
  (*data)[prop->length] = '\0';
  if (prop->length > 0)
    memmove (*data, prop->data, prop->length);

  GDK_NOTE (DND, {
      gchar *type_name = gdk_atom_name (prop->target);

      g_print (" %s format:%d length:%"G_GSIZE_FORMAT"\n", type_name, prop->bitness, prop->length);
      g_free (type_name);
    });

  if (ret_type)
    *ret_type = prop->target;

  if (ret_format)
    *ret_format = prop->bitness;

  return prop->length;
}

void
_gdk_selection_property_delete (GdkWindow *window)
{
  GDK_NOTE (DND, g_print ("_gdk_selection_property_delete: %p (no-op)\n",
			   GDK_WINDOW_HWND (window)));

#if 0
  prop = g_hash_table_lookup (sel_prop_table, GDK_WINDOW_HWND (window));
  if (prop != NULL)
    {
      g_free (prop->data);
      g_free (prop);
      g_hash_table_remove (sel_prop_table, GDK_WINDOW_HWND (window));
    }
#endif
}

void
_gdk_win32_display_send_selection_notify (GdkDisplay   *display,
					  GdkWindow    *requestor,
					  GdkAtom     	selection,
					  GdkAtom     	target,
					  GdkAtom     	property,
					  guint32     	time)
{
  GDK_NOTE (DND, {
      gchar *sel_name = gdk_atom_name (selection);
      gchar *tgt_name = gdk_atom_name (target);
      gchar *prop_name = gdk_atom_name (property);

      g_print ("gdk_selection_send_notify_for_display: %p %s %s %s (no-op)\n",
	       requestor, sel_name, tgt_name, prop_name);
      g_free (sel_name);
      g_free (tgt_name);
      g_free (prop_name);
    });
}

/* It's hard to say whether implementing this actually is of any use
 * on the Win32 platform? gtk calls only
 * gdk_text_property_to_utf8_list_for_display().
 */
gint
gdk_text_property_to_text_list_for_display (GdkDisplay   *display,
					    GdkAtom       encoding,
					    gint          format,
					    const guchar *text,
					    gint          length,
					    gchar      ***list)
{
  gchar *result;
  const gchar *charset;
  gchar *source_charset;

  GDK_NOTE (DND, {
      gchar *enc_name = gdk_atom_name (encoding);

      g_print ("gdk_text_property_to_text_list_for_display: %s %d %.20s %d\n",
	       enc_name, format, text, length);
      g_free (enc_name);
    });

  if (!list)
    return 0;

  if (encoding == GDK_TARGET_STRING)
    source_charset = g_strdup ("ISO-8859-1");
  else if (encoding == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_UTF8_STRING))
    source_charset = g_strdup ("UTF-8");
  else
    source_charset = gdk_atom_name (encoding);

  g_get_charset (&charset);

  result = g_convert ((const gchar *) text, length, charset, source_charset,
		      NULL, NULL, NULL);
  g_free (source_charset);

  if (!result)
    return 0;

  *list = g_new (gchar *, 1);
  **list = result;

  return 1;
}

void
gdk_free_text_list (gchar **list)
{
  g_return_if_fail (list != NULL);

  g_free (*list);
  g_free (list);
}

static gint
make_list (const gchar  *text,
	   gint          length,
	   gboolean      latin1,
	   gchar      ***list)
{
  GSList *strings = NULL;
  gint n_strings = 0;
  gint i;
  const gchar *p = text;
  const gchar *q;
  GSList *tmp_list;
  GError *error = NULL;

  while (p < text + length)
    {
      gchar *str;

      q = p;
      while (*q && q < text + length)
	q++;

      if (latin1)
	{
	  str = g_convert (p, q - p,
			   "UTF-8", "ISO-8859-1",
			   NULL, NULL, &error);

	  if (!str)
	    {
	      g_warning ("Error converting selection from STRING: %s",
			 error->message);
	      g_error_free (error);
	    }
	}
      else
	str = g_strndup (p, q - p);

      if (str)
	{
	  strings = g_slist_prepend (strings, str);
	  n_strings++;
	}

      p = q + 1;
    }

  if (list)
    *list = g_new (gchar *, n_strings + 1);

  (*list)[n_strings] = NULL;

  i = n_strings;
  tmp_list = strings;
  while (tmp_list)
    {
      if (list)
	(*list)[--i] = tmp_list->data;
      else
	g_free (tmp_list->data);

      tmp_list = tmp_list->next;
    }

  g_slist_free (strings);

  return n_strings;
}

gint
_gdk_win32_display_text_property_to_utf8_list (GdkDisplay    *display,
					       GdkAtom        encoding,
					       gint           format,
					       const guchar  *text,
					       gint           length,
					       gchar       ***list)
{
  g_return_val_if_fail (text != NULL, 0);
  g_return_val_if_fail (length >= 0, 0);

  if (encoding == GDK_TARGET_STRING)
    {
      return make_list ((gchar *)text, length, TRUE, list);
    }
  else if (encoding == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_UTF8_STRING))
    {
      return make_list ((gchar *)text, length, FALSE, list);
    }
  else
    {
      gchar *enc_name = gdk_atom_name (encoding);

      g_warning ("gdk_text_property_to_utf8_list_for_display: encoding %s not handled\n", enc_name);
      g_free (enc_name);

      if (list)
	*list = NULL;

      return 0;
    }
}

gchar *
_gdk_win32_display_utf8_to_string_target (GdkDisplay *display,
					  const gchar *str)
{
  return _gdk_utf8_to_string_target_internal (str, strlen (str));
}

void
gdk_win32_selection_clear_targets (GdkDisplay *display,
                                   GdkAtom     selection)
{
  GdkWin32Selection *win32_sel = _gdk_win32_selection_get ();

  if (selection == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_OLE2_DND) ||
      selection == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_LOCAL_DND_SELECTION))
    {
      g_array_set_size (win32_sel->dnd_selection_targets, 0);
    }
  else if (selection == GDK_SELECTION_CLIPBOARD)
    {
      g_array_set_size (win32_sel->clipboard_selection_targets, 0);
    }
  else if (selection == GDK_SELECTION_PRIMARY)
    {
      /* Do nothing */
    }
  else
    {
      gchar *sel_name = gdk_atom_name (selection);

      g_warning ("Unsupported generic selection %s (0x%p)", sel_name, selection);
      g_free (sel_name);
    }
}

gint
_gdk_win32_add_target_to_selformats (GdkAtom  target,
                                     GArray  *array)
{
  gint added_count = 0;
  gchar *target_name;
  wchar_t *target_name_w;
  GdkSelTargetFormat fmt;
  gint i;
  GArray *compatibility_formats;
  gint starting_point;

  for (i = 0; i < array->len; i++)
    if (g_array_index (array, GdkSelTargetFormat, i).target == target)
      break;

  /* Don't put duplicates into the array */
  if (i < array->len)
    return added_count;

  if (target == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_TARGETS) ||
      target == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_COMPOUND_TEXT) || target == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_SAVE_TARGETS))
    {
      /* Add the "we don't really support transferring that to
       * other processes" format, just to keep the taget around.
       */
      fmt.target = target;
      fmt.format = 0;
      fmt.transmute = FALSE;
      g_array_append_val (array, fmt);
      added_count += 1;
      return added_count;
    }

  /* Only check the newly-added pairs for duplicates,
   * all the ones that exist right now have different targets.
   */
  starting_point = array->len;

  target_name = gdk_atom_name (target);
  target_name_w = g_utf8_to_utf16 (target_name, -1, NULL, NULL, NULL);
  g_free (target_name);

  if (target_name_w == NULL)
    return added_count;

  fmt.format = RegisterClipboardFormatW (target_name_w);
  GDK_NOTE (DND, g_print ("Registered clipboard format %S as 0x%x\n", target_name_w, fmt.format));
  g_free (target_name_w);
  fmt.target = target;
  fmt.transmute = FALSE;

  /* Add the "as-is" format */
  g_array_append_val (array, fmt);
  added_count += 1;

  compatibility_formats = get_compatibility_formats_for_target (target);
  for (i = 0; compatibility_formats != NULL && i < compatibility_formats->len; i++)
    {
      gint j;

      fmt = g_array_index (compatibility_formats, GdkSelTargetFormat, i);

      /* Don't put duplicates into the array */
      for (j = starting_point; j < array->len; j++)
        if (g_array_index (array, GdkSelTargetFormat, j).format == fmt.format)
          break;

      if (j < array->len)
        continue;

      /* Add a compatibility format */
      g_array_append_val (array, fmt);
      added_count += 1;
    }

  return added_count;
}

/* This function is called from gtk_selection_add_target() and
 * gtk_selection_add_targets() in gtkselection.c. It is this function
 * that takes care of setting those clipboard formats for which we use
 * delayed rendering (that is, all formats, as we use delayed rendering
 * for everything). This function only registers the formats, but does
 * not announce them as supported. That is handled as a special case
 * in gdk_window_property_change().
 *
 * Implementation detail:
 * This function will be called repeatedly, every time the PRIMARY selection changes.
 * It will also be called immediately before the CLIPBOARD selection changes.
 * We let GTK+ handle the PRIMARY selection internally and do nothing here
 * (therefore it's not possible to middle-click-paste between processes,
 * unless one process deliberately puts PRIMARY selection contents into
 * CLIPBOARD selection, and the other process does paste on middle-click).
 */
void
gdk_win32_selection_add_targets (GdkWindow  *owner,
				 GdkAtom     selection,
				 gint	     n_targets,
				 GdkAtom    *targets)
{
  GdkWin32Selection *win32_sel = _gdk_win32_selection_get ();
  gint i;

  GDK_NOTE (DND, {
      gchar *sel_name = gdk_atom_name (selection);

      g_print ("gdk_win32_selection_add_targets: %p: %s: ",
	       owner ? GDK_WINDOW_HWND (owner) : NULL,
	       sel_name);
      g_free (sel_name);

      for (i = 0; i < n_targets; i++)
	{
	  gchar *tgt_name = gdk_atom_name (targets[i]);

	  g_print ("%s", tgt_name);
	  g_free (tgt_name);
	  if (i < n_targets - 1)
	    g_print (", ");
	}
      g_print ("\n");
    });

  if (selection == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_OLE2_DND) ||
      selection == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_LOCAL_DND_SELECTION) ||
      selection == GDK_SELECTION_CLIPBOARD)
    {
      GArray *fmts = NULL;
      gint added_count = 0;

      if (selection == GDK_SELECTION_CLIPBOARD)
        fmts = win32_sel->clipboard_selection_targets;
      else
        fmts = win32_sel->dnd_selection_targets;

      for (i = 0; i < n_targets; i++)
        added_count += _gdk_win32_add_target_to_selformats (targets[i], fmts);

      /* Re-announce our list of supported formats */
      if (added_count > 0)
        send_targets_request (GDK_CURRENT_TIME);
    }
  else if (selection == GDK_SELECTION_PRIMARY)
    {
      /* Do nothing */
    }
  else
    {
      gchar *sel_name = gdk_atom_name (selection);

      g_warning ("Unsupported generic selection %s (0x%p)", sel_name, selection);
      g_free (sel_name);
    }
}
