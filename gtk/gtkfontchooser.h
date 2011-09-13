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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#ifndef __GTK_FONT_CHOOSER_H__
#define __GTK_FONT_CHOOSER_H__

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

/**
 * GtkFontFilterFunc:
 * @family: a #PangoFontFamily
 * @face: a #PangoFontFace belonging to @family
 * @data: (closure): user data passed to gtk_font_chooser_set_filter_func()
 *
 * The type of function that is used for deciding what fonts get
 * shown in a #GtkFontChooser. See gtk_font_chooser_set_filter_func().
 *
 * Returns: %TRUE if the font should be displayed
 */
typedef gboolean (*GtkFontFilterFunc) (const PangoFontFamily *family,
                                       const PangoFontFace   *face,
                                       gpointer               data);

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
  PangoFontFamily * (* get_font_family)         (GtkFontChooser  *chooser);
  PangoFontFace *   (* get_font_face)           (GtkFontChooser  *chooser);
  gint              (* get_font_size)           (GtkFontChooser  *chooser);

  void              (* set_filter_func)         (GtkFontChooser   *chooser,
                                                 GtkFontFilterFunc filter,
                                                 gpointer          data,
                                                 GDestroyNotify    destroy);

  /* Signals */
  void (* font_activated) (GtkFontChooser *chooser,
                           const gchar    *fontname);

  /* Paddig */
  gpointer padding[12];
};

GType            gtk_font_chooser_get_type                 (void) G_GNUC_CONST;

PangoFontFamily *gtk_font_chooser_get_font_family          (GtkFontChooser   *fontchooser);
PangoFontFace   *gtk_font_chooser_get_font_face            (GtkFontChooser   *fontchooser);
gint             gtk_font_chooser_get_font_size            (GtkFontChooser   *fontchooser);

PangoFontDescription *
                 gtk_font_chooser_get_font_desc            (GtkFontChooser             *fontchooser);
void             gtk_font_chooser_set_font_desc            (GtkFontChooser             *fontchooser,
                                                            const PangoFontDescription *font_desc);

gchar*           gtk_font_chooser_get_font                 (GtkFontChooser   *fontchooser);

void             gtk_font_chooser_set_font                 (GtkFontChooser   *fontchooser,
                                                            const gchar      *fontname);
gchar*           gtk_font_chooser_get_preview_text         (GtkFontChooser   *fontchooser);
void             gtk_font_chooser_set_preview_text         (GtkFontChooser   *fontchooser,
                                                            const gchar      *text);
gboolean         gtk_font_chooser_get_show_preview_entry   (GtkFontChooser   *fontchooser);
void             gtk_font_chooser_set_show_preview_entry   (GtkFontChooser   *fontchooser,
                                                            gboolean          show_preview_entry);
void             gtk_font_chooser_set_filter_func          (GtkFontChooser   *fontchooser,
                                                            GtkFontFilterFunc filter,
                                                            gpointer          user_data,
                                                            GDestroyNotify    destroy);

G_END_DECLS

#endif /* __GTK_FONT_CHOOSER_H__ */
