/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc.
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
 *
 * Global clipboard abstraction.
 */

#ifndef __GTK_CLIPBOARD_PRIVATE_H__
#define __GTK_CLIPBOARD_PRIVATE_H__

#include <gtk/gtkclipboard.h>

#define GTK_CLIPBOARD_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_CLIPBOARD, GtkClipboardClass))
#define GTK_IS_CLIPBOARD_CLASS(klass)	        (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_CLIPBOARD))
#define GTK_CLIPBOARD_GET_CLASS(obj)            (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CLIPBOARD, GtkClipboardClass))

typedef struct _GtkClipboardClass GtkClipboardClass;

struct _GtkClipboard 
{
  GObject parent_instance;

  GdkAtom selection;

  GtkClipboardGetFunc get_func;
  GtkClipboardClearFunc clear_func;
  gpointer user_data;
  gboolean have_owner;

  guint32 timestamp;

  gboolean have_selection;
  GdkDisplay *display;

  GdkAtom *cached_targets;
  gint     n_cached_targets;

  gulong     notify_signal_id;
  gboolean   storing_selection;
  GMainLoop *store_loop;
  guint      store_timeout;
  gint       n_storable_targets;
  GdkAtom   *storable_targets;
};

struct _GtkClipboardClass
{
  GObjectClass parent_class;

  /* vfuncs */
  gboolean      (* set_contents)                (GtkClipboard                   *clipboard,
                                                 const GtkTargetEntry           *targets,
                                                 guint                           n_targets,
                                                 GtkClipboardGetFunc             get_func,
                                                 GtkClipboardClearFunc           clear_func,
                                                 gpointer                        user_data,
                                                 gboolean                        have_owner);
  void          (* clear)                       (GtkClipboard                   *clipboard);
  void          (* request_contents)            (GtkClipboard                   *clipboard,
                                                 GdkAtom                         target,
                                                 GtkClipboardReceivedFunc        callback,
                                                 gpointer                        user_data);
  void          (* set_can_store)               (GtkClipboard                   *clipboard,
                                                 const GtkTargetEntry           *targets,
                                                 gint                            n_targets);
  void          (* store)                       (GtkClipboard                   *clipboard);

  /* signals */
  void          (* owner_change)                (GtkClipboard                   *clipboard,
                                                 GdkEventOwnerChange            *event);
};

#endif /* __GTK_CLIPBOARD_PRIVATE_H__ */
