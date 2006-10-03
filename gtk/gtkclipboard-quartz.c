/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc.
 * Copyright (C) 2004 Nokia Corporation
 * Copyright (C) 2006 Imendio AB
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
 *
 */

#include <config.h>
#include <string.h>

#import <Cocoa/Cocoa.h>

#include "gtkclipboard.h"
#include "gtkinvisible.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkintl.h"
#include "gtktextbuffer.h"
#include "gtkquartz.h"
#include "gtkalias.h"

enum {
  OWNER_CHANGE,
  LAST_SIGNAL
};

typedef struct _GtkClipboardClass GtkClipboardClass;

struct _GtkClipboard 
{
  GObject parent_instance;

  NSPasteboard *pasteboard;

  GdkAtom selection;

  GtkClipboardGetFunc get_func;
  GtkClipboardClearFunc clear_func;
  gpointer user_data;
  gboolean have_owner;

  gboolean have_selection;
  GdkDisplay *display;

  GdkAtom *cached_targets;
  gint     n_cached_targets;

  guint      notify_signal_id;
  gboolean   storing_selection;
  GMainLoop *store_loop;
  guint      store_timeout;
  gint       n_storable_targets;
  GdkAtom   *storable_targets;
};

struct _GtkClipboardClass
{
  GObjectClass parent_class;

  void (*owner_change) (GtkClipboard        *clipboard,
			GdkEventOwnerChange *event);
};

@interface GtkClipboardOwner : NSObject {
  GtkClipboard *clipboard;

  GtkClipboardGetFunc get_func;
  GtkClipboardClearFunc clear_func;
  gpointer user_data;
  
}

@end

@implementation GtkClipboardOwner
-(void)pasteboard:(NSPasteboard *)sender provideDataForType:(NSString *)type
{
  GtkSelectionData selection_data;

  selection_data.selection = clipboard->selection;
  selection_data.data = NULL;
  selection_data.target = _gtk_quartz_pasteboard_type_to_atom (type);

  /* FIXME: We need to find out what the info argument should be
   * here by storing it in the clipboard object in
   * gtk_clipboard_set_contents
   */
  clipboard->get_func (clipboard, &selection_data, 0, clipboard->user_data);

  _gtk_quartz_set_selection_data_for_pasteboard (clipboard->pasteboard, &selection_data);

  g_free (selection_data.data);
}

- (void)pasteboardChangedOwner:(NSPasteboard *)sender
{
  if (clear_func)
    clear_func (clipboard, user_data);

  [self release];
}

- (id)initWithClipboard:(GtkClipboard *)aClipboard
{
  self = [super init];

  if (self) 
    {
      clipboard = aClipboard;
    }

  return self;
}

@end

static void gtk_clipboard_class_init   (GtkClipboardClass   *class);
static void gtk_clipboard_finalize     (GObject             *object);
static void gtk_clipboard_owner_change (GtkClipboard        *clipboard,
					GdkEventOwnerChange *event);

static void          clipboard_unset      (GtkClipboard     *clipboard);
static GtkClipboard *clipboard_peek       (GdkDisplay       *display,
					   GdkAtom           selection,
					   gboolean          only_if_exists);

static const gchar clipboards_owned_key[] = "gtk-clipboards-owned";
static GQuark clipboards_owned_key_id = 0;

static GObjectClass *parent_class;
static guint         clipboard_signals[LAST_SIGNAL] = { 0 };

GType
gtk_clipboard_get_type (void)
{
  static GType clipboard_type = 0;
  
  if (!clipboard_type)
    {
      const GTypeInfo clipboard_info =
      {
	sizeof (GtkClipboardClass),
	NULL,           /* base_init */
	NULL,           /* base_finalize */
	(GClassInitFunc) gtk_clipboard_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data */
	sizeof (GtkClipboard),
	0,              /* n_preallocs */
	(GInstanceInitFunc) NULL,
      };
      
      clipboard_type = g_type_register_static (G_TYPE_OBJECT, I_("GtkClipboard"),
					       &clipboard_info, 0);
    }
  
  return clipboard_type;
}

static void
gtk_clipboard_class_init (GtkClipboardClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  parent_class = g_type_class_peek_parent (class);
  
  gobject_class->finalize = gtk_clipboard_finalize;

  class->owner_change = gtk_clipboard_owner_change;

  clipboard_signals[OWNER_CHANGE] =
    g_signal_new (I_("owner_change"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkClipboardClass, owner_change),
		  NULL, NULL,
		  _gtk_marshal_VOID__BOXED,
		  G_TYPE_NONE, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
}

static void
gtk_clipboard_finalize (GObject *object)
{
  GtkClipboard *clipboard;
  GSList *clipboards;

  clipboard = GTK_CLIPBOARD (object);

  clipboards = g_object_get_data (G_OBJECT (clipboard->display), "gtk-clipboard-list");
  if (g_slist_index (clipboards, clipboard) >= 0)
    g_warning ("GtkClipboard prematurely finalized");

  clipboard_unset (clipboard);
  
  clipboards = g_object_get_data (G_OBJECT (clipboard->display), "gtk-clipboard-list");
  clipboards = g_slist_remove (clipboards, clipboard);
  g_object_set_data (G_OBJECT (clipboard->display), I_("gtk-clipboard-list"), clipboards);

  if (clipboard->store_loop && g_main_loop_is_running (clipboard->store_loop))
    g_main_loop_quit (clipboard->store_loop);

  if (clipboard->store_timeout != 0)
    g_source_remove (clipboard->store_timeout);

  g_free (clipboard->storable_targets);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clipboard_display_closed (GdkDisplay   *display,
			  gboolean      is_error,
			  GtkClipboard *clipboard)
{
  GSList *clipboards;

  clipboards = g_object_get_data (G_OBJECT (display), "gtk-clipboard-list");
  g_object_run_dispose (G_OBJECT (clipboard));
  clipboards = g_slist_remove (clipboards, clipboard);
  g_object_set_data (G_OBJECT (display), I_("gtk-clipboard-list"), clipboards);
  g_object_unref (clipboard);
}

/**
 * gtk_clipboard_get_for_display:
 * @display: the display for which the clipboard is to be retrieved or created
 * @selection: a #GdkAtom which identifies the clipboard
 *             to use.
 * 
 * Returns the clipboard object for the given selection.
 * Cut/copy/paste menu items and keyboard shortcuts should use
 * the default clipboard, returned by passing %GDK_SELECTION_CLIPBOARD for @selection.
 * (%GDK_NONE is supported as a synonym for GDK_SELECTION_CLIPBOARD
 * for backwards compatibility reasons.)
 * The currently-selected object or text should be provided on the clipboard
 * identified by #GDK_SELECTION_PRIMARY. Cut/copy/paste menu items
 * conceptually copy the contents of the #GDK_SELECTION_PRIMARY clipboard
 * to the default clipboard, i.e. they copy the selection to what the
 * user sees as the clipboard.
 *
 * (Passing #GDK_NONE is the same as using <literal>gdk_atom_intern
 * ("CLIPBOARD", FALSE)</literal>. See <ulink
 * url="http://www.freedesktop.org/Standards/clipboards-spec">
 * http://www.freedesktop.org/Standards/clipboards-spec</ulink>
 * for a detailed discussion of the "CLIPBOARD" vs. "PRIMARY"
 * selections under the X window system. On Win32 the
 * #GDK_SELECTION_PRIMARY clipboard is essentially ignored.)
 *
 * It's possible to have arbitrary named clipboards; if you do invent
 * new clipboards, you should prefix the selection name with an
 * underscore (because the ICCCM requires that nonstandard atoms are
 * underscore-prefixed), and namespace it as well. For example,
 * if your application called "Foo" has a special-purpose
 * clipboard, you might call it "_FOO_SPECIAL_CLIPBOARD".
 * 
 * Return value: the appropriate clipboard object. If no
 *             clipboard already exists, a new one will
 *             be created. Once a clipboard object has
 *             been created, it is persistent and, since
 *             it is owned by GTK+, must not be freed or
 *             unrefd.
 *
 * Since: 2.2
 **/
GtkClipboard *
gtk_clipboard_get_for_display (GdkDisplay *display, 
			       GdkAtom     selection)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (!display->closed, NULL);

  return clipboard_peek (display, selection, FALSE);
}


/**
 * gtk_clipboard_get():
 * @selection: a #GdkAtom which identifies the clipboard
 *             to use.
 * 
 * Returns the clipboard object for the given selection.
 * See gtk_clipboard_get_for_display() for complete details.
 * 
 * Return value: the appropriate clipboard object. If no
 *             clipboard already exists, a new one will
 *             be created. Once a clipboard object has
 *             been created, it is persistent and, since
 *             it is owned by GTK+, must not be freed or
 *             unrefd.
 **/
GtkClipboard *
gtk_clipboard_get (GdkAtom selection)
{
  return gtk_clipboard_get_for_display (gdk_display_get_default (), selection);
}

static void
clipboard_owner_destroyed (gpointer data)
{
  GSList *clipboards = data;
  GSList *tmp_list;

  tmp_list = clipboards;
  while (tmp_list)
    {
      GtkClipboard *clipboard = tmp_list->data;

      clipboard->get_func = NULL;
      clipboard->clear_func = NULL;
      clipboard->user_data = NULL;
      clipboard->have_owner = FALSE;

      gtk_clipboard_clear (clipboard);

      tmp_list = tmp_list->next;
    }
  
  g_slist_free (clipboards);
}

static void
clipboard_add_owner_notify (GtkClipboard *clipboard)
{
  if (!clipboards_owned_key_id)
    clipboards_owned_key_id = g_quark_from_static_string (clipboards_owned_key);
  
  if (clipboard->have_owner)
    g_object_set_qdata_full (clipboard->user_data, clipboards_owned_key_id,
			     g_slist_prepend (g_object_steal_qdata (clipboard->user_data,
								    clipboards_owned_key_id),
					      clipboard),
			     clipboard_owner_destroyed);
}

static void
clipboard_remove_owner_notify (GtkClipboard *clipboard)
{
  if (clipboard->have_owner)
     g_object_set_qdata_full (clipboard->user_data, clipboards_owned_key_id,
			      g_slist_remove (g_object_steal_qdata (clipboard->user_data,
								    clipboards_owned_key_id),
					      clipboard),
			      clipboard_owner_destroyed);
}

static gboolean
gtk_clipboard_set_contents (GtkClipboard         *clipboard,
			    const GtkTargetEntry *targets,
			    guint                 n_targets,
			    GtkClipboardGetFunc   get_func,
			    GtkClipboardClearFunc clear_func,
			    gpointer              user_data,
			    gboolean              have_owner)
{
  GtkClipboardOwner *owner;
  NSArray *types;
  NSAutoreleasePool *pool;

  pool = [[NSAutoreleasePool alloc] init];

  owner = [[GtkClipboardOwner alloc] initWithClipboard:clipboard];

  types = _gtk_quartz_target_entries_to_pasteboard_types (targets, n_targets);

  clipboard->user_data = user_data;
  clipboard->have_owner = have_owner;
  if (have_owner)
    clipboard_add_owner_notify (clipboard);
  clipboard->get_func = get_func;
  clipboard->clear_func = clear_func;

  [clipboard->pasteboard declareTypes:types owner:owner];

  [pool release];

  return true;
}

/**
 * gtk_clipboard_set_with_data:
 * @clipboard:  a #GtkClipboard
 * @targets:    array containing information about the available forms for the
 *              clipboard data
 * @n_targets:  number of elements in @targets
 * @get_func:   function to call to get the actual clipboard data
 * @clear_func: when the clipboard contents are set again, this function will
 *              be called, and @get_func will not be subsequently called.
 * @user_data:  user data to pass to @get_func and @clear_func.
 * 
 * Virtually sets the contents of the specified clipboard by providing
 * a list of supported formats for the clipboard data and a function
 * to call to get the actual data when it is requested.
 * 
 * Return value: %TRUE if setting the clipboard data succeeded. If setting
 *               the clipboard data failed the provided callback functions
 *               will be ignored.
 **/
gboolean
gtk_clipboard_set_with_data (GtkClipboard          *clipboard,
			     const GtkTargetEntry  *targets,
			     guint                  n_targets,
			     GtkClipboardGetFunc    get_func,
			     GtkClipboardClearFunc  clear_func,
			     gpointer               user_data)
{
  g_return_val_if_fail (clipboard != NULL, FALSE);
  g_return_val_if_fail (targets != NULL, FALSE);
  g_return_val_if_fail (get_func != NULL, FALSE);

  return gtk_clipboard_set_contents (clipboard, targets, n_targets,
				     get_func, clear_func, user_data,
				     FALSE);
}

/**
 * gtk_clipboard_set_with_owner:
 * @clipboard:  a #GtkClipboard
 * @targets:    array containing information about the available forms for the
 *              clipboard data
 * @n_targets:  number of elements in @targets
 * @get_func:   function to call to get the actual clipboard data
 * @clear_func: when the clipboard contents are set again, this function will
 *              be called, and @get_func will not be subsequently called.
 * @owner:      an object that "owns" the data. This object will be passed
 *              to the callbacks when called. 
 * 
 * Virtually sets the contents of the specified clipboard by providing
 * a list of supported formats for the clipboard data and a function
 * to call to get the actual data when it is requested.
 *
 * The difference between this function and gtk_clipboard_set_with_data()
 * is that instead of an generic @user_data pointer, a #GObject is passed
 * in. 
 * 
 * Return value: %TRUE if setting the clipboard data succeeded. If setting
 *               the clipboard data failed the provided callback functions
 *               will be ignored.
 **/
gboolean
gtk_clipboard_set_with_owner (GtkClipboard          *clipboard,
			      const GtkTargetEntry  *targets,
			      guint                  n_targets,
			      GtkClipboardGetFunc    get_func,
			      GtkClipboardClearFunc  clear_func,
			      GObject               *owner)
{
  g_return_val_if_fail (clipboard != NULL, FALSE);
  g_return_val_if_fail (targets != NULL, FALSE);
  g_return_val_if_fail (get_func != NULL, FALSE);
  g_return_val_if_fail (G_IS_OBJECT (owner), FALSE);

  return gtk_clipboard_set_contents (clipboard, targets, n_targets,
				     get_func, clear_func, owner,
				     TRUE);
}

/**
 * gtk_clipboard_get_owner:
 * @clipboard: a #GtkClipboard
 * 
 * If the clipboard contents callbacks were set with 
 * gtk_clipboard_set_with_owner(), and the gtk_clipboard_set_with_data() or 
 * gtk_clipboard_clear() has not subsequently called, returns the owner set 
 * by gtk_clipboard_set_with_owner().
 * 
 * Return value: the owner of the clipboard, if any; otherwise %NULL.
 **/
GObject *
gtk_clipboard_get_owner (GtkClipboard *clipboard)
{
  g_return_val_if_fail (clipboard != NULL, NULL);

  if (clipboard->have_owner)
    return clipboard->user_data;
  else
    return NULL;
}

static void
clipboard_unset (GtkClipboard *clipboard)
{
  GtkClipboardClearFunc old_clear_func;
  gpointer old_data;
  gboolean old_have_owner;
  gint old_n_storable_targets;
  
  old_clear_func = clipboard->clear_func;
  old_data = clipboard->user_data;
  old_have_owner = clipboard->have_owner;
  old_n_storable_targets = clipboard->n_storable_targets;
  
  if (old_have_owner)
    {
      clipboard->have_owner = FALSE;
    }

  clipboard->n_storable_targets = -1;
  g_free (clipboard->storable_targets);
  clipboard->storable_targets = NULL;
      
  clipboard->get_func = NULL;
  clipboard->clear_func = NULL;
  clipboard->user_data = NULL;
  
  if (old_clear_func)
    old_clear_func (clipboard, old_data);

  /* If we've transferred the clipboard data to the manager,
   * unref the owner
   */
  if (old_have_owner &&
      old_n_storable_targets != -1)
    g_object_unref (old_data);
}

/**
 * gtk_clipboard_clear:
 * @clipboard:  a #GtkClipboard
 * 
 * Clears the contents of the clipboard. Generally this should only
 * be called between the time you call gtk_clipboard_set_with_owner()
 * or gtk_clipboard_set_with_data(),
 * and when the @clear_func you supplied is called. Otherwise, the
 * clipboard may be owned by someone else.
 **/
void
gtk_clipboard_clear (GtkClipboard *clipboard)
{
  [clipboard->pasteboard declareTypes:nil owner:nil];
}

static void 
text_get_func (GtkClipboard     *clipboard,
	       GtkSelectionData *selection_data,
	       guint             info,
	       gpointer          data)
{
  gtk_selection_data_set_text (selection_data, data, -1);
}

static void 
text_clear_func (GtkClipboard *clipboard,
		 gpointer      data)
{
  g_free (data);
}

/**
 * gtk_clipboard_set_text:
 * @clipboard: a #GtkClipboard object
 * @text:      a UTF-8 string.
 * @len:       length of @text, in bytes, or -1, in which case
 *             the length will be determined with <function>strlen()</function>.
 * 
 * Sets the contents of the clipboard to the given UTF-8 string. GTK+ will
 * make a copy of the text and take responsibility for responding
 * for requests for the text, and for converting the text into
 * the requested format.
 **/
void 
gtk_clipboard_set_text (GtkClipboard *clipboard,
			const gchar  *text,
			gint          len)
{
  GtkTargetEntry target = { "UTF8_STRING", 0, 0 };

  g_return_if_fail (clipboard != NULL);
  g_return_if_fail (text != NULL);
  
  if (len < 0)
    len = strlen (text);
  
  gtk_clipboard_set_with_data (clipboard, 
			       &target, 1,
			       text_get_func, text_clear_func,
			       g_strndup (text, len));
  gtk_clipboard_set_can_store (clipboard, NULL, 0);
}


static void 
pixbuf_get_func (GtkClipboard     *clipboard,
		 GtkSelectionData *selection_data,
		 guint             info,
		 gpointer          data)
{
  gtk_selection_data_set_pixbuf (selection_data, data);
}

static void 
pixbuf_clear_func (GtkClipboard *clipboard,
		   gpointer      data)
{
  g_object_unref (data);
}

/**
 * gtk_clipboard_set_image:
 * @clipboard: a #GtkClipboard object
 * @pixbuf:    a #GdkPixbuf 
 * 
 * Sets the contents of the clipboard to the given #GdkPixbuf. 
 * GTK+ will take responsibility for responding for requests 
 * for the image, and for converting the image into the 
 * requested format.
 * 
 * Since: 2.6
 **/
void
gtk_clipboard_set_image (GtkClipboard *clipboard,
			 GdkPixbuf    *pixbuf)
{
  GtkTargetList *list;
  GList *l;
  GtkTargetEntry *targets;
  gint n_targets, i;

  g_return_if_fail (clipboard != NULL);
  g_return_if_fail (GDK_IS_PIXBUF (pixbuf));

  list = gtk_target_list_new (NULL, 0);
  gtk_target_list_add_image_targets (list, 0, TRUE);

  n_targets = g_list_length (list->list);
  targets = g_new0 (GtkTargetEntry, n_targets);
  for (l = list->list, i = 0; l; l = l->next, i++)
    {
      GtkTargetPair *pair = (GtkTargetPair *)l->data;
      targets[i].target = gdk_atom_name (pair->target);
    }

  gtk_clipboard_set_with_data (clipboard, 
			       targets, n_targets,
			       pixbuf_get_func, pixbuf_clear_func,
			       g_object_ref (pixbuf));
  gtk_clipboard_set_can_store (clipboard, NULL, 0);

  for (i = 0; i < n_targets; i++)
    g_free (targets[i].target);
  g_free (targets);
  gtk_target_list_unref (list);
}

/**
 * gtk_clipboard_request_contents:
 * @clipboard: a #GtkClipboard
 * @target:    an atom representing the form into which the clipboard
 *             owner should convert the selection.
 * @callback:  A function to call when the results are received
 *             (or the retrieval fails). If the retrieval fails
 *             the length field of @selection_data will be
 *             negative.
 * @user_data: user data to pass to @callback
 * 
 * Requests the contents of clipboard as the given target.
 * When the results of the result are later received the supplied callback
 * will be called.
 **/
void 
gtk_clipboard_request_contents (GtkClipboard            *clipboard,
				GdkAtom                  target,
				GtkClipboardReceivedFunc callback,
				gpointer                 user_data)
{
  GtkSelectionData *data;

  data = gtk_clipboard_wait_for_contents (clipboard, target);

  callback (clipboard, data, user_data);

  gtk_selection_data_free (data);
}

/**
 * gtk_clipboard_request_text:
 * @clipboard: a #GtkClipboard
 * @callback:  a function to call when the text is received,
 *             or the retrieval fails. (It will always be called
 *             one way or the other.)
 * @user_data: user data to pass to @callback.
 * 
 * Requests the contents of the clipboard as text. When the text is
 * later received, it will be converted to UTF-8 if necessary, and
 * @callback will be called. 
 *
 * The @text parameter to @callback will contain the resulting text if
 * the request succeeded, or %NULL if it failed. This could happen for
 * various reasons, in particular if the clipboard was empty or if the
 * contents of the clipboard could not be converted into text form.
 **/
void 
gtk_clipboard_request_text (GtkClipboard                *clipboard,
			    GtkClipboardTextReceivedFunc callback,
			    gpointer                     user_data)
{
  gchar *data = gtk_clipboard_wait_for_text (clipboard);

  callback (clipboard, data, user_data);

  g_free (data);
}

void
gtk_clipboard_request_rich_text (GtkClipboard                    *clipboard,
                                 GtkTextBuffer                   *buffer,
                                 GtkClipboardRichTextReceivedFunc callback,
                                 gpointer                         user_data)
{
  /* FIXME: Implement */
}


guint8 *
gtk_clipboard_wait_for_rich_text (GtkClipboard  *clipboard,
                                  GtkTextBuffer *buffer,
                                  GdkAtom       *format,
                                  gsize         *length)
{
  /* FIXME: Implement */
  return NULL;
}

/**
 * gtk_clipboard_request_image:
 * @clipboard: a #GtkClipboard
 * @callback:  a function to call when the image is received,
 *             or the retrieval fails. (It will always be called
 *             one way or the other.)
 * @user_data: user data to pass to @callback.
 * 
 * Requests the contents of the clipboard as image. When the image is
 * later received, it will be converted to a #GdkPixbuf, and
 * @callback will be called. 
 *
 * The @pixbuf parameter to @callback will contain the resulting 
 * #GdkPixbuf if the request succeeded, or %NULL if it failed. This 
 * could happen for various reasons, in particular if the clipboard 
 * was empty or if the contents of the clipboard could not be 
 * converted into an image.
 *
 * Since: 2.6
 **/
void 
gtk_clipboard_request_image (GtkClipboard                  *clipboard,
			     GtkClipboardImageReceivedFunc  callback,
			     gpointer                       user_data)
{
  GdkPixbuf *pixbuf = gtk_clipboard_wait_for_image (clipboard);

  callback (clipboard, pixbuf, user_data);

  if (pixbuf)
    g_object_unref (pixbuf);
}

/**
 * gtk_clipboard_request_targets:
 * @clipboard: a #GtkClipboard
 * @callback:  a function to call when the targets are received,
 *             or the retrieval fails. (It will always be called
 *             one way or the other.)
 * @user_data: user data to pass to @callback.
 * 
 * Requests the contents of the clipboard as list of supported targets. 
 * When the list is later received, @callback will be called. 
 *
 * The @targets parameter to @callback will contain the resulting targets if
 * the request succeeded, or %NULL if it failed.
 *
 * Since: 2.4
 **/
void 
gtk_clipboard_request_targets (GtkClipboard                *clipboard,
			       GtkClipboardTargetsReceivedFunc callback,
			       gpointer                     user_data)
{
  GdkAtom *targets;
  gint n_targets;

  gtk_clipboard_wait_for_targets (clipboard, &targets, &n_targets);

  callback (clipboard, targets, n_targets, user_data);
}


/**
 * gtk_clipboard_wait_for_contents:
 * @clipboard: a #GtkClipboard
 * @target: an atom representing the form into which the clipboard
 *          owner should convert the selection.
 * 
 * Requests the contents of the clipboard using the given target.
 * This function waits for the data to be received using the main 
 * loop, so events, timeouts, etc, may be dispatched during the wait.
 * 
 * Return value: a newly-allocated #GtkSelectionData object or %NULL
 *               if retrieving the given target failed. If non-%NULL,
 *               this value must be freed with gtk_selection_data_free() 
 *               when you are finished with it.
 **/
GtkSelectionData *
gtk_clipboard_wait_for_contents (GtkClipboard *clipboard,
				 GdkAtom       target)
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  gchar *name;
  NSData *data;
  GtkSelectionData *selection_data = NULL;

  if (target == gdk_atom_intern_static_string ("TARGETS")) 
    {
      NSArray *types = [clipboard->pasteboard types];
      int i, count;
      GList *atom_list, *l;
      GdkAtom *atoms;

      count = [types count];
      atom_list = _gtk_quartz_pasteboard_types_to_atom_list (types);
      
      selection_data = g_new (GtkSelectionData, 1);
      selection_data->selection = clipboard->selection;
      selection_data->target = target;
      selection_data->type = GDK_SELECTION_TYPE_ATOM;
      selection_data->format = 32;
      selection_data->length = count * sizeof (GdkAtom);

      atoms = g_malloc (selection_data->length + 1);
      
      for (l = atom_list, i = 0; l ; l = l->next, i++)
	atoms[i] = GDK_POINTER_TO_ATOM (l->data);

      selection_data->data = (guchar *)atoms;
      selection_data->data[selection_data->length] = '\0';

      [pool release];

      g_list_free (atom_list);
      return selection_data;
    }

  selection_data = _gtk_quartz_get_selection_data_from_pasteboard (clipboard->pasteboard,
								   target,
								   clipboard->selection);

  [pool release];
  return selection_data;
}

/**
 * gtk_clipboard_wait_for_text:
 * @clipboard: a #GtkClipboard
 * 
 * Requests the contents of the clipboard as text and converts
 * the result to UTF-8 if necessary. This function waits for
 * the data to be received using the main loop, so events,
 * timeouts, etc, may be dispatched during the wait.
 * 
 * Return value: a newly-allocated UTF-8 string which must
 *               be freed with g_free(), or %NULL if retrieving
 *               the selection data failed. (This could happen
 *               for various reasons, in particular if the
 *               clipboard was empty or if the contents of the
 *               clipboard could not be converted into text form.)
 **/
gchar *
gtk_clipboard_wait_for_text (GtkClipboard *clipboard)
{
  GtkSelectionData *data;
  gchar *result;

  data = gtk_clipboard_wait_for_contents (clipboard, 
					  gdk_atom_intern_static_string ("UTF8_STRING"));

  result = (gchar *)gtk_selection_data_get_text (data);

  gtk_selection_data_free (data);

  return result;
}

/**
 * gtk_clipboard_wait_for_image:
 * @clipboard: a #GtkClipboard
 * 
 * Requests the contents of the clipboard as image and converts
 * the result to a #GdkPixbuf. This function waits for
 * the data to be received using the main loop, so events,
 * timeouts, etc, may be dispatched during the wait.
 * 
 * Return value: a newly-allocated #GdkPixbuf object which must
 *               be disposed with g_object_unref(), or %NULL if 
 *               retrieving the selection data failed. (This 
 *               could happen for various reasons, in particular 
 *               if the clipboard was empty or if the contents of 
 *               the clipboard could not be converted into an image.)
 *
 * Since: 2.6
 **/
GdkPixbuf *
gtk_clipboard_wait_for_image (GtkClipboard *clipboard)
{
  const gchar *priority[] = { "image/png", "image/tiff", "image/jpeg", "image/gif", "image/bmp" };
  int i;
  GtkSelectionData *data;

  for (i = 0; i < G_N_ELEMENTS (priority); i++) 
    {    
      data = gtk_clipboard_wait_for_contents (clipboard, gdk_atom_intern_static_string (priority[i]));

      if (data)
	{
	  GdkPixbuf *pixbuf = gtk_selection_data_get_pixbuf (data);

	  gtk_selection_data_free (data);

	  return pixbuf;
	}  
  }

  return NULL;
}

/**
 * gtk_clipboard_get_display:
 * @clipboard: a #GtkClipboard
 *
 * Gets the #GdkDisplay associated with @clipboard
 *
 * Return value: the #GdkDisplay associated with @clipboard
 *
 * Since: 2.2
 **/
GdkDisplay *
gtk_clipboard_get_display (GtkClipboard *clipboard)
{
  g_return_val_if_fail (clipboard != NULL, NULL);

  return clipboard->display;
}

/**
 * gtk_clipboard_wait_is_text_available:
 * @clipboard: a #GtkClipboard
 * 
 * Test to see if there is text available to be pasted
 * This is done by requesting the TARGETS atom and checking
 * if it contains any of the supported text targets. This function 
 * waits for the data to be received using the main loop, so events, 
 * timeouts, etc, may be dispatched during the wait.
 *
 * This function is a little faster than calling
 * gtk_clipboard_wait_for_text() since it doesn't need to retrieve
 * the actual text.
 * 
 * Return value: %TRUE is there is text available, %FALSE otherwise.
 **/
gboolean
gtk_clipboard_wait_is_text_available (GtkClipboard *clipboard)
{
  GtkSelectionData *data;
  gboolean result = FALSE;

  data = gtk_clipboard_wait_for_contents (clipboard, gdk_atom_intern_static_string ("TARGETS"));
  if (data)
    {
      result = gtk_selection_data_targets_include_text (data);
      gtk_selection_data_free (data);
    }

  return result;
}

gboolean
gtk_clipboard_wait_is_rich_text_available (GtkClipboard  *clipboard,
                                           GtkTextBuffer *buffer)
{
  GtkSelectionData *data;
  gboolean result = FALSE;

  g_return_val_if_fail (GTK_IS_CLIPBOARD (clipboard), FALSE);
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), FALSE);

  data = gtk_clipboard_wait_for_contents (clipboard, gdk_atom_intern_static_string ("TARGETS"));
  if (data)
    {
      result = gtk_selection_data_targets_include_rich_text (data, buffer);
      gtk_selection_data_free (data);
    }

  return result;
}

gboolean
gtk_clipboard_wait_is_image_available (GtkClipboard *clipboard)
{
  GtkSelectionData *data;
  gboolean result = FALSE;

  data = gtk_clipboard_wait_for_contents (clipboard, 
					  gdk_atom_intern_static_string ("TARGETS"));
  if (data)
    {
      result = gtk_selection_data_targets_include_image (data, FALSE);
      gtk_selection_data_free (data);
    }

  return result;
}

/**
 * gtk_clipboard_wait_for_targets
 * @clipboard: a #GtkClipboard
 * @targets: location to store an array of targets. The result
 *           stored here must be freed with g_free().
 * @n_targets: location to store number of items in @targets.
 *
 * Returns a list of targets that are present on the clipboard, or %NULL
 * if there aren't any targets available. The returned list must be 
 * freed with g_free().
 * This function waits for the data to be received using the main 
 * loop, so events, timeouts, etc, may be dispatched during the wait.
 *
 * Return value: %TRUE if any targets are present on the clipboard,
 *               otherwise %FALSE.
 *
 * Since: 2.4
 */
gboolean
gtk_clipboard_wait_for_targets (GtkClipboard  *clipboard, 
				GdkAtom      **targets,
				gint          *n_targets)
{
  GtkSelectionData *data;
  gboolean result = FALSE;
  
  g_return_val_if_fail (clipboard != NULL, FALSE);

  /* If the display supports change notification we cache targets */
  if (gdk_display_supports_selection_notification (gtk_clipboard_get_display (clipboard)) &&
      clipboard->n_cached_targets != -1)
    {
      if (n_targets)
 	*n_targets = clipboard->n_cached_targets;
 
      if (targets)
 	*targets = g_memdup (clipboard->cached_targets,
 			     clipboard->n_cached_targets * sizeof (GdkAtom));

       return TRUE;
    }
  
  if (n_targets)
    *n_targets = 0;
      
  if (targets)
    *targets = NULL;      

  data = gtk_clipboard_wait_for_contents (clipboard, gdk_atom_intern_static_string ("TARGETS"));

  if (data)
    {
      GdkAtom *tmp_targets;
      gint tmp_n_targets;
       
      result = gtk_selection_data_get_targets (data, &tmp_targets, &tmp_n_targets);
 
      if (gdk_display_supports_selection_notification (gtk_clipboard_get_display (clipboard)))
 	{
 	  clipboard->n_cached_targets = tmp_n_targets;
 	  clipboard->cached_targets = g_memdup (tmp_targets,
 						tmp_n_targets * sizeof (GdkAtom));
 	}
 
      if (n_targets)
 	*n_targets = tmp_n_targets;
 
      if (targets)
 	*targets = tmp_targets;
      else
 	g_free (tmp_targets);
      
      gtk_selection_data_free (data);
    }

  return result;
}

static GtkClipboard *
clipboard_peek (GdkDisplay *display, 
		GdkAtom     selection,
		gboolean    only_if_exists)
{
  GtkClipboard *clipboard = NULL;
  GSList *clipboards;
  GSList *tmp_list;

  if (selection == GDK_NONE)
    selection = GDK_SELECTION_CLIPBOARD;

  clipboards = g_object_get_data (G_OBJECT (display), "gtk-clipboard-list");

  tmp_list = clipboards;
  while (tmp_list)
    {
      clipboard = tmp_list->data;
      if (clipboard->selection == selection)
	break;

      tmp_list = tmp_list->next;
    }

  if (!tmp_list && !only_if_exists)
    {
      NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
      NSString *pasteboard_name;
      clipboard = g_object_new (GTK_TYPE_CLIPBOARD, NULL);

      if (selection == GDK_SELECTION_CLIPBOARD) 
	pasteboard_name = NSGeneralPboard;
      else 
	{
	  char *atom_string = gdk_atom_name (selection);

	  pasteboard_name = [NSString stringWithFormat:@"_GTK_%@", 
			     [NSString stringWithUTF8String:atom_string]];
	  g_free (atom_string);
	}

      clipboard->pasteboard = [NSPasteboard pasteboardWithName:pasteboard_name];

      [pool release];

      clipboard->selection = selection;
      clipboard->display = display;
      clipboard->n_cached_targets = -1;
      clipboard->n_storable_targets = -1;
      clipboards = g_slist_prepend (clipboards, clipboard);
      g_object_set_data (G_OBJECT (display), I_("gtk-clipboard-list"), clipboards);
      g_signal_connect (display, "closed",
			G_CALLBACK (clipboard_display_closed), clipboard);
      gdk_display_request_selection_notification (display, selection);
    }
  
  return clipboard;
}

static void
gtk_clipboard_owner_change (GtkClipboard        *clipboard,
			    GdkEventOwnerChange *event)
{
  if (clipboard->n_cached_targets != -1)
    {
      clipboard->n_cached_targets = -1;
      g_free (clipboard->cached_targets);
    }
}

/**
 * gtk_clipboard_wait_is_target_available:
 * @clipboard: a #GtkClipboard
 * @target:    A #GdkAtom indicating which target to look for.
 *
 * Checks if a clipboard supports pasting data of a given type. This
 * function can be used to determine if a "Paste" menu item should be
 * insensitive or not.
 *
 * If you want to see if there's text available on the clipboard, use
 * gtk_clipboard_wait_is_text_available () instead.
 *
 * Return value: %TRUE if the target is available, %FALSE otherwise.
 *
 * Since: 2.6
 */
gboolean
gtk_clipboard_wait_is_target_available (GtkClipboard *clipboard,
					GdkAtom       target)
{
  GdkAtom *targets;
  gint i, n_targets;
  gboolean retval = FALSE;
    
  if (!gtk_clipboard_wait_for_targets (clipboard, &targets, &n_targets))
    return FALSE;

  for (i = 0; i < n_targets; i++)
    {
      if (targets[i] == target)
	{
	  retval = TRUE;
	  break;
	}
    }

  g_free (targets);
  
  return retval;
}

/**
 * _gtk_clipboard_handle_event:
 * @event: a owner change event
 * 
 * Emits the ::owner_change signal on the appropriate @clipboard.
 *
 * Since: 2.6
 **/
void 
_gtk_clipboard_handle_event (GdkEventOwnerChange *event)
{
}


/**
 * gtk_clipboard_set_can_store:
 * @clipboard: a #GtkClipboard
 * @targets: array containing information about which forms should be stored
 *           or %NULL to indicate that all forms should be stored.
 * @n_targets: number of elements in @targets
 *
 * Hints that the clipboard data should be stored somewhere when the
 * application exits or when gtk_clipboard_store () is called.
 *
 * This value is reset when the clipboard owner changes.
 * Where the clipboard data is stored is platform dependent,
 * see gdk_display_store_clipboard () for more information.
 * 
 * Since: 2.6
 */
void
gtk_clipboard_set_can_store (GtkClipboard         *clipboard,
 			     const GtkTargetEntry *targets,
 			     gint                  n_targets)
{
  /* FIXME: Implement */
}

/**
 * gtk_clipboard_store:
 * @clipboard: a #GtkClipboard
 *
 * Stores the current clipboard data somewhere so that it will stay
 * around after the application has quit.
 *
 * Since: 2.6
 */
void
gtk_clipboard_store (GtkClipboard *clipboard)
{
  /* FIXME: Implement */
}

/* Stores all clipboard selections on all displays, called from
 * gtk_main_quit ().
 */
void
_gtk_clipboard_store_all (void)
{
  /* FIXME: Implement */
}

#define __GTK_CLIPBOARD_C__
#include "gtkaliasdef.c"
