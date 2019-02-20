/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * Copyright (C) 2004-2006 Christian Hammond
 * Copyright (C) 2008 Cody Russell
 * Copyright (C) 2008 Red Hat, Inc.
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

#ifndef __GTK_TEXT_H__
#define __GTK_TEXT_H__


#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkeditable.h>
#include <gtk/gtkentrybuffer.h>


G_BEGIN_DECLS

#define GTK_TYPE_TEXT                  (gtk_text_get_type ())
#define GTK_TEXT(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_TEXT, GtkText))
#define GTK_TEXT_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_TEXT, GtkTextClass))
#define GTK_IS_TEXT(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_TEXT))
#define GTK_IS_TEXT_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TEXT))
#define GTK_TEXT_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_TEXT, GtkTextClass))

typedef struct _GtkText              GtkText;
typedef struct _GtkTextClass         GtkTextClass;

struct _GtkText
{
  /*< private >*/
  GtkWidget  parent_instance;
};

/**
 * GtkTextClass:
 * @parent_class: The parent class.
 * @populate_popup: Class handler for the #GtkText::populate-popup signal. If
 *   non-%NULL, this will be called to add additional entries to the context
 *   menu when it is displayed.
 * @activate: Class handler for the #GtkText::activate signal. The default
 *   implementation calls gtk_window_activate_default() on the entry’s top-level
 *   window.
 * @move_cursor: Class handler for the #GtkText::move-cursor signal. The
 *   default implementation specifies the standard #GtkText cursor movement
 *   behavior.
 * @insert_at_cursor: Class handler for the #GtkText::insert-at-cursor signal.
 *   The default implementation inserts text at the cursor.
 * @delete_from_cursor: Class handler for the #GtkText::delete-from-cursor
 *   signal. The default implementation deletes the selection or the specified
 *   number of characters or words.
 * @backspace: Class handler for the #GtkText::backspace signal. The default
 *   implementation deletes the selection or a single character or word.
 * @cut_clipboard: Class handler for the #GtkText::cut-clipboard signal. The
 *   default implementation cuts the selection, if one exists.
 * @copy_clipboard: Class handler for the #GtkText::copy-clipboard signal. The
 *   default implementation copies the selection, if one exists.
 * @paste_clipboard: Class handler for the #GtkText::paste-clipboard signal.
 *   The default implementation pastes at the current cursor position or over
 *   the current selection if one exists.
 * @toggle_overwrite: Class handler for the #GtkText::toggle-overwrite signal.
 *   The default implementation toggles overwrite mode and blinks the cursor.
 * @insert_emoji: Class handler for the #GtkText::insert-emoji signal.
 *
 * Class structure for #GtkText. All virtual functions have a default
 * implementation. Derived classes may set the virtual function pointers for the
 * signal handlers to %NULL, but must keep @get_text_area_size and
 * @get_frame_size non-%NULL; either use the default implementation, or provide
 * a custom one.
 */
struct _GtkTextClass
{
  GtkWidgetClass parent_class;

  /* Hook to customize right-click popup */
  void (* populate_popup)     (GtkText         *self,
                               GtkWidget       *popup);

  /* Action signals
   */
  void (* activate)           (GtkText         *self);
  void (* move_cursor)        (GtkText         *self,
                               GtkMovementStep  step,
                               gint             count,
                               gboolean         extend);
  void (* insert_at_cursor)   (GtkText         *self,
                               const gchar     *str);
  void (* delete_from_cursor) (GtkText         *self,
                               GtkDeleteType    type,
                               gint             count);
  void (* backspace)          (GtkText         *self);
  void (* cut_clipboard)      (GtkText         *self);
  void (* copy_clipboard)     (GtkText         *self);
  void (* paste_clipboard)    (GtkText         *self);
  void (* toggle_overwrite)   (GtkText         *self);
  void (* insert_emoji)       (GtkText         *self);

  /*< private >*/

  /* Padding for future expansion */
  void (*_gtk_reserved1)      (void);
  void (*_gtk_reserved2)      (void);
  void (*_gtk_reserved3)      (void);
  void (*_gtk_reserved4)      (void);
  void (*_gtk_reserved5)      (void);
  void (*_gtk_reserved6)      (void);
};

GDK_AVAILABLE_IN_ALL
GType           gtk_text_get_type                       (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkWidget *     gtk_text_new                            (void);
GDK_AVAILABLE_IN_ALL
GtkWidget *     gtk_text_new_with_buffer                (GtkEntryBuffer  *buffer);

GDK_AVAILABLE_IN_ALL
GtkEntryBuffer *gtk_text_get_buffer                     (GtkText         *self);
GDK_AVAILABLE_IN_ALL
void            gtk_text_set_buffer                     (GtkText         *self,
                                                         GtkEntryBuffer  *buffer);

GDK_AVAILABLE_IN_ALL
void            gtk_text_set_visibility                 (GtkText         *self,
                                                         gboolean         visible);
GDK_AVAILABLE_IN_ALL
gboolean        gtk_text_get_visibility                 (GtkText         *self);

GDK_AVAILABLE_IN_ALL
void            gtk_text_set_invisible_char             (GtkText         *self,
                                                         gunichar         ch);
GDK_AVAILABLE_IN_ALL
gunichar        gtk_text_get_invisible_char             (GtkText         *self);
GDK_AVAILABLE_IN_ALL
void            gtk_text_unset_invisible_char           (GtkText         *self);

GDK_AVAILABLE_IN_ALL
void            gtk_text_set_overwrite_mode             (GtkText         *self,
                                                         gboolean         overwrite);
GDK_AVAILABLE_IN_ALL
gboolean        gtk_text_get_overwrite_mode             (GtkText         *self);

GDK_AVAILABLE_IN_ALL
void            gtk_text_set_max_length                 (GtkText         *self,
                                                         int              length);
GDK_AVAILABLE_IN_ALL
gint            gtk_text_get_max_length                 (GtkText         *self);
GDK_AVAILABLE_IN_ALL
guint16         gtk_text_get_text_length                (GtkText         *self);

GDK_AVAILABLE_IN_ALL
void            gtk_text_set_activates_default          (GtkText         *self,
                                                         gboolean         activates);
GDK_AVAILABLE_IN_ALL
gboolean        gtk_text_get_activates_default          (GtkText         *self);

GDK_AVAILABLE_IN_ALL
const char *    gtk_text_get_placeholder_text           (GtkText         *self);
GDK_AVAILABLE_IN_ALL
void            gtk_text_set_placeholder_text           (GtkText         *self,
                                                         const char      *text);

GDK_AVAILABLE_IN_ALL
void            gtk_text_set_input_purpose              (GtkText         *self,
                                                         GtkInputPurpose  purpose);
GDK_AVAILABLE_IN_ALL
GtkInputPurpose gtk_text_get_input_purpose              (GtkText         *self);

GDK_AVAILABLE_IN_ALL
void            gtk_text_set_input_hints                (GtkText         *self,
                                                         GtkInputHints    hints);
GDK_AVAILABLE_IN_ALL
GtkInputHints   gtk_text_get_input_hints                (GtkText         *self);

GDK_AVAILABLE_IN_ALL
void            gtk_text_set_attributes                 (GtkText         *self,
                                                         PangoAttrList   *attrs);
GDK_AVAILABLE_IN_ALL
PangoAttrList * gtk_text_get_attributes                 (GtkText         *self);

GDK_AVAILABLE_IN_ALL
void            gtk_text_set_tabs                       (GtkText         *self,
                                                         PangoTabArray   *tabs);

GDK_AVAILABLE_IN_ALL
PangoTabArray * gtk_text_get_tabs                       (GtkText         *self);

GDK_AVAILABLE_IN_ALL
void            gtk_text_grab_focus_without_selecting   (GtkText         *self);

G_END_DECLS

#endif /* __GTK_TEXT_H__ */
