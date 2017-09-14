/* GSK - The GTK Scene Kit
 *
 * Copyright 2017 © Benjamin Otte
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

#ifndef __GSK_PIXEL_SHADER_H__
#define __GSK_PIXEL_SHADER_H__

#if !defined (__GSK_H_INSIDE__) && !defined (GSK_COMPILATION)
#error "Only <gsk/gsk.h> can be included directly."
#endif

#include <gsk/gsktypes.h>

G_BEGIN_DECLS

#define GSK_TYPE_PIXEL_SHADER (gsk_pixel_shader_get_type ())

#define GSK_PIXEL_SHADER(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GSK_TYPE_PIXEL_SHADER, GskPixelShader))
#define GSK_IS_PIXEL_SHADER(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GSK_TYPE_PIXEL_SHADER))

typedef struct _GskPixelShaderClass        GskPixelShaderClass;

typedef void (* GskShaderErrorFunc)       (GskPixelShader        *shader,
                                           gboolean               fatal,
                                           const GskCodeLocation *location,
                                           const GError          *error,
                                           gpointer               user_data);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GskPixelShader, g_object_unref)


GDK_AVAILABLE_IN_3_92
GType gsk_pixel_shader_get_type (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_3_92
GskPixelShader *        gsk_pixel_shader_new_for_data           (GBytes                 *source,
                                                                 GskShaderErrorFunc      error_func,
                                                                 gpointer                error_data);

GDK_AVAILABLE_IN_3_92
void                    gsk_pixel_shader_print                  (GskPixelShader         *shader,
                                                                 GString                *string);
GDK_AVAILABLE_IN_3_92
char *                  gsk_pixel_shader_to_string              (GskPixelShader         *shader);

G_END_DECLS

#endif /* __GSK_PIXEL_SHADER_H__ */
