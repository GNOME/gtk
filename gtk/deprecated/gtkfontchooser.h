/* GTK - The GIMP Toolkit
 * gtkfontchooser.h - Abstract interface for font file selectors GUIs
 *
 * Copyright (C) 2006, Emmanuele Bassi
 * Copyright (C) 2011 Alberto Ruiz <aruiz@gnome.org>
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

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

/**
 * GtkFontFilterFunc:
 * @family: a `PangoFontFamily`
 * @face: a `PangoFontFace` belonging to @family
 * @data: (closure): user data passed to gtk_font_chooser_set_filter_func()
 *
 * The type of function that is used for deciding what fonts get
 * shown in a `GtkFontChooser`.
 *
 * See [method@Gtk.FontChooser.set_filter_func].
 *
 * Returns: %TRUE if the font should be displayed
 *
 * Deprecated: 4.20: There is no replacement
 */
typedef gboolean (*GtkFontFilterFunc) (const PangoFontFamily *family,
                                       const PangoFontFace   *face,
                                       gpointer               data);

/**
 * GtkFontChooserLevel:
 * @GTK_FONT_CHOOSER_LEVEL_FAMILY: Allow selecting a font family
 * @GTK_FONT_CHOOSER_LEVEL_STYLE: Allow selecting a specific font face
 * @GTK_FONT_CHOOSER_LEVEL_SIZE: Allow selecting a specific font size
 * @GTK_FONT_CHOOSER_LEVEL_VARIATIONS: Allow changing OpenType font variation axes
 * @GTK_FONT_CHOOSER_LEVEL_FEATURES: Allow selecting specific OpenType font features
 *
 * Specifies the granularity of font selection
 * that is desired in a `GtkFontChooser`.
 *
 * This enumeration may be extended in the future; applications should
 * ignore unknown values.
 *
 * Deprecated: 4.20: There is no replacement.
 */
typedef enum {
  GTK_FONT_CHOOSER_LEVEL_FAMILY     = 0,
  GTK_FONT_CHOOSER_LEVEL_STYLE      = 1 << 0,
  GTK_FONT_CHOOSER_LEVEL_SIZE       = 1 << 1,
  GTK_FONT_CHOOSER_LEVEL_VARIATIONS = 1 << 2,
  GTK_FONT_CHOOSER_LEVEL_FEATURES   = 1 << 3
} GtkFontChooserLevel;

#define GTK_TYPE_FONT_CHOOSER			(gtk_font_chooser_get_type ())
#define GTK_FONT_CHOOSER(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_FONT_CHOOSER, GtkFontChooser))
#define GTK_IS_FONT_CHOOSER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_FONT_CHOOSER))
#define GTK_FONT_CHOOSER_GET_IFACE(inst)	(G_TYPE_INSTANCE_GET_INTERFACE ((inst), GTK_TYPE_FONT_CHOOSER, GtkFontChooserIface))

typedef struct _GtkFontChooser      GtkFontChooser; /* dummy */
typedef struct _GtkFontChooserIface GtkFontChooserIface;

struct _GtkFontChooserIface
{
  GTypeInterface base_iface;

  /* Methods */
  PangoFontFamily * (* get_font_family)         (GtkFontChooser  *fontchooser);
  PangoFontFace *   (* get_font_face)           (GtkFontChooser  *fontchooser);
  int               (* get_font_size)           (GtkFontChooser  *fontchooser);

  void              (* set_filter_func)         (GtkFontChooser   *fontchooser,
                                                 GtkFontFilterFunc filter,
                                                 gpointer          user_data,
                                                 GDestroyNotify    destroy);

  /* Signals */
  void (* font_activated) (GtkFontChooser *chooser,
                           const char     *fontname);

  /* More methods */
  void              (* set_font_map)            (GtkFontChooser   *fontchooser,
                                                 PangoFontMap     *fontmap);
  PangoFontMap *    (* get_font_map)            (GtkFontChooser   *fontchooser);

  /*< private >*/
  /* Padding; remove in GTK-next */
  gpointer padding[10];
};

GDK_AVAILABLE_IN_ALL
GType            gtk_font_chooser_get_type                 (void) G_GNUC_CONST;

GDK_DEPRECATED_IN_4_10
PangoFontFamily *gtk_font_chooser_get_font_family          (GtkFontChooser   *fontchooser);
GDK_DEPRECATED_IN_4_10
PangoFontFace   *gtk_font_chooser_get_font_face            (GtkFontChooser   *fontchooser);
GDK_DEPRECATED_IN_4_10
int              gtk_font_chooser_get_font_size            (GtkFontChooser   *fontchooser);

GDK_DEPRECATED_IN_4_10
PangoFontDescription *
                 gtk_font_chooser_get_font_desc            (GtkFontChooser             *fontchooser);
GDK_DEPRECATED_IN_4_10
void             gtk_font_chooser_set_font_desc            (GtkFontChooser             *fontchooser,
                                                            const PangoFontDescription *font_desc);

GDK_DEPRECATED_IN_4_10
char *           gtk_font_chooser_get_font                 (GtkFontChooser   *fontchooser);

GDK_DEPRECATED_IN_4_10
void             gtk_font_chooser_set_font                 (GtkFontChooser   *fontchooser,
                                                            const char       *fontname);
GDK_DEPRECATED_IN_4_10
char *           gtk_font_chooser_get_preview_text         (GtkFontChooser   *fontchooser);
GDK_DEPRECATED_IN_4_10
void             gtk_font_chooser_set_preview_text         (GtkFontChooser   *fontchooser,
                                                            const char       *text);
GDK_DEPRECATED_IN_4_10
gboolean         gtk_font_chooser_get_show_preview_entry   (GtkFontChooser   *fontchooser);
GDK_DEPRECATED_IN_4_10
void             gtk_font_chooser_set_show_preview_entry   (GtkFontChooser   *fontchooser,
                                                            gboolean          show_preview_entry);
GDK_DEPRECATED_IN_4_10
void             gtk_font_chooser_set_filter_func          (GtkFontChooser   *fontchooser,
                                                            GtkFontFilterFunc filter,
                                                            gpointer          user_data,
                                                            GDestroyNotify    destroy);
GDK_DEPRECATED_IN_4_10
void             gtk_font_chooser_set_font_map             (GtkFontChooser   *fontchooser,
                                                            PangoFontMap     *fontmap);
GDK_DEPRECATED_IN_4_10
PangoFontMap *   gtk_font_chooser_get_font_map             (GtkFontChooser   *fontchooser);
GDK_DEPRECATED_IN_4_10
void             gtk_font_chooser_set_level                (GtkFontChooser   *fontchooser,
                                                            GtkFontChooserLevel level);
GDK_DEPRECATED_IN_4_10
GtkFontChooserLevel
                 gtk_font_chooser_get_level                (GtkFontChooser   *fontchooser);
GDK_DEPRECATED_IN_4_10
char *           gtk_font_chooser_get_font_features        (GtkFontChooser   *fontchooser);
GDK_DEPRECATED_IN_4_10
char *           gtk_font_chooser_get_language             (GtkFontChooser   *fontchooser);
GDK_DEPRECATED_IN_4_10
void             gtk_font_chooser_set_language             (GtkFontChooser   *fontchooser,
                                                            const char       *language);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GtkFontChooser, g_object_unref)

G_END_DECLS

