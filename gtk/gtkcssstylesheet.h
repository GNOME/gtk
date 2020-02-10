/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
 *               2020 Benjamin Otte <otte@gnome.org>
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

#ifndef __GTK_CSS_STYLE_SHEET_H__
#define __GTK_CSS_STYLE_SHEET_H__

#include <gio/gio.h>
#include <gtk/css/gtkcss.h>

G_BEGIN_DECLS

#define GTK_TYPE_CSS_STYLE_SHEET         (gtk_css_style_sheet_get_type ())
#define GTK_CSS_STYLE_SHEET(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_CSS_STYLE_SHEET, GtkCssStyleSheet))
#define GTK_IS_CSS_STYLE_SHEET(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_CSS_STYLE_SHEET))

typedef struct _GtkCssStyleSheet GtkCssStyleSheet;
typedef struct _GtkCssStyleSheetClass GtkCssStyleSheetClass;


GDK_AVAILABLE_IN_ALL
GType                   gtk_css_style_sheet_get_type            (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkCssStyleSheet *      gtk_css_style_sheet_new                 (void);

GDK_AVAILABLE_IN_ALL
char *                  gtk_css_style_sheet_to_string           (GtkCssStyleSheet       *self);

GDK_AVAILABLE_IN_ALL
void                    gtk_css_style_sheet_load_from_data      (GtkCssStyleSheet       *self,
                                                                 const gchar            *data,
                                                                 gssize                  length);
GDK_AVAILABLE_IN_ALL
void                    gtk_css_style_sheet_load_from_file      (GtkCssStyleSheet       *self,
                                                                 GFile                  *file);
GDK_AVAILABLE_IN_ALL
void                    gtk_css_style_sheet_load_from_path      (GtkCssStyleSheet       *self,
                                                                 const gchar            *path);

GDK_AVAILABLE_IN_ALL
void                    gtk_css_style_sheet_load_from_resource  (GtkCssStyleSheet       *self,
                                                                 const gchar            *resource_path);

GDK_AVAILABLE_IN_ALL
void                    gtk_css_style_sheet_load_named          (GtkCssStyleSheet       *self,
                                                                 const char             *name,
                                                                 const char             *variant);

GDK_AVAILABLE_IN_ALL
void                    gtk_css_style_sheet_set_priority        (GtkCssStyleSheet       *self,
                                                                 guint                   priority);
GDK_AVAILABLE_IN_ALL
guint                   gtk_css_style_sheet_get_priority        (GtkCssStyleSheet       *self) G_GNUC_PURE;

G_END_DECLS

#endif /* __GTK_CSS_STYLE_SHEET_H__ */
