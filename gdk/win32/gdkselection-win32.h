/* GDK - The GIMP Drawing Kit
 *
 * gdkselection-win32.h: Private Win32 specific selection object
 *
 * Copyright © 2017 LRN
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GDK_SELECTION_WIN32_H__
#define __GDK_SELECTION_WIN32_H__

G_BEGIN_DECLS

#define _gdk_win32_selection_get() (_win32_selection)
#define _gdk_atom_array_index(a, i) (g_array_index (a, GdkAtom, i))
#define _gdk_win32_selection_atom(i) (_gdk_atom_array_index (_gdk_win32_selection_get ()->known_atoms, i))
#define _gdk_cf_array_index(a, i) (g_array_index (a, UINT, i))
#define _gdk_win32_selection_cf(i) (_gdk_cf_array_index (_gdk_win32_selection_get ()->known_clipboard_formats, i))

/* Maps targets to formats or vice versa, depending on the
 * semantics of the array that holds these.
 * Also remembers whether the data needs to be transmuted.
 */
typedef struct {
  gint format;
  GdkAtom target;
  gboolean transmute;
} GdkSelTargetFormat;

/* We emulate the GDK_SELECTION window properties of windows (as used
 * in the X11 backend) by using a hash table from window handles to
 * GdkSelProp structs.
 */
typedef struct {
  guchar *data;
  gsize   length;
  gint    bitness;
  GdkAtom target;
} GdkSelProp;

/* OLE-based DND state */
typedef enum {
  GDK_WIN32_DND_NONE,
  GDK_WIN32_DND_PENDING,
  GDK_WIN32_DND_DROPPED,
  GDK_WIN32_DND_FAILED,
  GDK_WIN32_DND_DRAGGING,
} GdkWin32DndState;

enum _GdkWin32AtomIndex
{
/* GdkAtoms: properties, targets and types */
  GDK_WIN32_ATOM_INDEX_GDK_SELECTION = 0,
  GDK_WIN32_ATOM_INDEX_CLIPBOARD_MANAGER,
  GDK_WIN32_ATOM_INDEX_WM_TRANSIENT_FOR,
  GDK_WIN32_ATOM_INDEX_TARGETS,
  GDK_WIN32_ATOM_INDEX_DELETE,
  GDK_WIN32_ATOM_INDEX_SAVE_TARGETS,
  GDK_WIN32_ATOM_INDEX_UTF8_STRING,
  GDK_WIN32_ATOM_INDEX_TEXT,
  GDK_WIN32_ATOM_INDEX_COMPOUND_TEXT,
  GDK_WIN32_ATOM_INDEX_TEXT_URI_LIST,
  GDK_WIN32_ATOM_INDEX_TEXT_HTML,
  GDK_WIN32_ATOM_INDEX_IMAGE_PNG,
  GDK_WIN32_ATOM_INDEX_IMAGE_JPEG,
  GDK_WIN32_ATOM_INDEX_IMAGE_BMP,
  GDK_WIN32_ATOM_INDEX_IMAGE_GIF,
/* DND selections */
  GDK_WIN32_ATOM_INDEX_LOCAL_DND_SELECTION,
  GDK_WIN32_ATOM_INDEX_DROPFILES_DND,
  GDK_WIN32_ATOM_INDEX_OLE2_DND,
/* Clipboard formats */
  GDK_WIN32_ATOM_INDEX_PNG,
  GDK_WIN32_ATOM_INDEX_JFIF,
  GDK_WIN32_ATOM_INDEX_GIF,
  GDK_WIN32_ATOM_INDEX_CF_DIB,
  GDK_WIN32_ATOM_INDEX_CFSTR_SHELLIDLIST,
  GDK_WIN32_ATOM_INDEX_CF_TEXT,
  GDK_WIN32_ATOM_INDEX_CF_UNICODETEXT,
  GDK_WIN32_ATOM_INDEX_LAST
};

typedef enum _GdkWin32AtomIndex GdkWin32AtomIndex;

enum _GdkWin32CFIndex
{
  GDK_WIN32_CF_INDEX_PNG = 0,
  GDK_WIN32_CF_INDEX_JFIF,
  GDK_WIN32_CF_INDEX_GIF,
  GDK_WIN32_CF_INDEX_UNIFORMRESOURCELOCATORW,
  GDK_WIN32_CF_INDEX_CFSTR_SHELLIDLIST,
  GDK_WIN32_CF_INDEX_HTML_FORMAT,
  GDK_WIN32_CF_INDEX_TEXT_HTML,
  GDK_WIN32_CF_INDEX_IMAGE_PNG,
  GDK_WIN32_CF_INDEX_IMAGE_JPEG,
  GDK_WIN32_CF_INDEX_IMAGE_BMP,
  GDK_WIN32_CF_INDEX_IMAGE_GIF,
  GDK_WIN32_CF_INDEX_TEXT_URI_LIST,
  GDK_WIN32_CF_INDEX_UTF8_STRING,
  GDK_WIN32_CF_INDEX_LAST
};

typedef enum _GdkWin32CFIndex GdkWin32CFIndex;

#define GDK_TYPE_WIN32_SELECTION         (gdk_win32_selection_get_type ())
#define GDK_WIN32_SELECTION(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_WIN32_SELECTION, GdkWin32Selection))
#define GDK_WIN32_SELECTION_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), GDK_TYPE_WIN32_SELECTION, GdkWin32SelectionClass))
#define GDK_IS_WIN32_SELECTION(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_WIN32_SELECTION))
#define GDK_IS_WIN32_SELECTION_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), GDK_TYPE_WIN32_SELECTION))
#define GDK_WIN32_SELECTION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_WIN32_SELECTION, GdkWin32SelectionClass))

typedef struct _GdkWin32Selection GdkWin32Selection;
typedef struct _GdkWin32SelectionClass GdkWin32SelectionClass;

/* This object is just a sink to hold all the selection- and dnd-related data
 * that otherwise would be in global variables.
 */
struct _GdkWin32Selection
{
  GObject parent_instance;

  GHashTable *sel_prop_table;
  GdkSelProp *dropfiles_prop;
  /* We store the owner of each selection in this table. Obviously, this only
   * is valid intra-app, and in fact it is necessary for the intra-app DND to work.
   */
  GHashTable *sel_owner_table;

  /* GdkAtoms for well-known image formats */
  GdkAtom *known_pixbuf_formats;
  int n_known_pixbuf_formats;

  /* GArray of GdkAtoms for various known Selection and DnD strings.
   * Size is guaranteed to be at least GDK_WIN32_ATOM_INDEX_LAST
   */
  GArray *known_atoms;

  /* GArray of UINTs for various known clipboard formats.
   * Size is guaranteed to be at least GDK_WIN32_CF_INDEX_LAST.
   */
  GArray *known_clipboard_formats;

  GdkWin32DndState  dnd_target_state;
  GdkWin32DndState  dnd_source_state;

  /* Holds a reference to the data object for the target drop site.
   */
  IDataObject      *dnd_data_object_target;

  /* Carries DnD target context from idroptarget_*() to convert_selection() */
  GdkDragContext   *target_drag_context;

  /* Carries W32 format ID from idataobject_getdata() to property_change() */
  UINT              property_change_format;
  /* Carries the W32-wrapped data between idataobject_getdata() and property_change() */
  LPSTGMEDIUM       property_change_data;
  /* Carries the transmute field of the GdkSelTargetFormat from from idataobject_getdata() to property_change() */
  gboolean          property_change_transmute;
  /* Carries the target atom from GDK_SELECTION_REQUEST issuer to property_change() */
  GdkAtom           property_change_target_atom;

  /* TRUE when we are emptying the clipboard ourselves */
  gboolean          ignore_destroy_clipboard;

  /* Array of GdkSelTargetFormats describing the targets supported by the clipboard selection */
  GArray           *clipboard_selection_targets;

  /* Same for the DnD selection (applies for both LOCAL and OLE2 DnD) */
  GArray           *dnd_selection_targets;

  /* If TRUE, then we queued a GDK_SELECTION_REQUEST with TARGETS
   * target. This field is checked to prevent queueing
   * multiple selection requests.
   */
  gboolean          targets_request_pending;

  /* The handle that was given to OpenClipboard().
   * NULL is a valid handle,
   * INVALID_HANDLE_VALUE means that the clipboard is closed.
   */
  HWND              clipboard_opened_for;

  /* A target-keyed hash table of GArrays of GdkSelTargetFormats describing compatibility formats for a target */
  GHashTable       *compatibility_formats;
  /* A format-keyed hash table of GArrays of GdkAtoms describing compatibility targets for a format */
  GHashTable       *compatibility_targets;
};

struct _GdkWin32SelectionClass
{
  GObjectClass parent_class;
};

GType gdk_win32_selection_get_type (void) G_GNUC_CONST;

void    _gdk_win32_clear_clipboard_queue                          ();
gchar * _gdk_win32_get_clipboard_format_name                      (UINT               fmt,
                                                                   gboolean          *is_predefined);
void    _gdk_win32_add_format_to_targets                          (UINT               format,
                                                                   GArray            *array,
                                                                   GList            **list);
gint    _gdk_win32_add_target_to_selformats                       (GdkAtom            target,
                                                                   GArray            *array);
void    _gdk_win32_selection_property_change                      (GdkWin32Selection *win32_sel,
                                                                   GdkWindow         *window,
                                                                   GdkAtom            property,
                                                                   GdkAtom            type,
                                                                   gint               format,
                                                                   GdkPropMode        mode,
                                                                   const guchar      *data,
                                                                   gint               nelements);

#endif /* __GDK_SELECTION_WIN32_H__ */
