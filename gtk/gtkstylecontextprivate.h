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

#include "gtkicontheme.h"
#include "gtkstyleproviderprivate.h"
#include "gtkbitmaskprivate.h"
#include "gtkcssvalueprivate.h"

G_BEGIN_DECLS

void            _gtk_style_context_set_widget                (GtkStyleContext *context,
                                                              GtkWidget       *widget);
void            gtk_style_context_set_id                     (GtkStyleContext *context,
                                                              const char      *id);
const char *    gtk_style_context_get_id                     (GtkStyleContext *context);

const GtkBitmask *
                _gtk_style_context_get_changes               (GtkStyleContext *context);

GtkCssStyle *   gtk_style_context_lookup_style               (GtkStyleContext *context);
GtkCssValue   * _gtk_style_context_peek_property             (GtkStyleContext *context,
                                                              guint            property_id);
const GValue * _gtk_style_context_peek_style_property        (GtkStyleContext *context,
                                                              GType            widget_type,
                                                              GParamSpec      *pspec);
void           _gtk_style_context_validate                   (GtkStyleContext *context,
                                                              gint64           timestamp,
                                                              GtkCssChange     change,
                                                              const GtkBitmask*parent_changes);
void           _gtk_style_context_queue_invalidate           (GtkStyleContext *context,
                                                              GtkCssChange     change);
void           _gtk_style_context_invalidate_root_node       (GtkStyleContext *context,
                                                              GtkCssChange     change);
gboolean       _gtk_style_context_check_region_name          (const gchar     *str);

gboolean       _gtk_style_context_resolve_color              (GtkStyleContext    *context,
                                                              GtkCssValue        *color,
                                                              GdkRGBA            *result,
                                                              GtkCssDependencies *dependencies);
void           _gtk_style_context_get_cursor_color           (GtkStyleContext    *context,
                                                              GdkRGBA            *primary_color,
                                                              GdkRGBA            *secondary_color);

void           _gtk_style_context_update_animating           (GtkStyleContext    *context);

void           _gtk_style_context_get_icon_extents           (GtkStyleContext    *context,
                                                              GdkRectangle       *extents,
                                                              gint                x,
                                                              gint                y,
                                                              gint                width,
                                                              gint                height);

/* Accessibility support */
AtkAttributeSet *_gtk_style_context_get_attributes           (AtkAttributeSet    *attributes,
                                                              GtkStyleContext    *context,
                                                              GtkStateFlags       flags);

G_END_DECLS

#endif /* __GTK_STYLE_CONTEXT_PRIVATE_H__ */
