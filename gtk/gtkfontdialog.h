/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2022 Red Hat, Inc.
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

#pragma once

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gdk/gdk.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkfontchooser.h>

G_BEGIN_DECLS

#define GTK_TYPE_FONT_DIALOG (gtk_font_dialog_get_type ())

GDK_AVAILABLE_IN_4_10
G_DECLARE_FINAL_TYPE (GtkFontDialog, gtk_font_dialog, GTK, FONT_DIALOG, GObject)

GDK_AVAILABLE_IN_4_10
GtkFontDialog *  gtk_font_dialog_new           (void);

GDK_AVAILABLE_IN_4_10
void             gtk_font_dialog_set_title     (GtkFontDialog        *self,
                                                const char           *title);

GDK_AVAILABLE_IN_4_10
gboolean         gtk_font_dialog_get_modal     (GtkFontDialog        *self);

GDK_AVAILABLE_IN_4_10
void             gtk_font_dialog_set_modal     (GtkFontDialog        *self,
                                                gboolean              modal);

typedef enum
{
  GTK_FONT_DIALOG_LEVEL_FAMILY     = 0,
  GTK_FONT_DIALOG_LEVEL_STYLE      = 1 << 0,
  GTK_FONT_DIALOG_LEVEL_SIZE       = 1 << 1,
  GTK_FONT_DIALOG_LEVEL_VARIATIONS = 1 << 2
} GtkFontDialogLevel;

GDK_AVAILABLE_IN_4_10
const char *     gtk_font_dialog_get_title     (GtkFontDialog        *self);

GDK_AVAILABLE_IN_4_10
void             gtk_font_dialog_set_level     (GtkFontDialog        *self,
                                                GtkFontDialogLevel    level);

GDK_AVAILABLE_IN_4_10
GtkFontDialogLevel
                 gtk_font_dialog_get_level     (GtkFontDialog        *self);

GDK_AVAILABLE_IN_4_10
void             gtk_font_dialog_set_language  (GtkFontDialog        *self,
                                                PangoLanguage        *language);

GDK_AVAILABLE_IN_4_10
PangoLanguage *  gtk_font_dialog_get_language  (GtkFontDialog        *self);

GDK_AVAILABLE_IN_4_10
PangoFontMap *   gtk_font_dialog_get_fontmap   (GtkFontDialog        *self);

GDK_AVAILABLE_IN_4_10
void             gtk_font_dialog_set_fontmap   (GtkFontDialog        *self,
                                                PangoFontMap         *fontmap);

GDK_AVAILABLE_IN_4_10
void             gtk_font_dialog_set_filter    (GtkFontDialog        *self,
                                                GtkFontFilterFunc     filter,
                                                gpointer              user_data,
                                                GDestroyNotify        destroy);

GDK_AVAILABLE_IN_4_10
void             gtk_font_dialog_choose_font   (GtkFontDialog        *self,
                                                GtkWindow            *parent,
                                                PangoFontDescription *initial_font,
                                                GCancellable         *cancellable,
                                                GAsyncReadyCallback   callback,
                                                gpointer              user_data);

GDK_AVAILABLE_IN_4_10
PangoFontDescription *
                 gtk_font_dialog_choose_font_finish
                                               (GtkFontDialog         *self,
                                                GAsyncResult          *result,
                                                GError               **error);

GDK_AVAILABLE_IN_4_10
void             gtk_font_dialog_choose_font_and_features
                                               (GtkFontDialog        *self,
                                                GtkWindow            *parent,
                                                PangoFontDescription *initial_font,
                                                GCancellable         *cancellable,
                                                GAsyncReadyCallback   callback,
                                                gpointer              user_data);

GDK_AVAILABLE_IN_4_10
gboolean        gtk_font_dialog_choose_font_and_features_finish
                                               (GtkFontDialog         *self,
                                                GAsyncResult          *result,
                                                PangoFontDescription **font_desc,
                                                char                 **font_features,
                                                GError               **error);

G_END_DECLS
