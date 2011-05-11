/* GTK - The GIMP Toolkit
 * Copyright (C) 2011      Alberto Ruiz <aruiz@gnome.org>
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GtkFontChooser widget for Gtk+, by Damon Chaplin, May 1998.
 * Based on the GnomeFontSelector widget, by Elliot Lee, but major changes.
 * The GnomeFontSelector was derived from app/text_tool.c in the GIMP.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#include <gtk/gtkvbox.h>

G_BEGIN_DECLS

#define GTK_TYPE_FONT_CHOOSER              (gtk_font_chooser_get_type ())
#define GTK_FONT_CHOOSER(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_FONT_CHOOSER, GtkFontChooser))
#define GTK_FONT_CHOOSER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_FONT_CHOOSER, GtkFontChooserClass))
#define GTK_IS_FONT_CHOOSER(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_FONT_CHOOSER))
#define GTK_IS_FONT_CHOOSER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_FONT_CHOOSER))
#define GTK_FONT_CHOOSER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_FONT_CHOOSER, GtkFontChooserClass))

typedef struct _GtkFontChooser              GtkFontChooser;
typedef struct _GtkFontChooserPrivate       GtkFontChooserPrivate;
typedef struct _GtkFontChooserClass         GtkFontChooserClass;

struct _GtkFontChooser
{
  GtkVBox parent_instance;

  /*< private >*/
  GtkFontChooserPrivate *priv;
};

struct _GtkFontChooserClass
{
  GtkVBoxClass parent_class;

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

/*****************************************************************************
 * GtkFontChooser functions.
 *   see the comments in the GtkFontChooserDialog functions.
 *****************************************************************************/

GType        gtk_font_chooser_get_type                 (void) G_GNUC_CONST;
GtkWidget*   gtk_font_chooser_new                      (void);
PangoFontFamily*
             gtk_font_chooser_get_family               (GtkFontChooser *fontchooser);
PangoFontFace*
             gtk_font_chooser_get_face                 (GtkFontChooser *fontchooser);
gint         gtk_font_chooser_get_size                 (GtkFontChooser *fontchooser);
gchar*       gtk_font_chooser_get_font_name            (GtkFontChooser *fontchooser);

gboolean     gtk_font_chooser_set_font_name            (GtkFontChooser *fontchooser,
                                                        const gchar    *fontname);
const gchar* gtk_font_chooser_get_preview_text         (GtkFontChooser *fontchooser);
void         gtk_font_chooser_set_preview_text         (GtkFontChooser *fontchooser,
                                                        const gchar    *text);
gboolean     gtk_font_chooser_get_show_preview_entry   (GtkFontChooser *fontchooser);
void         gtk_font_chooser_set_show_preview_entry   (GtkFontChooser *fontchooser,
                                                        gboolean        show_preview_entry);

#endif /* __GTK_FONT_CHOOSER_H__ */
