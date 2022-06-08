/*
 * Copyright © 2022 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#ifndef __GTK_INSCRIPTION_H__
#define __GTK_INSCRIPTION_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_INSCRIPTION         (gtk_inscription_get_type ())

GDK_AVAILABLE_IN_4_8
G_DECLARE_FINAL_TYPE (GtkInscription, gtk_inscription, GTK, INSCRIPTION, GtkWidget)

GDK_AVAILABLE_IN_4_8
GtkWidget *             gtk_inscription_new                     (const char             *text);

GDK_AVAILABLE_IN_4_8
const char *            gtk_inscription_get_text                (GtkInscription         *self);
GDK_AVAILABLE_IN_4_8
void                    gtk_inscription_set_text                (GtkInscription         *self,
                                                                 const char             *text);
GDK_AVAILABLE_IN_4_8
PangoAttrList *         gtk_inscription_get_attributes          (GtkInscription         *self);
GDK_AVAILABLE_IN_4_8
void                    gtk_inscription_set_attributes          (GtkInscription         *self,
                                                                 PangoAttrList          *attrs);

GDK_AVAILABLE_IN_4_8
guint                   gtk_inscription_get_min_chars           (GtkInscription         *self);
GDK_AVAILABLE_IN_4_8
void                    gtk_inscription_set_min_chars           (GtkInscription         *self,
                                                                 guint                   min_chars);
GDK_AVAILABLE_IN_4_8
guint                   gtk_inscription_get_nat_chars           (GtkInscription         *self);
GDK_AVAILABLE_IN_4_8
void                    gtk_inscription_set_nat_chars           (GtkInscription         *self,
                                                                 guint                   nat_chars);
GDK_AVAILABLE_IN_4_8
guint                   gtk_inscription_get_min_lines           (GtkInscription         *self);
GDK_AVAILABLE_IN_4_8
void                    gtk_inscription_set_min_lines           (GtkInscription         *self,
                                                                 guint                   min_lines);
GDK_AVAILABLE_IN_4_8
guint                   gtk_inscription_get_nat_lines           (GtkInscription         *self);
GDK_AVAILABLE_IN_4_8
void                    gtk_inscription_set_nat_lines           (GtkInscription         *self,
                                                                 guint                   nat_lines);

GDK_AVAILABLE_IN_4_8
float                   gtk_inscription_get_xalign              (GtkInscription         *self);
GDK_AVAILABLE_IN_4_8
void                    gtk_inscription_set_xalign              (GtkInscription         *self,
                                                                 float                   xalign);
GDK_AVAILABLE_IN_4_8
float                   gtk_inscription_get_yalign              (GtkInscription         *self);
GDK_AVAILABLE_IN_4_8
void                    gtk_inscription_set_yalign              (GtkInscription         *self,
                                                                 float                   yalign);

G_END_DECLS

#endif  /* __GTK_INSCRIPTION_H__ */
