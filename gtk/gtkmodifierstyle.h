/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
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

#ifndef __GTK_MODIFIER_STYLE_H__
#define __GTK_MODIFIER_STYLE_H__

#include <glib-object.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTK_TYPE_MODIFIER_STYLE         (_gtk_modifier_style_get_type ())
#define GTK_MODIFIER_STYLE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_MODIFIER_STYLE, GtkModifierStyle))
#define GTK_MODIFIER_STYLE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST    ((c), GTK_TYPE_MODIFIER_STYLE, GtkModifierStyleClass))
#define GTK_IS_MODIFIER_STYLE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_MODIFIER_STYLE))
#define GTK_IS_MODIFIER_STYLE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE    ((c), GTK_TYPE_MODIFIER_STYLE))
#define GTK_MODIFIER_STYLE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS  ((o), GTK_TYPE_MODIFIER_STYLE, GtkModifierStyleClass))

typedef struct _GtkModifierStyle GtkModifierStyle;
typedef struct _GtkModifierStyleClass GtkModifierStyleClass;
typedef struct _GtkModifierStylePrivate GtkModifierStylePrivate;

struct _GtkModifierStyle
{
  GObject parent_object;
  GtkModifierStylePrivate *priv;
};

struct _GtkModifierStyleClass
{
  GObjectClass parent_class;

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

GType _gtk_modifier_style_get_type (void) G_GNUC_CONST;

GtkModifierStyle * _gtk_modifier_style_new (void);

void _gtk_modifier_style_set_background_color (GtkModifierStyle *style,
                                               GtkStateFlags     state,
                                               const GdkRGBA    *color);
void _gtk_modifier_style_set_color            (GtkModifierStyle *style,
                                               GtkStateFlags     state,
                                               const GdkRGBA    *color);
void _gtk_modifier_style_set_font             (GtkModifierStyle           *style,
                                               const PangoFontDescription *font_desc);

void _gtk_modifier_style_map_color            (GtkModifierStyle *style,
                                               const gchar      *name,
                                               const GdkRGBA    *color);

void _gtk_modifier_style_set_color_property   (GtkModifierStyle *style,
                                               GType             widget_type,
                                               const gchar      *prop_name,
                                               const GdkRGBA    *color);

G_END_DECLS

#endif /* __GTK_MODIFIER_STYLE_H__ */
