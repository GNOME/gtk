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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_STYLE_CONTEXT_PRIVATE_H__
#define __GTK_STYLE_CONTEXT_PRIVATE_H__

#include "gtkstylecontext.h"
#include "gtksymboliccolor.h"
#include "gtkbitmaskprivate.h"

G_BEGIN_DECLS

const GValue * _gtk_style_context_peek_property              (GtkStyleContext *context,
                                                              const char      *property_name);
double         _gtk_style_context_get_number                 (GtkStyleContext *context,
                                                              const char      *property_name,
                                                              double           one_hundred_percent);
const GValue * _gtk_style_context_peek_style_property        (GtkStyleContext *context,
                                                              GType            widget_type,
                                                              GtkStateFlags    state,
                                                              GParamSpec      *pspec);
void           _gtk_style_context_invalidate_animation_areas (GtkStyleContext *context);
void           _gtk_style_context_coalesce_animation_areas   (GtkStyleContext *context,
                                                              GtkWidget       *widget);
gboolean       _gtk_style_context_check_region_name          (const gchar     *str);

gboolean       _gtk_style_context_resolve_color              (GtkStyleContext  *context,
                                                              GtkSymbolicColor *color,
                                                              GdkRGBA          *result);
void           _gtk_style_context_get_cursor_color           (GtkStyleContext *context,
                                                              GdkRGBA         *primary_color,
                                                              GdkRGBA         *secondary_color);

guint          _gtk_style_class_get_mask                     (const gchar     *class_name);
const gchar *  _gtk_style_class_get_name_from_mask           (guint            mask);
guint          _gtk_style_region_get_mask                    (const gchar     *region_name);
const gchar *  _gtk_style_region_get_name_from_mask          (guint            mask);

void            _gtk_widget_path_iter_add_classes            (GtkWidgetPath   *path,
							      gint             pos,
							      GtkBitmask      *classes);
void            _gtk_widget_path_iter_add_regions            (GtkWidgetPath   *path,
							      gint             pos,
							      GtkBitmask      *regions);

#define GTK_REGION_FLAGS_MASK  ((1 << 6) - 1)
#define GTK_REGION_FLAGS_NUM_BITS 7
#define GTK_REGION_ADDED (1 << 6)

G_END_DECLS

#endif /* __GTK_STYLE_CONTEXT_PRIVATE_H__ */
