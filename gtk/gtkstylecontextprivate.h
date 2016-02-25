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

#ifndef __GTK_STYLE_CONTEXT_PRIVATE_H__
#define __GTK_STYLE_CONTEXT_PRIVATE_H__

#include "gtkstylecontext.h"

#include "gtkcssnodeprivate.h"
#include "gtkicontheme.h"
#include "gtkstyleproviderprivate.h"
#include "gtkbitmaskprivate.h"
#include "gtkcssvalueprivate.h"

G_BEGIN_DECLS

GtkStyleContext *gtk_style_context_new_for_node              (GtkCssNode      *node);

GtkCssNode     *gtk_style_context_get_node                   (GtkStyleContext *context);
void            gtk_style_context_set_id                     (GtkStyleContext *context,
                                                              const char      *id);
const char *    gtk_style_context_get_id                     (GtkStyleContext *context);
GtkStyleProviderPrivate *
                gtk_style_context_get_style_provider         (GtkStyleContext *context);

void            gtk_style_context_save_named                 (GtkStyleContext *context,
                                                              const char      *name);
void            gtk_style_context_save_to_node               (GtkStyleContext *context,
                                                              GtkCssNode      *node);

GtkCssStyleChange *
                gtk_style_context_get_change                 (GtkStyleContext *context);

GtkCssStyle *   gtk_style_context_lookup_style               (GtkStyleContext *context);
GtkCssValue   * _gtk_style_context_peek_property             (GtkStyleContext *context,
                                                              guint            property_id);
const GValue * _gtk_style_context_peek_style_property        (GtkStyleContext *context,
                                                              GType            widget_type,
                                                              GParamSpec      *pspec);
void            gtk_style_context_validate                   (GtkStyleContext *context,
                                                              GtkCssStyleChange *change);
void            gtk_style_context_clear_property_cache       (GtkStyleContext *context);
gboolean       _gtk_style_context_check_region_name          (const gchar     *str);

gboolean       _gtk_style_context_resolve_color              (GtkStyleContext    *context,
                                                              GtkCssValue        *color,
                                                              GdkRGBA            *result);
void           _gtk_style_context_get_cursor_color           (GtkStyleContext    *context,
                                                              GdkRGBA            *primary_color,
                                                              GdkRGBA            *secondary_color);

void           _gtk_style_context_get_icon_extents           (GtkStyleContext    *context,
                                                              GdkRectangle       *extents,
                                                              gint                x,
                                                              gint                y,
                                                              gint                width,
                                                              gint                height);

PangoAttrList *_gtk_style_context_get_pango_attributes       (GtkStyleContext *context);

/* Accessibility support */
AtkAttributeSet *_gtk_style_context_get_attributes           (AtkAttributeSet    *attributes,
                                                              GtkStyleContext    *context,
                                                              GtkStateFlags       flags);

G_END_DECLS

#endif /* __GTK_STYLE_CONTEXT_PRIVATE_H__ */
